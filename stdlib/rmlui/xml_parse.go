// Port of the RmlUi XML parse used by Factory::InstanceDocumentStream and
// InstanceElementText: a small RML reader (tags, attributes, comments,
// head/style/link/title, body). It is not a general XML stack.
package rmlui

type rmlParser struct {
	src   string
	i     int
	doc   *ElementDocument
	sheet string
	// hold is set while reading <head>, so unknown tags are not inserted
	// into the document body.
	hold bool
}

// ParseRMLDocument fills doc from an RML string. The document element is
// the body; style and link in head or body are merged into the sheet.
func ParseRMLDocument(doc *ElementDocument, rml string) bool {
	if doc == nil {
		return false
	}
	p := &rmlParser{src: rml, doc: doc}
	ok := p.parseChildren(nil, "")
	if p.sheet != "" {
		doc.AppendStyleSheet(p.sheet)
	}
	doc.element.DirtyDefinition(dirtySelf)
	return ok
}

// ParseRMLFragment instances rml as children of parent. Style tags in the
// fragment are merged into the owner document when it has one.
func ParseRMLFragment(parent *Element, rml string) bool {
	if parent == nil {
		return false
	}
	p := &rmlParser{src: rml, doc: parent.GetOwnerDocument()}
	ok := p.parseChildren(parent, "")
	if p.sheet != "" && p.doc != nil {
		p.doc.AppendStyleSheet(p.sheet)
	}
	return ok
}

func (p *rmlParser) parseChildren(parent *Element, stop string) bool {
	for p.i < len(p.src) {
		if p.startsWith("<!--") {
			p.skipUntil("-->")
			continue
		}
		if p.startsWith("<?") {
			p.skipUntil("?>")
			continue
		}
		if p.startsWith("</") {
			p.i += 2
			name := p.readName()
			p.skipSpace()
			if p.i < len(p.src) && p.src[p.i] == '>' {
				p.i++
			}
			if stop == "" || name == stop {
				return true
			}
			continue
		}
		if p.i < len(p.src) && p.src[p.i] == '<' {
			p.parseTag(parent)
			continue
		}
		text := p.readText()
		if parent != nil && !stringIsBlank(text) {
			FactoryInstanceElementText(parent, StringDecodeRml(text))
		}
	}
	return true
}

func (p *rmlParser) parseTag(parent *Element) {
	p.i++ // '<'
	name := StringToLower(p.readName())
	attrs := map[string]Variant{}
	selfClose := false
	for p.i < len(p.src) {
		p.skipSpace()
		if p.i >= len(p.src) {
			break
		}
		if p.startsWith("/>") {
			p.i += 2
			selfClose = true
			break
		}
		if p.src[p.i] == '>' {
			p.i++
			break
		}
		attr := p.readName()
		p.skipSpace()
		value := ""
		if p.i < len(p.src) && p.src[p.i] == '=' {
			p.i++
			p.skipSpace()
			value = p.readAttrValue()
		}
		if attr != "" {
			attrs[attr] = VariantString(StringDecodeRml(value))
		}
	}

	if name == "rml" || name == "html" {
		if !selfClose {
			p.parseChildren(parent, name)
		}
		return
	}
	if name == "head" {
		previous := p.hold
		p.hold = true
		if !selfClose {
			p.parseChildren(nil, "head")
		}
		p.hold = previous
		return
	}
	if name == "style" {
		if !selfClose {
			p.sheet = p.sheet + p.readRawUntil("style")
		}
		return
	}
	if name == "link" {
		p.takeLink(attrs)
		return
	}
	if name == "title" {
		title := ""
		if !selfClose {
			title = StringStripWhitespace(p.readRawUntil("title"))
		}
		if p.doc != nil {
			p.doc.SetTitle(title)
		}
		return
	}
	if name == "script" {
		if !selfClose {
			p.readRawUntil("script")
		}
		return
	}

	target := parent
	if name == "body" && p.doc != nil {
		target = p.doc.element
		p.applyAttrs(target, attrs)
		if !selfClose {
			p.parseChildren(target, "body")
		}
		return
	}
	if target == nil && p.doc != nil && !p.hold {
		target = p.doc.element
	}
	if target == nil || name == "" {
		if !selfClose {
			p.parseChildren(nil, name)
		}
		return
	}
	child := FactoryInstanceElement(target, name, name, attrs)
	if !selfClose {
		p.parseChildren(child, name)
	}
}

func (p *rmlParser) applyAttrs(element *Element, attrs map[string]Variant) {
	for name, value := range attrs {
		element.SetAttribute(name, value)
	}
}

func (p *rmlParser) takeLink(attrs map[string]Variant) {
	href := ""
	if value, ok := attrs["href"]; ok {
		href = value.GetString()
	}
	if href == "" {
		return
	}
	fi := GetFileInterface()
	if fi == nil {
		return
	}
	joined := href
	if p.doc != nil {
		if si := GetSystemInterface(); si != nil {
			joined = si.JoinPath(p.doc.GetSourceURL(), href)
		}
	}
	contents, ok := fi.LoadFile(joined)
	if !ok {
		contents, ok = fi.LoadFile(href)
	}
	if ok {
		p.sheet = p.sheet + contents
	}
}

func (p *rmlParser) readRawUntil(tag string) string {
	close := "</" + tag
	start := p.i
	lower := StringToLower(p.src)
	at := indexFrom(lower, close, p.i)
	if at < 0 {
		p.i = len(p.src)
		return p.src[start:]
	}
	text := p.src[start:at]
	p.i = at + len(close)
	p.skipSpace()
	if p.i < len(p.src) && p.src[p.i] == '>' {
		p.i++
	}
	return text
}

func (p *rmlParser) readText() string {
	start := p.i
	for p.i < len(p.src) && p.src[p.i] != '<' {
		p.i++
	}
	return p.src[start:p.i]
}

func (p *rmlParser) readName() string {
	start := p.i
	for p.i < len(p.src) {
		c := p.src[p.i]
		if (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ':' {
			p.i++
			continue
		}
		break
	}
	return p.src[start:p.i]
}

func (p *rmlParser) readAttrValue() string {
	if p.i >= len(p.src) {
		return ""
	}
	quote := p.src[p.i]
	if quote == '"' || quote == '\'' {
		p.i++
		start := p.i
		for p.i < len(p.src) && p.src[p.i] != quote {
			p.i++
		}
		text := p.src[start:p.i]
		if p.i < len(p.src) {
			p.i++
		}
		return text
	}
	start := p.i
	for p.i < len(p.src) {
		c := p.src[p.i]
		if c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '>' || c == '/' {
			break
		}
		p.i++
	}
	return p.src[start:p.i]
}

func (p *rmlParser) skipSpace() {
	for p.i < len(p.src) {
		c := p.src[p.i]
		if c != ' ' && c != '\t' && c != '\n' && c != '\r' {
			return
		}
		p.i++
	}
}

func (p *rmlParser) startsWith(s string) bool {
	if p.i+len(s) > len(p.src) {
		return false
	}
	return p.src[p.i:p.i+len(s)] == s
}

func (p *rmlParser) skipUntil(end string) {
	at := indexFrom(p.src, end, p.i)
	if at < 0 {
		p.i = len(p.src)
		return
	}
	p.i = at + len(end)
}

func stringIsBlank(s string) bool {
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c != ' ' && c != '\t' && c != '\n' && c != '\r' {
			return false
		}
	}
	return true
}
