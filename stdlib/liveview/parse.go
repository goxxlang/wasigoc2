package liveview

import (
	"strconv"
	"strings"
)

type parser struct {
	src string
	i   int
	err string
}

func (p *parser) fail(msg string) {
	if p.err == "" {
		p.err = "liveview: " + msg
	}
}

func (p *parser) atEnd() bool {
	return p.i >= len(p.src)
}

func (p *parser) peek() byte {
	if p.i >= len(p.src) {
		return 0
	}
	return p.src[p.i]
}

func (p *parser) peekAt(off int) byte {
	j := p.i + off
	if j >= len(p.src) {
		return 0
	}
	return p.src[j]
}

func (p *parser) bump() {
	if p.i < len(p.src) {
		p.i = p.i + 1
	}
}

func (p *parser) starts(s string) bool {
	if p.i+len(s) > len(p.src) {
		return false
	}
	return p.src[p.i:p.i+len(s)] == s
}

func (p *parser) startsFold(s string) bool {
	if p.i+len(s) > len(p.src) {
		return false
	}
	return strings.EqualFold(p.src[p.i:p.i+len(s)], s)
}

func isSpace(c byte) bool {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r'
}

func isLetter(c byte) bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
}

func isDigit(c byte) bool {
	return c >= '0' && c <= '9'
}

func isIdentCont(c byte) bool {
	return isLetter(c) || isDigit(c) || c == '-'
}

func (p *parser) skipLang() {
	for p.err == "" && !p.atEnd() {
		c := p.peek()
		if isSpace(c) {
			p.bump()
			continue
		}
		if c == '/' && p.peekAt(1) == '/' {
			p.i = p.i + 2
			for !p.atEnd() && p.peek() != '\n' {
				p.bump()
			}
			continue
		}
		if c == '/' && p.peekAt(1) == '*' {
			p.i = p.i + 2
			for !p.atEnd() && !(p.peek() == '*' && p.peekAt(1) == '/') {
				p.bump()
			}
			if !p.atEnd() {
				p.i = p.i + 2
			}
			continue
		}
		return
	}
}

func (p *parser) takeIdent() string {
	if !isLetter(p.peek()) {
		return ""
	}
	start := p.i
	p.bump()
	for isIdentCont(p.peek()) {
		p.bump()
	}
	return p.src[start:p.i]
}

func (p *parser) takeNumber() string {
	start := p.i
	if p.peek() == '-' {
		p.bump()
	}
	for isDigit(p.peek()) {
		p.bump()
	}
	if p.peek() == '.' && isDigit(p.peekAt(1)) {
		p.bump()
		for isDigit(p.peek()) {
			p.bump()
		}
	}
	return p.src[start:p.i]
}

func (p *parser) takeString() string {
	q := p.peek()
	if q != '"' && q != '\'' && q != '`' {
		return ""
	}
	p.bump()
	var b strings.Builder
	for !p.atEnd() {
		c := p.peek()
		p.bump()
		if c == q {
			break
		}
		if c == '\\' && q != '`' && !p.atEnd() {
			n := p.peek()
			p.bump()
			if n == 'n' {
				b.WriteString("\n")
			} else if n == 't' {
				b.WriteString("\t")
			} else if n == 'r' {
				b.WriteString("\r")
			} else {
				b.WriteString(string([]byte{n}))
			}
			continue
		}
		b.WriteString(string([]byte{c}))
	}
	return b.String()
}

func (p *parser) skipTo(s string) {
	k := strings.Index(p.src[p.i:], s)
	if k < 0 {
		p.i = len(p.src)
		return
	}
	p.i = p.i + k + len(s)
}

func (p *parser) takeSimpleBlock() string {
	if p.peek() != '{' {
		p.fail("expected '{'")
		return ""
	}
	p.bump()
	start := p.i
	depth := 1
	quote := byte(0)
	for !p.atEnd() && depth > 0 {
		c := p.peek()
		if quote != 0 {
			if c == '\\' && quote != '`' {
				p.bump()
				p.bump()
				continue
			}
			if c == quote {
				quote = 0
			}
			p.bump()
			continue
		}
		if p.starts("{{") {
			p.skipTo("}}")
			continue
		}
		if p.starts("{%") {
			p.skipTo("%}")
			continue
		}
		if c == '"' || c == '\'' || c == '`' {
			quote = c
			p.bump()
			continue
		}
		if c == '/' && p.peekAt(1) == '/' {
			for !p.atEnd() && p.peek() != '\n' {
				p.bump()
			}
			continue
		}
		if c == '{' {
			depth = depth + 1
			p.bump()
			continue
		}
		if c == '}' {
			depth = depth - 1
			if depth == 0 {
				inner := p.src[start:p.i]
				p.bump()
				return inner
			}
			p.bump()
			continue
		}
		p.bump()
	}
	p.fail("unterminated block")
	return p.src[start:]
}

func (p *parser) takeViewBlock() string {
	if p.peek() != '{' {
		p.fail("expected '{'")
		return ""
	}
	p.bump()
	start := p.i
	depth := 1
	quote := byte(0)
	inStyle := false
	inScript := false
	for !p.atEnd() && depth > 0 {
		c := p.peek()
		if quote != 0 {
			if c == '\\' && quote != '`' {
				p.bump()
				p.bump()
				continue
			}
			if c == quote {
				quote = 0
			}
			p.bump()
			continue
		}
		if p.starts("{{") {
			p.skipTo("}}")
			continue
		}
		if p.starts("{%") {
			p.skipTo("%}")
			continue
		}
		if inStyle {
			if p.startsFold("</style>") {
				inStyle = false
				p.i = p.i + 8
				continue
			}
			if c == '"' || c == '\'' {
				quote = c
			}
			p.bump()
			continue
		}
		if inScript {
			if p.startsFold("</script>") {
				inScript = false
				p.i = p.i + 9
				continue
			}
			if c == '"' || c == '\'' || c == '`' {
				quote = c
			}
			p.bump()
			continue
		}
		if c == '"' || c == '\'' || c == '`' {
			quote = c
			p.bump()
			continue
		}
		if c == '<' {
			if p.startsFold("<style") {
				inStyle = true
			} else if p.startsFold("<script") {
				inScript = true
			}
			p.bump()
			continue
		}
		if c == '{' {
			depth = depth + 1
			p.bump()
			continue
		}
		if c == '}' {
			depth = depth - 1
			if depth == 0 {
				inner := p.src[start:p.i]
				p.bump()
				return inner
			}
			p.bump()
			continue
		}
		p.bump()
	}
	p.fail("unterminated view block")
	return p.src[start:]
}

func (p *parser) parseDoc() *Doc {
	d := &Doc{}
	for p.err == "" {
		p.skipLang()
		if p.atEnd() {
			break
		}
		kw := p.takeIdent()
		if kw != "live" {
			if len(d.Comps) == 0 {
				p.fail("expected 'live'")
			}
			break
		}
		p.skipLang()
		name := p.takeIdent()
		if name == "" {
			p.fail("expected component name")
			break
		}
		p.skipLang()
		if p.peek() != '{' {
			p.fail("expected '{' after live name")
			break
		}
		inner := p.takeSimpleBlock()
		c := parseLiveBody(name, inner)
		d.Comps = append(d.Comps, c)
	}
	return d
}

func parseLiveBody(name string, src string) Component {
	c := Component{
		Name:   name,
		State:  map[string]any{},
		Events: map[string][]Stmt{},
		Funcs:  map[string]Func{},
	}
	p := &parser{src: src, i: 0}
	for p.err == "" && !p.atEnd() {
		p.skipLang()
		if p.atEnd() {
			break
		}
		kw := p.takeIdent()
		if kw == "" {
			p.fail("unexpected token in live body")
			break
		}
		p.skipLang()
		if kw == "state" {
			inner := p.takeSimpleBlock()
			sp := &parser{src: inner, i: 0}
			c.State = sp.parseStateMap()
		} else if kw == "style" {
			c.Style = strings.TrimSpace(p.takeSimpleBlock())
		} else if kw == "view" {
			inner := p.takeViewBlock()
			vp := &parser{src: inner, i: 0}
			c.View = vp.parseMarkup("", true)
		} else if kw == "event" || kw == "handle" {
			ev := p.takeIdent()
			p.skipLang()
			inner := p.takeSimpleBlock()
			sp := &parser{src: inner, i: 0}
			c.Events[ev] = sp.parseStmts()
		} else if kw == "func" {
			fn := p.takeIdent()
			p.skipLang()
			params := []string{}
			if p.peek() == '(' {
				p.bump()
				for p.err == "" && p.peek() != ')' && !p.atEnd() {
					p.skipLang()
					if p.peek() == ')' {
						break
					}
					prm := p.takeIdent()
					if prm != "" {
						params = append(params, prm)
					}
					p.skipLang()
					if p.peek() == ',' {
						p.bump()
					}
				}
				if p.peek() == ')' {
					p.bump()
				}
			}
			p.skipLang()
			inner := p.takeSimpleBlock()
			sp := &parser{src: inner, i: 0}
			c.Funcs[fn] = Func{Params: params, Body: sp.parseStmts()}
		} else {
			p.fail("unknown live section '" + kw + "'")
			break
		}
	}
	return c
}

func (p *parser) parseStateMap() map[string]any {
	m := map[string]any{}
	for p.err == "" && !p.atEnd() {
		p.skipLang()
		if p.atEnd() {
			break
		}
		key := p.takeIdent()
		if key == "" {
			p.fail("expected state key")
			break
		}
		p.skipLang()
		if p.peek() == ':' || p.peek() == '=' {
			p.bump()
		}
		p.skipLang()
		m[key] = p.parseValue()
		p.skipLang()
		if p.peek() == ',' {
			p.bump()
		}
	}
	return m
}

func (p *parser) parseValue() any {
	p.skipLang()
	c := p.peek()
	if c == '"' || c == '\'' || c == '`' {
		return p.takeString()
	}
	if c == '[' {
		p.bump()
		var arr []any
		for p.err == "" && p.peek() != ']' && !p.atEnd() {
			p.skipLang()
			if p.peek() == ']' {
				break
			}
			arr = append(arr, p.parseValue())
			p.skipLang()
			if p.peek() == ',' {
				p.bump()
			}
		}
		if p.peek() == ']' {
			p.bump()
		}
		return arr
	}
	if c == '{' {
		inner := p.takeSimpleBlock()
		sp := &parser{src: inner, i: 0}
		return sp.parseStateMap()
	}
	if isLetter(c) {
		id := p.takeIdent()
		if id == "true" {
			return true
		}
		if id == "false" {
			return false
		}
		if id == "null" || id == "nil" {
			return nil
		}
		return id
	}
	if isDigit(c) || (c == '-' && isDigit(p.peekAt(1))) {
		num := p.takeNumber()
		if strings.Contains(num, ".") {
			f, err := strconv.ParseFloat(num)
			if err != nil {
				return 0
			}
			return f
		}
		n, err := strconv.Atoi(num)
		if err != nil {
			return 0
		}
		return n
	}
	p.fail("invalid value")
	return nil
}

func (p *parser) skipMarkupSpace() {
	for !p.atEnd() && isSpace(p.peek()) {
		p.bump()
	}
}

func (p *parser) parseMarkup(endTag string, gmlOK bool) []Node {
	var nodes []Node
	for p.err == "" && !p.atEnd() {
		p.skipMarkupSpace()
		if p.atEnd() {
			break
		}
		if endTag == "" && p.peek() == '}' {
			break
		}
		if endTag == ")" && p.peek() == ')' {
			break
		}
		if p.peek() == '<' && p.peekAt(1) == '/' {
			name := p.takeCloseTag()
			if endTag == "" || strings.EqualFold(name, endTag) {
				break
			}
			continue
		}
		n := p.parseMarkupOne(gmlOK)
		if n.Kind == nText && n.Text == "" && len(n.Kids) == 0 {
			continue
		}
		nodes = append(nodes, n)
	}
	return nodes
}

func (p *parser) takeCloseTag() string {
	p.i = p.i + 2
	p.skipMarkupSpace()
	name := p.takeIdent()
	for !p.atEnd() && p.peek() != '>' {
		p.bump()
	}
	if p.peek() == '>' {
		p.bump()
	}
	return name
}

func (p *parser) parseMarkupOne(gmlOK bool) Node {
	if p.starts("{{") || p.starts("{%") {
		return Node{Kind: nText, Text: p.takeAction()}
	}
	if p.peek() == '<' {
		return p.parseHTML()
	}
	c := p.peek()
	if c == '"' || c == '\'' || c == '`' {
		return Node{Kind: nText, Text: p.takeString()}
	}
	if gmlOK && isLetter(c) {
		return p.parseGML()
	}
	start := p.i
	p.bump()
	for !p.atEnd() {
		ch := p.peek()
		if ch == '<' || p.starts("{{") || p.starts("{%") {
			break
		}
		if gmlOK && (isLetter(ch) || ch == '"' || ch == '`') {
			break
		}
		if ch == '}' || ch == ')' {
			break
		}
		p.bump()
	}
	return Node{Kind: nText, Text: p.src[start:p.i]}
}

func (p *parser) takeAction() string {
	start := p.i
	if p.starts("{{") {
		p.skipTo("}}")
		return p.src[start:p.i]
	}
	if p.starts("{%") {
		p.skipTo("%}")
		return p.src[start:p.i]
	}
	return ""
}

func isVoid(tag string) bool {
	t := strings.ToLower(tag)
	return t == "area" || t == "base" || t == "br" || t == "col" || t == "embed" ||
		t == "hr" || t == "img" || t == "input" || t == "link" || t == "meta" ||
		t == "param" || t == "source" || t == "track" || t == "wbr"
}

func (p *parser) parseHTML() Node {
	p.bump()
	p.skipMarkupSpace()
	tag := p.takeIdent()
	n := Node{Kind: nElem, Tag: tag}
	selfClose := false
	for p.err == "" && !p.atEnd() {
		p.skipMarkupSpace()
		c := p.peek()
		if c == '>' {
			p.bump()
			break
		}
		if c == '/' && p.peekAt(1) == '>' {
			selfClose = true
			p.i = p.i + 2
			break
		}
		key := p.takeAttrName()
		if key == "" {
			p.bump()
			continue
		}
		p.skipMarkupSpace()
		val := ""
		if p.peek() == '=' {
			p.bump()
			p.skipMarkupSpace()
			qc := p.peek()
			if qc == '"' || qc == '\'' || qc == '`' {
				val = p.takeString()
			} else {
				start := p.i
				for !p.atEnd() && !isSpace(p.peek()) && p.peek() != '>' && p.peek() != '/' {
					p.bump()
				}
				val = p.src[start:p.i]
			}
		}
		n.Attrs = append(n.Attrs, Attr{Key: key, Val: val})
	}
	if selfClose || isVoid(tag) {
		return n
	}
	n.Kids = p.parseMarkup(tag, false)
	return n
}

func (p *parser) takeAttrName() string {
	if !isLetter(p.peek()) && p.peek() != ':' {
		return ""
	}
	start := p.i
	p.bump()
	for {
		c := p.peek()
		if isIdentCont(c) || c == ':' {
			p.bump()
			continue
		}
		break
	}
	return p.src[start:p.i]
}

func (p *parser) parseGML() Node {
	tag := p.takeIdent()
	n := Node{Kind: nElem, Tag: tag}
	for {
		p.skipMarkupSpace()
		if p.peek() != '.' && p.peek() != '#' && p.peek() != ':' {
			break
		}
		mod := p.peek()
		p.bump()
		if mod == '.' {
			cls := p.takeIdentOrString()
			n.Attrs = appendClass(n.Attrs, cls)
		} else if mod == '#' {
			n.Attrs = setAttr(n.Attrs, "id", p.takeIdentOrString())
		} else {
			key := p.takeIdentOrString()
			val := "true"
			if p.peek() == '.' {
				p.bump()
				val = p.takeIdentOrString()
			}
			if key == "class" {
				n.Attrs = appendClass(n.Attrs, val)
			} else {
				n.Attrs = setAttr(n.Attrs, key, val)
			}
		}
	}
	p.skipMarkupSpace()
	if p.peek() == '{' {
		p.bump()
		n.Kids = p.parseMarkup("", true)
		p.skipMarkupSpace()
		if p.peek() == '}' {
			p.bump()
		}
		return n
	}
	if p.peek() == '(' {
		p.bump()
		n.Kids = p.parseMarkup(")", true)
		p.skipMarkupSpace()
		if p.peek() == ')' {
			p.bump()
		}
		return n
	}
	qc := p.peek()
	if qc == '"' || qc == '\'' || qc == '`' {
		n.Kids = []Node{{Kind: nText, Text: p.takeString()}}
		return n
	}
	if p.starts("{{") || p.starts("{%") {
		n.Kids = []Node{{Kind: nText, Text: p.takeAction()}}
		return n
	}
	return n
}

func (p *parser) takeIdentOrString() string {
	c := p.peek()
	if c == '"' || c == '\'' || c == '`' {
		return p.takeString()
	}
	return p.takeIdent()
}

func appendClass(attrs []Attr, cls string) []Attr {
	for i := 0; i < len(attrs); i++ {
		if attrs[i].Key == "class" {
			cur := strings.TrimSpace(attrs[i].Val + " " + cls)
			attrs[i].Val = cur
			return attrs
		}
	}
	return append(attrs, Attr{Key: "class", Val: cls})
}

func setAttr(attrs []Attr, key string, val string) []Attr {
	for i := 0; i < len(attrs); i++ {
		if attrs[i].Key == key {
			attrs[i].Val = val
			return attrs
		}
	}
	return append(attrs, Attr{Key: key, Val: val})
}

func (p *parser) parseStmts() []Stmt {
	var out []Stmt
	for p.err == "" && !p.atEnd() {
		p.skipLang()
		if p.atEnd() || p.peek() == '}' {
			break
		}
		st := p.parseStmt()
		if st.Kind != 0 {
			out = append(out, st)
		}
	}
	return out
}

func (p *parser) parseStmt() Stmt {
	p.skipLang()
	id := p.takeIdent()
	if id == "" {
		p.bump()
		return Stmt{}
	}
	if id == "if" {
		return p.parseIf()
	}
	if id == "for" {
		return p.parseFor()
	}
	if id == "return" {
		return Stmt{Kind: sReturn, X: p.parseExpr(0)}
	}
	path := id
	for p.peek() == '.' {
		p.bump()
		path = path + "." + p.takeIdent()
	}
	p.skipLang()
	if p.peek() == '(' {
		p.bump()
		var args []*Expr
		for p.err == "" && p.peek() != ')' && !p.atEnd() {
			p.skipLang()
			if p.peek() == ')' {
				break
			}
			args = append(args, p.parseExpr(0))
			p.skipLang()
			if p.peek() == ',' {
				p.bump()
			}
		}
		if p.peek() == ')' {
			p.bump()
		}
		return Stmt{Kind: sCall, CallName: path, Args: args}
	}
	op := ""
	if p.peek() == '+' && p.peekAt(1) == '=' {
		op = "+="
		p.i = p.i + 2
	} else if p.peek() == '-' && p.peekAt(1) == '=' {
		op = "-="
		p.i = p.i + 2
	} else if p.peek() == '=' {
		op = "="
		p.bump()
	} else {
		p.fail("expected assignment or call")
		return Stmt{}
	}
	return Stmt{Kind: sAssign, Path: path, Op: op, X: p.parseExpr(0)}
}

func (p *parser) parseIf() Stmt {
	cond := p.parseExpr(0)
	p.skipLang()
	inner := p.takeSimpleBlock()
	bp := &parser{src: inner, i: 0}
	st := Stmt{Kind: sIf, X: cond, Body: bp.parseStmts()}
	p.skipLang()
	if p.takeIdentLookahead("else") {
		p.skipLang()
		p.takeIdent()
		p.skipLang()
		if p.takeIdentLookahead("if") {
			p.takeIdent()
			nested := p.parseIf()
			st.Else = []Stmt{nested}
			return st
		}
		einner := p.takeSimpleBlock()
		ep := &parser{src: einner, i: 0}
		st.Else = ep.parseStmts()
	}
	return st
}

func (p *parser) takeIdentLookahead(want string) bool {
	save := p.i
	p.skipLang()
	got := p.takeIdent()
	p.i = save
	return got == want
}

func (p *parser) parseFor() Stmt {
	p.skipLang()
	iter := p.takeIdent()
	p.skipLang()
	if p.takeIdent() != "in" {
		p.fail("expected 'in' in for")
	}
	p.skipLang()
	coll := p.takeIdent()
	for p.peek() == '.' {
		p.bump()
		coll = coll + "." + p.takeIdent()
	}
	p.skipLang()
	inner := p.takeSimpleBlock()
	bp := &parser{src: inner, i: 0}
	return Stmt{Kind: sFor, Iter: iter, Coll: coll, Body: bp.parseStmts()}
}

func precOf(op string) int {
	if op == "||" {
		return 1
	}
	if op == "&&" {
		return 2
	}
	if op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=" {
		return 3
	}
	if op == "+" || op == "-" {
		return 4
	}
	if op == "*" || op == "/" {
		return 5
	}
	return 0
}

func (p *parser) peekOp() string {
	c := p.peek()
	n := p.peekAt(1)
	if c == '|' && n == '|' {
		return "||"
	}
	if c == '&' && n == '&' {
		return "&&"
	}
	if c == '=' && n == '=' {
		return "=="
	}
	if c == '!' && n == '=' {
		return "!="
	}
	if c == '<' && n == '=' {
		return "<="
	}
	if c == '>' && n == '=' {
		return ">="
	}
	if c == '<' {
		return "<"
	}
	if c == '>' {
		return ">"
	}
	if c == '+' {
		return "+"
	}
	if c == '-' {
		return "-"
	}
	if c == '*' {
		return "*"
	}
	if c == '/' {
		return "/"
	}
	return ""
}

func (p *parser) parseExpr(prec int) *Expr {
	left := p.parsePrimary()
	for p.err == "" {
		p.skipLang()
		op := p.peekOp()
		opPrec := precOf(op)
		if op == "" || opPrec <= prec {
			break
		}
		p.i = p.i + len(op)
		right := p.parseExpr(opPrec)
		left = &Expr{Kind: xBin, Op: op, Left: left, Right: right}
	}
	return left
}

func (p *parser) parsePrimary() *Expr {
	p.skipLang()
	c := p.peek()
	if c == '!' {
		p.bump()
		inner := p.parsePrimary()
		return &Expr{Kind: xUn, Op: "!", Left: inner}
	}
	if c == '-' {
		p.bump()
		inner := p.parsePrimary()
		return &Expr{Kind: xUn, Op: "-", Left: inner}
	}
	if c == '"' || c == '\'' || c == '`' {
		return &Expr{Kind: xLit, Val: p.takeString()}
	}
	if c == '(' {
		p.bump()
		x := p.parseExpr(0)
		p.skipLang()
		if p.peek() == ')' {
			p.bump()
		}
		return x
	}
	if isDigit(c) {
		num := p.takeNumber()
		if strings.Contains(num, ".") {
			f, _ := strconv.ParseFloat(num)
			return &Expr{Kind: xLit, Val: f}
		}
		n, _ := strconv.Atoi(num)
		return &Expr{Kind: xLit, Val: n}
	}
	if isLetter(c) {
		id := p.takeIdent()
		if id == "true" {
			return &Expr{Kind: xLit, Val: true}
		}
		if id == "false" {
			return &Expr{Kind: xLit, Val: false}
		}
		if id == "null" || id == "nil" {
			return &Expr{Kind: xLit, Val: nil}
		}
		path := id
		for p.peek() == '.' {
			p.bump()
			path = path + "." + p.takeIdent()
		}
		p.skipLang()
		if p.peek() == '(' {
			p.bump()
			var args []*Expr
			for p.err == "" && p.peek() != ')' && !p.atEnd() {
				p.skipLang()
				if p.peek() == ')' {
					break
				}
				args = append(args, p.parseExpr(0))
				p.skipLang()
				if p.peek() == ',' {
					p.bump()
				}
			}
			if p.peek() == ')' {
				p.bump()
			}
			return &Expr{Kind: xCall, Name: path, Args: args}
		}
		return &Expr{Kind: xVar, Path: path}
	}
	p.fail("expected expression")
	return &Expr{Kind: xLit, Val: nil}
}

