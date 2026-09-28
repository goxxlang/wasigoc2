// Port of RmlUi Source/Core/Context.cpp and Include/RmlUi/Core/Context.h,
// for the document, focus, data-model, and update surface the rest of
// the core calls. Input processing beyond click generation is left to
// the host, which feeds events with Element.DispatchEvent.
package rmlui

// Context is Rml::Context.
type Context struct {
	name         string
	dimensions   Vector2i
	dpRatio      float32
	render       *RenderManager
	documents    []*ElementDocument
	focus        *Element
	models       map[string]*DataModel
	themes       map[string]bool
	nextUpdate   float64
}

var contexts = map[string]*Context{}

// CreateContext is Rml::CreateContext.
func CreateContext(name string, dimensions Vector2i) *Context {
	Initialise()
	if _, exists := contexts[name]; exists {
		LogMessage(LogWarning, "Context '"+name+"' already exists.")
		return nil
	}
	ctx := &Context{
		name:       name,
		dimensions: dimensions,
		dpRatio:    1,
		render:     NewRenderManager(nullRenderInterface{}),
		models:     map[string]*DataModel{},
		themes:     map[string]bool{},
		nextUpdate: -1,
	}
	ctx.render.SetViewport(dimensions)
	contexts[name] = ctx
	return ctx
}

// GetContext is Rml::GetContext.
func GetContext(name string) *Context {
	ctx, ok := contexts[name]
	if !ok {
		return nil
	}
	return ctx
}

// RemoveContext is Rml::RemoveContext.
func RemoveContext(name string) {
	delete(contexts, name)
}

func (c *Context) GetName() string { return c.name }

func (c *Context) GetDimensions() Vector2i { return c.dimensions }

// SetDimensions is Context::SetDimensions. Documents are reflowed on the
// next Update.
func (c *Context) SetDimensions(dimensions Vector2i) {
	c.dimensions = dimensions
	if c.render != nil {
		c.render.SetViewport(dimensions)
	}
	docs := c.documents
	for _, doc := range docs {
		doc.GetElement().DirtyLayout()
	}
}

func (c *Context) GetDensityIndependentPixelRatio() float32 { return c.dpRatio }

func (c *Context) SetDensityIndependentPixelRatio(ratio float32) {
	if ratio <= 0 {
		ratio = 1
	}
	if c.dpRatio == ratio {
		return
	}
	c.dpRatio = ratio
	docs := c.documents
	for _, doc := range docs {
		doc.GetElement().DirtyLayout()
	}
}

func (c *Context) GetRenderManager() *RenderManager { return c.render }

func (c *Context) GetFocusElement() *Element { return c.focus }

// IsThemeActive is Context::IsThemeActive (media query "theme").
func (c *Context) IsThemeActive(theme string) bool { return c.themes[theme] }

// ActivateTheme sets a named theme for media queries.
func (c *Context) ActivateTheme(theme string, active bool) {
	if active {
		c.themes[theme] = true
	} else {
		delete(c.themes, theme)
	}
}

// RequestNextUpdate is Context::RequestNextUpdate.
func (c *Context) RequestNextUpdate(delay float64) {
	if c.nextUpdate < 0 || delay < c.nextUpdate {
		c.nextUpdate = delay
	}
}

// OnFocusChange is the focus move Element::Focus asks the context for.
func (c *Context) OnFocusChange(element *Element, focusVisible bool) bool {
	if c.focus != nil && c.focus != element {
		c.focus.SetPseudoClass("focus", false)
		c.focus.SetPseudoClass("focus-visible", false)
	}
	c.focus = element
	if element != nil {
		element.SetPseudoClass("focus", true)
		if focusVisible {
			element.SetPseudoClass("focus-visible", true)
		}
	}
	return true
}

func (c *Context) OnElementDetach(element *Element) {
	if c.focus == element {
		c.focus = nil
	}
}

// GenerateClickEvent is Context::GenerateClickEvent.
func (c *Context) GenerateClickEvent(element *Element) {
	if element == nil {
		return
	}
	element.DispatchEvent("click", map[string]Variant{})
}

// PerformSmoothscrollOnTarget applies the scroll delta immediately. The
// smooth animation clock is the host's; the offset still lands.
func (c *Context) PerformSmoothscrollOnTarget(element *Element, delta Vector2f, behavior ScrollBehavior) {
	if element == nil {
		return
	}
	element.SetScrollLeft(element.GetScrollLeft() + delta.X)
	element.SetScrollTop(element.GetScrollTop() + delta.Y)
}

func (c *Context) addDocument(doc *ElementDocument) {
	c.documents = append(c.documents, doc)
}

func (c *Context) removeDocument(doc *ElementDocument) {
	out := []*ElementDocument{}
	docs := c.documents
	for _, have := range docs {
		if have != doc {
			out = append(out, have)
		}
	}
	c.documents = out
}

// LoadDocument reads path through the FileInterface and instances it.
func (c *Context) LoadDocument(path string) *ElementDocument {
	fi := GetFileInterface()
	if fi == nil {
		return nil
	}
	contents, ok := fi.LoadFile(path)
	if !ok {
		LogMessage(LogWarning, "Unable to open document "+path+".")
		return nil
	}
	doc := c.LoadDocumentFromMemory(contents)
	if doc != nil {
		doc.sourceURL = path
	}
	return doc
}

// LoadDocumentFromMemory instances an RML document from a string.
func (c *Context) LoadDocumentFromMemory(rml string) *ElementDocument {
	doc := NewElementDocument(c)
	if !ParseRMLDocument(doc, rml) {
		doc.Close()
		return nil
	}
	return doc
}

// GetDataModelPtr is Context::GetDataModel (the raw pointer).
func (c *Context) GetDataModelPtr(name string) *DataModel {
	model, ok := c.models[name]
	if !ok {
		return nil
	}
	return model
}

// RegisterDataModel inserts a model under name. The same pointer is
// returned so the caller can keep filling it.
func (c *Context) RegisterDataModel(name string) *DataModel {
	if have := c.GetDataModelPtr(name); have != nil {
		return have
	}
	model := NewDataModel(name)
	model.context = c
	c.models[name] = model
	return model
}

// Update is Context::Update: style, data views, then layout.
func (c *Context) Update() {
	c.nextUpdate = -1
	vp := c.dimensions.ToFloat()
	docs := c.documents
	for _, doc := range docs {
		doc.UpdateDocument(c.dpRatio, vp)
	}
}

// Render is Context::Render.
func (c *Context) Render() {
	if c.render != nil {
		c.render.PrepareRender(c.dimensions)
	}
	docs := c.documents
	for _, doc := range docs {
		doc.GetElement().Render()
	}
}
