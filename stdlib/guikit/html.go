// HTML pages from GML documents.
package guikit

import (
	"errors"
	"strings"

	"rmlui"
)

// Page is what HTML puts around the document.
type Page struct {
	Title   string
	Head    string   // more HTML in <head> (a <link>, a <meta>)
	Tail    string   // HTML after the elements (the page's <script>s)
	Require []string // ids the view must have: a missing one is an error
}

var voidTags = map[string]bool{"input": true, "img": true, "br": true, "hr": true, "meta": true, "link": true}

// eventAttrs are guikit's handler attributes: the page's script binds
// those events itself, so they are not written out.
var eventAttrs = map[string]bool{"onclick": true, "onsubmit": true, "onchange": true}

func htmlEscape(s string) string {
	s = strings.ReplaceAll(s, "&", "&amp;")
	s = strings.ReplaceAll(s, "<", "&lt;")
	s = strings.ReplaceAll(s, ">", "&gt;")
	s = strings.ReplaceAll(s, "\"", "&quot;")
	return s
}

func sortNames(a []string) {
	for i := 1; i < len(a); i++ {
		for j := i; j > 0 && a[j] < a[j-1]; j-- {
			a[j], a[j-1] = a[j-1], a[j]
		}
	}
}

func writeElement(b *strings.Builder, el *rmlui.Element) {
	if el == nil {
		return
	}
	if el.GetTagName() == "#text" {
		text := rmlui.ElementTextOf(el)
		if text != nil {
			b.WriteString(htmlEscape(text.GetText()))
		}
		return
	}
	tag := el.GetTagName()
	if tag == "" {
		tag = "div"
	}
	b.WriteString("<" + tag)
	names := el.GetAttributeNames()
	sortNames(names)
	for i := 0; i < len(names); i++ {
		name := names[i]
		if eventAttrs[name] {
			continue
		}
		value := el.GetAttributeString(name, "")
		if value == "true" && (name == "selected" || name == "checked" || name == "disabled" || name == "hidden" ||
			name == "multiple" || name == "webkitdirectory") {
			b.WriteString(" " + name)
			continue
		}
		b.WriteString(" " + name + "=\"" + htmlEscape(value) + "\"")
	}
	b.WriteString(">")
	if voidTags[tag] {
		return
	}
	n := el.GetNumChildren(false)
	for i := 0; i < n; i++ {
		writeElement(b, el.GetChild(i))
	}
	b.WriteString("</" + tag + ">")
}

// block is the text inside `word { ... }` at the top of a document.
func block(src string, word string) string {
	i := strings.Index(src, word+" {")
	if i < 0 {
		return ""
	}
	start := i + len(word) + 1
	depth := 0
	for k := start; k < len(src); k++ {
		if src[k] == '{' {
			depth++
		}
		if src[k] == '}' {
			depth--
			if depth == 0 {
				return src[start+1 : k]
			}
		}
	}
	return ""
}

var rmluiReady = false

// HTML renders a GML document as a standalone HTML page: its style block
// becomes the page's CSS and its view the page's elements, as guikit
// builds them on an rmlui tree.
func HTML(src string, p Page) (string, error) {
	if !rmluiReady {
		quiet := rmlui.NewDefaultSystemInterface()
		quiet.Quiet = true
		rmlui.SetSystemInterface(quiet)
		rmlui.Initialise()
		rmluiReady = true
	}
	ctx := rmlui.CreateContext("guikit-html", rmlui.Vector2i{1440, 900})
	doc, err := Load(ctx, src)
	if err != nil {
		return "", err
	}
	if doc == nil {
		return "", errors.New("guikit: the document has no view")
	}
	for _, id := range p.Require {
		if doc.GetElementById(id) == nil {
			return "", errors.New("guikit: the view has no #" + id)
		}
	}
	var b strings.Builder
	b.WriteString("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\"/>\n")
	b.WriteString("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n")
	b.WriteString("<title>" + htmlEscape(p.Title) + "</title>\n")
	b.WriteString(p.Head)
	b.WriteString("<style>\n" + block(src, "style") + "\n</style>\n</head>\n<body>\n")
	root := doc.GetElement()
	n := root.GetNumChildren(false)
	for i := 0; i < n; i++ {
		writeElement(&b, root.GetChild(i))
		b.WriteString("\n")
	}
	b.WriteString(p.Tail)
	b.WriteString("</body>\n</html>\n")
	return b.String(), nil
}
