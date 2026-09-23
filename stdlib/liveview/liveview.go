// Package liveview is a LiveView web framework for Go++: one language
// covering HTML, CSS, JS-events, and Jinja2/Go templates.
//
// A .live document is a `live Name { ... }` block (or several). Inside:
//
//	state { Count: 0, Title: "Hi" }     // component state
//	style { .n { font-size: 2rem; } }    // CSS, copied into the page
//	view { <div id="counter">...</div> } // HTML and/or GML markup
//	event inc { Count += 1 }             // server-side handlers (the JS)
//	func add(a, b) { return a + b }
//
// Markup in `view` is HTML tags (`<div class="n">`) and GuiKit GML
// (`div#counter.n :gk-click."inc" { "..." }`). Template actions survive
// markup eval and run against state:
//
//	{{ .Title }}                         // Go html/template
//	{{ if .Count }}hot{{ else }}cold{{ end }}
//	{{ range .Items }}{{ . }}{{ end }}
//	{{ .Title | upper }}
//	{% if Count %}yes{% else %}no{% endif %}   // Jinja2
//	{% for item in Items %}<li>{{ item }}</li>{% endfor %}
//
// `gk-click` / `gk-submit` / `gk-change` replace client JS. The generated
// runtime POSTs JSON to `/live/event` (HTTP/1.0; this net/http has no
// WebSocket) and patches the component's outerHTML. DML instructions
// (`dml.append`, `dml.remove`, `dml.setValue`, `dml.addClass`,
// `dml.removeClass`) apply on top of the patch.
//
// Bounds: no wrapper/import filesystem, no WebSocket, no contextual
// (JS/CSS/URL) autoescape — interpolations are HTML-escaped unless
// piped through `| safe`. `net/http` is HTTP/1.0 exact-path ServeMux.
package liveview

import (
	"encoding/json"
	"errors"
	"fmt"
	"html"
	"net/http"
	"strconv"
	"strings"
)

const (
	nText = 0
	nElem = 1

	sAssign = 1
	sIf     = 2
	sFor    = 3
	sReturn = 4
	sCall   = 5

	xLit  = 1
	xVar  = 2
	xBin  = 3
	xCall = 4
	xUn   = 5
)

type Attr struct {
	Key string
	Val string
}

type Node struct {
	Kind  int
	Tag   string
	Text  string
	Attrs []Attr
	Kids  []Node
}

type Expr struct {
	Kind  int
	Val   any
	Path  string
	Op    string
	Left  *Expr
	Right *Expr
	Name  string
	Args  []*Expr
}

type Stmt struct {
	Kind     int
	Path     string
	Op       string
	X        *Expr
	Body     []Stmt
	Else     []Stmt
	Iter     string
	Coll     string
	CallName string
	Args     []*Expr
}

type Func struct {
	Params []string
	Body   []Stmt
}

type DML struct {
	Action   string `json:"action"`
	TargetID string `json:"target_id"`
	Value    string `json:"value"`
}

type Component struct {
	Name   string
	State  map[string]any
	Style  string
	View   []Node
	Events map[string][]Stmt
	Funcs  map[string]Func
}

type Doc struct {
	Comps []Component
}

type EventPatch struct {
	ID   string `json:"id"`
	HTML string `json:"html"`
	DML  []DML  `json:"dml"`
}

type Engine struct {
	comps []Component
}

func Parse(src string) (*Doc, error) {
	p := &parser{src: src, i: 0}
	d := p.parseDoc()
	if p.err != "" {
		return nil, errors.New(p.err)
	}
	if len(d.Comps) == 0 {
		return nil, errors.New("liveview: no live blocks")
	}
	return d, nil
}

func (d *Doc) Component(name string) *Component {
	want := strings.ToLower(name)
	for i := 0; i < len(d.Comps); i++ {
		if strings.ToLower(d.Comps[i].Name) == want {
			return &d.Comps[i]
		}
	}
	if len(d.Comps) == 1 && name == "" {
		return &d.Comps[0]
	}
	return nil
}

func (c *Component) ID() string {
	if c == nil {
		return ""
	}
	return strings.ToLower(c.Name)
}

func (c *Component) Render() string {
	if c == nil {
		return ""
	}
	raw := evalNodes(c.View)
	htmlOut := execTmpl(raw, c.State, nil)
	id := c.ID()
	if id != "" && !rootHasID(htmlOut, id) {
		htmlOut = `<div id="` + id + `">` + htmlOut + `</div>`
	}
	return htmlOut
}

func (c *Component) Invoke(event string, data map[string]string) []DML {
	if c == nil {
		return nil
	}
	body, ok := c.Events[event]
	if !ok {
		return nil
	}
	if data == nil {
		data = map[string]string{}
	}
	ex := &execEnv{
		comp:    c,
		payload: data,
		locals:  map[string]any{},
	}
	runStmts(ex, body)
	return ex.dml
}

func (c *Component) Patch(event string, data map[string]string) EventPatch {
	dml := c.Invoke(event, data)
	return EventPatch{ID: c.ID(), HTML: c.Render(), DML: dml}
}

func NewEngine() *Engine {
	return &Engine{}
}

func (e *Engine) Add(src string) error {
	d, err := Parse(src)
	if err != nil {
		return err
	}
	for i := 0; i < len(d.Comps); i++ {
		e.comps = append(e.comps, d.Comps[i])
	}
	return nil
}

func (e *Engine) Get(id string) *Component {
	if e == nil {
		return nil
	}
	want := strings.ToLower(id)
	for i := 0; i < len(e.comps); i++ {
		if strings.ToLower(e.comps[i].Name) == want {
			return &e.comps[i]
		}
	}
	return nil
}

func (e *Engine) firstID() string {
	if e == nil || len(e.comps) == 0 {
		return ""
	}
	return strings.ToLower(e.comps[0].Name)
}

func (e *Engine) styles() string {
	var b strings.Builder
	for i := 0; i < len(e.comps); i++ {
		if e.comps[i].Style != "" {
			b.WriteString(e.comps[i].Style)
			b.WriteString("\n")
		}
	}
	return b.String()
}

func (e *Engine) PageHTML(id string) string {
	if id == "" {
		id = e.firstID()
	}
	c := e.Get(id)
	body := ""
	title := id
	if c != nil {
		body = c.Render()
		title = c.Name
	}
	st := e.styles()
	var b strings.Builder
	b.WriteString("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><title>")
	b.WriteString(html.EscapeString(title))
	b.WriteString("</title>\n")
	if st != "" {
		b.WriteString("<style>\n")
		b.WriteString(st)
		b.WriteString("</style>\n")
	}
	b.WriteString("</head><body>\n")
	b.WriteString(body)
	b.WriteString("\n<script src=\"/live.js\"></script>\n</body></html>")
	return b.String()
}

func (e *Engine) HandleEvent(id string, event string, data map[string]string) EventPatch {
	c := e.Get(id)
	if c == nil {
		return EventPatch{ID: id, HTML: "", DML: nil}
	}
	return c.Patch(event, data)
}

func (e *Engine) HandleEventJSON(body string) string {
	var v any
	err := json.Unmarshal([]byte(body), &v)
	if err != nil {
		return `{"id":"","html":"","dml":null}`
	}
	m, ok := v.(map[string]any)
	if !ok {
		return `{"id":"","html":"","dml":null}`
	}
	id, _ := m["id"].(string)
	event, _ := m["event"].(string)
	data := map[string]string{}
	if dm, ok2 := m["data"].(map[string]any); ok2 {
		for k, val := range dm {
			data[k] = stringifyRaw(val)
		}
	}
	p := e.HandleEvent(id, event, data)
	out, merr := json.Marshal(p)
	if merr != nil {
		return `{"id":"` + id + `","html":"","dml":null}`
	}
	return string(out)
}

func (e *Engine) Mount(mux *http.ServeMux) {
	if e == nil || mux == nil {
		return
	}
	mux.HandleFunc("/live.js", e.serveJS)
	mux.HandleFunc("/live/event", e.serveEvent)
	mux.HandleFunc("/", e.serveRoot)
}

func (e *Engine) serveJS(req *http.Request, resp *http.Response) {
	resp.ContentType = "application/javascript"
	resp.Body = ClientJS()
}

func (e *Engine) serveEvent(req *http.Request, resp *http.Response) {
	resp.ContentType = "application/json"
	resp.Body = e.HandleEventJSON(req.Body)
}

func (e *Engine) serveRoot(req *http.Request, resp *http.Response) {
	resp.ContentType = "text/html"
	resp.Body = e.PageHTML("")
}

func ClientJS() string {
	return clientJS
}

func rootHasID(htmlOut string, id string) bool {
	i := strings.Index(htmlOut, "<")
	if i < 0 {
		return false
	}
	rest := htmlOut[i:]
	j := strings.Index(rest, ">")
	if j < 0 {
		return false
	}
	open := rest[0:j]
	return strings.Contains(open, `id="`+id+`"`)
}

func stringifyRaw(v any) string {
	if v == nil {
		return ""
	}
	if s, ok := v.(string); ok {
		return s
	}
	if b, ok := v.(bool); ok {
		if b {
			return "true"
		}
		return "false"
	}
	if n, ok := v.(int); ok {
		return strconv.Itoa(n)
	}
	if f, ok := v.(float64); ok {
		return strconv.Itoa(int(f))
	}
	return fmt.Sprintf("%v", v)
}

const clientJS = `(function(){
function send(id, event, data) {
  var x = new XMLHttpRequest();
  x.open("POST", "/live/event", true);
  x.setRequestHeader("Content-Type", "application/json");
  x.onreadystatechange = function() {
    if (x.readyState !== 4 || x.status !== 200) return;
    var msg;
    try { msg = JSON.parse(x.responseText); } catch (e) { return; }
    if (msg.id && msg.html) {
      var el = document.getElementById(msg.id);
      if (el) el.outerHTML = msg.html;
    }
    if (msg.dml) {
      for (var i = 0; i < msg.dml.length; i++) applyDml(msg.dml[i]);
    }
  };
  x.send(JSON.stringify({id: id, event: event, data: data || {}}));
}
function applyDml(d) {
  var el = document.getElementById(d.target_id);
  if (!el) return;
  if (d.action === "append") el.insertAdjacentHTML("beforeend", d.value || "");
  else if (d.action === "remove") el.remove();
  else if (d.action === "setValue") {
    if ("value" in el) el.value = d.value || "";
    else el.textContent = d.value || "";
  } else if (d.action === "addClass") el.classList.add(d.value);
  else if (d.action === "removeClass") el.classList.remove(d.value);
}
document.addEventListener("click", function(e) {
  var t = e.target.closest("[gk-click]");
  if (!t) return;
  e.preventDefault();
  var root = t.closest("[id]");
  if (!root) return;
  send(root.id, t.getAttribute("gk-click"), {});
});
document.addEventListener("submit", function(e) {
  var t = e.target.closest("[gk-submit]");
  if (!t) return;
  e.preventDefault();
  var root = t.closest("[id]");
  if (!root) return;
  var data = {};
  var els = t.querySelectorAll("[name]");
  for (var i = 0; i < els.length; i++) {
    var n = els[i];
    if (n.name) data[n.name] = n.value || "";
  }
  send(root.id, t.getAttribute("gk-submit"), data);
});
document.addEventListener("change", function(e) {
  var t = e.target.closest("[gk-change]");
  if (!t) return;
  var root = t.closest("[id]");
  if (!root) return;
  var data = {};
  if (t.name) data[t.name] = t.value || "";
  send(root.id, t.getAttribute("gk-change"), data);
});
window.lv = {send: send};
})();
`
