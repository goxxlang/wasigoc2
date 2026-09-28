// Load compiles a GML document or a state/style/view/handle document onto
// an rmlui context. Invoke runs one GS handler against that document.
package guikit

import (
	"errors"
	"strings"

	"rmlui"
)

// kitFiles are paths AddFile published for wrapper and import.
var kitFiles = map[string]string{}

var (
	instancerReady bool
	loaded         = map[*rmlui.ElementDocument]*Component{}
)

type kitInstancer struct{}

func (k kitInstancer) InstanceEventListener(value string, element *rmlui.Element) rmlui.EventListener {
	if element == nil {
		return nil
	}
	comp := loaded[element.GetOwnerDocument()]
	if comp == nil {
		return nil
	}
	return &kitListener{comp: comp, name: value}
}

type kitListener struct {
	comp *Component
	name string
}

func (k *kitListener) ProcessEvent(event *rmlui.Event) {
	if k.comp == nil {
		return
	}
	exec := &gsExec{comp: k.comp, locals: map[string]gval{}}
	k.comp.run(k.name, exec)
}

func (k *kitListener) OnAttach(element *rmlui.Element) {}

func (k *kitListener) OnDetach(element *rmlui.Element) {}

func (c *Component) run(name string, exec *gsExec) {
	body, ok := c.handlers[name]
	if !ok {
		return
	}
	exec.run(body)
}

// AddFile registers path for GML wrapper and import lookups.
func AddFile(path string, contents string) {
	if kitFiles == nil {
		kitFiles = map[string]string{}
	}
	kitFiles[path] = contents
}

// Load compiles src into a document on ctx and lays it out.
func Load(ctx *rmlui.Context, src string) (*rmlui.ElementDocument, error) {
	if ctx == nil {
		return nil, errors.New("guikit: nil context")
	}
	rmlui.Initialise()
	if !instancerReady {
		rmlui.FactoryRegisterEventListenerInstancer(kitInstancer{})
		instancerReady = true
	}
	doc := rmlui.NewElementDocument(ctx)
	model := ctx.RegisterDataModel(ModelName)
	comp := newComponent(doc, model)
	loaded[doc] = comp

	script, css, gml, isDoc := splitDocument(src)
	b := &builder{doc: doc, files: kitFiles}
	if isDoc {
		nodes := parseGML(gml)
		for i := 0; i < len(nodes); i++ {
			b.add(doc.GetElement(), nodes[i])
		}
	} else {
		nodes := parseGML(src)
		for i := 0; i < len(nodes); i++ {
			b.add(doc.GetElement(), nodes[i])
		}
	}
	script = script + "\n" + b.gs
	if strings.TrimSpace(script) != "" {
		msg := parseGS(script, comp)
		if msg != "" {
			return doc, errors.New("guikit: " + msg)
		}
	}
	sheet := css + "\n" + b.css
	if strings.TrimSpace(sheet) != "" {
		doc.AppendStyleSheet(sheet)
	}
	doc.GetElement().BindDataModel(model)
	ctx.Update()
	return doc, nil
}

// Invoke runs the GS handler name on the document Load returned.
func Invoke(doc *rmlui.ElementDocument, name string, data map[string]string) error {
	comp := loaded[doc]
	if comp == nil {
		return errors.New("guikit: document has no script")
	}
	body, ok := comp.handlers[name]
	if !ok {
		return errors.New("guikit: no handler " + name)
	}
	exec := &gsExec{comp: comp, data: data, locals: map[string]gval{}}
	exec.run(body)
	if comp.model != nil {
		comp.model.UpdateViews()
	}
	return nil
}

// ComponentOf returns the script bound to doc.
func ComponentOf(doc *rmlui.ElementDocument) *Component { return loaded[doc] }

func splitDocument(src string) (script string, css string, gml string, isDoc bool) {
	i := 0
	for i < len(src) {
		i = skipSpaceComments(src, i)
		if i >= len(src) {
			break
		}
		if !isGSIdent(src[i]) {
			break
		}
		start := i
		for i < len(src) && (isGSIdent(src[i]) || (src[i] >= '0' && src[i] <= '9')) {
			i++
		}
		word := src[start:i]
		j := skipSpaceComments(src, i)
		if word == "state" || word == "handle" || word == "func" {
			isDoc = true
			end := endOfGSBlock(src, start)
			script = script + src[start:end] + "\n"
			i = end
			continue
		}
		if (word == "style" || word == "view") && j < len(src) && src[j] == '{' {
			isDoc = true
			bodyStart := j + 1
			bodyEnd := matchBrace(src, j)
			body := ""
			if bodyEnd > bodyStart {
				body = src[bodyStart:bodyEnd]
			}
			if word == "style" {
				css = css + body + "\n"
			} else {
				gml = gml + body + "\n"
			}
			i = bodyEnd
			if i < len(src) && src[i] == '}' {
				i++
			}
			continue
		}
		break
	}
	return script, css, gml, isDoc
}

func skipSpaceComments(src string, i int) int {
	for i < len(src) {
		if src[i] == ' ' || src[i] == '\t' || src[i] == '\n' || src[i] == '\r' {
			i++
			continue
		}
		if src[i] == '/' && i+1 < len(src) && src[i+1] == '/' {
			i += 2
			for i < len(src) && src[i] != '\n' {
				i++
			}
			continue
		}
		if src[i] == '/' && i+1 < len(src) && src[i+1] == '*' {
			i += 2
			for i+1 < len(src) && !(src[i] == '*' && src[i+1] == '/') {
				i++
			}
			if i+1 < len(src) {
				i += 2
			}
			continue
		}
		return i
	}
	return i
}

func endOfGSBlock(src string, i int) int {
	depth := 0
	seen := false
	for i < len(src) {
		if src[i] == '"' || src[i] == '\'' || src[i] == '`' {
			i = skipGMLString(src, i)
			continue
		}
		if src[i] == '{' {
			depth++
			seen = true
		} else if src[i] == '}' {
			depth--
			if seen && depth == 0 {
				return i + 1
			}
		}
		i++
	}
	return len(src)
}

func matchBrace(src string, open int) int {
	depth := 0
	i := open
	for i < len(src) {
		if src[i] == '"' || src[i] == '\'' || src[i] == '`' {
			i = skipGMLString(src, i)
			continue
		}
		if src[i] == '{' {
			depth++
		} else if src[i] == '}' {
			depth--
			if depth == 0 {
				return i
			}
		}
		i++
	}
	return len(src)
}
