// Port of RmlUi Source/Core/ElementText.cpp and
// Include/RmlUi/Core/ElementText.h. Generated text geometry is produced
// by the font engine widths; the mesh path waits for a host font engine
// that can rasterise.
package rmlui

// ElementText is Rml::ElementText.
type ElementText struct {
	element  *Element
	text     string
	template string
}

func newElementText(text string) *ElementText {
	el := NewElement("#text")
	textEl := &ElementText{element: el, text: text, template: text}
	el.SetSubclass(textEl)
	virtuals := el.Virtuals()
	virtuals.GetRML = textEl.rml
	virtuals.GetInnerRML = textEl.inner
	return textEl
}

// ElementTextOf returns the text subclass, or nil.
func ElementTextOf(element *Element) *ElementText {
	if element == nil || element.Subclass() == nil {
		return nil
	}
	text, ok := element.Subclass().(*ElementText)
	if !ok {
		return nil
	}
	return text
}

func (t *ElementText) GetElement() *Element { return t.element }

func (t *ElementText) GetText() string { return t.text }

func (t *ElementText) GetTemplate() string { return t.template }

// SetTemplate records the source string, including {{ }} bindings.
func (t *ElementText) SetTemplate(text string) { t.template = text }

// SetText replaces the displayed string and dirties layout when it changes.
func (t *ElementText) SetText(text string) {
	if t.text == text {
		return
	}
	t.text = text
	if t.template == "" {
		t.template = text
	}
	t.element.DirtyLayout()
}

func (t *ElementText) rml() string { return StringEncodeRml(t.text) }

func (t *ElementText) inner() string { return t.text }
