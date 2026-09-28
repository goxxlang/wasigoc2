// GS, the GuiKit script. state, handle, and func blocks read and write the
// document data model. dml.* edits the element tree directly.
package guikit

import (
	"strconv"
	"strings"

	"rmlui"
)

const (
	sAssign = 1
	sExpr   = 2
	sIf     = 3
	sFor    = 4
	sReturn = 5

	eLit  = 1
	ePath = 2
	eBin  = 3
	eCall = 4
	eUn   = 5

	vNil = 0
	vBool = 1
	vInt = 2
	vStr = 3
	vList = 4
)

type gval struct {
	kind int
	b    bool
	i    int
	s    string
	list []gval
}

type gsExpr struct {
	kind  int
	s     string
	i     int
	b     bool
	op    string
	path  string
	name  string
	left  *gsExpr
	right *gsExpr
	args  []*gsExpr
}

type gsStmt struct {
	kind   int
	path   string
	op     string
	expr   *gsExpr
	body   []gsStmt
	alt    []gsStmt
	iter   string
	coll   string
}

type gsFunc struct {
	params []string
	body   []gsStmt
}

// Component is one GS script bound to a document and its data model.
type Component struct {
	doc      *rmlui.ElementDocument
	model    *rmlui.DataModel
	state    map[string]gval
	handlers map[string][]gsStmt
	funcs    map[string]*gsFunc
}

func newComponent(doc *rmlui.ElementDocument, model *rmlui.DataModel) *Component {
	return &Component{
		doc:      doc,
		model:    model,
		state:    map[string]gval{},
		handlers: map[string][]gsStmt{},
		funcs:    map[string]*gsFunc{},
	}
}

func (c *Component) set(path string, value gval) {
	c.state[path] = value
	if c.model != nil && !strings.Contains(path, ".") {
		c.model.Set(path, gvalVariant(value))
	}
}

func (c *Component) get(path string) gval {
	if value, ok := c.state[path]; ok {
		return value
	}
	if c.model != nil && !strings.Contains(path, ".") {
		if variant, ok := c.model.Get(path); ok {
			return variantGval(variant)
		}
	}
	return gval{}
}

func gvalVariant(value gval) rmlui.Variant {
	if value.kind == vBool {
		return rmlui.VariantBool(value.b)
	}
	if value.kind == vInt {
		return rmlui.VariantInt(value.i)
	}
	if value.kind == vStr {
		return rmlui.VariantString(value.s)
	}
	if value.kind == vList {
		return rmlui.VariantString(value.String())
	}
	return rmlui.VariantString("")
}

func variantGval(value rmlui.Variant) gval {
	switch value.GetType() {
	case rmlui.VariantBOOL:
		return gval{kind: vBool, b: value.GetBool()}
	case rmlui.VariantINT, rmlui.VariantINT64:
		return gval{kind: vInt, i: value.GetInt()}
	default:
		return gval{kind: vStr, s: value.GetString()}
	}
}

func (v gval) String() string {
	if v.kind == vBool {
		if v.b {
			return "true"
		}
		return "false"
	}
	if v.kind == vInt {
		return strconv.Itoa(v.i)
	}
	if v.kind == vStr {
		return v.s
	}
	if v.kind == vList {
		out := ""
		for i := 0; i < len(v.list); i++ {
			if i > 0 {
				out = out + ","
			}
			out = out + v.list[i].String()
		}
		return out
	}
	return ""
}

func (v gval) truth() bool {
	if v.kind == vBool {
		return v.b
	}
	if v.kind == vInt {
		return v.i != 0
	}
	if v.kind == vStr {
		return v.s != ""
	}
	if v.kind == vList {
		return len(v.list) != 0
	}
	return false
}

func (v gval) integer() (int, bool) {
	if v.kind == vInt {
		return v.i, true
	}
	if v.kind == vBool {
		if v.b {
			return 1, true
		}
		return 0, true
	}
	if v.kind == vStr {
		n, err := strconv.Atoi(v.s)
		if err != nil {
			return 0, false
		}
		return n, true
	}
	return 0, false
}

type gsExec struct {
	comp    *Component
	data    map[string]string
	locals  map[string]gval
	ret     gval
	hasRet  bool
}

func (x *gsExec) lookup(path string) gval {
	if path == "data" {
		return gval{kind: vStr, s: ""}
	}
	if strings.HasPrefix(path, "data.") {
		key := path[5:]
		if x.data != nil {
			return gval{kind: vStr, s: x.data[key]}
		}
		return gval{}
	}
	if value, ok := x.locals[path]; ok {
		return value
	}
	return x.comp.get(path)
}

func (x *gsExec) store(path string, value gval) {
	if _, ok := x.locals[path]; ok {
		x.locals[path] = value
		return
	}
	x.comp.set(path, value)
}

func (x *gsExec) run(list []gsStmt) {
	for i := 0; i < len(list); i++ {
		list[i].exec(x)
		if x.hasRet {
			return
		}
	}
}

func (s gsStmt) exec(x *gsExec) {
	if s.kind == sAssign {
		rhs := s.expr.eval(x)
		if s.op == "=" {
			x.store(s.path, rhs)
			return
		}
		leftN, leftOK := x.lookup(s.path).integer()
		rightN, rightOK := rhs.integer()
		if leftOK && rightOK {
			if s.op == "+=" {
				x.store(s.path, gval{kind: vInt, i: leftN + rightN})
			} else if s.op == "-=" {
				x.store(s.path, gval{kind: vInt, i: leftN - rightN})
			}
		}
		return
	}
	if s.kind == sExpr {
		if s.expr != nil {
			s.expr.eval(x)
		}
		return
	}
	if s.kind == sIf {
		branch := s.alt
		if s.expr != nil && s.expr.eval(x).truth() {
			branch = s.body
		}
		x.run(branch)
		return
	}
	if s.kind == sFor {
		coll := x.lookup(s.coll)
		if coll.kind != vList {
			return
		}
		for i := 0; i < len(coll.list); i++ {
			x.locals[s.iter] = coll.list[i]
			x.run(s.body)
			if x.hasRet {
				return
			}
		}
		return
	}
	if s.kind == sReturn {
		if s.expr != nil {
			x.ret = s.expr.eval(x)
		}
		x.hasRet = true
	}
}

func (e *gsExpr) eval(x *gsExec) gval {
	if e == nil {
		return gval{}
	}
	if e.kind == eLit {
		if e.s != "" || e.op == "str" {
			return gval{kind: vStr, s: e.s}
		}
		if e.op == "bool" {
			return gval{kind: vBool, b: e.b}
		}
		return gval{kind: vInt, i: e.i}
	}
	if e.kind == ePath {
		if e.path == "true" {
			return gval{kind: vBool, b: true}
		}
		if e.path == "false" {
			return gval{kind: vBool, b: false}
		}
		return x.lookup(e.path)
	}
	if e.kind == eUn {
		value := e.left.eval(x)
		if e.op == "!" {
			return gval{kind: vBool, b: !value.truth()}
		}
		n, ok := value.integer()
		if ok {
			return gval{kind: vInt, i: -n}
		}
		return gval{}
	}
	if e.kind == eBin {
		if e.op == "&&" {
			if !e.left.eval(x).truth() {
				return gval{kind: vBool, b: false}
			}
			return gval{kind: vBool, b: e.right.eval(x).truth()}
		}
		if e.op == "||" {
			if e.left.eval(x).truth() {
				return gval{kind: vBool, b: true}
			}
			return gval{kind: vBool, b: e.right.eval(x).truth()}
		}
		left := e.left.eval(x)
		right := e.right.eval(x)
		li, lok := left.integer()
		ri, rok := right.integer()
		if lok && rok && e.op != "+" {
			if e.op == "-" {
				return gval{kind: vInt, i: li - ri}
			}
			if e.op == "*" {
				return gval{kind: vInt, i: li * ri}
			}
			if e.op == "/" {
				if ri == 0 {
					return gval{kind: vInt, i: 0}
				}
				return gval{kind: vInt, i: li / ri}
			}
			if e.op == "==" {
				return gval{kind: vBool, b: li == ri}
			}
			if e.op == "!=" {
				return gval{kind: vBool, b: li != ri}
			}
			if e.op == "<" {
				return gval{kind: vBool, b: li < ri}
			}
			if e.op == ">" {
				return gval{kind: vBool, b: li > ri}
			}
			if e.op == "<=" {
				return gval{kind: vBool, b: li <= ri}
			}
			if e.op == ">=" {
				return gval{kind: vBool, b: li >= ri}
			}
		}
		if e.op == "+" {
			if lok && rok && left.kind != vStr && right.kind != vStr {
				return gval{kind: vInt, i: li + ri}
			}
			return gval{kind: vStr, s: left.String() + right.String()}
		}
		ls := left.String()
		rs := right.String()
		if e.op == "==" {
			return gval{kind: vBool, b: ls == rs}
		}
		if e.op == "!=" {
			return gval{kind: vBool, b: ls != rs}
		}
		return gval{}
	}
	if e.kind == eCall {
		return x.call(e.name, e.args)
	}
	return gval{}
}

func (x *gsExec) call(name string, args []*gsExpr) gval {
	values := []gval{}
	for i := 0; i < len(args); i++ {
		values = append(values, args[i].eval(x))
	}
	if fn, ok := x.comp.funcs[name]; ok {
		nested := &gsExec{comp: x.comp, data: x.data, locals: map[string]gval{}}
		for i := 0; i < len(fn.params) && i < len(values); i++ {
			nested.locals[fn.params[i]] = values[i]
		}
		nested.run(fn.body)
		return nested.ret
	}
	if name == "len" && len(values) == 1 {
		if values[0].kind == vList {
			return gval{kind: vInt, i: len(values[0].list)}
		}
		return gval{kind: vInt, i: len(values[0].String())}
	}
	if name == "append" && len(values) == 2 {
		list := values[0].list
		list = append(list, values[1])
		return gval{kind: vList, list: list}
	}
	if name == "dml.setValue" && len(values) == 2 {
		x.dmlSet(values[0].String(), values[1].String())
		return gval{}
	}
	if name == "dml.addClass" && len(values) == 2 {
		el := x.comp.doc.GetElementById(values[0].String())
		if el != nil {
			el.SetClass(values[1].String(), true)
		}
		return gval{}
	}
	if name == "dml.removeClass" && len(values) == 2 {
		el := x.comp.doc.GetElementById(values[0].String())
		if el != nil {
			el.SetClass(values[1].String(), false)
		}
		return gval{}
	}
	if name == "dml.remove" && len(values) == 1 {
		el := x.comp.doc.GetElementById(values[0].String())
		if el != nil && el.GetParentNode() != nil {
			el.GetParentNode().RemoveChild(el)
		}
		return gval{}
	}
	if name == "dml.append" && len(values) == 2 {
		el := x.comp.doc.GetElementById(values[0].String())
		if el != nil {
			fragment := values[1].String()
			if strings.Contains(fragment, "<") {
				rmlui.ParseRMLFragment(el, fragment)
			} else {
				nodes := parseGML(fragment)
				b := &builder{doc: x.comp.doc}
				for i := 0; i < len(nodes); i++ {
					b.add(el, nodes[i])
				}
			}
		}
		return gval{}
	}
	if name == "cloudUser" {
		return gval{kind: vStr, s: "anonymous"}
	}
	return gval{}
}

func (x *gsExec) dmlSet(id string, value string) {
	el := x.comp.doc.GetElementById(id)
	if el == nil {
		return
	}
	child := el.GetFirstChild()
	if text := rmlui.ElementTextOf(child); text != nil && el.GetNumChildren(false) == 1 {
		text.SetTemplate(value)
		text.SetText(value)
		return
	}
	el.SetInnerRML(value)
}

const (
	gsEOF = iota
	gsIdent
	gsNum
	gsStr
	gsSym
)

type gsToken struct {
	kind int
	text string
}

func lexGS(src string) []gsToken {
	out := []gsToken{}
	i := 0
	for i < len(src) {
		c := src[i]
		if c == ' ' || c == '\t' || c == '\n' || c == '\r' {
			i++
			continue
		}
		if c == '/' && i+1 < len(src) && src[i+1] == '/' {
			i += 2
			for i < len(src) && src[i] != '\n' {
				i++
			}
			continue
		}
		if c == '/' && i+1 < len(src) && src[i+1] == '*' {
			i += 2
			for i+1 < len(src) && !(src[i] == '*' && src[i+1] == '/') {
				i++
			}
			if i+1 < len(src) {
				i += 2
			}
			continue
		}
		if c == '"' || c == '\'' || c == '`' {
			end := skipGMLString(src, i)
			out = append(out, gsToken{gsStr, unquoteGML(src[i:end])})
			i = end
			continue
		}
		if (c >= '0' && c <= '9') || (c == '-' && i+1 < len(src) && src[i+1] >= '0' && src[i+1] <= '9') {
			start := i
			i++
			for i < len(src) && ((src[i] >= '0' && src[i] <= '9') || src[i] == '.') {
				i++
			}
			out = append(out, gsToken{gsNum, src[start:i]})
			continue
		}
		if isGSIdent(c) {
			start := i
			i++
			for i < len(src) && (isGSIdent(src[i]) || src[i] == '.' || (src[i] >= '0' && src[i] <= '9')) {
				i++
			}
			out = append(out, gsToken{gsIdent, src[start:i]})
			continue
		}
		two := ""
		if i+1 < len(src) {
			two = src[i : i+2]
		}
		if two == "==" || two == "!=" || two == "<=" || two == ">=" || two == "+=" || two == "-=" || two == "&&" || two == "||" {
			out = append(out, gsToken{gsSym, two})
			i += 2
			continue
		}
		out = append(out, gsToken{gsSym, src[i : i+1]})
		i++
	}
	out = append(out, gsToken{gsEOF, ""})
	return out
}

func isGSIdent(c byte) bool {
	return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
}

type gsParser struct {
	toks []gsToken
	i    int
	err  string
}

func (p *gsParser) peek() gsToken { return p.toks[p.i] }

func (p *gsParser) next() gsToken {
	t := p.toks[p.i]
	if p.i+1 < len(p.toks) {
		p.i++
	}
	return t
}

func (p *gsParser) accept(kind int, text string) bool {
	t := p.peek()
	if t.kind == kind && (text == "" || t.text == text) {
		p.next()
		return true
	}
	return false
}

func parseGS(src string, comp *Component) string {
	p := &gsParser{toks: lexGS(src)}
	for p.peek().kind != gsEOF && p.err == "" {
		if p.peek().kind == gsSym && p.peek().text == ";" {
			p.next()
			continue
		}
		if p.peek().kind != gsIdent {
			p.err = "expected state, handle, or func"
			break
		}
		word := p.next().text
		if word == "state" {
			if !p.accept(gsSym, "{") {
				p.err = "state expects {"
				break
			}
			body := p.parseBlock()
			exec := &gsExec{comp: comp, locals: map[string]gval{}}
			exec.run(body)
		} else if word == "handle" {
			if p.peek().kind != gsIdent {
				p.err = "handle expects a name"
				break
			}
			name := p.next().text
			if !p.accept(gsSym, "{") {
				p.err = "handle expects {"
				break
			}
			comp.handlers[name] = p.parseBlock()
		} else if word == "func" {
			if p.peek().kind != gsIdent {
				p.err = "func expects a name"
				break
			}
			name := p.next().text
			params := []string{}
			if !p.accept(gsSym, "(") {
				p.err = "func expects ("
				break
			}
			for !p.accept(gsSym, ")") && p.peek().kind != gsEOF {
				if p.peek().kind == gsIdent {
					params = append(params, p.next().text)
				} else {
					p.next()
				}
				p.accept(gsSym, ",")
			}
			if !p.accept(gsSym, "{") {
				p.err = "func expects {"
				break
			}
			comp.funcs[name] = &gsFunc{params: params, body: p.parseBlock()}
		} else {
			p.err = "unknown script block " + word
			break
		}
	}
	return p.err
}

func (p *gsParser) parseBlock() []gsStmt {
	list := []gsStmt{}
	for p.err == "" && p.peek().kind != gsEOF && !(p.peek().kind == gsSym && p.peek().text == "}") {
		if p.peek().kind == gsSym && p.peek().text == ";" {
			p.next()
			continue
		}
		list = append(list, p.parseStmt())
	}
	p.accept(gsSym, "}")
	return list
}

func (p *gsParser) parseStmt() gsStmt {
	if p.accept(gsIdent, "if") {
		cond := p.parseExpr()
		if !p.accept(gsSym, "{") {
			p.err = "if expects {"
			return gsStmt{}
		}
		body := p.parseBlock()
		alt := []gsStmt{}
		if p.accept(gsIdent, "else") {
			if !p.accept(gsSym, "{") {
				p.err = "else expects {"
				return gsStmt{}
			}
			alt = p.parseBlock()
		}
		return gsStmt{kind: sIf, expr: cond, body: body, alt: alt}
	}
	if p.accept(gsIdent, "for") {
		iter := ""
		if p.peek().kind == gsIdent {
			iter = p.next().text
		}
		if !p.accept(gsIdent, "in") {
			p.err = "for expects in"
			return gsStmt{}
		}
		coll := ""
		if p.peek().kind == gsIdent {
			coll = p.next().text
		}
		if !p.accept(gsSym, "{") {
			p.err = "for expects {"
			return gsStmt{}
		}
		return gsStmt{kind: sFor, iter: iter, coll: coll, body: p.parseBlock()}
	}
	if p.accept(gsIdent, "return") {
		var value *gsExpr
		if !(p.peek().kind == gsSym && (p.peek().text == "}" || p.peek().text == ";")) {
			value = p.parseExpr()
		}
		return gsStmt{kind: sReturn, expr: value}
	}
	if p.peek().kind == gsIdent && p.i+1 < len(p.toks) {
		op := p.toks[p.i+1]
		if op.kind == gsSym && (op.text == "=" || op.text == "+=" || op.text == "-=" || op.text == ":") {
			path := p.next().text
			got := p.next().text
			if got == ":" {
				got = "="
			}
			return gsStmt{kind: sAssign, path: path, op: got, expr: p.parseExpr()}
		}
	}
	return gsStmt{kind: sExpr, expr: p.parseExpr()}
}

func (p *gsParser) parseExpr() *gsExpr { return p.parseOr() }

func (p *gsParser) parseOr() *gsExpr {
	left := p.parseAnd()
	for p.peek().kind == gsSym && p.peek().text == "||" {
		p.next()
		left = &gsExpr{kind: eBin, op: "||", left: left, right: p.parseAnd()}
	}
	return left
}

func (p *gsParser) parseAnd() *gsExpr {
	left := p.parseCmp()
	for p.peek().kind == gsSym && p.peek().text == "&&" {
		p.next()
		left = &gsExpr{kind: eBin, op: "&&", left: left, right: p.parseCmp()}
	}
	return left
}

func (p *gsParser) parseCmp() *gsExpr {
	left := p.parseAdd()
	op := p.peek()
	if op.kind == gsSym && (op.text == "==" || op.text == "!=" || op.text == "<" || op.text == ">" || op.text == "<=" || op.text == ">=") {
		p.next()
		return &gsExpr{kind: eBin, op: op.text, left: left, right: p.parseAdd()}
	}
	return left
}

func (p *gsParser) parseAdd() *gsExpr {
	left := p.parseMul()
	for p.peek().kind == gsSym && (p.peek().text == "+" || p.peek().text == "-") {
		op := p.next().text
		left = &gsExpr{kind: eBin, op: op, left: left, right: p.parseMul()}
	}
	return left
}

func (p *gsParser) parseMul() *gsExpr {
	left := p.parseUnary()
	for p.peek().kind == gsSym && (p.peek().text == "*" || p.peek().text == "/") {
		op := p.next().text
		left = &gsExpr{kind: eBin, op: op, left: left, right: p.parseUnary()}
	}
	return left
}

func (p *gsParser) parseUnary() *gsExpr {
	if p.peek().kind == gsSym && (p.peek().text == "!" || p.peek().text == "-") {
		op := p.next().text
		return &gsExpr{kind: eUn, op: op, left: p.parseUnary()}
	}
	return p.parsePrimary()
}

func (p *gsParser) parsePrimary() *gsExpr {
	t := p.peek()
	if t.kind == gsNum {
		p.next()
		n, _ := strconv.Atoi(t.text)
		return &gsExpr{kind: eLit, i: n}
	}
	if t.kind == gsStr {
		p.next()
		return &gsExpr{kind: eLit, op: "str", s: t.text}
	}
	if t.kind == gsIdent {
		p.next()
		if p.accept(gsSym, "(") {
			args := []*gsExpr{}
			for !p.accept(gsSym, ")") && p.peek().kind != gsEOF {
				args = append(args, p.parseExpr())
				p.accept(gsSym, ",")
			}
			return &gsExpr{kind: eCall, name: t.text, args: args}
		}
		return &gsExpr{kind: ePath, path: t.text}
	}
	if p.accept(gsSym, "(") {
		inner := p.parseExpr()
		p.accept(gsSym, ")")
		return inner
	}
	p.err = "bad expression"
	p.next()
	return &gsExpr{kind: eLit, op: "str", s: ""}
}
