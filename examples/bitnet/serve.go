// Local inference, the same process as the page's model. One checkpoint stays
// loaded. HTTP is the Ollama shape: /api/tags, /api/chat, /api/generate.
// The GPU work is the recorded WebGPU step (model.Step), unchanged.
package main

import (
	"encoding/json"
	"errors"
	"net"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"
)

type histMsg struct {
	role    string
	content string
}

// extraUser is the one new user message when neu is old plus that message.
func extraUser(old, neu []histMsg) (string, bool) {
	if len(neu) != len(old)+1 {
		return "", false
	}
	for i := range old {
		if old[i].role != neu[i].role || old[i].content != neu[i].content {
			return "", false
		}
	}
	last := neu[len(neu)-1]
	if last.role != "user" || strings.TrimSpace(last.content) == "" {
		return "", false
	}
	return last.content, true
}

func modelName(dir string) string {
	name := strings.ReplaceAll(dir, "\\", "/")
	name = strings.TrimSuffix(name, "/")
	if i := strings.LastIndex(name, "/"); i >= 0 {
		name = name[i+1:]
	}
	if name == "" {
		return "bitnet"
	}
	return name
}

type memSink struct {
	text string
	st   stats
}

func (m *memSink) Token(id int, text string, p float64, top []cand) { m.text += text }
func (m *memSink) Flow(seq int, pos int, id int)                    {}
func (m *memSink) Done(st stats)                                    { m.st = st }
func (m *memSink) Stopped() bool                                    { return false }

// streamSink writes one Ollama chunk per token. A failed write stops the answer.
// mem is named, not embedded: wasigoc lowers an embedded memSink to a base
// type, so the text is not a field streamSink can name.
type streamSink struct {
	mem    memSink
	c      *net.Conn
	prefix string
	tail   string // closes the token field, then ,"done":false}
	failed bool
}

func (s *streamSink) Token(id int, text string, p float64, top []cand) {
	s.mem.text += text
	if s.failed || text == "" {
		return
	}
	if err := writeAll(s.c, []byte(s.prefix+jstr(text)+s.tail+"\n")); err != nil {
		s.failed = true
	}
}

func (s *streamSink) Flow(seq int, pos int, id int) {}
func (s *streamSink) Done(st stats)                 { s.mem.st = st }
func (s *streamSink) Stopped() bool                 { return s.failed }

func writeAll(c *net.Conn, b []byte) error {
	for len(b) > 0 {
		n, err := c.Write(b)
		if n > 0 {
			b = b[n:]
		}
		if err != nil {
			return err
		}
		if n == 0 {
			return errors.New("short write")
		}
	}
	return nil
}

func (c *conversation) endTok() int {
	if c.stop >= 0 {
		return c.stop
	}
	if len(c.s.gen.eos) > 0 {
		return c.s.gen.eos[0]
	}
	return -1
}

func (c *conversation) prefill(ids []int) error {
	if len(ids) == 0 {
		return nil
	}
	_, err := c.s.feed(ids)
	return err
}

// replay fills the cache from a full message list and samples the last user turn.
func (c *conversation) replay(msgs []histMsg, system0 string, out sink) error {
	c.reset()
	c.system = system0
	for _, m := range msgs {
		if m.role == "system" && m.content != "" {
			c.system = m.content
		}
	}
	for i, m := range msgs {
		if m.role == "system" {
			continue
		}
		if m.role == "assistant" {
			ids := c.s.tok.Encode(m.content, true)
			if err := c.prefill(ids); err != nil {
				return err
			}
			c.carry = c.endTok()
			continue
		}
		if m.role != "user" {
			continue
		}
		ids := c.turn(m.content)
		last := i == len(msgs)-1
		if !last {
			if err := c.prefill(ids); err != nil {
				return err
			}
			continue
		}
		end, err := c.s.answer(ids, out)
		if err != nil {
			return err
		}
		if end < 0 {
			end = c.stop
		}
		c.carry = end
	}
	return nil
}

type ollama struct {
	conv    *conversation
	name    string
	system0 string
	hist    []histMsg
	mu      sync.Mutex
}

func (o *ollama) tags() string {
	m := o.conv.s.m
	size := m.WeightBytes() + m.EmbedBytes()
	return `{"models":[{"name":` + jstr(o.name) + `,"model":` + jstr(o.name) + `,"size":` + jint(size) + `}]}`
}

func stamp() string {
	return strconv.FormatInt(time.Now().Unix(), 10)
}

func numOpt(m map[string]any, k string) (float64, bool) {
	opt, _ := m["options"].(map[string]any)
	if opt == nil {
		return 0, false
	}
	f, ok := opt[k].(float64)
	return f, ok
}

func parseMsgs(v map[string]any) []histMsg {
	raw, _ := v["messages"].([]any)
	var out []histMsg
	for _, e := range raw {
		o, _ := e.(map[string]any)
		if o == nil {
			continue
		}
		role, _ := o["role"].(string)
		content, _ := o["content"].(string)
		if role == "" {
			continue
		}
		out = append(out, histMsg{role, content})
	}
	return out
}

func (o *ollama) applyOpts(v map[string]any) (restore func()) {
	s := o.conv.s
	saved := s.set
	if f, ok := numOpt(v, "temperature"); ok && f >= 0 && f <= 2 {
		s.set.temp = f
	}
	if f, ok := numOpt(v, "top_p"); ok && f > 0 && f <= 1 {
		s.set.topP = f
	}
	if f, ok := numOpt(v, "repeat_penalty"); ok && f >= 1 && f <= 2 {
		s.set.repeat = f
	}
	thisMax := s.set.maxNew
	if f, ok := numOpt(v, "num_predict"); ok && f >= 1 {
		thisMax = int(f)
	}
	s.set.maxNew = thisMax
	return func() { s.set = saved }
}

func (o *ollama) chat(v map[string]any, out sink) error {
	msgs := parseMsgs(v)
	if len(msgs) == 0 {
		return errors.New("no messages")
	}
	if text, ok := extraUser(o.hist, msgs); ok {
		ids := o.conv.turn(text)
		end, err := o.conv.s.answer(ids, out)
		if err != nil {
			return err
		}
		if end < 0 {
			end = o.conv.stop
		}
		o.conv.carry = end
		o.hist = append(append([]histMsg{}, msgs...), histMsg{"assistant", sinkText(out)})
		return nil
	}
	if err := o.conv.replay(msgs, o.system0, out); err != nil {
		return err
	}
	text := sinkText(out)
	o.hist = append([]histMsg{}, msgs...)
	if len(msgs) > 0 && msgs[len(msgs)-1].role == "user" {
		o.hist = append(o.hist, histMsg{"assistant", text})
	}
	return nil
}

func sinkText(out sink) string {
	if m, ok := out.(*memSink); ok {
		return m.text
	}
	if s, ok := out.(*streamSink); ok {
		return s.mem.text
	}
	return ""
}

func sinkStats(out sink) stats {
	if m, ok := out.(*memSink); ok {
		return m.st
	}
	if s, ok := out.(*streamSink); ok {
		return s.mem.st
	}
	return stats{}
}

func doneReason(r string) string {
	if r == "eos" || r == "" {
		return "stop"
	}
	if r == "max" {
		return "length"
	}
	return r
}

func (o *ollama) handle(req *http.Request, resp *http.Response) {
	path := req.Path
	if i := strings.Index(path, "?"); i >= 0 {
		path = path[:i]
	}
	if req.Method == "GET" && (path == "/" || path == "/api/version") {
		resp.ContentType = "application/json"
		resp.Body = `{"version":"bitnet"}`
		return
	}
	if req.Method == "GET" && (path == "/api/tags" || path == "/api/ps") {
		resp.ContentType = "application/json"
		resp.Body = o.tags()
		return
	}
	if req.Method != "POST" || (path != "/api/chat" && path != "/api/generate") {
		resp.Status = 404
		resp.Body = "not found"
		return
	}
	var v any
	if json.Unmarshal([]byte(req.Body), &v) != nil {
		resp.Status = 400
		resp.Body = "not JSON"
		return
	}
	cmd, _ := v.(map[string]any)
	if cmd == nil {
		resp.Status = 400
		resp.Body = "not JSON"
		return
	}
	stream := true
	if b, ok := cmd["stream"].(bool); ok {
		stream = b
	}
	// Pointer, not the mutex value. wasigoc copies a deferred receiver into a
	// const lambda, and Unlock is not const.
	mu := &o.mu
	mu.Lock()
	defer mu.Unlock()
	restore := o.applyOpts(cmd)
	defer restore()
	if path == "/api/generate" {
		o.conv.reset()
		o.hist = nil
		prompt, _ := cmd["prompt"].(string)
		ids := append(append([]int{}, o.conv.bos...), o.conv.s.tok.Encode(prompt, true)...)
		o.answerHTTP(req, resp, stream, "gen", ids)
		return
	}
	o.answerChat(req, resp, stream, cmd)
}

func (o *ollama) answerHTTP(req *http.Request, resp *http.Response, stream bool, kind string, ids []int) {
	var out sink
	var ss *streamSink
	if stream {
		if err := writeAll(req.Conn, []byte("HTTP/1.1 200 OK\r\nContent-Type: application/x-ndjson\r\nConnection: close\r\n\r\n")); err != nil {
			resp.Status = 500
			resp.Body = err.Error()
			return
		}
		resp.Hijack = true
		ss = &streamSink{c: req.Conn, prefix: o.chunkPrefix(kind), tail: o.chunkTail(kind)}
		out = ss
	} else {
		out = &memSink{}
	}
	end, err := o.conv.s.answer(ids, out)
	if err != nil {
		if stream {
			_ = writeAll(req.Conn, []byte(`{"error":`+jstr(err.Error())+`}`+"\n"))
			req.Conn.Close()
			return
		}
		resp.Status = 500
		resp.Body = err.Error()
		return
	}
	if end < 0 {
		end = o.conv.stop
	}
	o.conv.carry = end
	o.finish(req, resp, stream, kind, out)
}

func (o *ollama) answerChat(req *http.Request, resp *http.Response, stream bool, cmd map[string]any) {
	var out sink
	if stream {
		if err := writeAll(req.Conn, []byte("HTTP/1.1 200 OK\r\nContent-Type: application/x-ndjson\r\nConnection: close\r\n\r\n")); err != nil {
			resp.Status = 500
			resp.Body = err.Error()
			return
		}
		resp.Hijack = true
		out = &streamSink{c: req.Conn, prefix: o.chunkPrefix("chat"), tail: o.chunkTail("chat")}
	} else {
		out = &memSink{}
	}
	if err := o.chat(cmd, out); err != nil {
		if stream {
			_ = writeAll(req.Conn, []byte(`{"error":`+jstr(err.Error())+`}`+"\n"))
			req.Conn.Close()
			return
		}
		resp.Status = 400
		resp.Body = err.Error()
		return
	}
	o.finish(req, resp, stream, "chat", out)
}

func (o *ollama) chunkPrefix(kind string) string {
	if kind == "gen" {
		return `{"model":` + jstr(o.name) + `,"created_at":` + jstr(stamp()) + `,"response":`
	}
	return `{"model":` + jstr(o.name) + `,"created_at":` + jstr(stamp()) + `,"message":{"role":"assistant","content":`
}

func (o *ollama) chunkTail(kind string) string {
	if kind == "gen" {
		return `,"done":false}`
	}
	return `"},"done":false}`
}

func (o *ollama) finish(req *http.Request, resp *http.Response, stream bool, kind string, out sink) {
	st := sinkStats(out)
	text := sinkText(out)
	done := `,"done":true,"done_reason":` + jstr(doneReason(st.reason)) +
		`,"prompt_eval_count":` + jint(st.prompt) + `,"eval_count":` + jint(st.tokens) +
		`,"prompt_eval_duration":` + jint(st.prefillMs) + `,"eval_duration":` + jint(st.decodeMs) + `}`
	if stream {
		line := o.chunkPrefix(kind) + `""`
		if kind != "gen" {
			line += `}`
		}
		_ = writeAll(req.Conn, []byte(line+done+"\n"))
		req.Conn.Close()
		return
	}
	resp.ContentType = "application/json"
	if kind == "gen" {
		resp.Body = `{"model":` + jstr(o.name) + `,"created_at":` + jstr(stamp()) + `,"response":` + jstr(text) + done
		return
	}
	resp.Body = `{"model":` + jstr(o.name) + `,"created_at":` + jstr(stamp()) +
		`,"message":{"role":"assistant","content":` + jstr(text) + `}` + done
}

func serveLocal(conv *conversation, addr string, name string) error {
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}
	o := &ollama{conv: conv, name: name, system0: conv.system}
	mux := http.NewServeMux()
	mux.HandleFunc("/", o.handle)
	mux.HandleFunc("/api/version", o.handle)
	mux.HandleFunc("/api/tags", o.handle)
	mux.HandleFunc("/api/ps", o.handle)
	mux.HandleFunc("/api/chat", o.handle)
	mux.HandleFunc("/api/generate", o.handle)
	return http.ServeHandler(ln, mux)
}
