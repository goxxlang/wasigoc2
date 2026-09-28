// GML parser. The grammar matches GuiKit's engine: tag.class#id
// :attr."value" ( children ), with quoted text and {{ }} actions kept
// as text. The result is rmlui elements, not an HTML string.
package guikit

import (
	"strings"

	"rmlui"
)

type gmlNode struct {
	tag    string
	text   string
	isText bool
	attrs  map[string]string
	kids   []*gmlNode
}

type gmlTok struct {
	kind int
	text string
}

const (
	gEOF = iota
	gIdent
	gString
	gOther
)

func lexGML(src string) []gmlTok {
	out := []gmlTok{}
	i := 0
	for i < len(src) {
		c := src[i]
		if c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' {
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
		if c == '{' && i+1 < len(src) && src[i+1] == '{' {
			start := i
			i += 2
			for i+1 < len(src) && !(src[i] == '}' && src[i+1] == '}') {
				if src[i] == '"' || src[i] == '\'' || src[i] == '`' {
					i = skipGMLString(src, i)
					continue
				}
				i++
			}
			if i+1 < len(src) {
				i += 2
			}
			out = append(out, gmlTok{gString, src[start:i]})
			continue
		}
		if c == '"' || c == '\'' || c == '`' {
			end := skipGMLString(src, i)
			out = append(out, gmlTok{gString, src[i:end]})
			i = end
			continue
		}
		if isGMLIdent(c, true) {
			start := i
			i++
			for i < len(src) && isGMLIdent(src[i], false) {
				i++
			}
			out = append(out, gmlTok{gIdent, src[start:i]})
			continue
		}
		out = append(out, gmlTok{gOther, src[i : i+1]})
		i++
	}
	out = append(out, gmlTok{gEOF, ""})
	return out
}

func isGMLIdent(c byte, first bool) bool {
	if c == '_' || c == '-' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') {
		return true
	}
	if !first && c >= '0' && c <= '9' {
		return true
	}
	return false
}

func skipGMLString(src string, i int) int {
	if i >= len(src) {
		return i
	}
	quote := src[i]
	i++
	for i < len(src) {
		if src[i] == '\\' && i+1 < len(src) {
			i += 2
			continue
		}
		if src[i] == quote {
			return i + 1
		}
		i++
	}
	return i
}

func unquoteGML(s string) string {
	if len(s) < 2 {
		return s
	}
	first := s[0]
	last := s[len(s)-1]
	if first != last || (first != '"' && first != '\'' && first != '`') {
		return s
	}
	inner := s[1 : len(s)-1]
	if first == '`' {
		return inner
	}
	out := ""
	for i := 0; i < len(inner); i++ {
		if inner[i] == '\\' && i+1 < len(inner) {
			n := inner[i+1]
			if n == 'n' {
				out = out + "\n"
			} else if n == 't' {
				out = out + "\t"
			} else {
				out = out + inner[i+1:i+2]
			}
			i++
			continue
		}
		out = out + inner[i:i+1]
	}
	return out
}

type gmlParser struct {
	toks []gmlTok
	i    int
}

func (p *gmlParser) peek() gmlTok { return p.toks[p.i] }

func (p *gmlParser) next() gmlTok {
	t := p.toks[p.i]
	if p.i+1 < len(p.toks) {
		p.i++
	}
	return t
}

func parseGML(src string) []*gmlNode {
	p := &gmlParser{toks: lexGML(src)}
	nodes := []*gmlNode{}
	for p.peek().kind != gEOF {
		nodes = append(nodes, p.parseExpr())
	}
	return nodes
}

func (p *gmlParser) parseExpr() *gmlNode {
	t := p.peek()
	if t.kind == gIdent {
		p.next()
		node := &gmlNode{tag: t.text, attrs: map[string]string{}}
		for {
			pk := p.peek()
			if pk.kind != gOther || (pk.text != "." && pk.text != "#" && pk.text != ":") {
				break
			}
			p.next()
			if pk.text == "." {
				className := unquoteGML(p.next().text)
				node.attrs["class"] = strings.TrimSpace(node.attrs["class"] + " " + className)
			} else if pk.text == "#" {
				node.attrs["id"] = unquoteGML(p.next().text)
			} else {
				attr := unquoteGML(p.next().text)
				value := "true"
				if p.peek().kind == gOther && p.peek().text == "." {
					p.next()
					value = unquoteGML(p.next().text)
				}
				if attr == "class" {
					node.attrs["class"] = strings.TrimSpace(node.attrs["class"] + " " + value)
				} else {
					node.attrs[attr] = value
				}
			}
		}
		if p.peek().kind == gOther && p.peek().text == "(" {
			p.next()
			for !(p.peek().kind == gOther && p.peek().text == ")") && p.peek().kind != gEOF {
				node.kids = append(node.kids, p.parseExpr())
				if p.peek().kind == gOther && p.peek().text == "," {
					p.next()
				}
			}
			if p.peek().kind == gOther && p.peek().text == ")" {
				p.next()
			}
		}
		return node
	}
	p.next()
	text := t.text
	if t.kind == gString {
		text = unquoteGML(t.text)
	}
	return &gmlNode{isText: true, text: text}
}

type builder struct {
	doc   *rmlui.ElementDocument
	css   string
	gs    string
	slot  []*gmlNode
	files map[string]string
}

func (b *builder) add(parent *rmlui.Element, node *gmlNode) {
	if node == nil || parent == nil {
		return
	}
	if node.isText {
		rmlui.FactoryInstanceElementText(parent, node.text)
		return
	}
	tag := strings.ToLower(node.tag)
	if tag == "style" {
		b.css = b.css + "\n" + nodeText(node)
		return
	}
	if tag == "script" {
		b.gs = b.gs + "\n" + nodeText(node)
		return
	}
	if tag == "rule" {
		b.addRule(node)
		return
	}
	if tag == "wrapper" {
		b.addWrapper(parent, node)
		return
	}
	if tag == "import" {
		b.addImport(parent, node)
		return
	}
	if tag == "markdown" {
		b.addMarkdown(parent, nodeText(node))
		return
	}
	if tag == "slot" {
		saved := b.slot
		for _, child := range saved {
			b.add(parent, child)
		}
		return
	}
	if tag == "html" || tag == "head" {
		for _, child := range node.kids {
			b.add(parent, child)
		}
		return
	}
	if tag == "body" && parent == b.doc.GetElement() {
		b.apply(parent, node.attrs)
		for _, child := range node.kids {
			b.add(parent, child)
		}
		return
	}
	attrs := map[string]rmlui.Variant{}
	for key, value := range node.attrs {
		attrs[gkAttr(key)] = rmlui.VariantString(value)
	}
	element := rmlui.FactoryInstanceElement(parent, tag, tag, attrs)
	if element == nil {
		return
	}
	for _, child := range node.kids {
		b.add(element, child)
	}
}

func gkAttr(name string) string {
	if name == "gk-click" {
		return "onclick"
	}
	if name == "gk-submit" {
		return "onsubmit"
	}
	if name == "gk-change" {
		return "onchange"
	}
	return name
}

func (b *builder) apply(element *rmlui.Element, attrs map[string]string) {
	for key, value := range attrs {
		element.SetAttribute(gkAttr(key), rmlui.VariantString(value))
	}
}

func nodeText(node *gmlNode) string {
	if node == nil {
		return ""
	}
	if node.isText {
		return node.text
	}
	out := ""
	for _, child := range node.kids {
		if out != "" {
			out = out + " "
		}
		out = out + nodeText(child)
	}
	return out
}

func (b *builder) addRule(node *gmlNode) {
	if len(node.kids) == 0 {
		return
	}
	b.css = b.css + "\n" + nodeText(node.kids[0]) + " {\n"
	for i := 1; i < len(node.kids); i++ {
		b.css = b.css + nodeText(node.kids[i]) + ";\n"
	}
	b.css = b.css + "}\n"
}

func (b *builder) addWrapper(parent *rmlui.Element, node *gmlNode) {
	if len(node.kids) == 0 {
		return
	}
	path := cleanGMLPath(nodeText(node.kids[0]))
	if path == "" || strings.HasPrefix(path, "../") || strings.Contains(path, "..") {
		return
	}
	src, ok := b.readFile(path)
	if !ok {
		return
	}
	content := []*gmlNode{}
	if len(node.kids) > 1 {
		content = node.kids[1:]
	}
	saved := b.slot
	b.slot = content
	nodes := parseGML(src)
	for _, child := range nodes {
		b.add(parent, child)
	}
	b.slot = saved
}

func (b *builder) addImport(parent *rmlui.Element, node *gmlNode) {
	if len(node.kids) == 0 {
		return
	}
	path := cleanGMLPath(nodeText(node.kids[0]))
	if path == "" || strings.Contains(path, "..") {
		return
	}
	src, ok := b.readFile(path)
	if !ok {
		return
	}
	if strings.HasSuffix(strings.ToLower(path), ".js") {
		return
	}
	nodes := parseGML(src)
	for _, child := range nodes {
		b.add(parent, child)
	}
}

func (b *builder) readFile(path string) (string, bool) {
	if b.files != nil {
		if contents, ok := b.files[path]; ok {
			return contents, true
		}
	}
	if contents, ok := kitFiles[path]; ok {
		return contents, true
	}
	fi := rmlui.GetFileInterface()
	if fi == nil {
		return "", false
	}
	return fi.LoadFile(path)
}

func cleanGMLPath(path string) string {
	path = strings.TrimSpace(path)
	for strings.Contains(path, "//") {
		path = strings.ReplaceAll(path, "//", "/")
	}
	return path
}

func (b *builder) addMarkdown(parent *rmlui.Element, src string) {
	lines := splitLines(src)
	para := ""
	flush := func() {
		if strings.TrimSpace(para) == "" {
			para = ""
			return
		}
		el := rmlui.FactoryInstanceElement(parent, "p", "p", map[string]rmlui.Variant{})
		if el != nil {
			rmlui.FactoryInstanceElementText(el, inlineMD(para))
		}
		para = ""
	}
	for _, line := range lines {
		trim := strings.TrimSpace(line)
		if trim == "" {
			flush()
			continue
		}
		if strings.HasPrefix(trim, "### ") {
			flush()
			addHeading(parent, "h3", trim[4:])
			continue
		}
		if strings.HasPrefix(trim, "## ") {
			flush()
			addHeading(parent, "h2", trim[3:])
			continue
		}
		if strings.HasPrefix(trim, "# ") {
			flush()
			addHeading(parent, "h1", trim[2:])
			continue
		}
		if para != "" {
			para = para + " "
		}
		para = para + trim
	}
	flush()
}

func addHeading(parent *rmlui.Element, tag string, text string) {
	el := rmlui.FactoryInstanceElement(parent, tag, tag, map[string]rmlui.Variant{})
	if el != nil {
		rmlui.FactoryInstanceElementText(el, inlineMD(text))
	}
}

func inlineMD(s string) string {
	for strings.Contains(s, "**") {
		a := strings.Index(s, "**")
		b := strings.Index(s[a+2:], "**")
		if b < 0 {
			break
		}
		s = s[:a] + s[a+2:a+2+b] + s[a+2+b+2:]
	}
	return s
}

func splitLines(s string) []string {
	out := []string{}
	start := 0
	for i := 0; i < len(s); i++ {
		if s[i] == '\n' {
			line := s[start:i]
			if len(line) > 0 && line[len(line)-1] == '\r' {
				line = line[:len(line)-1]
			}
			out = append(out, line)
			start = i + 1
		}
	}
	if start <= len(s) {
		out = append(out, s[start:])
	}
	return out
}
