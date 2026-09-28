// Port of the RmlUi data-model surface Element and Context call
// (DataModel::OnElementRemove, AttachModelRootElement, GetDataModel) plus
// the {{ path }} text views those bindings expand to. Structural
// data-for views are not in this cut; GS and GML bind scalar variables.
package rmlui

// DataModel is Rml::DataModel: a named bag of variables and the text
// views that read them.
type DataModel struct {
	name    string
	context *Context
	vars    map[string]Variant
	views   []*dataTextView
	roots   []*Element
}

type dataTextView struct {
	element *Element
	raw     string
	model   *DataModel
}

// NewDataModel is an empty model. Register it on a context before
// elements with data-model="name" are parented.
func NewDataModel(name string) *DataModel {
	return &DataModel{name: name, vars: map[string]Variant{}}
}

func (m *DataModel) GetName() string { return m.name }

// Set stores a variable and refreshes views that read the model.
func (m *DataModel) Set(name string, value Variant) {
	if m.vars == nil {
		m.vars = map[string]Variant{}
	}
	m.vars[name] = value
	m.UpdateViews()
}

func (m *DataModel) Get(name string) (Variant, bool) {
	value, ok := m.vars[name]
	return value, ok
}

func (m *DataModel) Has(name string) bool {
	_, ok := m.vars[name]
	return ok
}

// AttachModelRootElement is DataModel::AttachModelRootElement.
func (m *DataModel) AttachModelRootElement(element *Element) {
	if element == nil {
		return
	}
	m.roots = append(m.roots, element)
}

// OnElementRemove is DataModel::OnElementRemove.
func (m *DataModel) OnElementRemove(element *Element) {
	keptRoots := []*Element{}
	roots := m.roots
	for _, root := range roots {
		if root != element {
			keptRoots = append(keptRoots, root)
		}
	}
	m.roots = keptRoots
	keptViews := []*dataTextView{}
	views := m.views
	for _, view := range views {
		if view.element != element {
			keptViews = append(keptViews, view)
		}
	}
	m.views = keptViews
}

// BindText registers a text element whose source contains {{ }} bindings.
func (m *DataModel) BindText(element *Element, raw string) {
	views := m.views
	for _, view := range views {
		if view.element == element {
			view.raw = raw
			view.refresh()
			return
		}
	}
	view := &dataTextView{element: element, raw: raw, model: m}
	m.views = append(m.views, view)
	view.refresh()
}

// UpdateViews rewrites every bound text element from the current variables.
func (m *DataModel) UpdateViews() {
	views := m.views
	for _, view := range views {
		view.refresh()
	}
}

func (v *dataTextView) refresh() {
	text := ElementTextOf(v.element)
	if text == nil {
		return
	}
	text.SetText(expandDataTemplate(v.raw, v.model))
}

func expandDataTemplate(raw string, model *DataModel) string {
	out := ""
	i := 0
	for i < len(raw) {
		start := indexFrom(raw, "{{", i)
		if start < 0 {
			out = out + raw[i:]
			break
		}
		out = out + raw[i:start]
		end := indexFrom(raw, "}}", start+2)
		if end < 0 {
			out = out + raw[start:]
			break
		}
		expr := StringStripWhitespace(raw[start+2 : end])
		if len(expr) > 0 && expr[0] == '.' {
			expr = StringStripWhitespace(expr[1:])
		}
		out = out + formatDataExpr(expr, model)
		i = end + 2
	}
	return out
}

func formatDataExpr(expr string, model *DataModel) string {
	if model == nil || expr == "" {
		return ""
	}
	value, ok := model.Get(expr)
	if !ok {
		return ""
	}
	return value.GetString()
}

func indexFrom(s string, needle string, from int) int {
	if from < 0 {
		from = 0
	}
	if needle == "" || from >= len(s) {
		return -1
	}
	last := len(s) - len(needle)
	for i := from; i <= last; i++ {
		if s[i:i+len(needle)] == needle {
			return i
		}
	}
	return -1
}

// BindDataModel attaches model to element and its descendants. This is
// the exported form of the data-model attribute path.
func (e *Element) BindDataModel(model *DataModel) {
	if model != nil {
		model.AttachModelRootElement(e)
	}
	e.setDataModel(model)
}
