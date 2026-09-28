// Port of RmlUi Source/Core/ElementDocument.cpp and
// Include/RmlUi/Core/ElementDocument.h. The document element is the
// body; head (style, link, title) is consumed while the document is loaded.
package rmlui

// ElementDocument is Rml::ElementDocument.
type ElementDocument struct {
	element     *Element
	context     *Context
	sheet       *StyleSheet
	userSheet   string
	sourceURL   string
	title       string
	layoutDirty bool
	modal       bool
}

// NewElementDocument builds a body document and attaches it to context.
func NewElementDocument(context *Context) *ElementDocument {
	doc := &ElementDocument{context: context, layoutDirty: true, title: ""}
	element := NewElement("body")
	doc.element = element
	element.SetSubclass(doc)
	element.SetOwnerDocument(doc, true)
	virtuals := element.Virtuals()
	virtuals.DirtyLayout = doc.markLayoutDirty
	virtuals.IsLayoutDirty = doc.layoutIsDirty
	virtuals.GetStyleSheet = doc.styleSheet
	if context != nil {
		context.addDocument(doc)
		doc.sheet = DefaultStyleSheet(context)
	}
	element.DirtyDefinition(dirtySelf)
	return doc
}

func (d *ElementDocument) GetElement() *Element { return d.element }

func (d *ElementDocument) GetContext() *Context { return d.context }

func (d *ElementDocument) GetComputedValues() *ComputedValues {
	return d.element.GetComputedValues()
}

func (d *ElementDocument) GetStyleSheet() *StyleSheet { return d.sheet }

func (d *ElementDocument) styleSheet() *StyleSheet { return d.sheet }

func (d *ElementDocument) GetSourceURL() string { return d.sourceURL }

func (d *ElementDocument) SetSourceURL(url string) { d.sourceURL = url }

func (d *ElementDocument) GetTitle() string { return d.title }

func (d *ElementDocument) SetTitle(title string) { d.title = title }

func (d *ElementDocument) markLayoutDirty() { d.layoutDirty = true }

func (d *ElementDocument) layoutIsDirty() bool { return d.layoutDirty }

// SetStyleSheet replaces the cascade with the default sheet plus rcss.
func (d *ElementDocument) SetStyleSheet(rcss string) {
	d.userSheet = rcss
	d.rebuildStyleSheet()
}

// AppendStyleSheet merges another RCSS chunk into the user sheet.
func (d *ElementDocument) AppendStyleSheet(rcss string) {
	if rcss == "" {
		return
	}
	if d.userSheet != "" {
		d.userSheet = d.userSheet + "\n"
	}
	d.userSheet = d.userSheet + rcss
	d.rebuildStyleSheet()
}

func (d *ElementDocument) rebuildStyleSheet() {
	base := DefaultStyleSheet(d.context)
	if d.userSheet == "" || d.context == nil {
		d.sheet = base
	} else {
		user := FactoryInstanceStyleSheetString(d.userSheet, d.context)
		if base != nil && user != nil {
			d.sheet = base.CombineStyleSheet(user)
		} else if user != nil {
			d.sheet = user
		} else {
			d.sheet = base
		}
	}
	d.element.DirtyDefinition(dirtySelf)
	d.markLayoutDirty()
}

// UpdateDocument runs style and data-view refresh, then lays the tree out
// when it is dirty. Context::Update calls this once per document.
func (d *ElementDocument) UpdateDocument(dpRatio float32, vpDimensions Vector2f) {
	if model := d.element.GetDataModel(); model != nil {
		model.UpdateViews()
	}
	d.element.Update(dpRatio, vpDimensions)
	if d.layoutDirty {
		d.layoutDirty = false
		if d.context != nil {
			LayoutDocument(d)
		}
	}
}

// Show makes the document visible.
func (d *ElementDocument) Show() { d.element.SetProperty("visibility", "visible") }

// Hide sets visibility hidden.
func (d *ElementDocument) Hide() { d.element.SetProperty("visibility", "hidden") }

// Close detaches the document from its context and destroys the tree.
func (d *ElementDocument) Close() {
	if d.context != nil {
		d.context.removeDocument(d)
		d.context = nil
	}
	if d.element != nil {
		d.element.Destroy()
	}
}

// GetElementById searches this document.
func (d *ElementDocument) GetElementById(id string) *Element {
	return ElementUtilitiesGetElementById(d.element, id)
}
