// bitnet: BitNet b1.58 inference in WebAssembly, the matrix work in WGSL on
// the host's WebGPU. A Go++ program on the wasigocvm machine; the model is
// a Hugging Face packed BitNet checkpoint (config.json, tokenizer.json,
// model.safetensors), such as microsoft/bitnet-b1.58-2B-4T.
//
//	compile.bat examples\bitnet\main.go -o examples\bitnet\web\bitnet.wasm
//	node WASMJsLoader/serve.mjs --mount ex=examples --mount models=<dir>
//	http://127.0.0.1:8792/ex/bitnet/web/  (see README.md)
//
// Flags:
//
//	-model DIR     the checkpoint directory (required)
//	-kernels DIR   the WGSL kernels (default: kernels next to this program)
//	-prompt TEXT   complete TEXT and exit; without it, chat on stdin
//	-ui            the Model UI protocol on stdin/stdout (see uiSink)
//	-stop FILE     with -ui: a file whose first byte, when nonzero, stops
//	               the answer being generated (a page maps shared memory)
//	-n N           most tokens per answer (0, the default, runs until an end token or the window is full)
//	-ctx N         context length (0, the default, is the model's max_position_embeddings)
//	-temp T        temperature (default: generation_config.json; 0 is greedy)
//	-top-p P       nucleus sampling (default: generation_config.json)
//	-min-p P       drop candidates under P times the best one's probability (default 0.1; 0 is off)
//	-repeat R      repetition penalty over the answer's last 256 tokens, never on end or special tokens (default 1.08; 1 is off)
//	-seed S        sampling seed (default: the clock)
//	-system TEXT   chat system message
//	-debug         print the prompt's ids and its top logits
//	-serve ADDR    stay up and serve Ollama's HTTP API (127.0.0.1:11434)
package main

import (
	"bufio"
	"encoding/base64"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"math"
	"os"
	"sort"
	"strconv"
	"strings"
	"time"
	"tokenizers"
	"unicode/utf8"

	"bitnet/chatfmt"
	"bitnet/model"
)

type genConfig struct {
	temp float64
	topP float64
	topK int // generation_config top_k; 0 means no cutoff
	eos  []int
}

func loadGen(path string) genConfig {
	g := genConfig{temp: 0.6, topP: 0.9}
	raw, err := os.ReadFile(path)
	if err != nil {
		return g
	}
	var v any
	if json.Unmarshal(raw, &v) != nil {
		return g
	}
	m, _ := v.(map[string]any)
	if f, ok := m["temperature"].(float64); ok {
		g.temp = f
	}
	if f, ok := m["top_p"].(float64); ok {
		g.topP = f
	}
	if f, ok := m["top_k"].(float64); ok && f >= 1 {
		g.topK = int(f)
	}
	if f, ok := m["eos_token_id"].(float64); ok {
		g.eos = append(g.eos, int(f))
	}
	if a, ok := m["eos_token_id"].([]any); ok {
		for _, e := range a {
			if f, ok := e.(float64); ok {
				g.eos = append(g.eos, int(f))
			}
		}
	}
	if do, ok := m["do_sample"].(bool); ok && !do {
		g.temp = 0
	}
	return g
}

func chatTemplateOf(path string) string {
	raw, err := os.ReadFile(path)
	if err != nil {
		return ""
	}
	var v any
	if json.Unmarshal(raw, &v) != nil {
		return ""
	}
	m, _ := v.(map[string]any)
	s, _ := m["chat_template"].(string)
	return s
}

// hiddenActivation is config.json's hidden_activation. Gemma, including VaultGemma, sets it.
func hiddenActivation(path string) string {
	raw, err := os.ReadFile(path)
	if err != nil {
		return ""
	}
	var v any
	if json.Unmarshal(raw, &v) != nil {
		return ""
	}
	m, _ := v.(map[string]any)
	s, _ := m["hidden_activation"].(string)
	return s
}

// ---- sampling

// rng is xorshift64*.
type rng struct {
	s uint64
}

func (r *rng) next() uint64 {
	r.s ^= r.s >> 12
	r.s ^= r.s << 25
	r.s ^= r.s >> 27
	return r.s * 2685821657736338717
}

func (r *rng) unit() float64 {
	return float64(r.next()>>11) / 9007199254740992.0
}

// key orders f32 bits as the floats they encode.
func key(b uint32) uint32 {
	if b&0x80000000 != 0 {
		return ^b
	}
	return b | 0x80000000
}

func bitsAt(l []byte, i int) uint32 {
	return uint32(l[4*i]) | uint32(l[4*i+1])<<8 | uint32(l[4*i+2])<<16 | uint32(l[4*i+3])<<24
}

func logitAt(l []byte, i int) float64 {
	return float64(math.Float32frombits(bitsAt(l, i)))
}

type cand struct {
	id int
	p  float64
}

// dist is softmax(logits / temp) over the tokens within 20 temperatures of
// the best (each other one is under e^-20), most likely first. temp <= 0
// gives the distribution at temperature 1, whose first entry is the argmax.
func dist(logits []byte, vocab int, temp float64) []cand {
	if temp <= 0 {
		temp = 1
	}
	best := 0
	bestKey := key(bitsAt(logits, 0))
	for i := 1; i < vocab; i++ {
		k := key(bitsAt(logits, i))
		if k > bestKey {
			best = i
			bestKey = k
		}
	}
	top := logitAt(logits, best)
	floor := key(math.Float32bits(float32(top - 20*temp)))
	var cs []cand
	sum := 0.0
	for i := 0; i < vocab; i++ {
		if key(bitsAt(logits, i)) < floor {
			continue
		}
		p := math.Exp((logitAt(logits, i) - top) / temp)
		cs = append(cs, cand{id: i, p: p})
		sum += p
	}
	for i := range cs {
		cs[i].p = cs[i].p / sum
	}
	sort.Slice(cs, func(i int, j int) bool { return cs[i].p > cs[j].p })
	return cs
}

// Sampling defaults beyond generation_config.json. minP drops a candidate
// under minP times the best one's probability: the long tail is where a small
// model's derailments start. repeatPen is the CTRL / llama.cpp repetition
// penalty, taken once per distinct token among the last repeatLast tokens of
// this answer. The prompt is not in that window, so the chat format's own
// turn tokens never count against the answer.
const (
	defaultMinP       = 0.1
	defaultRepeatPen  = 1.08
	defaultRepeatLast = 256
)

func putLogit(l []byte, i int, v float64) {
	put32(l[4*i:], math.Float32bits(float32(v)))
}

// penalize applies the repetition penalty in place to each distinct id in
// recent: a positive logit is divided by pen, a negative one multiplied.
// keep(id) true leaves that id alone: end tokens and specials, so a penalty
// can never hold an answer open past its end.
func penalize(logits []byte, vocab int, recent []int, pen float64, keep func(id int) bool) {
	if pen <= 1 {
		return
	}
	for i, id := range recent {
		if id < 0 || id >= vocab || (keep != nil && keep(id)) {
			continue
		}
		seen := false
		for _, prev := range recent[:i] {
			if prev == id {
				seen = true
				break
			}
		}
		if seen {
			continue
		}
		v := logitAt(logits, id)
		if v > 0 {
			v = v / pen
		} else {
			v = v * pen
		}
		putLogit(logits, id, v)
	}
}

// remember keeps the last n ids of this answer for penalize.
func remember(recent []int, id int, n int) []int {
	recent = append(recent, id)
	if n > 0 && len(recent) > n {
		recent = recent[len(recent)-n:]
	}
	return recent
}

// presenceStackCap stops a refrain from paying without bound. The first
// sighting costs only pen. Each further copy costs one more logit.
const presenceStackCap = 4.0

func countAt(counts []uint16, i int) uint16 {
	if counts == nil || i < 0 || i >= len(counts) {
		return 0
	}
	return counts[i]
}

// stackedPen is the idle-gate nudge the first time a token has been said,
// then one logit per extra copy, capped. pen 0 or a zero count costs nothing.
func stackedPen(pen float64, n uint16) float64 {
	if pen <= 0 || n == 0 {
		return 0
	}
	sub := pen
	if n > 1 {
		sub += float64(n - 1)
	}
	if sub > presenceStackCap {
		return presenceStackCap
	}
	return sub
}

func adjLogit(logits []byte, i int, counts []uint16, pen float64, keep func(int) bool) float64 {
	v := logitAt(logits, i)
	if keep != nil && keep(i) {
		return v
	}
	return v - stackedPen(pen, countAt(counts, i))
}

// pickPenalized is the argmax after stackedPen. pen 0 is plain argmax.
// keep(id) true is never reduced: an end token stays able to win.
func pickPenalized(logits []byte, vocab int, counts []uint16, pen float64, keep func(int) bool) int {
	best := 0
	bestV := math.Inf(-1)
	for i := 0; i < vocab; i++ {
		v := adjLogit(logits, i, counts, pen, keep)
		if v > bestV {
			best = i
			bestV = v
		}
	}
	return best
}

// distAdj is dist after stackedPen, so the nucleus is taken on the penalized
// scores. The logit buffer is left as the step wrote it. keep(id) true is
// not reduced.
func distAdj(logits []byte, vocab int, temp float64, counts []uint16, pen float64, keep func(int) bool) []cand {
	if temp <= 0 {
		temp = 1
	}
	bestV := adjLogit(logits, 0, counts, pen, keep)
	for i := 1; i < vocab; i++ {
		v := adjLogit(logits, i, counts, pen, keep)
		if v > bestV {
			bestV = v
		}
	}
	floorV := bestV - 20*temp
	var cs []cand
	sum := 0.0
	for i := 0; i < vocab; i++ {
		v := adjLogit(logits, i, counts, pen, keep)
		if v < floorV {
			continue
		}
		p := math.Exp((v - bestV) / temp)
		cs = append(cs, cand{id: i, p: p})
		sum += p
	}
	for i := range cs {
		cs[i].p = cs[i].p / sum
	}
	sort.Slice(cs, func(i int, j int) bool { return cs[i].p > cs[j].p })
	return cs
}

// mixTempAt is one engine's temperature, in gate order:
// 0 lite 0.6, 1 balanced 0.85, 2 thinking 0.7, 3 pro 0.95.
// One model is loaded, so the mixture is which temperature this token uses.
func mixTempAt(i int) float64 {
	switch i {
	case 1:
		return 0.85
	case 2:
		return 0.7
	case 3:
		return 0.95
	default:
		return 0.6
	}
}

func mixTemperature(weights [4]float64, x float64) float64 {
	if x < 0 {
		x = 0
	}
	acc := 0.0
	for i, p := range weights {
		acc += p
		if x <= acc || i == len(weights)-1 {
			return mixTempAt(i)
		}
	}
	return mixTempAt(3)
}

func (s *session) tokenTemp() float64 {
	if !s.set.mixOn {
		return s.set.temp
	}
	return mixTemperature(s.set.mix, s.r.unit())
}

func (s *session) beginCounts(ids []int) {
	if len(s.counts) != s.m.C.Vocab {
		s.counts = make([]uint16, s.m.C.Vocab)
	} else {
		for i := range s.counts {
			s.counts[i] = 0
		}
	}
	for _, id := range ids {
		if s.unpenalized(id) {
			continue
		}
		s.addCount(id)
	}
}

func (s *session) addCount(id int) {
	if s.unpenalized(id) {
		return
	}
	if id >= 0 && id < len(s.counts) && s.counts[id] < 65535 {
		s.counts[id]++
	}
}

// sample draws from dist's output: the most likely when temp <= 0, else
// within top-k, the min-p floor, and the top-p nucleus. topK 0 is no cutoff.
// end(id) true stays in the draw after those filters, at its own probability,
// so a stop token is not cut out of the answer.
func sample(cs []cand, temp float64, topP float64, minP float64, topK int, end func(int) bool, r *rng) cand {
	if len(cs) == 0 {
		return cand{}
	}
	if temp <= 0 {
		return cs[0]
	}
	keep := len(cs)
	if topK > 0 && topK < keep {
		keep = topK
	}
	if minP > 0 && minP < 1 && keep > 0 {
		floor := cs[0].p * minP
		for i := 0; i < keep; i++ {
			if cs[i].p < floor {
				keep = i
				break
			}
		}
	}
	if keep < 1 {
		keep = 1
	}
	if topP > 0 && topP < 1 {
		acc := 0.0
		for i := 0; i < keep; i++ {
			acc += cs[i].p
			if acc >= topP {
				keep = i + 1
				break
			}
		}
	}
	var extra []cand
	if end != nil {
		for _, c := range cs[keep:] {
			if !end(c.id) {
				continue
			}
			seen := false
			for i := 0; i < keep; i++ {
				if cs[i].id == c.id {
					seen = true
					break
				}
			}
			if !seen {
				extra = append(extra, c)
			}
		}
	}
	total := 0.0
	for i := 0; i < keep; i++ {
		total += cs[i].p
	}
	for _, c := range extra {
		total += c.p
	}
	x := r.unit() * total
	for i := 0; i < keep; i++ {
		x -= cs[i].p
		if x <= 0 {
			return cs[i]
		}
	}
	for _, c := range extra {
		x -= c.p
		if x <= 0 {
			return c
		}
	}
	if len(extra) > 0 {
		return extra[len(extra)-1]
	}
	return cs[keep-1]
}

// ---- generation

// settings are a chat's sampling knobs; the UI may change them per message.
type settings struct {
	temp       float64
	topP       float64
	minP       float64 // candidates under minP times the best are dropped; 0 is off
	repeat     float64 // repetition penalty over the answer's last repeatLast tokens; 1 is off
	repeatLast int
	maxNew     int
	topK       int // candidates reported per token (UI); 0 is none
	sampleK    int // top-k cutoff from generation_config or the chat; 0 is off
	presence   float64
	presenceOn bool
	mix        [4]float64
	mixOn      bool // draw each token's temperature from the MoE gate
}

// stats are one answer's numbers.
type stats struct {
	prompt    int
	prefillMs int
	tokens    int
	decodeMs  int
	pos       int
	ctx       int
	reason    string // eos, stop, max, ctx
}

// sink is where an answer goes: text on a terminal, or the UI's events.
type sink interface {
	Token(id int, text string, p float64, top []cand)
	Flow(seq int, pos int, id int) // an instrumented step's frame is in the -flow memory
	Done(st stats)
	Stopped() bool
}

type session struct {
	m      *model.Model
	tok    *tokenizers.Tokenizer
	gen    genConfig
	set    settings
	r      rng
	pos    int
	debug  bool
	flow   *flowOut // instrumented body is loaded (-flow); frames go to the page
	flowOn bool     // this answer records currents; the page sets it per chat
	counts []uint16 // how often each content token was said; end tokens are not counted
}

// flowOut writes each instrumented step's flow to the shared memory the page maps (-flow), one frame
// at a time: a 16-byte header (sequence number, position, token id, flow bytes, little-endian u32)
// then the flow (model.FlowLayout). The page copies a frame and writes its sequence number into
// FILE.ack; the next frame waits for that, so no frame is overwritten before the page has it.
type flowOut struct {
	path string
	seq  uint32
}

func le32(b []byte) uint32 {
	return uint32(b[0]) | uint32(b[1])<<8 | uint32(b[2])<<16 | uint32(b[3])<<24
}

func put32(b []byte, v uint32) {
	b[0] = byte(v)
	b[1] = byte(v >> 8)
	b[2] = byte(v >> 16)
	b[3] = byte(v >> 24)
}

func (f *flowOut) write(pos int, id int, data []byte) (uint32, error) {
	if f.seq > 0 {
		t0 := time.Now()
		for {
			b, err := os.ReadFile(f.path + ".ack")
			if err != nil {
				return 0, err
			}
			if len(b) >= 4 && le32(b) >= f.seq {
				break
			}
			if time.Now().Sub(t0) > 5*time.Second {
				return 0, errors.New("flow: the page did not read frame " + strconv.Itoa(int(f.seq)) + " within 5 s")
			}
		}
	}
	f.seq++
	frame := make([]byte, 16+len(data))
	put32(frame[0:], f.seq)
	put32(frame[4:], uint32(pos))
	put32(frame[8:], uint32(id))
	put32(frame[12:], uint32(len(data)))
	copy(frame[16:], data)
	if err := os.WriteFile(f.path, frame, 0o644); err != nil {
		return 0, errors.New("flow: " + err.Error())
	}
	return f.seq, nil
}

func (s *session) isEOS(id int) bool {
	for _, e := range s.gen.eos {
		if e == id {
			return true
		}
	}
	return false
}

// unpenalized is an id the repetition penalty never touches.
func (s *session) unpenalized(id int) bool {
	return s.isEOS(id) || s.tok.IsSpecial(id)
}

// feed runs ids through the model (a few per call) and returns the logits
// after the last.
func (s *session) feed(ids []int) ([]byte, error) {
	var logits []byte
	for at := 0; at < len(ids); {
		n := len(ids) - at
		if n > 16 {
			n = 16
		}
		var err error
		logits, err = s.m.Step(ids[at:at+n], s.pos)
		if err != nil {
			return nil, err
		}
		s.pos += n
		at += n
	}
	return logits, nil
}

// printable is the longest prefix of b that does not end inside a UTF-8
// sequence.
func printable(b []byte) int {
	for cut := len(b); cut >= 0 && cut >= len(b)-3; cut-- {
		if utf8.Valid(b[:cut]) {
			return cut
		}
	}
	return len(b)
}

// debugTop prints the prompt's ids and the n best logits after it.
func debugTop(prompt []int, logits []byte, vocab int, n int) {
	ids := "ids"
	for _, id := range prompt {
		ids += " " + strconv.Itoa(id)
	}
	fmt.Fprintln(os.Stderr, ids)
	cs := dist(logits, vocab, 1)
	for i := 0; i < n && i < len(cs); i++ {
		fmt.Fprintln(os.Stderr, "logit", cs[i].id, logitAt(logits, cs[i].id))
	}
}

// answer feeds prompt and samples until an end token, a stop, maxNew or a
// full context. It returns the end token (to feed before the next turn), or
// -1.
func (s *session) answer(prompt []int, out sink) (int, error) {
	t0 := time.Now()
	logits, err := s.feed(prompt)
	if err != nil {
		return -1, err
	}
	if s.debug {
		debugTop(prompt, logits, s.m.C.Vocab, 10)
	}
	t1 := time.Now()
	var pending []byte
	var recent []int
	n := 0
	end := -1
	reason := "max"
	for n < s.set.maxNew {
		if out.Stopped() {
			reason = "stop"
			break
		}
		penalize(logits, s.m.C.Vocab, recent, s.set.repeat, s.unpenalized)
		if s.set.presenceOn && n == 0 {
			s.beginCounts(prompt)
		}
		temp := s.tokenTemp()
		var cs []cand
		var pick cand
		isEnd := func(id int) bool { return s.isEOS(id) }
		if s.set.presenceOn && temp <= 0 {
			pick = cand{id: pickPenalized(logits, s.m.C.Vocab, s.counts, s.set.presence, isEnd), p: 1}
			cs = []cand{pick}
		} else if s.set.presenceOn {
			cs = distAdj(logits, s.m.C.Vocab, temp, s.counts, s.set.presence, isEnd)
			pick = sample(cs, temp, s.set.topP, s.set.minP, s.set.sampleK, isEnd, &s.r)
		} else {
			cs = dist(logits, s.m.C.Vocab, temp)
			pick = sample(cs, temp, s.set.topP, s.set.minP, s.set.sampleK, isEnd, &s.r)
		}
		if s.isEOS(pick.id) {
			end = pick.id
			reason = "eos"
			break
		}
		n++
		if s.set.presenceOn {
			s.addCount(pick.id)
		}
		recent = remember(recent, pick.id, s.set.repeatLast)
		pending = append(pending, s.tok.Bytes(pick.id)...)
		cut := printable(pending)
		text := string(pending[:cut])
		pending = pending[cut:]
		var top []cand
		if s.set.topK > 0 {
			k := s.set.topK
			if k > len(cs) {
				k = len(cs)
			}
			top = cs[:k]
		}
		out.Token(pick.id, text, pick.p, top)
		if s.pos >= s.m.Ctx {
			reason = "ctx"
			break
		}
		if s.flow != nil && s.flowOn {
			lg, fb, err := s.m.StepFlow(pick.id, s.pos)
			if err != nil {
				return -1, err
			}
			seq, err := s.flow.write(s.pos, pick.id, fb)
			if err != nil {
				return -1, err
			}
			out.Flow(int(seq), s.pos, pick.id)
			logits = lg
		} else {
			next := []int{pick.id}
			logits, err = s.m.Step(next, s.pos)
			if err != nil {
				return -1, err
			}
		}
		s.pos++
	}
	if len(pending) > 0 {
		out.Token(-1, string(pending), 0, nil)
	}
	t2 := time.Now()
	out.Done(stats{prompt: len(prompt), prefillMs: int(time.DurationSeconds(t1.Sub(t0)) * 1000), tokens: n,
		decodeMs: int(time.DurationSeconds(t2.Sub(t1)) * 1000), pos: s.pos, ctx: s.m.Ctx, reason: reason})
	return end, nil
}

// ---- the terminal

type textSink struct{}

func (t *textSink) Token(id int, text string, p float64, top []cand) {
	fmt.Print(text)
}

func (t *textSink) Flow(seq int, pos int, id int) {}

func (t *textSink) Done(st stats) {
	fmt.Println()
	fmt.Fprintln(os.Stderr, "["+strconv.Itoa(st.prompt)+" prompt tokens in "+strconv.Itoa(st.prefillMs)+" ms; "+
		strconv.Itoa(st.tokens)+" tokens in "+strconv.Itoa(st.decodeMs)+" ms, "+tokPerSec(st.tokens, st.decodeMs)+
		" tok/s; context "+strconv.Itoa(st.pos)+"/"+strconv.Itoa(st.ctx)+"]")
}

func (t *textSink) Stopped() bool {
	return false
}

func tokPerSec(n int, ms int) string {
	if ms <= 0 {
		return "-"
	}
	tenths := (n*10000 + ms/2) / ms
	return strconv.Itoa(tenths/10) + "." + strconv.Itoa(tenths%10)
}

func fail(err error) {
	fmt.Fprintln(os.Stderr, "bitnet:", err)
	os.Exit(1)
}

func dirOf(p string) string {
	i := strings.LastIndex(strings.ReplaceAll(p, "\\", "/"), "/")
	if i < 0 {
		return "."
	}
	return p[:i]
}

// conversation is the chat state across turns.
type conversation struct {
	s      *session
	bos    []int
	system string
	kind   chatfmt.Kind
	stop   int // the turn token fed before the next user message, or -1
	first  bool
	carry  int // the last answer's end token, fed before the next turn
}

func (c *conversation) reset() {
	c.s.pos = 0
	c.first = true
	c.carry = -1
}

// mediaMark stands where a message's tower tokens go while the chat format is
// rendered (a private-use rune, never in typed text).
const mediaMark = ""

// turn is the ids of a user message, in this model's own chat format. media
// (reserved tower ids, model.SetMedia) opens the message, before its text.
func (c *conversation) turn(text string, media []int) []int {
	var ids []int
	first := c.first
	if c.first {
		ids = append(ids, c.bos...)
		c.first = false
	} else if c.carry >= 0 {
		ids = append(ids, c.carry)
	}
	if len(media) == 0 {
		t := chatfmt.Render(c.kind, c.system, text, first)
		return append(ids, c.s.tok.Encode(t, true)...)
	}
	text = strings.ReplaceAll(text, mediaMark, "")
	t := chatfmt.Render(c.kind, c.system, mediaMark+"\n"+text, first)
	at := strings.Index(t, mediaMark)
	ids = append(ids, c.s.tok.Encode(t[:at], true)...)
	ids = append(ids, media...)
	return append(ids, c.s.tok.Encode(t[at+len(mediaMark):], true)...)
}

// f32s is little-endian f32 bytes, base64, as floats.
func f32s(b64 string) ([]float32, error) {
	b, err := base64.StdEncoding.DecodeString(b64)
	if err != nil {
		return nil, err
	}
	if len(b)%4 != 0 {
		return nil, errors.New("media: " + strconv.Itoa(len(b)) + " bytes is not f32")
	}
	o := make([]float32, len(b)/4)
	for i := range o {
		o[i] = math.Float32frombits(le32(b[4*i:]))
	}
	return o, nil
}

// mediaIds projects the chat command's media, [{"kind":"vision"|"audio","n":N,
// "feat":base64 f32 N*width,"phi":base64 f32 N}] (the page's tower.wasm), and
// returns their reserved ids in order.
func mediaIds(m *model.Model, v any) ([]int, error) {
	list, _ := v.([]any)
	var ids []int
	for _, it := range list {
		o, ok := it.(map[string]any)
		if !ok {
			return nil, errors.New("media: an item is not an object")
		}
		kind, _ := o["kind"].(string)
		n, _ := o["n"].(float64)
		fs, _ := o["feat"].(string)
		ps, _ := o["phi"].(string)
		feat, err := f32s(fs)
		if err != nil {
			return nil, err
		}
		phi, err := f32s(ps)
		if err != nil {
			return nil, err
		}
		got, err := m.SetMedia(kind, feat, int(n), phi)
		if err != nil {
			return nil, err
		}
		ids = append(ids, got...)
	}
	return ids, nil
}

func main() {
	modelDir := flag.String("model", "", "checkpoint directory")
	kernels := flag.String("kernels", "", "WGSL kernel directory")
	prompt := flag.String("prompt", "", "complete this text and exit")
	uiMode := flag.Bool("ui", false, "Model UI protocol on stdin/stdout")
	stopFile := flag.String("stop", "", "with -ui: stop flag file")
	maxNew := flag.Int("n", 0, "most tokens per answer (0 runs until the context is full)")
	ctx := flag.Int("ctx", 0, "context length (0 is the model's max_position_embeddings)")
	tempS := flag.String("temp", "", "temperature")
	topPS := flag.String("top-p", "", "nucleus sampling")
	minPS := flag.String("min-p", "", "drop candidates under this fraction of the best one's probability (0 is off)")
	repeatS := flag.String("repeat", "", "repetition penalty over the answer's last 256 tokens (1 is off)")
	seed := flag.Int("seed", 0, "sampling seed")
	system := flag.String("system", "You are a helpful AI assistant.", "chat system message")
	debug := flag.Bool("debug", false, "print prompt ids and the top logits")
	flowFile := flag.String("flow", "", "with -ui: shared memory for each generated token's currents (model/flow.go); the page writes each frame's number into FILE.ack; the cells are the model folder's volume.safetensors")
	flag.Parse()
	var ui *uiSink
	if *uiMode {
		ui = &uiSink{stopFile: *stopFile}
	}
	report := func(err error) {
		if ui != nil {
			ui.fatal(err)
		}
		fail(err)
	}
	if *modelDir == "" {
		report(errors.New("-model DIR is required"))
	}
	kdir := *kernels
	if kdir == "" {
		kdir = dirOf(os.Args[0]) + "/kernels"
	}

	tok, err := tokenizers.Load(*modelDir + "/tokenizer.json")
	if err != nil {
		report(err)
	}
	gen := loadGen(*modelDir + "/generation_config.json")
	if *tempS != "" {
		if gen.temp, err = strconv.ParseFloat(*tempS); err != nil {
			report(errors.New("-temp: " + err.Error()))
		}
	}
	if *topPS != "" {
		if gen.topP, err = strconv.ParseFloat(*topPS); err != nil {
			report(errors.New("-top-p: " + err.Error()))
		}
	}
	minP := defaultMinP
	if *minPS != "" {
		if minP, err = strconv.ParseFloat(*minPS); err != nil {
			report(errors.New("-min-p: " + err.Error()))
		}
	}
	repeat := defaultRepeatPen
	if *repeatS != "" {
		if repeat, err = strconv.ParseFloat(*repeatS); err != nil {
			report(errors.New("-repeat: " + err.Error()))
		}
	}
	t0 := time.Now()
	if ui == nil {
		fmt.Fprintln(os.Stderr, "loading", *modelDir)
	}
	m, err := model.Load(*modelDir, kdir, *ctx, func(done float64) {
		if ui != nil {
			ui.progress(done)
		} else {
			fmt.Fprintln(os.Stderr, "loading "+strconv.Itoa(int(done*100))+" %")
		}
	})
	if err != nil {
		report(err)
	}
	var fo *flowOut
	if *flowFile != "" {
		if err := m.Instrument(*modelDir + "/volume.safetensors"); err != nil {
			report(err)
		}
		fo = &flowOut{path: *flowFile}
	}
	loadMs := int(time.DurationSeconds(time.Now().Sub(t0)) * 1000)
	if len(gen.eos) == 0 {
		gen.eos = append(gen.eos, tok.ID("<|end_of_text|>"))
	}

	nTok := *maxNew
	if nTok <= 0 {
		nTok = m.Ctx
	}
	s := &session{m: m, tok: tok, gen: gen, debug: *debug, flow: fo, flowOn: fo != nil,
		set: settings{temp: gen.temp, topP: gen.topP, minP: minP, repeat: repeat, repeatLast: defaultRepeatLast, maxNew: nTok, sampleK: gen.topK}}
	s.r.s = uint64(*seed)
	if s.r.s == 0 {
		s.r.s = uint64(time.Now().UnixNano()) | 1
	}
	kind := chatfmt.Choose(func(piece string) bool { return tok.ID(piece) >= 0 },
		chatTemplateOf(*modelDir+"/tokenizer_config.json"), hiddenActivation(*modelDir+"/config.json"))
	conv := &conversation{s: s, system: *system, kind: kind, stop: -1}
	conv.reset()
	if m.C.BOS >= 0 {
		conv.bos = append(conv.bos, m.C.BOS)
	}
	for _, piece := range chatfmt.Stops(kind) {
		id := tok.ID(piece)
		if id < 0 {
			continue
		}
		if conv.stop < 0 {
			conv.stop = id
		}
		if !s.isEOS(id) {
			s.gen.eos = append(s.gen.eos, id)
		}
	}

	if ui != nil {
		ui.serve(conv, *modelDir, loadMs)
		return
	}
	fmt.Fprintln(os.Stderr, m.Info())
	text := &textSink{}
	if *prompt != "" {
		ids := append(conv.bos, tok.Encode(*prompt, true)...)
		fmt.Print(*prompt)
		if _, err := s.answer(ids, text); err != nil {
			fail(err)
		}
		return
	}
	fmt.Fprintln(os.Stderr, "chat ("+chatfmt.Name(conv.kind)+"): type a message and press enter")
	in := bufio.NewScanner(os.Stdin)
	for {
		fmt.Print("> ")
		if !in.Scan() {
			return
		}
		line := strings.TrimSpace(in.Text())
		if line == "" {
			continue
		}
		ids := conv.turn(line, nil)
		if s.pos+len(ids) >= m.Ctx {
			fmt.Fprintln(os.Stderr, "context full; start again")
			return
		}
		end, err := s.answer(ids, text)
		if err != nil {
			fail(err)
		}
		conv.carry = end
	}
}

// ---- the Model UI (-ui)
//
// stdin, one JSON command per line:
//
//	{"op":"chat","text":"...","temp":0.6,"top_p":0.9,"top_k":64,"min_p":0.05,"repeat":1.08,"repeat_last":256,"max":160,"topk":5,"seed":1,"system":"..."}
//
// mix is the four engine gates and draws each token's temperature. presence is the idle-gate
// penalty on content tokens. top_k is the sampling cutoff. topk is how many candidates to report.
// End tokens stay out of both penalties and stay in the draw, so the answer can stop.
//
// max, when present, caps that one answer. Absent runs until an end token or the window.
//
//	{"op":"reset"}    a new chat: the context is cleared
//
// Every chat field but text is optional and stays set for later messages;
// system takes effect at the start of a chat. stdout, one JSON event per
// line:
//
//	{"ev":"progress","p":0.42}                weights uploaded so far
//	{"ev":"ready",...}                        device, model, memory, defaults
//	{"ev":"token","id":9,"text":"..","p":0.8,"top":[{"id":..,"text":"..","p":..}]}
//	{"ev":"done","reason":"eos|stop|max|ctx","prompt":..,"prefill_ms":..,"tokens":..,"decode_ms":..,"pos":..,"ctx":..}
//	{"ev":"reset","pos":0}
//	{"ev":"notice","msg":".."}                a command that was not run
//	{"ev":"error","msg":".."}                 fatal; the program exits
//
// Stop: the page maps shared memory as the -stop file; a nonzero first byte
// ends the answer at the next token.
type uiSink struct {
	stopFile string
	tok      *tokenizers.Tokenizer
}

// jstr is s as a JSON string.
func jstr(s string) string {
	b := []byte(`"`)
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c == 34 {
			b = append(b, []byte(`\"`)...)
		} else if c == 92 {
			b = append(b, []byte(`\\`)...)
		} else if c == 10 {
			b = append(b, []byte(`\n`)...)
		} else if c == 13 {
			b = append(b, []byte(`\r`)...)
		} else if c == 9 {
			b = append(b, []byte(`\t`)...)
		} else if c < 32 {
			hex := "0123456789abcdef"
			b = append(b, []byte(`\u00`)...)
			b = append(b, hex[c>>4], hex[c&15])
		} else {
			b = append(b, c)
		}
	}
	b = append(b, 34)
	return string(b)
}

// jnum is f with six decimals (probabilities, temperatures).
func jnum(f float64) string {
	if f != f || f > 1e15 || f < -1e15 {
		return "0"
	}
	neg := f < 0
	if neg {
		f = -f
	}
	u := int64(math.Floor(f*1e6 + 0.5))
	frac := strconv.FormatInt(u%1000000, 10)
	for len(frac) < 6 {
		frac = "0" + frac
	}
	s := strconv.FormatInt(u/1000000, 10) + "." + frac
	if neg {
		s = "-" + s
	}
	return s
}

func jint(n int) string {
	return strconv.Itoa(n)
}

// tokenText is a token's text for display: its bytes, or <0xNN> for bytes
// that are part of a character split across tokens.
func (u *uiSink) tokenText(id int) string {
	b := u.tok.Bytes(id)
	if utf8.Valid(b) {
		return string(b)
	}
	hex := "0123456789ABCDEF"
	s := ""
	for _, c := range b {
		s += "<0x" + string(hex[c>>4]) + string(hex[c&15]) + ">"
	}
	return s
}

func (u *uiSink) emit(fields string) {
	fmt.Println("{" + fields + "}")
}

func (u *uiSink) progress(done float64) {
	u.emit(`"ev":"progress","p":` + jnum(done))
}

func (u *uiSink) notice(msg string) {
	u.emit(`"ev":"notice","msg":` + jstr(msg))
}

func (u *uiSink) fatal(err error) {
	u.emit(`"ev":"error","msg":` + jstr(err.Error()))
	os.Exit(1)
}

func (u *uiSink) Token(id int, text string, p float64, top []cand) {
	f := `"ev":"token","id":` + jint(id) + `,"text":` + jstr(text) + `,"p":` + jnum(p)
	if len(top) > 0 {
		f += `,"top":[`
		for i, c := range top {
			if i > 0 {
				f += ","
			}
			f += `{"id":` + jint(c.id) + `,"text":` + jstr(u.tokenText(c.id)) + `,"p":` + jnum(c.p) + `}`
		}
		f += "]"
	}
	u.emit(f)
}

func (u *uiSink) Flow(seq int, pos int, id int) {
	u.emit(`"ev":"flow","seq":` + jint(seq) + `,"pos":` + jint(pos) + `,"id":` + jint(id))
}

func (u *uiSink) Done(st stats) {
	u.emit(`"ev":"done","reason":` + jstr(st.reason) + `,"prompt":` + jint(st.prompt) + `,"prefill_ms":` +
		jint(st.prefillMs) + `,"tokens":` + jint(st.tokens) + `,"decode_ms":` + jint(st.decodeMs) + `,"pos":` +
		jint(st.pos) + `,"ctx":` + jint(st.ctx))
}

func (u *uiSink) Stopped() bool {
	if u.stopFile == "" {
		return false
	}
	b, err := os.ReadFile(u.stopFile)
	return err == nil && len(b) > 0 && b[0] != 0
}

func jlist(xs []string) string {
	s := "["
	for i, x := range xs {
		if i > 0 {
			s += ","
		}
		s += jstr(x)
	}
	return s + "]"
}

func (u *uiSink) ready(conv *conversation, modelDir string, loadMs int) {
	s := conv.s
	m := s.m
	c := m.C
	d := m.Dev
	name := strings.ReplaceAll(modelDir, "\\", "/")
	name = strings.TrimSuffix(name, "/")
	if i := strings.LastIndex(name, "/"); i >= 0 {
		name = name[i+1:]
	}
	f := `"ev":"ready","load_ms":` + jint(loadMs) + `,"chat":true,"chat_format":` + jstr(chatfmt.Name(conv.kind))
	towers := "false"
	if conv.s.m.HasTowers() {
		towers = "true"
	}
	f += `,"towers":` + towers
	f += `,"device":{"vendor":` + jstr(d.Info["vendor"]) + `,"architecture":` + jstr(d.Info["architecture"]) +
		`,"description":` + jstr(d.Info["description"]) + `,"type":` + jstr(d.Info["type"]) +
		`,"dot4":` + jstr(m.Dot4()) + `,"features":` + jlist(d.Features) + `,"wgsl":` + jlist(d.WGSL) +
		`,"maxBufferSize":` + d.Info["maxBufferSize"] + `,"maxStorageBufferBindingSize":` +
		d.Info["maxStorageBufferBindingSize"] + `}`
	f += `,"model":{"name":` + jstr(name) + `,"layers":` + jint(c.Layers) + `,"hidden":` + jint(c.Dim) +
		`,"ffn":` + jint(c.FFN) + `,"heads":` + jint(c.Heads) + `,"kv_heads":` + jint(c.KVHeads) +
		`,"head_dim":` + jint(c.HeadDim) + `,"vocab":` + jint(c.Vocab) + `,"ctx":` + jint(m.Ctx) +
		`,"max_pos":` + jint(c.MaxPos) + `}`
	f += `,"memory":{"weights":` + jint(m.WeightBytes()) + `,"embed":` + jint(m.EmbedBytes()) +
		`,"cache":` + jint(m.CacheBytes()) + `}`
	f += `,"defaults":{"temp":` + jnum(s.set.temp) + `,"top_p":` + jnum(s.set.topP) + `,"top_k":` + jint(s.set.sampleK) +
		`,"min_p":` + jnum(s.set.minP) + `,"repeat":` + jnum(s.set.repeat) + `,"max":` +
		jint(s.set.maxNew) + `,"system":` + jstr(conv.system) + `}`
	if m.Instrumented() {
		f += `,"flow":` + m.FlowLayout()
	}
	u.emit(f)
}

// serve answers the page's commands until stdin closes.
func (u *uiSink) serve(conv *conversation, modelDir string, loadMs int) {
	s := conv.s
	u.tok = s.tok
	u.ready(conv, modelDir, loadMs)
	in := bufio.NewScanner(os.Stdin)
	for in.Scan() {
		line := strings.TrimSpace(in.Text())
		if line == "" {
			continue
		}
		var v any
		if err := json.Unmarshal([]byte(line), &v); err != nil {
			u.notice("not JSON: " + err.Error())
			continue
		}
		cmd, _ := v.(map[string]any)
		op, _ := cmd["op"].(string)
		if op == "reset" {
			conv.reset()
			u.emit(`"ev":"reset","pos":0`)
			continue
		}
		if op != "chat" {
			u.notice("unknown op " + op)
			continue
		}
		// The page's MoE gate ("presence", "mix") is not read: sampling is the model's
		// temperature and top-p, min-p and the repetition penalty, nothing routed.
		if f, ok := cmd["temp"].(float64); ok && f >= 0 && f <= 2 {
			s.set.temp = f
		}
		if f, ok := cmd["top_p"].(float64); ok && f > 0 && f <= 1 {
			s.set.topP = f
		}
		if f, ok := cmd["min_p"].(float64); ok && f >= 0 && f < 1 {
			s.set.minP = f
		}
		if f, ok := cmd["repeat"].(float64); ok && f >= 1 && f <= 2 {
			s.set.repeat = f
		}
		if f, ok := cmd["repeat_last"].(float64); ok && f >= 16 && f <= 1024 {
			s.set.repeatLast = int(f)
		}
		if f, ok := cmd["top_k"].(float64); ok && f >= 0 {
			s.set.sampleK = int(f)
		}
		// max covers this answer only. A short summary must not stick and cap the next chat.
		thisMax := s.set.maxNew
		if f, ok := cmd["max"].(float64); ok && f >= 1 {
			thisMax = int(f)
		}
		if f, ok := cmd["topk"].(float64); ok && f >= 0 {
			s.set.topK = int(f)
		}
		if f, ok := cmd["seed"].(float64); ok && f != 0 {
			s.r.s = uint64(f)
		}
		// flow false is plain decode (Step). Absent keeps the current answer's mode,
		// which starts on when -flow loaded the instrumented body.
		if on, ok := cmd["flow"].(bool); ok {
			s.flowOn = on
		}
		if sys, ok := cmd["system"].(string); ok && conv.first {
			conv.system = sys
		}
		text, _ := cmd["text"].(string)
		media, err := mediaIds(s.m, cmd["media"])
		if err != nil {
			u.notice(err.Error())
			continue
		}
		if strings.TrimSpace(text) == "" && len(media) == 0 {
			u.notice("empty message")
			continue
		}
		first := conv.first
		carry := conv.carry
		ids := conv.turn(text, media)
		if s.pos+len(ids) >= s.m.Ctx {
			conv.first = first
			conv.carry = carry
			u.notice("the context is full: start a new chat")
			continue
		}
		savedMax := s.set.maxNew
		s.set.maxNew = thisMax
		end, err := s.answer(ids, u)
		s.set.maxNew = savedMax
		if err != nil {
			u.fatal(err)
		}
		if end < 0 {
			end = conv.stop
		}
		conv.carry = end
	}
}
