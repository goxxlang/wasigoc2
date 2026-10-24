// Package model is BitNet b1.58 inference on WebGPU: the Hugging Face
// BitNetForCausalLM architecture (packed ternary safetensors, "bitnet"
// quantization, autobitlinear), run as WGSL kernels (../kernels) through
// the go++ webgpu package.
//
// Sizes come from the model's config.json and the device's limits; nothing
// about a particular checkpoint is built in.
package model

import (
	"encoding/json"
	"errors"
	"fmt"
	"math"
	"os"
	"safetensors"
	"strconv"
	"strings"
	"webgpu"
)

// Config is what the forward pass needs from config.json.
type Config struct {
	Dim     int
	FFN     int
	Heads   int
	KVHeads int
	HeadDim int
	Layers  int
	Vocab   int
	MaxPos  int
	Eps     float64
	Theta   float64
	Tied    bool
	BOS     int
	Act     int // hidden_act: 0 relu2, 1 silu, 2 gelu tanh (norm.wgsl ACT)
	Gemma   int // 1: RMS is (1+weight) and the token embedding is scaled by sqrt(hidden)
	// rope_scaling (rope.wgsl): Scaling 0 none, 1 linear, 2 llama3
	Scaling                        int
	Factor, LowFF, HighFF, OrigPos float64
	EmbedScale                     float64 // 1, or sqrt(hidden) when Gemma is set
	AttnDenom                      float64 // query_pre_attn_scalar, else head_dim
}

func num(m map[string]any, k string) (float64, bool) {
	v, ok := m[k].(float64)
	return v, ok
}

// str is m[k] when it is a string, else "".
func str(m map[string]any, k string) string {
	s, _ := m[k].(string)
	return s
}

// LoadConfig reads config.json and checks it is a model this engine runs.
func LoadConfig(path string) (*Config, error) {
	raw, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var v any
	if err := json.Unmarshal(raw, &v); err != nil {
		return nil, errors.New(path + ": " + err.Error())
	}
	m, _ := v.(map[string]any)
	mt := str(m, "model_type")
	if mt == "" {
		return nil, errors.New(path + ": no model_type")
	}
	if mt == "gpt_oss" || strings.Contains(mt, "moe") || strings.Contains(mt, "mixtral") {
		return nil, errors.New(path + ": model_type " + mt + " is not a dense block this page runs")
	}
	// Packed weights are the tensors. A quantization_config the source already has must
	// be the packed-ternary marker. A file without one still loads.
	if q, ok := m["quantization_config"].(map[string]any); ok && q != nil {
		if str(q, "quant_method") != "bitnet" || str(q, "linear_class") != "autobitlinear" || str(q, "quantization_mode") != "offline" {
			return nil, errors.New(path + ": want quantization_config bitnet / autobitlinear / offline (packed weights)")
		}
	}
	// Rope defaults match rope.wgsl. Zero here is not "unset": the pipeline
	// specializes ORIG and LOW_FF, and Tint folds ORIG / LOW_FF to 0.0 / 0.0,
	// which the HLSL backend rejects.
	c := &Config{Eps: 1e-5, Theta: 10000, Tied: true, BOS: -1,
		Factor: 1, LowFF: 1, HighFF: 4, OrigPos: 8192, EmbedScale: 1}
	act := str(m, "hidden_act")
	if act == "" {
		act = str(m, "hidden_activation")
	}
	switch act {
	case "relu2":
	case "silu", "swish":
		c.Act = 1
	case "gelu_pytorch_tanh", "gelu_new":
		c.Act = 2
	default:
		return nil, errors.New(path + ": hidden_act " + act + " is not one norm.wgsl runs (relu2, silu, gelu tanh)")
	}
	// Gemma, including VaultGemma, names the activation hidden_activation. The stored
	// RMS weight is applied as (1+w), and the token is scaled by sqrt(hidden).
	// model_type stays the source's. A bitnet stamp that left hidden_activation in
	// the file is still this block.
	switch mt {
	case "gemma", "gemma2", "gemma3", "gemma3_text", "vaultgemma":
		c.Gemma = 1
	}
	if str(m, "hidden_activation") != "" || str(m, "rms_norm") == "gemma" {
		c.Gemma = 1
	}
	if rs, ok := m["rope_scaling"].(map[string]any); ok {
		t := str(rs, "rope_type")
		if t == "" {
			t = str(rs, "type")
		}
		f := func(k string, d float64) float64 {
			if v, ok := num(rs, k); ok {
				return v
			}
			return d
		}
		c.Factor, c.LowFF, c.HighFF, c.OrigPos = f("factor", 1), f("low_freq_factor", 1), f("high_freq_factor", 4), f("original_max_position_embeddings", 8192)
		switch t {
		case "", "default":
		case "linear":
			c.Scaling = 1
		case "llama3":
			c.Scaling = 2
		default:
			return nil, errors.New(path + ": rope_scaling " + t + " is not one rope.wgsl runs (linear, llama3)")
		}
	}
	if b, _ := m["attention_bias"].(bool); b {
		return nil, errors.New(path + ": attention_bias is set: attention.wgsl has no bias")
	}
	// Each run says which layers slide (layer_types, or use_sliding_window). A sliding_window
	// number beside an all-full_attention layer_types is not sliding attention. Same rule as
	// hodgemix champ.go runSlides.
	if slide, detail, errS := runSlides(m); errS != "" {
		return nil, errors.New(path + ": " + errS)
	} else if slide {
		return nil, errors.New(path + ": sliding-window attention (" + detail + "): attention.wgsl is full causal attention")
	}
	ints := []string{"hidden_size", "intermediate_size", "num_attention_heads", "num_key_value_heads",
		"num_hidden_layers", "vocab_size", "max_position_embeddings"}
	dst := []*int{&c.Dim, &c.FFN, &c.Heads, &c.KVHeads, &c.Layers, &c.Vocab, &c.MaxPos}
	for i, k := range ints {
		f, ok := num(m, k)
		if !ok {
			return nil, errors.New(path + ": no " + k)
		}
		*dst[i] = int(f)
	}
	if f, ok := num(m, "rms_norm_eps"); ok {
		c.Eps = f
	}
	if f, ok := num(m, "rope_theta"); ok {
		c.Theta = f
	}
	if b, ok := m["tie_word_embeddings"].(bool); ok {
		c.Tied = b
	}
	if f, ok := num(m, "bos_token_id"); ok {
		c.BOS = int(f)
	}
	c.HeadDim = c.Dim / c.Heads
	if f, ok := num(m, "head_dim"); ok {
		c.HeadDim = int(f)
	}
	if c.Dim%4 != 0 || c.FFN%4 != 0 || c.Heads%c.KVHeads != 0 || c.HeadDim%2 != 0 {
		return nil, errors.New(path + ": shape this engine cannot run")
	}
	c.AttnDenom = float64(c.HeadDim)
	if f, ok := num(m, "query_pre_attn_scalar"); ok && f > 0 {
		c.AttnDenom = f
	}
	if c.Gemma == 1 {
		c.EmbedScale = math.Sqrt(float64(c.Dim))
	}
	return c, nil
}

// runSlides is this run's own attention configuration. layer_types wins, then use_sliding_window.
// A sliding_window number with neither is not assumed: the run has to say which layers use it.
func runSlides(m map[string]any) (slide bool, detail string, errS string) {
	w, hasW := num(m, "sliding_window")
	use, hasUse := m["use_sliding_window"].(bool)
	raw, hasTypes := m["layer_types"].([]any)
	if hasTypes && len(raw) > 0 {
		if L, ok := num(m, "num_hidden_layers"); ok && int(L) > 0 && int(L) != len(raw) {
			return false, "", "layer_types has " + itoa(len(raw)) + " entries, the model has " + itoa(int(L)) + " layers"
		}
		n := 0
		for i, v := range raw {
			s, _ := v.(string)
			switch s {
			case "full_attention":
			case "sliding_attention":
				n++
				if !hasW || w <= 0 {
					return false, "", "layer " + itoa(i) + " is sliding_attention and sliding_window is unset"
				}
			default:
				return false, "", "layer_types \"" + s + "\" is not full_attention or sliding_attention"
			}
		}
		if n == 0 {
			return false, "", ""
		}
		return true, itoa(n) + " of " + itoa(len(raw)) + " layers sliding_attention, window " + fmt.Sprint(w), ""
	}
	if hasUse {
		if !use {
			return false, "", ""
		}
		if !hasW || w <= 0 {
			return false, "", "use_sliding_window is true and sliding_window is unset"
		}
		return true, "use_sliding_window, window " + fmt.Sprint(w), ""
	}
	if hasW && w > 0 {
		return false, "", "sliding_window is " + fmt.Sprint(w) + " but this run does not say which layers use it (layer_types or use_sliding_window)"
	}
	return false, "", ""
}

// region is part of a GPU buffer.
type region struct {
	buf  webgpu.Buffer
	off  int
	size int
}

func (r region) bind(binding int) webgpu.Binding {
	return webgpu.Binding{Binding: binding, Buffer: r.buf, Offset: r.off, Size: r.size}
}

// pool hands out aligned regions of large read-only buffers (weights).
type pool struct {
	d     *webgpu.Device
	block int
	align int
	cur   webgpu.Buffer
	used  int
	have  bool
	bytes int
}

func (p *pool) alloc(size int) (region, error) {
	if size > p.block {
		return region{}, errors.New("model: a " + strconv.Itoa(size) + "-byte tensor is over the device's binding limit")
	}
	at := (p.used + p.align - 1) / p.align * p.align
	if !p.have || at+size > p.block {
		b, err := p.d.NewBuffer(p.block)
		if err != nil {
			return region{}, err
		}
		p.cur = b
		p.have = true
		at = 0
		p.bytes += p.block
	}
	p.used = at + size
	return region{buf: p.cur, off: at, size: size}, nil
}

type linear struct {
	name  string
	w     region // packed rows
	s     region // weight scale (f32)
	k     int    // inputs
	n     int    // outputs
	limbs []*limb
}

// limb is a further digit matrix of a BitLinear (hodgemix limbs.go): X.weight_limb<i> packed like the
// ternary weight with digits of radix 2..4, its X.weight_limb<i>_scale, and X.weight_limb<i>_radix
// (I32, read on upload). Its output adds to the ternary limb's (bitlinear.wgsl CENTER, ACCUM).
type limb struct {
	w, s  region
	radix int
}

type layer struct {
	attnNorm, subNorm, ffnNorm, ffnSubNorm     region
	ffnNormName                                string // post_attention_layernorm, or pre_feedforward_layernorm
	qProj, kProj, vProj, oProj, gate, up, down *linear
}

type chunk struct {
	r     region
	first int
	rows  int
}

// Model is a loaded model on the device.
type Model struct {
	C   *Config
	Ctx int
	Dev *webgpu.Device

	kernelDir string
	dot4      string
	shaders   map[string]webgpu.Shader
	pipes     map[string]webgpu.Pipeline

	weights   pool
	layers    []*layer
	finalNorm region
	emb       []chunk
	headW     []chunk // the LM head's rows: the embedding when tied

	step, x, ao, qf, kf, vf, g, u, y, xq, xs, att, logits, kc, vc region

	body   webgpu.Program // every layer
	head   webgpu.Program // the final norm and the LM head
	embeds []webgpu.Group // per embedding chunk
	embedP webgpu.Pipeline
	prog   webgpu.Program
	ropeP  webgpu.Pipeline
	attnP  webgpu.Pipeline
	fl     *flowState // the instrumented body, after Instrument (flow.go)

	vis, aud *towerTab           // the towers' embedding layers, when the checkpoint has them (tower.go)
	slots    map[int]slot        // reserved ids past the vocabulary: tower tokens
	towerRaw map[string][]byte   // the embedding layers' bytes from upload, until loadTowers
}

func (m *Model) own(size int) (region, error) {
	b, err := m.Dev.NewBuffer(size)
	return region{buf: b, size: (size + 15) / 16 * 16}, err
}

// shader compiles kernels/<name>.wgsl, with dot4 in front when it asks.
func (m *Model) shader(name string) (webgpu.Shader, error) {
	if s, ok := m.shaders[name]; ok {
		return s, nil
	}
	src, err := os.ReadFile(m.kernelDir + "/" + name + ".wgsl")
	if err != nil {
		return 0, err
	}
	text := string(src)
	if strings.Contains(text, "dot4(") {
		pre, err := os.ReadFile(m.kernelDir + "/" + m.dot4 + ".wgsl")
		if err != nil {
			return 0, err
		}
		text = string(pre) + "\n" + text
	}
	s, err := m.Dev.NewShader(text)
	if err != nil {
		return 0, errors.New(name + ".wgsl: " + err.Error())
	}
	m.shaders[name] = s
	return s, nil
}

// pipe is kernel name with overrides, made once.
func (m *Model) pipe(name string, consts ...string) (webgpu.Pipeline, error) {
	key := name + " " + strings.Join(consts, " ")
	if p, ok := m.pipes[key]; ok {
		return p, nil
	}
	s, err := m.shader(name)
	if err != nil {
		return 0, err
	}
	p, err := m.Dev.NewPipeline(s, "main", consts...)
	if err != nil {
		return 0, errors.New(name + ": " + err.Error())
	}
	m.pipes[key] = p
	return p, nil
}

func itoa(n int) string { return strconv.Itoa(n) }

// dims spreads n workgroups over x (and y when n is over the limit).
func (m *Model) dims(n int) (int, int) {
	max := int(m.Dev.Limit("maxComputeWorkgroupsPerDimension"))
	if max <= 0 {
		max = 65535
	}
	if n <= max {
		return n, 1
	}
	return max, (n + max - 1) / max
}

// fitCtx is the context the device can hold. want 0, or a want above the model,
// is max_position_embeddings. The KV cache is one buffer, so a device whose
// binding is smaller gets the longest window that still binds.
func fitCtx(c *Config, want, maxBind int) int {
	ctx := want
	if ctx <= 0 || ctx > c.MaxPos {
		ctx = c.MaxPos
	}
	stride := c.Layers * c.KVHeads * c.HeadDim * 4
	if stride > 0 && maxBind > 0 {
		fit := maxBind / stride
		if fit < 1 {
			fit = 1
		}
		if ctx > fit {
			ctx = fit
		}
	}
	return ctx
}

// Load opens the device, compiles the kernels and uploads the weights of
// the model in dir. ctx is the context length; progress gets 0..1. ctx 0 is the model's.
func Load(dir string, kernelDir string, ctx int, progress func(done float64)) (*Model, error) {
	c, err := LoadConfig(dir + "/config.json")
	if err != nil {
		return nil, err
	}
	d, err := webgpu.Open()
	if err != nil {
		return nil, err
	}
	m := &Model{C: c, Ctx: ctx, Dev: d, kernelDir: kernelDir, shaders: map[string]webgpu.Shader{},
		pipes: map[string]webgpu.Pipeline{}}
	m.dot4 = "dot4_soft"
	if d.Has("packed_4x8_integer_dot_product") {
		m.dot4 = "dot4_native"
	}
	bind := d.Limit("maxStorageBufferBindingSize")
	if mb := d.Limit("maxBufferSize"); mb < bind {
		bind = mb
	}
	align := int(d.Limit("minStorageBufferOffsetAlignment"))
	if align <= 0 {
		align = 256
	}
	block := 256 << 20
	if int(bind) < block {
		block = int(bind) / align * align
	}
	m.weights = pool{d: d, block: block, align: align}
	m.Ctx = fitCtx(c, ctx, int(bind)/align*align)

	st, err := safetensors.Open(dir + "/model.safetensors")
	if err != nil {
		return nil, err
	}
	if err := m.alloc(st, int(bind)/align*align); err != nil {
		return nil, err
	}
	if err := m.upload(st, progress); err != nil {
		return nil, err
	}
	if err := m.loadTowers(st); err != nil {
		return nil, err
	}
	if err := m.record(); err != nil {
		return nil, err
	}
	return m, nil
}

// ffnPreNorm is the FFN pre-norm. Llama stores it as post_attention_layernorm.
// VaultGemma stores it as pre_feedforward_layernorm and has no post-attention norm.
func ffnPreNorm(st *safetensors.File, p string) string {
	post := p + "post_attention_layernorm.weight"
	if st.Get(post) != nil {
		return post
	}
	pre := p + "pre_feedforward_layernorm.weight"
	if st.Get(pre) != nil {
		return pre
	}
	return post
}

func (m *Model) tensor(st *safetensors.File, name string, dtype string) (*safetensors.Tensor, error) {
	t := st.Get(name)
	if t == nil {
		return nil, errors.New("model: no tensor " + name)
	}
	if t.DType != dtype {
		return nil, errors.New("model: " + name + " is " + t.DType + ", want " + dtype)
	}
	return t, nil
}

// f32of is a weight region for an n-value bf16 tensor, widened to f32.
func (m *Model) f32of(st *safetensors.File, name string, n int) (region, error) {
	t, err := m.tensor(st, name, "BF16")
	if err != nil {
		return region{}, err
	}
	if int(t.Elements()) != n {
		return region{}, errors.New("model: " + name + " has " + itoa(int(t.Elements())) + " values, want " + itoa(n))
	}
	return m.weights.alloc(4 * n)
}

// lin is the packed ternary weight and scale of a BitLinear k -> n.
func (m *Model) lin(st *safetensors.File, name string, k int, n int) (*linear, error) {
	t, err := m.tensor(st, name+".weight", "U8")
	if err != nil {
		return nil, err
	}
	if len(t.Shape) != 2 || t.Shape[0] != n/4 || t.Shape[1] != k {
		return nil, errors.New("model: " + name + ".weight is not packed [" + itoa(n/4) + ", " + itoa(k) + "]")
	}
	l := &linear{name: name, k: k, n: n}
	w, err := m.weights.alloc(int(t.Size()))
	if err != nil {
		return nil, err
	}
	l.w = w
	sc, err := m.f32of(st, name+".weight_scale", 1)
	if err != nil {
		return nil, err
	}
	l.s = sc
	for i := 1; ; i++ {
		ln := name + ".weight_limb" + itoa(i)
		lt := st.Get(ln)
		if lt == nil {
			break
		}
		if lt.DType != "U8" || len(lt.Shape) != 2 || lt.Shape[0] != n/4 || lt.Shape[1] != k {
			return nil, errors.New("model: " + ln + " is not packed [" + itoa(n/4) + ", " + itoa(k) + "]")
		}
		if rt := st.Get(ln + "_radix"); rt == nil || rt.DType != "I32" || rt.Elements() != 1 {
			return nil, errors.New("model: " + ln + " has no I32 radix")
		}
		lb := &limb{}
		if lb.w, err = m.weights.alloc(int(lt.Size())); err != nil {
			return nil, err
		}
		if lb.s, err = m.f32of(st, ln+"_scale", 1); err != nil {
			return nil, err
		}
		l.limbs = append(l.limbs, lb)
	}
	return l, nil
}

// chunks is a [vocab, hidden] bf16 tensor's rows in buffers no larger than
// a binding.
func (m *Model) chunks(st *safetensors.File, name string, maxBind int) ([]chunk, error) {
	c := m.C
	t, err := m.tensor(st, name, "BF16")
	if err != nil {
		return nil, err
	}
	if len(t.Shape) != 2 || t.Shape[0] != c.Vocab || t.Shape[1] != c.Dim {
		return nil, errors.New("model: " + name + " is not [vocab, hidden]")
	}
	rowBytes := 2 * c.Dim
	// The head binds logits at 4*first, so a chunk's first row keeps that offset aligned.
	step := m.weights.align / 4
	per := maxBind / rowBytes / step * step
	var out []chunk
	for first := 0; first < c.Vocab; first += per {
		rows := per
		if first+rows > c.Vocab {
			rows = c.Vocab - first
		}
		b, err := m.Dev.NewBuffer(rows * rowBytes)
		if err != nil {
			return nil, err
		}
		out = append(out, chunk{r: region{buf: b, size: rows * rowBytes}, first: first, rows: rows})
	}
	return out, nil
}

// alloc makes every buffer: weights by the safetensors header, activations
// and the KV cache by the config.
func (m *Model) alloc(st *safetensors.File, maxBind int) error {
	c := m.C
	hd := c.HeadDim
	var err error
	for i := 0; i < c.Layers; i++ {
		p := "model.layers." + itoa(i) + "."
		L := &layer{}
		if L.attnNorm, err = m.f32of(st, p+"input_layernorm.weight", c.Dim); err != nil {
			return err
		}
		if L.subNorm, err = m.f32of(st, p+"self_attn.attn_sub_norm.weight", c.Heads*hd); err != nil {
			return err
		}
		L.ffnNormName = ffnPreNorm(st, p)
		if L.ffnNorm, err = m.f32of(st, L.ffnNormName, c.Dim); err != nil {
			return err
		}
		if L.ffnSubNorm, err = m.f32of(st, p+"mlp.ffn_sub_norm.weight", c.FFN); err != nil {
			return err
		}
		if L.qProj, err = m.lin(st, p+"self_attn.q_proj", c.Dim, c.Heads*hd); err != nil {
			return err
		}
		if L.kProj, err = m.lin(st, p+"self_attn.k_proj", c.Dim, c.KVHeads*hd); err != nil {
			return err
		}
		if L.vProj, err = m.lin(st, p+"self_attn.v_proj", c.Dim, c.KVHeads*hd); err != nil {
			return err
		}
		if L.oProj, err = m.lin(st, p+"self_attn.o_proj", c.Heads*hd, c.Dim); err != nil {
			return err
		}
		if L.gate, err = m.lin(st, p+"mlp.gate_proj", c.Dim, c.FFN); err != nil {
			return err
		}
		if L.up, err = m.lin(st, p+"mlp.up_proj", c.Dim, c.FFN); err != nil {
			return err
		}
		if L.down, err = m.lin(st, p+"mlp.down_proj", c.FFN, c.Dim); err != nil {
			return err
		}
		m.layers = append(m.layers, L)
	}
	if m.finalNorm, err = m.f32of(st, "model.norm.weight", c.Dim); err != nil {
		return err
	}

	if m.emb, err = m.chunks(st, "model.embed_tokens.weight", maxBind); err != nil {
		return err
	}
	m.headW = m.emb
	if !c.Tied {
		if m.headW, err = m.chunks(st, "lm_head.weight", maxBind); err != nil {
			return err
		}
	}

	// Activations, scratch and the cache: each its own buffer, so a dispatch
	// never binds one buffer both read-only and writable.
	maxK := c.Dim
	if c.FFN > maxK {
		maxK = c.FFN
	}
	if c.Heads*hd > maxK {
		maxK = c.Heads * hd
	}
	sizes := []int{16, 4 * c.Dim, 4 * c.Heads * hd, 4 * c.Heads * hd, 4 * c.KVHeads * hd, 4 * c.KVHeads * hd,
		4 * c.FFN, 4 * c.FFN, 4 * c.Dim, maxK, 16, 4 * c.Heads * m.Ctx, 4 * c.Vocab}
	regs := []*region{&m.step, &m.x, &m.ao, &m.qf, &m.kf, &m.vf, &m.g, &m.u, &m.y, &m.xq, &m.xs, &m.att, &m.logits}
	for i, sz := range sizes {
		r, err := m.own(sz)
		if err != nil {
			return err
		}
		*regs[i] = r
	}
	cache := c.Layers * m.Ctx * c.KVHeads * hd * 4
	if cache > maxBind {
		return errors.New("model: a " + itoa(m.Ctx) + "-token KV cache is over the device's binding limit; use a smaller context")
	}
	if m.kc, err = m.own(cache); err != nil {
		return err
	}
	if m.vc, err = m.own(cache); err != nil {
		return err
	}
	return nil
}

// bf16 -> f32: each 2-byte value becomes the high half of a 4-byte one.
func widen(b []byte) []byte {
	out := make([]byte, 2*len(b))
	for i := 0; i+1 < len(b); i += 2 {
		out[2*i+2] = b[i]
		out[2*i+3] = b[i+1]
	}
	return out
}

// dest is where a tensor's bytes go: a region (wide: bf16 widened to f32),
// or embedding chunks of rowsz-byte rows.
type dest struct {
	r     region
	wide  bool
	emb   []chunk
	rowsz int
	radix *int // a limb's radix: read on the CPU, not uploaded
}

// upload streams the safetensors file to the device, in file order.
func (m *Model) upload(st *safetensors.File, progress func(done float64)) error {
	dests := map[string]*dest{}
	for i, L := range m.layers {
		p := "model.layers." + itoa(i) + "."
		dests[p+"input_layernorm.weight"] = &dest{r: L.attnNorm, wide: true}
		dests[p+"self_attn.attn_sub_norm.weight"] = &dest{r: L.subNorm, wide: true}
		dests[L.ffnNormName] = &dest{r: L.ffnNorm, wide: true}
		dests[p+"mlp.ffn_sub_norm.weight"] = &dest{r: L.ffnSubNorm, wide: true}
		lins := []*linear{L.qProj, L.kProj, L.vProj, L.oProj, L.gate, L.up, L.down}
		for _, l := range lins {
			dests[l.name+".weight"] = &dest{r: l.w}
			dests[l.name+".weight_scale"] = &dest{r: l.s, wide: true}
			limbs := l.limbs // wasigoc: an outer range variable's field in an inner range does not resolve
			for j, lb := range limbs {
				ln := l.name + ".weight_limb" + itoa(j+1)
				dests[ln] = &dest{r: lb.w}
				dests[ln+"_scale"] = &dest{r: lb.s, wide: true}
				dests[ln+"_radix"] = &dest{radix: &lb.radix}
			}
		}
	}
	dests["model.norm.weight"] = &dest{r: m.finalNorm, wide: true}
	dests["model.embed_tokens.weight"] = &dest{emb: m.emb, rowsz: 2 * m.C.Dim}
	if !m.C.Tied {
		dests["lm_head.weight"] = &dest{emb: m.headW, rowsz: 2 * m.C.Dim}
	}
	var total, done int64
	for _, t := range st.Tensors {
		if dests[t.Name] != nil {
			total += t.Size()
		}
	}
	// The towers' embedding layers come along in this pass: the page's weight heap
	// is read from the start only twice (header, then this), so there is no later read.
	m.towerRaw = map[string][]byte{}
	want := func(t *safetensors.Tensor) bool { return dests[t.Name] != nil || isTowerTensor(t.Name) }
	r, err := st.Stream(8<<20, want)
	if err != nil {
		return err
	}
	last := -1
	for r.Next() {
		t := r.Cur
		dst := dests[t.Name]
		if dst == nil {
			b := m.towerRaw[t.Name]
			if b == nil {
				b = make([]byte, int(t.Size()))
				m.towerRaw[t.Name] = b
			}
			copy(b[int(r.Off):], r.Data)
			continue
		}
		if dst.radix != nil {
			if len(r.Data) == 4 && r.Off == 0 {
				*dst.radix = int(uint32(r.Data[0]) | uint32(r.Data[1])<<8 | uint32(r.Data[2])<<16 | uint32(r.Data[3])<<24)
			}
		} else if dst.emb != nil {
			err = m.writeRows(dst, int(r.Off), r.Data)
		} else if dst.wide {
			err = m.Dev.Write(dst.r.buf, dst.r.off+2*int(r.Off), widen(r.Data))
		} else {
			err = m.Dev.Write(dst.r.buf, dst.r.off+int(r.Off), r.Data)
		}
		if err != nil {
			return errors.New(t.Name + ": " + err.Error())
		}
		done += int64(len(r.Data))
		if progress != nil && total > 0 {
			pct := int(done * 100 / total)
			if pct != last {
				last = pct
				progress(float64(done) / float64(total))
			}
		}
	}
	return r.Err
}

// writeRows puts bytes at byte offset pos of a chunked row tensor; a piece
// may cross from one chunk into the next.
func (m *Model) writeRows(dst *dest, pos int, data []byte) error {
	rest := data
	for len(rest) > 0 {
		row := pos / dst.rowsz
		ci := 0
		for ci+1 < len(dst.emb) && row >= dst.emb[ci+1].first {
			ci++
		}
		ch := dst.emb[ci]
		at := pos - ch.first*dst.rowsz
		n := ch.rows*dst.rowsz - at
		if n > len(rest) {
			n = len(rest)
		}
		if err := m.Dev.Write(ch.r.buf, ch.r.off+at, rest[:n]); err != nil {
			return err
		}
		pos += n
		rest = rest[n:]
	}
	return nil
}

// group binds regions 0, 1, ... of pipeline p.
func (m *Model) group(p webgpu.Pipeline, rs ...region) (webgpu.Group, error) {
	var bs []webgpu.Binding
	for i, r := range rs {
		bs = append(bs, r.bind(i))
	}
	return m.Dev.NewGroup(p, 0, bs...)
}

func (m *Model) dispatch(prog *webgpu.Program, p webgpu.Pipeline, g webgpu.Group, n int) {
	x, y := m.dims(n)
	prog.Dispatch(p, []webgpu.Group{g}, x, y, 1)
}

func (m *Model) normOp(prog *webgpu.Program, k int, mode int, a region, b region, w region) error {
	p, err := m.pipe("norm", "K="+itoa(k), "MODE="+itoa(mode), "EPS="+fmt.Sprint(m.C.Eps), "ACT="+itoa(m.C.Act), "GEMMA="+itoa(m.C.Gemma))
	if err != nil {
		return err
	}
	g, err := m.group(p, a, b, w, m.xq, m.xs, m.y)
	if err != nil {
		return err
	}
	m.dispatch(prog, p, g, 1)
	return nil
}

func (m *Model) linOp(prog *webgpu.Program, l *linear, out region, accum bool) error {
	acc := "ACCUM=0"
	if accum {
		acc = "ACCUM=1"
	}
	p, err := m.pipe("bitlinear", "K="+itoa(l.k), "NP="+itoa(l.n/4), acc)
	if err != nil {
		return err
	}
	g, err := m.group(p, m.xq, m.xs, l.w, l.s, out)
	if err != nil {
		return err
	}
	m.dispatch(prog, p, g, (l.n/4+7)/8)
	for _, lb := range l.limbs { // further digit limbs add their (code - CENTER) * scale
		if lb.radix < 2 || lb.radix > 4 {
			return errors.New("model: " + l.name + ": limb radix " + itoa(lb.radix) + " is not 2, 3 or 4")
		}
		lp, err := m.pipe("bitlinear", "K="+itoa(l.k), "NP="+itoa(l.n/4), "ACCUM=1", "CENTER="+itoa((lb.radix-1)/2))
		if err != nil {
			return err
		}
		lg, err := m.group(lp, m.xq, m.xs, lb.w, lb.s, out)
		if err != nil {
			return err
		}
		m.dispatch(prog, lp, lg, (l.n/4+7)/8)
	}
	return nil
}

// record builds the fixed parts of a step: every layer (body), then the
// final norm and the LM head (head). Per token only the step uniform and the
// embedding chunk change.
func (m *Model) record() error {
	c := m.C
	hd := c.HeadDim
	var err error
	m.embedP, err = m.pipe("embed", "D="+itoa(c.Dim), "SCALE="+fmt.Sprint(c.EmbedScale))
	if err != nil {
		return err
	}
	for _, ch := range m.emb {
		g, err := m.group(m.embedP, m.step, ch.r, m.x)
		if err != nil {
			return err
		}
		m.embeds = append(m.embeds, g)
	}
	rope, err := m.pipe("rope", "H="+itoa(c.Heads), "KVH="+itoa(c.KVHeads), "HD="+itoa(hd), "THETA="+fmt.Sprint(c.Theta),
		"SCALING="+itoa(c.Scaling), "FACTOR="+fmt.Sprint(c.Factor), "LOW_FF="+fmt.Sprint(c.LowFF), "HIGH_FF="+fmt.Sprint(c.HighFF), "ORIG="+fmt.Sprint(c.OrigPos))
	if err != nil {
		return err
	}
	attn, err := m.pipe("attention", "H="+itoa(c.Heads), "KVH="+itoa(c.KVHeads), "HD="+itoa(hd), "CTX="+itoa(m.Ctx), "DENOM="+fmt.Sprint(c.AttnDenom))
	if err != nil {
		return err
	}
	m.ropeP, m.attnP = rope, attn
	if err := m.layers2(&m.body, nil); err != nil {
		return err
	}
	h := &m.head
	if err := m.normOp(h, c.Dim, 2, m.x, m.x, m.finalNorm); err != nil {
		return err
	}
	for _, ch := range m.headW {
		p, err := m.pipe("lmhead", "D="+itoa(c.Dim), "ROWS="+itoa(ch.rows))
		if err != nil {
			return err
		}
		out := region{buf: m.logits.buf, off: 4 * ch.first, size: 4 * ch.rows}
		g, err := m.group(p, m.y, ch.r, out)
		if err != nil {
			return err
		}
		m.dispatch(h, p, g, (ch.rows+7)/8)
	}
	return nil
}

// layers2 records every layer into b; with fl, each BitLinear's currents and the cell sums between
// them are captured too (flow.go).
func (m *Model) layers2(b *webgpu.Program, fl *flowState) error {
	c := m.C
	hd := c.HeadDim
	per := m.Ctx * c.KVHeads * hd * 4
	for i, L := range m.layers {
		kc := region{buf: m.kc.buf, off: i * per, size: per}
		vc := region{buf: m.vc.buf, off: i * per, size: per}
		if err := m.capSum(fl, b, i, nodeD, m.x, m.x, axD, 0); err != nil {
			return err
		}
		if err := m.normOp(b, c.Dim, 0, m.x, m.x, L.attnNorm); err != nil {
			return err
		}
		if err := m.linOp(b, L.qProj, m.qf, false); err != nil {
			return err
		}
		if err := m.capLin(fl, b, i, roleQ, L.qProj); err != nil {
			return err
		}
		if err := m.linOp(b, L.kProj, m.kf, false); err != nil {
			return err
		}
		if err := m.capLin(fl, b, i, roleK, L.kProj); err != nil {
			return err
		}
		if err := m.linOp(b, L.vProj, m.vf, false); err != nil {
			return err
		}
		if err := m.capLin(fl, b, i, roleV, L.vProj); err != nil {
			return err
		}
		if err := m.capSum(fl, b, i, nodeQ, m.qf, m.qf, axQ, 0); err != nil {
			return err
		}
		if err := m.capSum(fl, b, i, nodeK, m.kf, m.kf, axKV, 0); err != nil {
			return err
		}
		if err := m.capSum(fl, b, i, nodeV, m.vf, m.vf, axKV, 0); err != nil {
			return err
		}
		g, err := m.group(m.ropeP, m.step, m.qf, m.kf, m.vf, kc, vc)
		if err != nil {
			return err
		}
		m.dispatch(b, m.ropeP, g, (c.Heads*hd/2+c.KVHeads*hd/2+63)/64)
		g, err = m.group(m.attnP, m.step, m.qf, kc, vc, m.att, m.ao)
		if err != nil {
			return err
		}
		m.dispatch(b, m.attnP, g, c.Heads)
		if err := m.capSum(fl, b, i, nodeA, m.ao, m.ao, axQ, 0); err != nil {
			return err
		}
		if err := m.normOp(b, c.Heads*hd, 0, m.ao, m.ao, L.subNorm); err != nil {
			return err
		}
		if err := m.linOp(b, L.oProj, m.x, true); err != nil {
			return err
		}
		if err := m.capLin(fl, b, i, roleO, L.oProj); err != nil {
			return err
		}
		if err := m.capSum(fl, b, i, nodeDm, m.x, m.x, axD, 0); err != nil {
			return err
		}
		if err := m.normOp(b, c.Dim, 0, m.x, m.x, L.ffnNorm); err != nil {
			return err
		}
		if err := m.linOp(b, L.gate, m.g, false); err != nil {
			return err
		}
		if err := m.capLin(fl, b, i, roleGate, L.gate); err != nil {
			return err
		}
		if err := m.linOp(b, L.up, m.u, false); err != nil {
			return err
		}
		if err := m.capLin(fl, b, i, roleUp, L.up); err != nil {
			return err
		}
		if err := m.capSum(fl, b, i, nodeGate, m.g, m.g, axF, 0); err != nil {
			return err
		}
		if err := m.capSum(fl, b, i, nodeUp, m.u, m.u, axF, 0); err != nil {
			return err
		}
		if err := m.capSum(fl, b, i, nodeH, m.g, m.u, axF, 1); err != nil {
			return err
		}
		if err := m.normOp(b, c.FFN, 1, m.g, m.u, L.ffnSubNorm); err != nil {
			return err
		}
		if err := m.linOp(b, L.down, m.x, true); err != nil {
			return err
		}
		if err := m.capLin(fl, b, i, roleDown, L.down); err != nil {
			return err
		}
		if err := m.capSum(fl, b, i, nodeDout, m.x, m.x, axD, 0); err != nil {
			return err
		}
	}
	return nil
}

// Step runs tokens at positions pos, pos+1, ... and returns the logits
// after the last one (little-endian f32 bits, one per vocabulary id).
func (m *Model) Step(tokens []int, pos int) ([]byte, error) {
	if pos+len(tokens) > m.Ctx {
		return nil, errors.New("model: context full (" + itoa(m.Ctx) + " tokens)")
	}
	p := &m.prog
	p.Reset()
	for i, tok := range tokens {
		if err := m.queueEmbed(p, tok, pos+i); err != nil {
			return nil, err
		}
		p.Append(&m.body)
	}
	p.Append(&m.head)
	p.Read(m.logits.buf, 0, 4*m.C.Vocab)
	return m.Dev.Run(p)
}

// queueEmbed writes the token's row into the residual stream. A reserved id past
// the vocabulary is a tower token: its projected vector, and its phase for rope.
func (m *Model) queueEmbed(p *webgpu.Program, tok int, pos int) error {
	if sl, ok := m.slots[tok]; ok && tok >= m.C.Vocab {
		p.WriteU32(m.step.buf, 0, uint32(pos), 0, math.Float32bits(sl.phase), 0)
		p.Write(m.x.buf, m.x.off, sl.x)
		return nil
	}
	if tok < 0 || tok >= m.C.Vocab {
		return errors.New("model: token " + itoa(tok) + " is outside the vocabulary")
	}
	ci := 0
	for ci+1 < len(m.emb) && tok >= m.emb[ci+1].first {
		ci++
	}
	p.WriteU32(m.step.buf, 0, uint32(pos), uint32(tok-m.emb[ci].first), 0, 0)
	m.dispatch(p, m.embedP, m.embeds[ci], (m.C.Dim+255)/256)
	return nil
}

// Dot4 is the dot4 the kernels use: dot4_native or dot4_soft.
func (m *Model) Dot4() string {
	return m.dot4
}

// WeightBytes is the device memory holding the ternary weights, norms and
// scales.
func (m *Model) WeightBytes() int {
	return m.weights.bytes
}

// EmbedBytes is the embedding (and, untied, the LM head) on the device.
func (m *Model) EmbedBytes() int {
	n := 0
	for _, ch := range m.emb {
		n += ch.r.size
	}
	if !m.C.Tied {
		for _, ch := range m.headW {
			n += ch.r.size
		}
	}
	return n
}

// CacheBytes is the KV cache on the device.
func (m *Model) CacheBytes() int {
	return m.kc.size + m.vc.size
}

// Info is a line about the device and the model's memory.
func (m *Model) Info() string {
	c := m.C
	gpu := m.Dev.Info["description"]
	if gpu == "" {
		gpu = m.Dev.Info["vendor"] + " " + m.Dev.Info["architecture"]
	}
	return "BitNet " + itoa(c.Layers) + " layers, hidden " + itoa(c.Dim) + ", ffn " + itoa(c.FFN) +
		", heads " + itoa(c.Heads) + "/" + itoa(c.KVHeads) + ", vocab " + itoa(c.Vocab) + ", ctx " + itoa(m.Ctx) +
		" on " + gpu + " (" + m.dot4 + "), weights " + itoa(m.weights.bytes>>20) + " MiB"
}
