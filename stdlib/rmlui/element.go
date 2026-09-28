// Port of RmlUi Source/Core/Element.cpp and Include/RmlUi/Core/Element.h.
package rmlui

// childNotifyLevels is how many levels up OnChildAdd/OnChildRemove are
// called, starting at the child itself.
const childNotifyLevels = 2

// ScrollBehavior is Rml::ScrollBehavior.
type ScrollBehavior = int

const (
	ScrollBehaviorAuto ScrollBehavior = iota
	ScrollBehaviorSmooth
	ScrollBehaviorInstant
)

// ScrollAlignment is Rml::ScrollAlignment.
type ScrollAlignment = int

const (
	ScrollAlignmentStart ScrollAlignment = iota
	ScrollAlignmentCenter
	ScrollAlignmentEnd
	ScrollAlignmentNearest
	ScrollAlignmentAdaptive
)

// ScrollParentage is Rml::ScrollParentage.
type ScrollParentage = int

const (
	ScrollParentageAll ScrollParentage = iota
	ScrollParentageClosest
)

// ScrollIntoViewOptions is Rml::ScrollIntoViewOptions.
type ScrollIntoViewOptions struct {
	Vertical   ScrollAlignment
	Horizontal ScrollAlignment
	Behavior   ScrollBehavior
	Parentage  ScrollParentage
}

func DefaultScrollIntoViewOptions() ScrollIntoViewOptions {
	return ScrollIntoViewOptions{ScrollAlignmentStart, ScrollAlignmentNearest, ScrollBehaviorInstant, ScrollParentageAll}
}

// EdgeSizes is Rml::EdgeSizes: top, right, bottom, left.
type EdgeSizes = [4]float32

// RenderBox is Rml::RenderBox.
type RenderBox struct {
	FillSize     Vector2f
	BorderOffset Vector2f
	BorderWidths EdgeSizes
	BorderRadius CornerSizes
}

func (r RenderBox) GetFillOffset() Vector2f { return Vector2f{r.BorderWidths[3], r.BorderWidths[0]} }

func (r RenderBox) Equals(o RenderBox) bool {
	if !r.FillSize.Equals(o.FillSize) || !r.BorderOffset.Equals(o.BorderOffset) {
		return false
	}
	for i := 0; i < 4; i++ {
		if r.BorderWidths[i] != o.BorderWidths[i] || r.BorderRadius[i] != o.BorderRadius[i] {
			return false
		}
	}
	return true
}

// IntrinsicDimensions is the out-parameters of GetIntrinsicDimensions.
type IntrinsicDimensions struct {
	Dimensions Vector2f
	Ratio      float32
	Replaced   bool
}

// ElementVirtuals holds an element subclass's overrides of the Element
// virtual methods. A nil entry uses the base behavior. Each entry is
// bound to the subclass object (a method value), so it can reach its own
// state; overrides call the matching Base* method on Element to chain up.
type ElementVirtuals struct {
	OnUpdate               func()
	OnRender               func()
	OnResize               func()
	OnLayout               func()
	OnDpRatioChange        func()
	OnStyleSheetChange     func()
	OnAttributeChange      func(changed map[string]Variant)
	OnPropertyChange       func(changed PropertyIdSet)
	OnPseudoClassChange    func(pseudoClass string, activate bool)
	OnChildAdd             func(child *Element)
	OnChildRemove          func(child *Element)
	DirtyLayout            func()
	IsLayoutDirty          func() bool
	GetRML                 func() string
	GetInnerRML            func() string
	SetInnerRML            func(rml string)
	GetBaseline            func() float32
	GetIntrinsicDimensions func() IntrinsicDimensions
	IsPointWithinElement   func(point Vector2f) bool
	ProcessDefaultAction   func(event *Event)
	GetStyleSheet          func() *StyleSheet
	OnDestroy              func()
}

// ElementSubclass is implemented by the Go type behind an element subclass
// (ElementText, ElementDocument, form controls, ...), so an element can be
// downcast with a type assertion on Element.Subclass().
type ElementSubclass interface {
	GetElement() *Element
}

type positionedBox struct {
	box    Box
	offset Vector2f
}

// Stacking-context render orders (RenderOrder).
const (
	renderOrderStackNegative = iota
	renderOrderBlock
	renderOrderTableColumnGroup
	renderOrderTableColumn
	renderOrderTableRowGroup
	renderOrderTableRow
	renderOrderTableCell
	renderOrderFloating
	renderOrderInline
	renderOrderPositioned
	renderOrderStackPositive
)

type stackingContextChild struct {
	element *Element
	order   int
}

// Element is Rml::Element: a node in the document tree.
type Element struct {
	localStackingContext                bool
	localStackingContextForced          bool
	stackingContextDirty                bool
	computedValuesAreDefaultInitialized bool
	visible                             bool
	offsetFixed                         bool
	absoluteOffsetDirty                 bool
	roundedMainPaddingSizeDirty         bool
	dirtyDefinition                     bool
	dirtyChildDefinitions               bool
	dirtyAnimation                      bool
	dirtyTransition                     bool
	dirtyTransform                      bool
	dirtyPerspective                    bool

	children          []*Element
	numNonDomChildren int
	clipArea          BoxArea
	tag               string
	id                string
	instancer         ElementInstancer
	parent            *Element
	focus             *Element
	ownerDocument     *ElementDocument
	dataModel         *DataModel
	attributes        map[string]*Variant

	offsetParent           *Element
	relativeOffsetBase     Vector2f
	relativeOffsetPosition Vector2f
	absoluteOffset         Vector2f
	roundedMainPaddingSize Vector2f
	scrollOffset           Vector2f

	mainBox                     Box
	additionalBoxes             []positionedBox
	scrollableOverflowRectangle Vector2f
	baseline                    float32
	zIndex                      float32
	stackingContext             []*Element
	transformState              *TransformState
	animations                  []*ElementAnimation

	// ElementMeta.
	style                   *ElementStyle
	computedValues          *ComputedValues
	eventDispatcher         *EventDispatcher
	scroll                  *ElementScroll
	backgroundBorder        *ElementBackgroundBorder
	effects                 *ElementEffects
	attributeEventListeners map[EventId]EventListener

	virtuals *ElementVirtuals
	subclass ElementSubclass
}

// NewElement is Element(tag). Use the Factory to create elements with
// their registered subclass.
func NewElement(tag string) *Element {
	e := &Element{
		computedValuesAreDefaultInitialized: true,
		visible:                             true,
		absoluteOffsetDirty:                 true,
		roundedMainPaddingSizeDirty:         true,
		tag:                                 tag,
		clipArea:                            BoxAreaPadding,
		attributes:                          map[string]*Variant{},
		attributeEventListeners:             map[EventId]EventListener{},
		virtuals:                            &ElementVirtuals{},
	}
	e.style = NewElementStyle(e)
	e.computedValues = NewComputedValues(e)
	e.eventDispatcher = NewEventDispatcher(e)
	e.scroll = NewElementScroll(e)
	e.backgroundBorder = NewElementBackgroundBorder()
	e.effects = NewElementEffects(e)
	return e
}

// Virtuals returns the override table; subclasses fill it at construction.
func (e *Element) Virtuals() *ElementVirtuals { return e.virtuals }

// SetSubclass records the subclass object for downcasting.
func (e *Element) SetSubclass(s ElementSubclass) { e.subclass = s }

// Subclass returns the subclass object, or nil for a plain element.
func (e *Element) Subclass() ElementSubclass { return e.subclass }

// Destroy is ~Element: detaches every child and notifies them.
func (e *Element) Destroy() {
	PluginRegistryNotifyElementDestroy(e)
	if e.virtuals.OnDestroy != nil {
		e.virtuals.OnDestroy()
	}
	children := e.children
	for _, child := range children {
		ancestor := child
		for i := 0; i <= childNotifyLevels && ancestor != nil; i++ {
			ancestor.OnChildRemove(child)
			ancestor = ancestor.GetParentNode()
		}
		child.setParent(nil)
	}
	e.children = nil
	e.numNonDomChildren = 0
}

func getScrollOffsetDelta(alignment ScrollAlignment, beginOffset float32, endOffset float32) float32 {
	switch alignment {
	case ScrollAlignmentStart:
		return beginOffset
	case ScrollAlignmentCenter:
		return (beginOffset + endOffset) / 2
	case ScrollAlignmentEnd:
		return endOffset
	case ScrollAlignmentNearest:
		if beginOffset >= 0 && endOffset <= 0 {
			return 0
		} else if beginOffset < 0 && endOffset < 0 {
			return MathMax(beginOffset, endOffset)
		} else if beginOffset > 0 && endOffset > 0 {
			return MathMin(beginOffset, endOffset)
		}
		return 0
	case ScrollAlignmentAdaptive:
		if beginOffset >= 0 && endOffset <= 0 {
			return 0
		}
		return (beginOffset + endOffset) / 2
	}
	return 0
}

// Update is Element::Update, called once per context update.
func (e *Element) Update(dpRatio float32, vpDimensions Vector2f) {
	e.OnUpdate()
	e.handleTransitionProperty()
	e.handleAnimationProperty()
	e.advanceAnimations()
	e.scroll.Update()
	e.UpdateProperties(dpRatio, vpDimensions)
	if e.dirtyAnimation {
		e.handleAnimationProperty()
		e.advanceAnimations()
		e.UpdateProperties(dpRatio, vpDimensions)
	}
	e.effects.InstanceEffects()
	for i := 0; i < len(e.children); i++ {
		e.children[i].Update(dpRatio, vpDimensions)
	}
	if len(e.animations) > 0 && e.IsVisible(true) {
		if ctx := e.GetContext(); ctx != nil {
			ctx.RequestNextUpdate(0)
		}
	}
}

func (e *Element) UpdateProperties(dpRatio float32, vpDimensions Vector2f) {
	e.updateDefinition()
	if e.style.AnyPropertiesDirty() {
		var parentValues *ComputedValues
		if e.parent != nil {
			parentValues = e.parent.GetComputedValues()
		}
		var documentValues *ComputedValues
		if e.ownerDocument != nil {
			documentValues = e.ownerDocument.GetComputedValues()
		}
		dirty := e.style.ComputeValues(e.computedValues, parentValues, documentValues, e.computedValuesAreDefaultInitialized, dpRatio, vpDimensions)
		e.computedValuesAreDefaultInitialized = false
		if !dirty.Empty() {
			e.OnPropertyChange(dirty)
		}
	}
}

// Render is Element::Render.
func (e *Element) Render() {
	e.updateAbsoluteOffsetAndRenderBoxData()
	if e.stackingContextDirty {
		e.buildLocalStackingContext()
	}
	e.updateTransformState()
	ElementUtilitiesApplyTransform(e)
	e.effects.RenderEffects(RenderStageEnter)
	if ElementUtilitiesSetClippingRegion(e, false) {
		e.backgroundBorder.Render(e)
		e.effects.RenderEffects(RenderStageDecoration)
		e.OnRender()
	}
	stacking := e.stackingContext
	for _, element := range stacking {
		element.Render()
	}
	ElementUtilitiesApplyTransform(e)
	e.effects.RenderEffects(RenderStageExit)
}

// Clone returns a new, unparented copy of the element and its content.
func (e *Element) Clone() *Element {
	var clone *Element
	attrs := e.GetAttributes()
	if e.instancer != nil {
		clone = e.instancer.InstanceElement(nil, e.GetTagName(), attrs)
		if clone != nil {
			clone.SetInstancer(e.instancer)
		}
	} else {
		clone = FactoryInstanceElement(nil, e.GetTagName(), e.GetTagName(), attrs)
	}
	if clone != nil {
		cloneAttributes := map[string]Variant{}
		for name, value := range attrs {
			if name != "style" && name != "class" {
				cloneAttributes[name] = value
			}
		}
		clone.SetAttributes(cloneAttributes)
		local := e.style.GetLocalStyleProperties()
		props := local.GetProperties()
		for id, p := range props {
			clone.SetPropertyById(id, *p)
		}
		customs := local.GetCustomProperties()
		for name, p := range customs {
			clone.SetProperty(name, p.ToString())
		}
		shorthands := local.GetVarShorthands()
		for id, p := range shorthands {
			clone.SetProperty(GetShorthandName(id), p.ToString())
		}
		clone.GetStyle().SetClassNames(e.style.GetClassNames())
		clone.SetInnerRML(e.GetInnerRML())
	}
	return clone
}

func (e *Element) SetClass(className string, activate bool) {
	if e.style.SetClass(className, activate) {
		e.DirtyDefinition(dirtySelfAndSiblings)
	}
}

func (e *Element) IsClassSet(className string) bool { return e.style.IsClassSet(className) }

func (e *Element) SetClassNames(classNames string) {
	e.SetAttribute("class", VariantString(classNames))
}

func (e *Element) GetClassNames() string { return e.style.GetClassNames() }

// GetStyleSheet returns the document's style sheet (virtual).
func (e *Element) GetStyleSheet() *StyleSheet {
	if e.virtuals.GetStyleSheet != nil {
		return e.virtuals.GetStyleSheet()
	}
	if doc := e.GetOwnerDocument(); doc != nil {
		return doc.GetStyleSheet()
	}
	return nil
}

// GetAddress builds a selector-like address such as "div#id.class < body".
func (e *Element) GetAddress(includePseudoClasses bool, includeParents bool) string {
	address := e.tag
	if e.id != "" {
		address = address + "#" + e.id
	}
	classes := e.style.GetClassNames()
	if classes != "" {
		address = address + "." + StringReplaceChar(classes, ' ', '.')
	}
	if includePseudoClasses {
		pcs := e.style.GetActivePseudoClasses()
		for _, pc := range pcs {
			address = address + ":" + pc
		}
	}
	if includeParents && e.parent != nil {
		return address + " < " + e.parent.GetAddress(includePseudoClasses, true)
	}
	return address
}

func (e *Element) SetOffset(offset Vector2f, offsetParent *Element, offsetFixed bool) {
	offsetFixed = offsetFixed || e.GetPosition() == PositionFixed
	if !e.relativeOffsetBase.Equals(offset) || e.offsetParent != offsetParent || e.offsetFixed != offsetFixed {
		e.relativeOffsetBase = offset
		e.offsetFixed = offsetFixed
		e.offsetParent = offsetParent
		e.updateOffset()
		e.DirtyAbsoluteOffset()
	} else {
		oldBase := e.relativeOffsetBase
		oldPosition := e.relativeOffsetPosition
		e.updateOffset()
		if !oldBase.Equals(e.relativeOffsetBase) || !oldPosition.Equals(e.relativeOffsetPosition) {
			e.DirtyAbsoluteOffset()
		}
	}
}

func (e *Element) GetRelativeOffset(area BoxArea) Vector2f {
	return e.relativeOffsetBase.Add(e.relativeOffsetPosition).Add(e.mainBox.GetPosition(area))
}

func (e *Element) GetAbsoluteOffset(area BoxArea) Vector2f {
	e.updateAbsoluteOffsetAndRenderBoxData()
	if area == BoxAreaBorder {
		return e.absoluteOffset
	}
	return e.absoluteOffset.Add(e.mainBox.GetPosition(area))
}

func (e *Element) updateAbsoluteOffsetAndRenderBoxData() {
	if !e.absoluteOffsetDirty && !e.roundedMainPaddingSizeDirty {
		return
	}
	e.absoluteOffsetDirty = false
	e.roundedMainPaddingSizeDirty = false
	offsetFromAncestors := Vector2f{}
	if e.offsetParent != nil {
		offsetFromAncestors = e.offsetParent.GetAbsoluteOffset(BoxAreaBorder)
	}
	if !e.offsetFixed {
		if e.offsetParent != nil {
			offsetFromAncestors = offsetFromAncestors.Sub(e.offsetParent.scrollOffset)
		}
		ancestor := e.parent
		for ancestor != nil && ancestor != e.offsetParent {
			offsetFromAncestors = offsetFromAncestors.Add(ancestor.relativeOffsetPosition)
			ancestor = ancestor.parent
		}
	}
	relativeOffset := e.relativeOffsetBase.Add(e.relativeOffsetPosition)
	e.absoluteOffset = relativeOffset.Add(offsetFromAncestors)
	mainPaddingSize := e.mainBox.GetSize(BoxAreaPadding)
	bottomRight := relativeOffset.Add(mainPaddingSize).Add(offsetFromAncestors)
	newRounded := bottomRight.Round().Sub(e.absoluteOffset.Round())
	if !newRounded.Equals(e.roundedMainPaddingSize) {
		e.roundedMainPaddingSize = newRounded
		e.backgroundBorder.DirtyBackground()
		e.backgroundBorder.DirtyBorder()
		e.effects.DirtyEffectsData()
	}
}

func (e *Element) SetClipArea(clipArea BoxArea) { e.clipArea = clipArea }
func (e *Element) GetClipArea() BoxArea         { return e.clipArea }

func (e *Element) SetScrollableOverflowRectangle(rect Vector2f, clampScrollOffset bool) {
	if !e.scrollableOverflowRectangle.Equals(rect) {
		e.scrollableOverflowRectangle = rect
		if clampScrollOffset {
			e.clampScrollOffset()
		}
	}
}

func (e *Element) SetBox(box Box) {
	if !box.Equals(&e.mainBox) || len(e.additionalBoxes) > 0 {
		e.mainBox = box
		e.additionalBoxes = nil
		e.OnResize()
		e.roundedMainPaddingSizeDirty = true
		e.backgroundBorder.DirtyBackground()
		e.backgroundBorder.DirtyBorder()
		e.effects.DirtyEffectsData()
		if e.transformState != nil {
			e.dirtyTransformState(true, true)
		}
	}
}

func (e *Element) AddBox(box Box, offset Vector2f) {
	e.additionalBoxes = append(e.additionalBoxes, positionedBox{box, offset})
	e.OnResize()
	e.backgroundBorder.DirtyBackground()
	e.backgroundBorder.DirtyBorder()
	e.effects.DirtyEffectsData()
}

// GetBox returns the main box.
func (e *Element) GetBox() *Box { return &e.mainBox }

// GetBoxIndex returns box index (0 = main) and its offset from the border box.
func (e *Element) GetBoxIndex(index int) (*Box, Vector2f) {
	additional := index - 1
	if index < 1 || additional >= len(e.additionalBoxes) {
		return &e.mainBox, Vector2f{}
	}
	return &e.additionalBoxes[additional].box, e.additionalBoxes[additional].offset
}

func (e *Element) GetRenderBox(fillArea BoxArea, index int) RenderBox {
	e.updateAbsoluteOffsetAndRenderBoxData()
	box := &e.mainBox
	paddingSize := e.roundedMainPaddingSize
	offset := Vector2f{}
	additional := index - 1
	if index >= 1 && additional < len(e.additionalBoxes) {
		box = &e.additionalBoxes[additional].box
		paddingSize = box.GetSize(BoxAreaPadding)
		offset = e.additionalBoxes[additional].offset.Round()
	}
	var edges EdgeSizes
	for area := BoxAreaBorder; area < fillArea; area++ {
		edges[0] += box.GetEdge(area, BoxEdgeTop)
		edges[1] += box.GetEdge(area, BoxEdgeRight)
		edges[2] += box.GetEdge(area, BoxEdgeBottom)
		edges[3] += box.GetEdge(area, BoxEdgeLeft)
	}
	inner := Vector2f{}
	switch fillArea {
	case BoxAreaBorder:
		inner = paddingSize.Add(box.GetFrameSize(BoxAreaBorder))
	case BoxAreaPadding:
		inner = paddingSize
	case BoxAreaContent:
		inner = paddingSize.Sub(box.GetFrameSize(BoxAreaPadding))
	}
	return RenderBox{FillSize: inner, BorderOffset: offset, BorderWidths: edges, BorderRadius: e.computedValues.BorderRadius()}
}

func (e *Element) GetNumBoxes() int { return 1 + len(e.additionalBoxes) }

// GetBaseline is the baseline in px from the bottom margin edge (virtual).
func (e *Element) GetBaseline() float32 {
	if e.virtuals.GetBaseline != nil {
		return e.virtuals.GetBaseline()
	}
	return e.baseline
}

// GetIntrinsicDimensions (virtual) reports a replaced element's size.
func (e *Element) GetIntrinsicDimensions() IntrinsicDimensions {
	if e.virtuals.GetIntrinsicDimensions != nil {
		return e.virtuals.GetIntrinsicDimensions()
	}
	return IntrinsicDimensions{}
}

func (e *Element) IsReplaced() bool { return e.GetIntrinsicDimensions().Replaced }

// IsPointWithinElement (virtual) tests a point against every border box.
func (e *Element) IsPointWithinElement(point Vector2f) bool {
	if e.virtuals.IsPointWithinElement != nil {
		return e.virtuals.IsPointWithinElement(point)
	}
	return e.BaseIsPointWithinElement(point)
}

func (e *Element) BaseIsPointWithinElement(point Vector2f) bool {
	position := e.GetAbsoluteOffset(BoxAreaBorder)
	for i := 0; i < e.GetNumBoxes(); i++ {
		box, boxOffset := e.GetBoxIndex(i)
		boxPosition := position.Add(boxOffset)
		dims := box.GetSize(BoxAreaBorder)
		if point.X >= boxPosition.X && point.X <= boxPosition.X+dims.X && point.Y >= boxPosition.Y && point.Y <= boxPosition.Y+dims.Y {
			return true
		}
	}
	return false
}

func (e *Element) IsVisible(includeAncestors bool) bool {
	if !includeAncestors {
		return e.visible
	}
	element := e
	for element != nil {
		if !element.visible {
			return false
		}
		element = element.parent
	}
	return true
}

func (e *Element) GetZIndex() float32              { return e.zIndex }
func (e *Element) GetFontFaceHandle() FontFaceHandle { return e.computedValues.FontFaceHandle() }

// SetProperty parses and sets an inline property (name may be a shorthand).
func (e *Element) SetProperty(name string, value string) bool {
	properties := NewPropertyDictionary()
	if !StyleParsePropertyDeclaration(properties, name, value) {
		LogMessage(LogWarning, "Syntax error parsing inline property declaration '"+name+": "+value+";'.")
		return false
	}
	props := properties.GetProperties()
	ids := sortedPropertyIds(props)
	for _, id := range ids {
		if !e.style.SetProperty(id, *props[id]) {
			return false
		}
	}
	customs := properties.GetCustomProperties()
	for n, p := range customs {
		e.style.SetCustomProperty(n, *p)
	}
	shorthands := properties.GetVarShorthands()
	for id, p := range shorthands {
		e.style.SetVarShorthand(id, *p)
	}
	return true
}

func sortedPropertyIds(props map[PropertyId]*Property) []PropertyId {
	set := PropertyIdSet{}
	for id := range props {
		set.Insert(id)
	}
	return set.Ids()
}

// SetPropertyById sets a pre-parsed inline property.
func (e *Element) SetPropertyById(id PropertyId, property Property) bool {
	return e.style.SetProperty(id, property)
}

func (e *Element) RemoveProperty(name string) {
	id := GetPropertyId(name)
	if id != PropertyIdInvalid {
		e.style.RemoveProperty(id)
		return
	}
	if StringStartsWith(name, "--") {
		e.style.RemoveCustomProperty(name)
		return
	}
	shorthandId := GetShorthandId(name)
	if shorthandId != ShorthandIdInvalid {
		ids := GetShorthandUnderlyingProperties(shorthandId).Ids()
		for _, pid := range ids {
			e.style.RemoveProperty(pid)
		}
		e.style.RemoveVarShorthand(shorthandId)
	}
}

func (e *Element) RemovePropertyById(id PropertyId) { e.style.RemoveProperty(id) }

// GetPropertyByName returns the cascaded property with variables resolved.
func (e *Element) GetPropertyByName(name string) *Property {
	id := GetPropertyId(name)
	if id != PropertyIdInvalid {
		return e.style.GetProperty(id)
	}
	if StringStartsWith(name, "--") {
		return e.style.GetCustomProperty(name)
	}
	return nil
}

func (e *Element) GetProperty(id PropertyId) *Property { return e.style.GetProperty(id) }

func (e *Element) GetLocalPropertyByName(name string) *Property {
	id := GetPropertyId(name)
	if id != PropertyIdInvalid {
		return e.style.GetLocalProperty(id)
	}
	if StringStartsWith(name, "--") {
		return e.style.GetLocalCustomProperty(name)
	}
	return nil
}

func (e *Element) GetLocalProperty(id PropertyId) *Property { return e.style.GetLocalProperty(id) }

func (e *Element) GetLocalStyleProperties() map[PropertyId]*Property {
	return e.style.GetLocalStyleProperties().GetProperties()
}

func (e *Element) ResolveLength(value NumericValue) float32 {
	if AnyUnit(value.Unit & UnitLENGTH) {
		return e.style.ResolveNumericValue(value, 0)
	}
	return 0
}

func (e *Element) ResolveNumericValue(value NumericValue, baseValue float32) float32 {
	if AnyUnit(value.Unit & UnitNUMERIC) {
		return e.style.ResolveNumericValue(value, baseValue)
	}
	return 0
}

func (e *Element) GetContainingBlock() Vector2f {
	block := Vector2f{}
	if e.offsetParent != nil {
		position := e.GetPosition()
		parentBox := e.offsetParent.GetBox()
		if position == PositionStatic || position == PositionRelative {
			block = parentBox.GetContentSize()
			block.X -= e.scroll.GetScrollbarSize(ScrollbarVertical)
			block.Y -= e.scroll.GetScrollbarSize(ScrollbarHorizontal)
		} else if position == PositionAbsolute || position == PositionFixed {
			block = parentBox.GetSize(BoxAreaPadding)
		}
	} else if ctx := e.GetContext(); ctx != nil {
		block = ctx.GetDimensions().ToFloat()
	}
	return block
}

func (e *Element) GetPosition() int      { return e.computedValues.Position() }
func (e *Element) GetFloat() int         { return e.computedValues.Float() }
func (e *Element) GetDisplay() int       { return e.computedValues.Display() }
func (e *Element) GetLineHeight() float32 { return e.computedValues.LineHeight().Value }

func (e *Element) GetTransformState() *TransformState { return e.transformState }

// Project maps a window-space point onto the element's (transformed) plane.
func (e *Element) Project(point Vector2f) (Vector2f, bool) {
	if e.transformState == nil || e.transformState.GetTransform() == nil {
		return point, true
	}
	inv := e.transformState.GetInverseTransform()
	if inv != nil {
		w0 := inv.MulVector(Vector4f{point.X, point.Y, -10, 1})
		w1 := inv.MulVector(Vector4f{point.X, point.Y, 10, 1})
		l0 := w0.PerspectiveDivide()
		l1 := w1.PerspectiveDivide()
		ray := l1.Sub(l0)
		if MathAbsolute(ray.Z) > 1.0 {
			t := -l0.Z / ray.Z
			p := l0.Add(ray.Mul(t))
			return Vector2f{p.X, p.Y}, true
		}
	}
	return point, false
}

func (e *Element) SetPseudoClass(pseudoClass string, activate bool) {
	if e.style.SetPseudoClass(pseudoClass, activate, false) {
		e.DirtyDefinition(dirtySelfAndSiblings)
		e.OnPseudoClassChange(pseudoClass, activate)
	}
}

func (e *Element) IsPseudoClassSet(pseudoClass string) bool { return e.style.IsPseudoClassSet(pseudoClass) }

func (e *Element) ArePseudoClassesSet(pseudoClasses []string) bool {
	for _, pc := range pseudoClasses {
		if !e.IsPseudoClassSet(pc) {
			return false
		}
	}
	return true
}

func (e *Element) GetActivePseudoClasses() []string { return e.style.GetActivePseudoClasses() }

// OverridePseudoClass is Element::OverridePseudoClass.
func OverridePseudoClass(element *Element, pseudoClass string, activate bool) {
	element.GetStyle().SetPseudoClass(pseudoClass, activate, true)
}

// SetAttribute is Element::SetAttribute<T>.
func (e *Element) SetAttribute(name string, value Variant) {
	v := new(Variant)
	*v = value
	e.attributes[name] = v
	changed := map[string]Variant{}
	changed[name] = value
	e.OnAttributeChange(changed)
}

func (e *Element) SetAttributeString(name string, value string) {
	e.SetAttribute(name, VariantString(value))
}

// GetAttribute returns the attribute, or nil if it does not exist.
func (e *Element) GetAttribute(name string) *Variant {
	if v, ok := e.attributes[name]; ok {
		return v
	}
	return nil
}

// GetAttributeString is GetAttribute<String>(name, default).
func (e *Element) GetAttributeString(name string, def string) string {
	if v, ok := e.attributes[name]; ok {
		if s, ok2 := v.GetStringOk(); ok2 {
			return s
		}
	}
	return def
}

func (e *Element) GetAttributeFloat(name string, def float32) float32 {
	if v, ok := e.attributes[name]; ok {
		if f, ok2 := v.GetFloatOk(); ok2 {
			return f
		}
	}
	return def
}

func (e *Element) GetAttributeInt(name string, def int) int {
	if v, ok := e.attributes[name]; ok {
		if n, ok2 := v.GetIntOk(); ok2 {
			return n
		}
	}
	return def
}

func (e *Element) GetAttributeBool(name string, def bool) bool {
	if v, ok := e.attributes[name]; ok {
		if b, ok2 := v.GetBoolOk(); ok2 {
			return b
		}
	}
	return def
}

func (e *Element) HasAttribute(name string) bool {
	_, ok := e.attributes[name]
	return ok
}

func (e *Element) RemoveAttribute(name string) {
	if _, ok := e.attributes[name]; ok {
		delete(e.attributes, name)
		changed := map[string]Variant{}
		changed[name] = NewVariant()
		e.OnAttributeChange(changed)
	}
}

func (e *Element) SetAttributes(attributes map[string]Variant) {
	for name, value := range attributes {
		v := new(Variant)
		*v = value
		e.attributes[name] = v
	}
	e.OnAttributeChange(attributes)
}

// GetAttributes returns a copy of the attribute map.
func (e *Element) GetAttributes() map[string]Variant {
	out := map[string]Variant{}
	attrs := e.attributes
	for name, v := range attrs {
		out[name] = *v
	}
	return out
}

// GetAttributeNames lists attribute names, sorted.
func (e *Element) GetAttributeNames() []string {
	names := []string{}
	attrs := e.attributes
	for name := range attrs {
		names = append(names, name)
	}
	sortStrings(names)
	return names
}

func (e *Element) GetNumAttributes() int { return len(e.attributes) }

func (e *Element) GetFocusLeafNode() *Element {
	if e.focus == nil {
		return e
	}
	focusElement := e.focus
	for focusElement.focus != nil {
		focusElement = focusElement.focus
	}
	return focusElement
}

func (e *Element) GetContext() *Context {
	if doc := e.GetOwnerDocument(); doc != nil {
		return doc.GetContext()
	}
	return nil
}

func (e *Element) GetRenderManager() *RenderManager {
	if ctx := e.GetContext(); ctx != nil {
		return ctx.GetRenderManager()
	}
	return nil
}

func (e *Element) GetTagName() string { return e.tag }
func (e *Element) GetId() string      { return e.id }
func (e *Element) SetId(id string)    { e.SetAttribute("id", VariantString(id)) }

func (e *Element) GetAbsoluteLeft() float32 { return e.GetAbsoluteOffset(BoxAreaBorder).X }
func (e *Element) GetAbsoluteTop() float32  { return e.GetAbsoluteOffset(BoxAreaBorder).Y }
func (e *Element) GetClientLeft() float32   { return e.mainBox.GetPosition(BoxAreaPadding).X }
func (e *Element) GetClientTop() float32    { return e.mainBox.GetPosition(BoxAreaPadding).Y }

func (e *Element) GetClientWidth() float32 {
	return e.mainBox.GetSize(BoxAreaPadding).X - e.scroll.GetScrollbarSize(ScrollbarVertical)
}

func (e *Element) GetClientHeight() float32 {
	return e.mainBox.GetSize(BoxAreaPadding).Y - e.scroll.GetScrollbarSize(ScrollbarHorizontal)
}

func (e *Element) GetOffsetParent() *Element { return e.offsetParent }
func (e *Element) GetOffsetLeft() float32    { return e.relativeOffsetBase.X + e.relativeOffsetPosition.X }
func (e *Element) GetOffsetTop() float32     { return e.relativeOffsetBase.Y + e.relativeOffsetPosition.Y }
func (e *Element) GetOffsetWidth() float32   { return e.mainBox.GetSize(BoxAreaBorder).X }
func (e *Element) GetOffsetHeight() float32  { return e.mainBox.GetSize(BoxAreaBorder).Y }
func (e *Element) GetScrollLeft() float32    { return e.scrollOffset.X }
func (e *Element) GetScrollTop() float32     { return e.scrollOffset.Y }

func (e *Element) SetScrollLeft(scrollLeft float32) {
	newOffset := MathRound(MathClamp(scrollLeft, 0, e.GetScrollWidth()-e.GetClientWidth()))
	if newOffset != e.scrollOffset.X {
		e.scrollOffset.X = newOffset
		e.scroll.UpdateScrollbar(ScrollbarHorizontal)
		e.DirtyAbsoluteOffset()
		e.DispatchEventId(EventIdScroll, map[string]Variant{})
	}
}

func (e *Element) SetScrollTop(scrollTop float32) {
	newOffset := MathRound(MathClamp(MathRound(scrollTop), 0, e.GetScrollHeight()-e.GetClientHeight()))
	if newOffset != e.scrollOffset.Y {
		e.scrollOffset.Y = newOffset
		e.scroll.UpdateScrollbar(ScrollbarVertical)
		e.DirtyAbsoluteOffset()
		e.DispatchEventId(EventIdScroll, map[string]Variant{})
	}
}

func (e *Element) GetScrollWidth() float32 {
	return MathMax(e.scrollableOverflowRectangle.X, e.GetClientWidth())
}

func (e *Element) GetScrollHeight() float32 {
	return MathMax(e.scrollableOverflowRectangle.Y, e.GetClientHeight())
}

func (e *Element) GetStyle() *ElementStyle                  { return e.style }
func (e *Element) GetOwnerDocument() *ElementDocument       { return e.ownerDocument }
func (e *Element) GetParentNode() *Element                  { return e.parent }
func (e *Element) GetComputedValues() *ComputedValues       { return e.computedValues }
func (e *Element) GetEventDispatcher() *EventDispatcher     { return e.eventDispatcher }
func (e *Element) GetEventDispatcherSummary() string        { return e.eventDispatcher.ToString() }
func (e *Element) GetElementBackgroundBorder() *ElementBackgroundBorder { return e.backgroundBorder }
func (e *Element) GetElementScroll() *ElementScroll         { return e.scroll }
func (e *Element) GetElementEffects() *ElementEffects       { return e.effects }
func (e *Element) GetDataModel() *DataModel                 { return e.dataModel }

// Closest returns the nearest ancestor matching selectors.
func (e *Element) Closest(selectors string) *Element {
	root := NewStyleSheetRootNode()
	leafs := ConstructSelectorNodes(root, selectors)
	if len(leafs) == 0 {
		LogMessage(LogWarning, "Query selector '"+selectors+"' is empty. In element "+e.GetAddress(false, true))
		return nil
	}
	parent := e.GetParentNode()
	for parent != nil {
		for _, node := range leafs {
			if node.IsApplicable(parent, e) {
				return parent
			}
		}
		parent = parent.GetParentNode()
	}
	return nil
}

func (e *Element) GetNextSibling() *Element {
	if e.parent == nil {
		return nil
	}
	siblings := e.parent.children
	for i := 0; i < len(siblings)-(e.parent.numNonDomChildren+1); i++ {
		if siblings[i] == e {
			return siblings[i+1]
		}
	}
	return nil
}

func (e *Element) GetPreviousSibling() *Element {
	if e.parent == nil {
		return nil
	}
	siblings := e.parent.children
	for i := 1; i < len(siblings)-e.parent.numNonDomChildren; i++ {
		if siblings[i] == e {
			return siblings[i-1]
		}
	}
	return nil
}

func (e *Element) GetFirstChild() *Element {
	if e.GetNumChildren(false) > 0 {
		return e.children[0]
	}
	return nil
}

func (e *Element) GetLastChild() *Element {
	if e.GetNumChildren(false) > 0 {
		return e.children[len(e.children)-(e.numNonDomChildren+1)]
	}
	return nil
}

func (e *Element) GetChild(index int) *Element {
	if index < 0 || index >= len(e.children) {
		return nil
	}
	return e.children[index]
}

func (e *Element) GetNumChildren(includeNonDomElements bool) int {
	if includeNonDomElements {
		return len(e.children)
	}
	return len(e.children) - e.numNonDomChildren
}

// GetInnerRML returns the markup of the DOM children (virtual).
func (e *Element) GetInnerRML() string {
	if e.virtuals.GetInnerRML != nil {
		return e.virtuals.GetInnerRML()
	}
	return e.BaseGetInnerRML()
}

func (e *Element) BaseGetInnerRML() string {
	content := ""
	for i := 0; i < e.GetNumChildren(false); i++ {
		content = content + e.children[i].GetRML()
	}
	return content
}

// SetInnerRML replaces the DOM children with parsed rml (virtual).
func (e *Element) SetInnerRML(rml string) {
	if e.virtuals.SetInnerRML != nil {
		e.virtuals.SetInnerRML(rml)
		return
	}
	e.BaseSetInnerRML(rml)
}

func (e *Element) BaseSetInnerRML(rml string) {
	for len(e.children) > e.numNonDomChildren {
		e.RemoveChild(e.children[0])
	}
	if rml != "" {
		FactoryInstanceElementText(e, rml)
	}
}

// Focus gives focus to this element; focusVisible sets :focus-visible.
func (e *Element) Focus(focusVisible bool) bool {
	if e.computedValues.Focus() == FocusNone {
		return false
	}
	ctx := e.GetContext()
	if ctx == nil {
		return false
	}
	if !ctx.OnFocusChange(e, focusVisible) {
		return false
	}
	e.focus = nil
	element := e
	parent := element.GetParentNode()
	for parent != nil {
		parent.focus = element
		element = parent
		parent = element.GetParentNode()
	}
	return true
}

func (e *Element) Blur() {
	if e.parent != nil {
		ctx := e.GetContext()
		if ctx == nil {
			return
		}
		if ctx.GetFocusElement() == e {
			e.parent.Focus(false)
		} else if e.parent.focus == e {
			e.parent.focus = nil
		}
	}
}

func (e *Element) Click() {
	ctx := e.GetContext()
	if ctx == nil {
		return
	}
	ctx.GenerateClickEvent(e)
}

func (e *Element) AddEventListener(event string, listener EventListener, inCapturePhase bool) {
	id := EventSpecificationGetIdOrInsert(event)
	e.eventDispatcher.AttachEvent(id, listener, inCapturePhase)
}

func (e *Element) AddEventListenerId(id EventId, listener EventListener, inCapturePhase bool) {
	e.eventDispatcher.AttachEvent(id, listener, inCapturePhase)
}

func (e *Element) RemoveEventListener(event string, listener EventListener, inCapturePhase bool) {
	id := EventSpecificationGetIdOrInsert(event)
	e.eventDispatcher.DetachEvent(id, listener, inCapturePhase)
}

func (e *Element) RemoveEventListenerId(id EventId, listener EventListener, inCapturePhase bool) {
	e.eventDispatcher.DetachEvent(id, listener, inCapturePhase)
}

// DispatchEvent sends an event by type name; false if it was interrupted.
func (e *Element) DispatchEvent(eventType string, parameters map[string]Variant) bool {
	spec := EventSpecificationGetOrInsert(eventType)
	return EventDispatcherDispatch(e, spec.Id, eventType, parameters, spec.Interruptible, spec.Bubbles, spec.DefaultActionPhase)
}

func (e *Element) DispatchEventFull(eventType string, parameters map[string]Variant, interruptible bool, bubbles bool) bool {
	spec := EventSpecificationGetOrInsert(eventType)
	return EventDispatcherDispatch(e, spec.Id, eventType, parameters, interruptible, bubbles, spec.DefaultActionPhase)
}

func (e *Element) DispatchEventId(id EventId, parameters map[string]Variant) bool {
	spec := EventSpecificationGet(id)
	return EventDispatcherDispatch(e, spec.Id, spec.Type, parameters, spec.Interruptible, spec.Bubbles, spec.DefaultActionPhase)
}

func (e *Element) ScrollIntoViewWith(options ScrollIntoViewOptions) {
	size := e.mainBox.GetSize(BoxAreaBorder)
	behavior := options.Behavior
	scrollParent := e.parent
	for scrollParent != nil {
		computed := scrollParent.GetComputedValues()
		scrollX := computed.OverflowX() != OverflowVisible && computed.OverflowX() != OverflowHidden
		scrollY := computed.OverflowY() != OverflowVisible && computed.OverflowY() != OverflowHidden
		parentScrollSize := Vector2f{scrollParent.GetScrollWidth(), scrollParent.GetScrollHeight()}
		parentClientSize := Vector2f{scrollParent.GetClientWidth(), scrollParent.GetClientHeight()}
		if (scrollX && parentScrollSize.X > parentClientSize.X) || (scrollY && parentScrollSize.Y > parentClientSize.Y) {
			relative := scrollParent.GetAbsoluteOffset(BoxAreaBorder).Sub(e.GetAbsoluteOffset(BoxAreaBorder))
			oldScroll := Vector2f{scrollParent.GetScrollLeft(), scrollParent.GetScrollTop()}
			clientOffset := Vector2f{scrollParent.GetClientLeft(), scrollParent.GetClientTop()}
			start := clientOffset.Sub(relative)
			end := start.Add(size).Sub(parentClientSize)
			delta := Vector2f{}
			if scrollX {
				delta.X = getScrollOffsetDelta(options.Horizontal, start.X, end.X)
			}
			if scrollY {
				delta.Y = getScrollOffsetDelta(options.Vertical, start.Y, end.Y)
			}
			scrollParent.ScrollTo(oldScroll.Add(delta), behavior)
			behavior = ScrollBehaviorInstant
		}
		if (scrollX || scrollY) && options.Parentage == ScrollParentageClosest {
			break
		}
		scrollParent = scrollParent.GetParentNode()
	}
}

func (e *Element) ScrollIntoView(alignWithTop bool) {
	options := DefaultScrollIntoViewOptions()
	if alignWithTop {
		options.Vertical = ScrollAlignmentStart
	} else {
		options.Vertical = ScrollAlignmentEnd
	}
	options.Horizontal = ScrollAlignmentNearest
	e.ScrollIntoViewWith(options)
}

func (e *Element) ScrollTo(offset Vector2f, behavior ScrollBehavior) {
	if behavior != ScrollBehaviorInstant {
		if ctx := e.GetContext(); ctx != nil {
			ctx.PerformSmoothscrollOnTarget(e, offset.Sub(e.scrollOffset), behavior)
			return
		}
	}
	e.SetScrollLeft(offset.X)
	e.SetScrollTop(offset.Y)
}

// AppendChild adds child at the end of the DOM children (or as a non-DOM
// child when domElement is false).
func (e *Element) AppendChild(child *Element, domElement bool) *Element {
	if domElement {
		index := len(e.children) - e.numNonDomChildren
		e.children = insertElement(e.children, index, child)
	} else {
		e.children = append(e.children, child)
		e.numNonDomChildren++
	}
	child.setParent(e)
	ancestor := child
	for i := 0; i <= childNotifyLevels && ancestor != nil; i++ {
		ancestor.OnChildAdd(child)
		ancestor = ancestor.GetParentNode()
	}
	e.dirtyStackingContext()
	e.DirtyDefinition(dirtySelf)
	if domElement {
		e.DirtyLayout()
	}
	return child
}

func insertElement(list []*Element, index int, el *Element) []*Element {
	list = append(list, nil)
	for i := len(list) - 1; i > index; i-- {
		list[i] = list[i-1]
	}
	list[index] = el
	return list
}

func removeElementAt(list []*Element, index int) []*Element {
	out := make([]*Element, 0, len(list)-1)
	for i := 0; i < len(list); i++ {
		if i != index {
			out = append(out, list[i])
		}
	}
	return out
}

func (e *Element) InsertBefore(child *Element, adjacent *Element) *Element {
	index := -1
	if adjacent != nil {
		for i := 0; i < len(e.children); i++ {
			if e.children[i] == adjacent {
				index = i
				break
			}
		}
	}
	if index < 0 {
		return e.AppendChild(child, true)
	}
	if index >= e.GetNumChildren(false) {
		e.numNonDomChildren++
	} else {
		e.DirtyLayout()
	}
	e.children = insertElement(e.children, index, child)
	child.setParent(e)
	ancestor := child
	for i := 0; i <= childNotifyLevels && ancestor != nil; i++ {
		ancestor.OnChildAdd(child)
		ancestor = ancestor.GetParentNode()
	}
	e.dirtyStackingContext()
	e.DirtyDefinition(dirtySelf)
	return child
}

// ReplaceChild inserts inserted in place of replaced, returning replaced.
func (e *Element) ReplaceChild(inserted *Element, replaced *Element) *Element {
	index := -1
	for i := 0; i < len(e.children); i++ {
		if e.children[i] == replaced {
			index = i
			break
		}
	}
	if index < 0 {
		e.AppendChild(inserted, true)
		return nil
	}
	e.children = insertElement(e.children, index, inserted)
	inserted.setParent(e)
	result := e.RemoveChild(replaced)
	ancestor := inserted
	for i := 0; i <= childNotifyLevels && ancestor != nil; i++ {
		ancestor.OnChildAdd(inserted)
		ancestor = ancestor.GetParentNode()
	}
	return result
}

// RemoveChild detaches child and returns it (nil if not a child).
func (e *Element) RemoveChild(child *Element) *Element {
	for index := 0; index < len(e.children); index++ {
		if e.children[index] != child {
			continue
		}
		ancestor := child
		for i := 0; i <= childNotifyLevels && ancestor != nil; i++ {
			ancestor.OnChildRemove(child)
			ancestor = ancestor.GetParentNode()
		}
		if index >= len(e.children)-e.numNonDomChildren {
			e.numNonDomChildren--
		}
		e.children = removeElementAt(e.children, index)
		if child == e.focus {
			e.focus = nil
			if ctx := e.GetContext(); ctx != nil {
				focusElement := ctx.GetFocusElement()
				for focusElement != nil {
					if focusElement == child {
						e.Focus(false)
						break
					}
					focusElement = focusElement.GetParentNode()
				}
			}
		}
		child.setParent(nil)
		e.DirtyLayout()
		e.dirtyStackingContext()
		e.DirtyDefinition(dirtySelf)
		return child
	}
	return nil
}

func (e *Element) HasChildNodes() bool { return len(e.children) > e.numNonDomChildren }

func (e *Element) GetElementById(id string) *Element {
	if id == "#self" {
		return e
	} else if id == "#document" {
		if doc := e.GetOwnerDocument(); doc != nil {
			return doc.GetElement()
		}
		return nil
	} else if id == "#parent" {
		return e.parent
	}
	var searchRoot *Element
	if doc := e.GetOwnerDocument(); doc != nil {
		searchRoot = doc.GetElement()
	} else {
		searchRoot = e
	}
	return ElementUtilitiesGetElementById(searchRoot, id)
}

func (e *Element) GetElementsByTagName(tag string) []*Element {
	return ElementUtilitiesGetElementsByTagName(e, tag)
}

func (e *Element) GetElementsByClassName(className string) []*Element {
	return ElementUtilitiesGetElementsByClassName(e, className)
}

func querySelectorMatchRecursive(nodes []*StyleSheetNode, element *Element, scope *Element) *Element {
	n := element.GetNumChildren(false)
	for i := 0; i < n; i++ {
		child := element.GetChild(i)
		if child.GetTagName() == "#text" {
			continue
		}
		for _, node := range nodes {
			if node.IsApplicable(child, scope) {
				return child
			}
		}
		if match := querySelectorMatchRecursive(nodes, child, scope); match != nil {
			return match
		}
	}
	return nil
}

func querySelectorAllMatchRecursive(matches []*Element, nodes []*StyleSheetNode, element *Element, scope *Element) []*Element {
	n := element.GetNumChildren(false)
	for i := 0; i < n; i++ {
		child := element.GetChild(i)
		if child.GetTagName() == "#text" {
			continue
		}
		for _, node := range nodes {
			if node.IsApplicable(child, scope) {
				matches = append(matches, child)
				break
			}
		}
		matches = querySelectorAllMatchRecursive(matches, nodes, child, scope)
	}
	return matches
}

func (e *Element) QuerySelector(selectors string) *Element {
	root := NewStyleSheetRootNode()
	leafs := ConstructSelectorNodes(root, selectors)
	if len(leafs) == 0 {
		LogMessage(LogWarning, "Query selector '"+selectors+"' is empty. In element "+e.GetAddress(false, true))
		return nil
	}
	return querySelectorMatchRecursive(leafs, e, e)
}

func (e *Element) QuerySelectorAll(selectors string) []*Element {
	root := NewStyleSheetRootNode()
	leafs := ConstructSelectorNodes(root, selectors)
	if len(leafs) == 0 {
		LogMessage(LogWarning, "Query selector '"+selectors+"' is empty. In element "+e.GetAddress(false, true))
		return []*Element{}
	}
	return querySelectorAllMatchRecursive([]*Element{}, leafs, e, e)
}

func (e *Element) Matches(selectors string) bool {
	root := NewStyleSheetRootNode()
	leafs := ConstructSelectorNodes(root, selectors)
	if len(leafs) == 0 {
		LogMessage(LogWarning, "Query selector '"+selectors+"' is empty. In element "+e.GetAddress(false, true))
		return false
	}
	for _, node := range leafs {
		if node.IsApplicable(e, e) {
			return true
		}
	}
	return false
}

func (e *Element) Contains(element *Element) bool {
	for element != nil {
		if element == e {
			return true
		}
		element = element.GetParentNode()
	}
	return false
}

// SetInstancer records the first instancer that created this element.
func (e *Element) SetInstancer(instancer ElementInstancer) {
	if e.instancer == nil {
		e.instancer = instancer
	}
}

func (e *Element) GetInstancer() ElementInstancer { return e.instancer }

func (e *Element) ForceLocalStackingContext() {
	e.localStackingContextForced = true
	e.localStackingContext = true
	e.dirtyStackingContext()
}

// ---- virtual dispatch ----

func (e *Element) OnUpdate() {
	if e.virtuals.OnUpdate != nil {
		e.virtuals.OnUpdate()
	}
}

func (e *Element) OnRender() {
	if e.virtuals.OnRender != nil {
		e.virtuals.OnRender()
	}
}

func (e *Element) OnResize() {
	if e.virtuals.OnResize != nil {
		e.virtuals.OnResize()
	}
}

func (e *Element) OnLayout() {
	if e.virtuals.OnLayout != nil {
		e.virtuals.OnLayout()
	}
}

func (e *Element) OnDpRatioChange() {
	if e.virtuals.OnDpRatioChange != nil {
		e.virtuals.OnDpRatioChange()
	}
}

func (e *Element) OnStyleSheetChange() {
	if e.virtuals.OnStyleSheetChange != nil {
		e.virtuals.OnStyleSheetChange()
	}
}

func (e *Element) OnAttributeChange(changed map[string]Variant) {
	if e.virtuals.OnAttributeChange != nil {
		e.virtuals.OnAttributeChange(changed)
		return
	}
	e.BaseOnAttributeChange(changed)
}

func (e *Element) OnPropertyChange(changed PropertyIdSet) {
	if e.virtuals.OnPropertyChange != nil {
		e.virtuals.OnPropertyChange(changed)
		return
	}
	e.BaseOnPropertyChange(changed)
}

func (e *Element) OnPseudoClassChange(pseudoClass string, activate bool) {
	if e.virtuals.OnPseudoClassChange != nil {
		e.virtuals.OnPseudoClassChange(pseudoClass, activate)
	}
}

func (e *Element) OnChildAdd(child *Element) {
	if e.virtuals.OnChildAdd != nil {
		e.virtuals.OnChildAdd(child)
	}
}

func (e *Element) OnChildRemove(child *Element) {
	if e.virtuals.OnChildRemove != nil {
		e.virtuals.OnChildRemove(child)
	}
}

func (e *Element) DirtyLayout() {
	if e.virtuals.DirtyLayout != nil {
		e.virtuals.DirtyLayout()
		return
	}
	if doc := e.GetOwnerDocument(); doc != nil {
		doc.GetElement().DirtyLayout()
	}
}

func (e *Element) IsLayoutDirty() bool {
	if e.virtuals.IsLayoutDirty != nil {
		return e.virtuals.IsLayoutDirty()
	}
	if doc := e.GetOwnerDocument(); doc != nil {
		return doc.GetElement().IsLayoutDirty()
	}
	return false
}

func (e *Element) GetRML() string {
	if e.virtuals.GetRML != nil {
		return e.virtuals.GetRML()
	}
	return e.BaseGetRML()
}

func (e *Element) ProcessDefaultAction(event *Event) {
	if e.virtuals.ProcessDefaultAction != nil {
		e.virtuals.ProcessDefaultAction(event)
		return
	}
	e.BaseProcessDefaultAction(event)
}

// BaseOnAttributeChange is Element::OnAttributeChange.
func (e *Element) BaseOnAttributeChange(changed map[string]Variant) {
	names := []string{}
	for name := range changed {
		names = append(names, name)
	}
	sortStrings(names)
	for _, attribute := range names {
		value := changed[attribute]
		display := e.computedValues.Display()
		if attribute == "id" {
			e.id = value.GetString()
		} else if attribute == "class" {
			e.style.SetClassNames(value.GetString())
		} else if ((attribute == "colspan" || attribute == "rowspan") && display == DisplayTableCell) ||
			(attribute == "span" && (display == DisplayTableColumn || display == DisplayTableColumnGroup)) {
			e.DirtyLayout()
		} else if len(attribute) > 2 && attribute[0] == 'o' && attribute[1] == 'n' {
			inCapture := StringEndsWith(attribute, "capture")
			nameLength := len(attribute) - 2
			if inCapture {
				nameLength -= 7
			}
			eventId := EventSpecificationGetIdOrInsert(attribute[2 : 2+nameLength])
			removeIfExists := func() {
				if listener, ok := e.attributeEventListeners[eventId]; ok {
					e.eventDispatcher.DetachEvent(eventId, listener, inCapture)
					delete(e.attributeEventListeners, eventId)
				}
			}
			if value.GetType() == VariantSTRING {
				removeIfExists()
				listener := FactoryInstanceEventListener(value.GetString(), e)
				if listener != nil {
					e.attributeEventListeners[eventId] = listener
					e.eventDispatcher.AttachEvent(eventId, listener, inCapture)
				}
			} else if value.GetType() == VariantNONE {
				removeIfExists()
			}
		} else if attribute == "style" {
			if value.GetType() == VariantSTRING {
				properties := NewPropertyDictionary()
				parser := NewStyleSheetParser()
				parser.ParseProperties(properties, value.GetString())
				props := properties.GetProperties()
				ids := sortedPropertyIds(props)
				for _, id := range ids {
					e.style.SetProperty(id, *props[id])
				}
				customs := properties.GetCustomProperties()
				for n, p := range customs {
					e.style.SetCustomProperty(n, *p)
				}
				shorthands := properties.GetVarShorthands()
				for id, p := range shorthands {
					e.style.SetVarShorthand(id, *p)
				}
			} else if value.GetType() != VariantNONE {
				LogMessage(LogWarning, "Invalid 'style' attribute, string type required. In element: "+e.GetAddress(false, true))
			}
		} else if attribute == "lang" {
			if value.GetType() == VariantSTRING {
				e.style.SetProperty(PropertyIdRmlUi_Language, PropertyString(value.GetString(), UnitSTRING))
			} else if value.GetType() != VariantNONE {
				LogMessage(LogWarning, "Invalid 'lang' attribute, string type required. In element: "+e.GetAddress(false, true))
			}
		} else if attribute == "dir" {
			if value.GetType() == VariantSTRING {
				dir := value.GetString()
				if dir == "auto" {
					e.style.SetProperty(PropertyIdRmlUi_Direction, PropertyKeyword(DirectionAuto))
				} else if dir == "ltr" {
					e.style.SetProperty(PropertyIdRmlUi_Direction, PropertyKeyword(DirectionLtr))
				} else if dir == "rtl" {
					e.style.SetProperty(PropertyIdRmlUi_Direction, PropertyKeyword(DirectionRtl))
				} else {
					LogMessage(LogWarning, "Invalid 'dir' attribute '"+dir+"', value must be 'auto', 'ltr', or 'rtl'. In element: "+e.GetAddress(false, true))
				}
			} else if value.GetType() != VariantNONE {
				LogMessage(LogWarning, "Invalid 'dir' attribute, string type required. In element: "+e.GetAddress(false, true))
			}
		}
	}
	e.DirtyDefinition(dirtySelfAndSiblings)
}

// BaseOnPropertyChange is Element::OnPropertyChange.
func (e *Element) BaseOnPropertyChange(changed PropertyIdSet) {
	trblChanged := changed.Contains(PropertyIdTop) || changed.Contains(PropertyIdRight) || changed.Contains(PropertyIdBottom) || changed.Contains(PropertyIdLeft)
	if !e.IsLayoutDirty() {
		forcing := changed.Intersection(GetRegisteredPropertiesForcingLayout())
		if !forcing.Empty() {
			e.DirtyLayout()
		} else if trblChanged {
			c := e.GetComputedValues()
			absolutely := c.Position() == PositionAbsolute || c.Position() == PositionFixed
			sizedWidth := c.Width().Type == LengthPercentageAutoAuto && c.Left().Type != LengthPercentageAutoAuto && c.Right().Type != LengthPercentageAutoAuto
			sizedHeight := c.Height().Type == LengthPercentageAutoAuto && c.Top().Type != LengthPercentageAutoAuto && c.Bottom().Type != LengthPercentageAutoAuto
			if absolutely && (sizedWidth || sizedHeight) {
				e.DirtyLayout()
			}
		}
	}
	if trblChanged {
		e.updateOffset()
		e.DirtyAbsoluteOffset()
	}
	if changed.Contains(PropertyIdVisibility) || changed.Contains(PropertyIdDisplay) {
		newVisibility := e.computedValues.Display() != DisplayNone && e.computedValues.Visibility() == VisibilityVisible
		if e.visible != newVisibility {
			e.visible = newVisibility
			if e.parent != nil {
				e.parent.dirtyStackingContext()
			}
			if !e.visible {
				e.Blur()
			}
		}
	}
	borderRadiusChanged := changed.Contains(PropertyIdBorderTopLeftRadius) || changed.Contains(PropertyIdBorderTopRightRadius) ||
		changed.Contains(PropertyIdBorderBottomRightRadius) || changed.Contains(PropertyIdBorderBottomLeftRadius)
	filterOrMaskChanged := changed.Contains(PropertyIdFilter) || changed.Contains(PropertyIdBackdropFilter) || changed.Contains(PropertyIdMaskImage)
	perspectiveChanged := changed.Contains(PropertyIdPerspective) || changed.Contains(PropertyIdPerspectiveOriginX) || changed.Contains(PropertyIdPerspectiveOriginY)
	transformChanged := changed.Contains(PropertyIdTransform) || changed.Contains(PropertyIdTransformOriginX) ||
		changed.Contains(PropertyIdTransformOriginY) || changed.Contains(PropertyIdTransformOriginZ)

	if changed.Contains(PropertyIdZIndex) || filterOrMaskChanged || perspectiveChanged || transformChanged {
		zi := e.computedValues.ZIndex()
		var newZ float32 = 0
		if zi.Type != NumberAutoAuto {
			newZ = zi.Value
		}
		c := e.computedValues
		enableLocal := zi.Type != NumberAutoAuto || e.localStackingContextForced || c.HasFilter() || c.HasBackdropFilter() ||
			c.HasMaskImage() || c.HasLocalTransform() || c.HasLocalPerspective()
		if e.zIndex != newZ || e.localStackingContext != enableLocal {
			e.zIndex = newZ
			if e.localStackingContext != enableLocal {
				e.localStackingContext = enableLocal
				e.stackingContext = nil
				e.stackingContextDirty = e.localStackingContext
			}
			if e.parent != nil {
				e.parent.dirtyStackingContext()
			}
		}
	}
	if borderRadiusChanged || changed.Contains(PropertyIdBackgroundColor) || changed.Contains(PropertyIdOpacity) ||
		changed.Contains(PropertyIdImageColor) || changed.Contains(PropertyIdBoxShadow) {
		e.backgroundBorder.DirtyBackground()
	}
	if borderRadiusChanged || changed.Contains(PropertyIdBorderTopWidth) || changed.Contains(PropertyIdBorderRightWidth) ||
		changed.Contains(PropertyIdBorderBottomWidth) || changed.Contains(PropertyIdBorderLeftWidth) ||
		changed.Contains(PropertyIdBorderTopColor) || changed.Contains(PropertyIdBorderRightColor) ||
		changed.Contains(PropertyIdBorderBottomColor) || changed.Contains(PropertyIdBorderLeftColor) || changed.Contains(PropertyIdOpacity) {
		e.backgroundBorder.DirtyBorder()
	}
	if borderRadiusChanged || filterOrMaskChanged || changed.Contains(PropertyIdDecorator) {
		e.effects.DirtyEffects()
	}
	fontChanged := changed.Contains(PropertyIdFontFamily) || changed.Contains(PropertyIdFontStyle) || changed.Contains(PropertyIdFontWeight) ||
		changed.Contains(PropertyIdFontSize) || changed.Contains(PropertyIdFontKerning) || changed.Contains(PropertyIdLetterSpacing)
	if borderRadiusChanged || fontChanged || changed.Contains(PropertyIdOpacity) || changed.Contains(PropertyIdColor) || changed.Contains(PropertyIdImageColor) {
		e.effects.DirtyEffectsData()
	}
	if perspectiveChanged {
		e.dirtyTransformState(true, false)
	}
	if transformChanged {
		e.dirtyTransformState(false, true)
	}
	if changed.Contains(PropertyIdAnimation) {
		e.dirtyAnimation = true
	}
	if changed.Contains(PropertyIdTransition) {
		e.dirtyTransition = true
	}
}

// GetClosestScrollableContainer walks up to the element that should
// receive a scroll of scrollDelta.
func (e *Element) GetClosestScrollableContainer(scrollDelta Vector2f) *Element {
	ox := e.computedValues.OverflowX()
	oy := e.computedValues.OverflowY()
	scrollableX := scrollDelta.X != 0 && (ox == OverflowAuto || ox == OverflowScroll) && e.GetScrollWidth() > e.GetClientWidth()
	scrollableY := scrollDelta.Y != 0 && (oy == OverflowAuto || oy == OverflowScroll) && e.GetScrollHeight() > e.GetClientHeight()
	if scrollableX || scrollableY || e.computedValues.OverscrollBehavior() == OverscrollBehaviorContain {
		return e
	} else if e.parent != nil {
		return e.parent.GetClosestScrollableContainer(scrollDelta)
	}
	return nil
}

// BaseProcessDefaultAction is Element::ProcessDefaultAction.
func (e *Element) BaseProcessDefaultAction(event *Event) {
	if event.GetId() == EventIdMousedown {
		mousePos := Vector2f{event.GetParameterFloat("mouse_x", 0), event.GetParameterFloat("mouse_y", 0)}
		if e.IsPointWithinElement(mousePos) && event.GetParameterInt("button", 0) == 0 {
			e.SetPseudoClass("active", true)
		}
	}
	if event.GetPhase() == EventPhaseTarget {
		switch event.GetId() {
		case EventIdMouseover:
			e.SetPseudoClass("hover", true)
		case EventIdMouseout:
			e.SetPseudoClass("hover", false)
		case EventIdFocus:
			e.SetPseudoClass("focus", true)
			if event.GetParameterBool("focus_visible", false) {
				e.SetPseudoClass("focus-visible", true)
			}
		case EventIdBlur:
			e.SetPseudoClass("focus", false)
			e.SetPseudoClass("focus-visible", false)
		}
	}
}

// BaseGetRML is Element::GetRML: this element's markup and content.
func (e *Element) BaseGetRML() string {
	content := "<" + e.tag
	names := e.GetAttributeNames()
	for _, name := range names {
		if name == "style" {
			continue
		}
		if value, ok := e.attributes[name].GetStringOk(); ok {
			content = content + " " + name + "=\"" + value + "\""
		}
	}
	local := e.style.GetLocalStyleProperties()
	if !local.Empty() {
		content = content + " style=\""
	}
	props := local.GetProperties()
	ids := sortedPropertyIds(props)
	for _, id := range ids {
		p := props[id]
		if p.Unit == UnitSHORTHAND_PLACEHOLDER {
			continue
		}
		content = content + GetPropertyName(id) + ": " + StringEncodeRml(p.ToString()) + "; "
	}
	customs := local.GetCustomProperties()
	customNames := []string{}
	for name := range customs {
		customNames = append(customNames, name)
	}
	sortStrings(customNames)
	for _, name := range customNames {
		content = content + name + ": " + StringEncodeRml(customs[name].ToString()) + "; "
	}
	shorthands := local.GetVarShorthands()
	for id := 1; id < ShorthandIdMaxNumIds; id++ {
		p, ok := shorthands[id]
		if !ok {
			continue
		}
		content = content + GetShorthandName(id) + ": " + StringEncodeRml(p.ToString()) + "; "
	}
	if !local.Empty() {
		content = content[:len(content)-1] + "\""
	}
	if e.HasChildNodes() {
		content = content + ">" + e.GetInnerRML() + "</" + e.tag + ">"
	} else {
		content = content + " />"
	}
	return content
}

// SetOwnerDocument propagates the owner document down the tree.
func (e *Element) SetOwnerDocument(document *ElementDocument, forceSet bool) {
	if e.ownerDocument == document {
		return
	}
	if e.ownerDocument != nil && document == nil {
		if ctx := e.ownerDocument.GetContext(); ctx != nil {
			ctx.OnElementDetach(e)
		}
	}
	isSelf := e.ownerDocument != nil && e.ownerDocument.GetElement() == e
	if !isSelf || forceSet {
		e.ownerDocument = document
		children := e.children
		for _, child := range children {
			child.SetOwnerDocument(document, false)
		}
	}
}

func (e *Element) setDataModel(newModel *DataModel) {
	if e.dataModel == newModel {
		return
	}
	if e.dataModel != nil && newModel != nil && e.dataModel != newModel {
		return
	}
	if e.dataModel != nil {
		e.dataModel.OnElementRemove(e)
	}
	e.dataModel = newModel
	if e.dataModel != nil {
		ElementUtilitiesApplyDataViewsControllers(e)
	}
	children := e.children
	for _, child := range children {
		child.setDataModel(newModel)
	}
}

func (e *Element) setParent(parent *Element) {
	e.parent = parent
	if parent != nil {
		e.DirtyDefinition(dirtySelf)
		e.style.DirtyInheritedProperties()
	}
	if e.transformState != nil || (parent != nil && parent.transformState != nil) {
		e.dirtyTransformState(true, true)
	}
	var doc *ElementDocument
	if parent != nil {
		doc = parent.GetOwnerDocument()
	}
	e.SetOwnerDocument(doc, false)
	if parent == nil {
		if e.dataModel != nil {
			e.setDataModel(nil)
		}
	} else {
		if attr, ok := e.attributes["data-model"]; !ok {
			e.setDataModel(parent.dataModel)
		} else if ctx := e.GetContext(); ctx != nil {
			name := attr.GetString()
			if model := ctx.GetDataModelPtr(name); model != nil {
				model.AttachModelRootElement(e)
				e.setDataModel(model)
			} else {
				LogMessage(LogError, "Could not locate data model '"+name+"' in element "+e.GetAddress(false, true)+".")
			}
		}
	}
}

func (e *Element) DirtyAbsoluteOffset() {
	if !e.absoluteOffsetDirty {
		e.dirtyAbsoluteOffsetRecursive()
	}
}

func (e *Element) dirtyAbsoluteOffsetRecursive() {
	if !e.absoluteOffsetDirty {
		e.absoluteOffsetDirty = true
		if e.transformState != nil {
			e.dirtyTransformState(true, true)
		}
	}
	for i := 0; i < len(e.children); i++ {
		e.children[i].dirtyAbsoluteOffsetRecursive()
	}
}

func (e *Element) updateOffset() {
	c := e.computedValues
	position := c.Position()
	if position == PositionAbsolute || position == PositionFixed {
		if e.offsetParent != nil {
			parentBox := e.offsetParent.GetBox()
			block := parentBox.GetSize(BoxAreaPadding)
			if c.Left().Type != LengthPercentageAutoAuto {
				e.relativeOffsetBase.X = parentBox.GetEdge(BoxAreaBorder, BoxEdgeLeft) + (ResolveValueAuto(c.Left(), block.X) + e.mainBox.GetEdge(BoxAreaMargin, BoxEdgeLeft))
			} else if c.Right().Type != LengthPercentageAutoAuto {
				e.relativeOffsetBase.X = block.X + parentBox.GetEdge(BoxAreaBorder, BoxEdgeLeft) -
					(ResolveValueAuto(c.Right(), block.X) + e.mainBox.GetSize(BoxAreaBorder).X + e.mainBox.GetEdge(BoxAreaMargin, BoxEdgeRight))
			}
			if c.Top().Type != LengthPercentageAutoAuto {
				e.relativeOffsetBase.Y = parentBox.GetEdge(BoxAreaBorder, BoxEdgeTop) + (ResolveValueAuto(c.Top(), block.Y) + e.mainBox.GetEdge(BoxAreaMargin, BoxEdgeTop))
			} else if c.Bottom().Type != LengthPercentageAutoAuto {
				e.relativeOffsetBase.Y = block.Y + parentBox.GetEdge(BoxAreaBorder, BoxEdgeTop) -
					(ResolveValueAuto(c.Bottom(), block.Y) + e.mainBox.GetSize(BoxAreaBorder).Y + e.mainBox.GetEdge(BoxAreaMargin, BoxEdgeBottom))
			}
		}
	} else if position == PositionRelative {
		if e.offsetParent != nil {
			block := e.offsetParent.GetBox().GetContentSize()
			if c.Left().Type != LengthPercentageAutoAuto {
				e.relativeOffsetPosition.X = ResolveValueAuto(c.Left(), block.X)
			} else if c.Right().Type != LengthPercentageAutoAuto {
				e.relativeOffsetPosition.X = -1 * ResolveValueAuto(c.Right(), block.X)
			} else {
				e.relativeOffsetPosition.X = 0
			}
			if c.Top().Type != LengthPercentageAutoAuto {
				e.relativeOffsetPosition.Y = ResolveValueAuto(c.Top(), block.Y)
			} else if c.Bottom().Type != LengthPercentageAutoAuto {
				e.relativeOffsetPosition.Y = -1 * ResolveValueAuto(c.Bottom(), block.Y)
			} else {
				e.relativeOffsetPosition.Y = 0
			}
		}
	} else {
		e.relativeOffsetPosition = Vector2f{}
	}
}

// SetBaseline is used by the layout engine.
func (e *Element) SetBaseline(baseline float32) { e.baseline = baseline }

func stackingChildLess(a stackingContextChild, b stackingContextChild) bool {
	if a.order == b.order {
		return a.element.GetZIndex() < b.element.GetZIndex()
	}
	return a.order < b.order
}

// stableSortStacking is std::stable_sort over [begin, len).
func stableSortStacking(list []stackingContextChild, begin int) {
	for i := begin + 1; i < len(list); i++ {
		j := i
		for j > begin && stackingChildLess(list[j], list[j-1]) {
			list[j], list[j-1] = list[j-1], list[j]
			j--
		}
	}
}

func stackingContextMakeAtomicRange(list []stackingContextChild, begin int, parentOrder int) {
	stableSortStacking(list, begin)
	for i := begin; i < len(list); i++ {
		order := list[i].order
		if order != renderOrderStackNegative && order != renderOrderPositioned && order != renderOrderStackPositive {
			list[i].order = parentOrder
		}
	}
}

func (e *Element) buildLocalStackingContext() {
	e.stackingContextDirty = false
	children := e.addChildrenToStackingContext([]stackingContextChild{})
	stableSortStacking(children, 0)
	e.stackingContext = nil
	for _, c := range children {
		e.stackingContext = append(e.stackingContext, c.element)
	}
	if e.transformState != nil {
		stacking := e.stackingContext
		for _, child := range stacking {
			child.dirtyTransformState(false, true)
		}
	}
}

func (e *Element) addChildrenToStackingContext(list []stackingContextChild) []stackingContextChild {
	isFlexContainer := e.GetDisplay() == DisplayFlex
	n := len(e.children)
	for i := 0; i < n; i++ {
		isNonDom := i >= n-e.numNonDomChildren
		list = e.children[i].addToStackingContext(list, isFlexContainer, isNonDom)
	}
	return list
}

func (e *Element) addToStackingContext(list []stackingContextChild, isFlexItem bool, isNonDomElement bool) []stackingContextChild {
	if !e.IsVisible(false) {
		return list
	}
	display := e.GetDisplay()
	order := renderOrderInline
	includeChildren := true
	atomic := false
	if e.localStackingContext {
		if e.zIndex > 0 {
			order = renderOrderStackPositive
		} else if e.zIndex < 0 {
			order = renderOrderStackNegative
		} else {
			order = renderOrderPositioned
		}
		includeChildren = false
	} else if display == DisplayTableRow || display == DisplayTableRowGroup || display == DisplayTableColumn || display == DisplayTableColumnGroup {
		switch display {
		case DisplayTableRow:
			order = renderOrderTableRow
		case DisplayTableRowGroup:
			order = renderOrderTableRowGroup
		case DisplayTableColumn:
			order = renderOrderTableColumn
		case DisplayTableColumnGroup:
			order = renderOrderTableColumnGroup
		}
	} else if e.GetPosition() != PositionStatic {
		order = renderOrderPositioned
		atomic = true
	} else if e.GetFloat() != FloatNone {
		order = renderOrderFloating
		atomic = true
	} else {
		switch display {
		case DisplayBlock, DisplayFlowRoot, DisplayTable, DisplayFlex:
			order = renderOrderBlock
			atomic = display == DisplayTable || isFlexItem
		case DisplayInline, DisplayInlineBlock, DisplayInlineFlex, DisplayInlineTable:
			order = renderOrderInline
			atomic = display != DisplayInline || isFlexItem
		case DisplayTableCell:
			order = renderOrderTableCell
			atomic = true
		}
	}
	if isNonDomElement {
		atomic = true
	}
	list = append(list, stackingContextChild{e, order})
	if includeChildren && len(e.children) > 0 {
		begin := len(list)
		list = e.addChildrenToStackingContext(list)
		if atomic {
			stackingContextMakeAtomicRange(list, begin, order)
		}
	}
	return list
}

func (e *Element) dirtyStackingContext() {
	if p := e.closestStackingContextContainer(); p != nil {
		p.stackingContextDirty = true
	}
}

func (e *Element) closestStackingContextContainer() *Element {
	p := e
	for p != nil && !p.localStackingContext {
		p = p.GetParentNode()
	}
	return p
}

// GetStackingContext returns the element's sorted stacking context.
func (e *Element) GetStackingContext() []*Element { return e.stackingContext }

const (
	dirtySelf = iota
	dirtySelfAndSiblings
)

// DirtyDefinition dirties the style definition (and siblings for sibling
// combinators).
func (e *Element) DirtyDefinition(nodes int) {
	e.dirtyDefinition = true
	if nodes == dirtySelfAndSiblings && e.parent != nil {
		e.parent.dirtyChildDefinitions = true
	}
}

func (e *Element) updateDefinition() {
	if e.dirtyDefinition {
		e.dirtyDefinition = false
		e.dirtyChildDefinitions = true
		e.style.UpdateDefinition()
	}
	if e.dirtyChildDefinitions {
		e.dirtyChildDefinitions = false
		children := e.children
		for _, child := range children {
			child.dirtyDefinition = true
		}
	}
}

func (e *Element) dirtyTransformState(perspectiveDirty bool, transformDirty bool) {
	e.dirtyPerspective = e.dirtyPerspective || perspectiveDirty
	e.dirtyTransform = e.dirtyTransform || transformDirty
}

func (e *Element) updateTransformState() {
	if !e.dirtyPerspective && !e.dirtyTransform {
		return
	}
	c := e.computedValues
	pos := e.GetAbsoluteOffset(BoxAreaBorder)
	size := e.mainBox.GetSize(BoxAreaBorder)
	changed := false
	if e.dirtyPerspective {
		hadPerspective := e.transformState != nil && e.transformState.GetLocalPerspective() != nil
		distance := c.Perspective()
		vanish := Vector2f{pos.X + size.X*0.5, pos.Y + size.Y*0.5}
		havePerspective := false
		if distance > 0 {
			havePerspective = true
			if c.PerspectiveOriginX().Type == LengthPercentagePercentage {
				vanish.X = pos.X + c.PerspectiveOriginX().Value*0.01*size.X
			} else {
				vanish.X = pos.X + c.PerspectiveOriginX().Value
			}
			if c.PerspectiveOriginY().Type == LengthPercentagePercentage {
				vanish.Y = pos.Y + c.PerspectiveOriginY().Value*0.01*size.Y
			} else {
				vanish.Y = pos.Y + c.PerspectiveOriginY().Value
			}
		}
		if havePerspective {
			perspective := Matrix4FromRows(
				Vector4f{1, 0, -vanish.X / distance, 0},
				Vector4f{0, 1, -vanish.Y / distance, 0},
				Vector4f{0, 0, 1, 0},
				Vector4f{0, 0, -1 / distance, 1})
			if e.transformState == nil {
				e.transformState = NewTransformState()
			}
			if e.transformState.SetLocalPerspective(&perspective) {
				changed = true
			}
		} else if e.transformState != nil {
			e.transformState.SetLocalPerspective(nil)
		}
		if havePerspective != hadPerspective {
			changed = true
		}
		e.dirtyPerspective = false
	}
	if e.dirtyTransform {
		hadTransform := e.transformState != nil && e.transformState.GetTransform() != nil
		haveTransform := false
		transform := Matrix4Identity()
		if t := c.TransformPtr(); t != nil {
			n := t.GetNumPrimitives()
			for i := 0; i < n; i++ {
				transform = transform.Mul(TransformResolve(t.GetPrimitive(i), e))
				haveTransform = true
			}
			if haveTransform {
				origin := Vector3f{pos.X + size.X*0.5, pos.Y + size.Y*0.5, 0}
				if c.TransformOriginX().Type == LengthPercentagePercentage {
					origin.X = pos.X + c.TransformOriginX().Value*size.X*0.01
				} else {
					origin.X = pos.X + c.TransformOriginX().Value
				}
				if c.TransformOriginY().Type == LengthPercentagePercentage {
					origin.Y = pos.Y + c.TransformOriginY().Value*size.Y*0.01
				} else {
					origin.Y = pos.Y + c.TransformOriginY().Value
				}
				origin.Z = c.TransformOriginZ()
				transform = Matrix4TranslateV(origin).Mul(transform).Mul(Matrix4TranslateV(origin.Mul(-1)))
			}
		}
		var stackingParent *Element
		if e.parent != nil {
			stackingParent = e.parent.closestStackingContextContainer()
		}
		if stackingParent != nil && stackingParent.transformState != nil {
			ps := stackingParent.transformState
			if pp := ps.GetLocalPerspective(); pp != nil {
				transform = pp.Mul(transform)
				haveTransform = true
			}
			if pt := ps.GetTransform(); pt != nil {
				transform = pt.Mul(transform)
				haveTransform = true
			}
		}
		if haveTransform {
			if e.transformState == nil {
				e.transformState = NewTransformState()
			}
			if e.transformState.SetTransform(&transform) {
				changed = true
			}
		} else if e.transformState != nil {
			e.transformState.SetTransform(nil)
		}
		if hadTransform != haveTransform {
			changed = true
		}
		e.dirtyTransform = false
	}
	if changed {
		stacking := e.stackingContext
		for _, child := range stacking {
			child.dirtyTransformState(false, true)
		}
	}
	if e.transformState != nil && e.transformState.GetTransform() == nil && e.transformState.GetLocalPerspective() == nil {
		e.transformState = nil
	}
}

// OnStyleSheetChangeRecursive notifies the element tree of a new sheet.
func (e *Element) OnStyleSheetChangeRecursive() {
	e.effects.DirtyEffects()
	e.OnStyleSheetChange()
	n := e.GetNumChildren(true)
	for i := 0; i < n; i++ {
		e.GetChild(i).OnStyleSheetChangeRecursive()
	}
}

func (e *Element) OnDpRatioChangeRecursive() {
	e.effects.DirtyEffects()
	e.style.DirtyPropertiesWithUnits(UnitDP_SCALABLE_LENGTH)
	e.OnDpRatioChange()
	n := e.GetNumChildren(true)
	for i := 0; i < n; i++ {
		e.GetChild(i).OnDpRatioChangeRecursive()
	}
}

func (e *Element) DirtyFontFaceRecursive() {
	e.style.DirtyProperty(PropertyIdFontSize)
	e.computedValues.SetFontFaceHandle(0)
	n := e.GetNumChildren(true)
	for i := 0; i < n; i++ {
		e.GetChild(i).DirtyFontFaceRecursive()
	}
}

func (e *Element) clampScrollOffset() {
	newOffset := Vector2f{
		MathRound(MathMin(e.scrollOffset.X, e.GetScrollWidth()-e.GetClientWidth())),
		MathRound(MathMin(e.scrollOffset.Y, e.GetScrollHeight()-e.GetClientHeight())),
	}
	if !newOffset.Equals(e.scrollOffset) {
		e.scrollOffset = newOffset
		e.DirtyAbsoluteOffset()
	}
	e.scroll.UpdateProperties()
}

func (e *Element) ClampScrollOffsetRecursive() {
	e.clampScrollOffset()
	n := e.GetNumChildren(false)
	for i := 0; i < n; i++ {
		e.GetChild(i).ClampScrollOffsetRecursive()
	}
}

// ---- animations ----

func (e *Element) Animate(propertyName string, targetValue Property, duration float32, tween Tween, numIterations int, alternateDirection bool, delay float32, startValue *Property) bool {
	return e.AnimateId(GetPropertyId(propertyName), targetValue, duration, tween, numIterations, alternateDirection, delay, startValue)
}

func (e *Element) AnimateId(id PropertyId, targetValue Property, duration float32, tween Tween, numIterations int, alternateDirection bool, delay float32, startValue *Property) bool {
	animation := e.startAnimation(id, startValue, numIterations, alternateDirection, delay, false)
	if animation == nil {
		return false
	}
	result := animation.AddKey(duration, targetValue, e, tween, true)
	if !result {
		e.removeAnimation(animation)
	}
	return result
}

func (e *Element) AddAnimationKey(propertyName string, targetValue Property, duration float32, tween Tween) bool {
	return e.AddAnimationKeyId(GetPropertyId(propertyName), targetValue, duration, tween)
}

func (e *Element) AddAnimationKeyId(id PropertyId, targetValue Property, duration float32, tween Tween) bool {
	animation := e.findAnimation(id)
	if animation == nil {
		return false
	}
	return animation.AddKey(animation.GetDuration()+duration, targetValue, e, tween, true)
}

func (e *Element) findAnimation(id PropertyId) *ElementAnimation {
	animations := e.animations
	for _, a := range animations {
		if a.GetPropertyId() == id {
			return a
		}
	}
	return nil
}

func (e *Element) removeAnimation(animation *ElementAnimation) {
	out := []*ElementAnimation{}
	animations := e.animations
	for _, a := range animations {
		if a != animation {
			out = append(out, a)
		}
	}
	e.animations = out
}

func (e *Element) startAnimation(id PropertyId, startValue *Property, numIterations int, alternateDirection bool, delay float32, initiatedByAnimationProperty bool) *ElementAnimation {
	existing := e.findAnimation(id)
	if existing != nil && initiatedByAnimationProperty {
		LogMessage(LogWarning, "Could not animate property '"+GetPropertyName(id)+"' on element: "+e.GetAddress(false, true)+
			". Please ensure that the property does not appear in multiple animations on the same element.")
		return existing
	}
	value := NewProperty()
	if startValue != nil {
		value = *startValue
		if value.Definition == nil {
			if def := e.GetProperty(id); def != nil {
				value.Definition = def.Definition
			}
		}
	} else if def := e.GetProperty(id); def != nil {
		value = *def
	}
	var animation *ElementAnimation
	if value.Definition != nil {
		origin := ElementAnimationOriginUser
		if initiatedByAnimationProperty {
			origin = ElementAnimationOriginAnimation
		}
		startTime := ClockGetElapsedTime() + float64(delay)
		animation = NewElementAnimation(id, origin, value, e, startTime, 0, numIterations, alternateDirection)
	} else {
		animation = &ElementAnimation{}
	}
	if existing != nil {
		for i := 0; i < len(e.animations); i++ {
			if e.animations[i] == existing {
				e.animations[i] = animation
			}
		}
	} else {
		e.animations = append(e.animations, animation)
	}
	if !animation.IsInitialized() {
		e.removeAnimation(animation)
		return nil
	}
	return animation
}

func (e *Element) addAnimationKeyTime(id PropertyId, targetValue *Property, time float32, tween Tween) bool {
	if targetValue == nil {
		targetValue = e.style.GetProperty(id)
	}
	if targetValue == nil {
		return false
	}
	animation := e.findAnimation(id)
	if animation == nil {
		return false
	}
	return animation.AddKey(time, *targetValue, e, tween, true)
}

// StartTransition starts (or restarts) a transition of one property.
func (e *Element) StartTransition(transition Transition, startValue Property, targetValue Property) bool {
	existing := e.findAnimation(transition.Id)
	if existing != nil && !existing.IsTransition() {
		return false
	}
	duration := transition.Duration
	startTime := ClockGetElapsedTime() + float64(transition.Delay)
	animation := NewElementAnimation(transition.Id, ElementAnimationOriginTransition, startValue, e, startTime, 0, 1, false)
	if existing == nil {
		e.animations = append(e.animations, animation)
	} else {
		f := existing.GetInterpolationFactor()
		f = 1.0 - (1.0-f)*transition.ReverseAdjustmentFactor
		duration = duration * f
		for i := 0; i < len(e.animations); i++ {
			if e.animations[i] == existing {
				e.animations[i] = animation
			}
		}
	}
	result := animation.AddKey(duration, targetValue, e, transition.Tween, true)
	if result {
		e.SetPropertyById(transition.Id, startValue)
	} else {
		e.removeAnimation(animation)
	}
	return result
}

func (e *Element) handleTransitionProperty() {
	if !e.dirtyTransition {
		return
	}
	e.dirtyTransition = false
	keep := e.GetComputedValues().Transitions()
	if keep != nil && keep.All {
		return
	}
	kept := []*ElementAnimation{}
	removed := []*ElementAnimation{}
	animations := e.animations
	for _, a := range animations {
		if !a.IsTransition() {
			kept = append(kept, a)
			continue
		}
		keepIt := false
		if keep != nil && !keep.None {
			transitions := keep.Transitions
			for _, t := range transitions {
				if t.Id == a.GetPropertyId() {
					keepIt = true
				}
			}
		}
		if keepIt {
			kept = append(kept, a)
		} else {
			removed = append(removed, a)
		}
	}
	for _, a := range removed {
		e.RemovePropertyById(a.GetPropertyId())
	}
	e.animations = kept
}

func (e *Element) handleAnimationProperty() {
	if !e.dirtyAnimation {
		return
	}
	e.dirtyAnimation = false
	animationList := e.computedValues.Animations()
	hasAnimations := (animationList != nil && len(animationList.List) > 0) || len(e.animations) > 0
	var sheet *StyleSheet
	if hasAnimations {
		sheet = e.GetStyleSheet()
	}
	if sheet == nil {
		return
	}
	kept := []*ElementAnimation{}
	animations := e.animations
	for _, a := range animations {
		if a.GetOrigin() != ElementAnimationOriginAnimation {
			kept = append(kept, a)
		} else {
			e.RemovePropertyById(a.GetPropertyId())
		}
	}
	e.animations = kept
	if animationList == nil {
		return
	}
	list := animationList.List
	for _, animation := range list {
		keyframes := sheet.GetKeyframes(animation.Name)
		if keyframes == nil || len(keyframes.Blocks) < 1 || animation.Paused {
			continue
		}
		propertyIds := keyframes.PropertyIds
		blocks := keyframes.Blocks
		previousBlock := -1
		var cache *PropertyDictionary
		resolve := func(id PropertyId, property *Property, blockIndex int) *Property {
			if property == nil {
				return nil
			}
			if blockIndex != previousBlock {
				cache = nil
				previousBlock = blockIndex
			}
			return e.style.ResolveKeyFrameProperty(id, property, blocks[blockIndex].Properties, &cache)
		}
		hasFrom := blocks[0].NormalizedTime == 0
		hasTo := blocks[len(blocks)-1].NormalizedTime == 1
		for _, id := range propertyIds {
			var p *Property
			if hasFrom {
				p = blocks[0].Properties.GetProperty(id)
			}
			e.startAnimation(id, resolve(id, p, 0), animation.NumIterations, animation.Alternate, animation.Delay, true)
		}
		first := 0
		if hasFrom {
			first = 1
		}
		last := len(blocks)
		if hasTo {
			last = len(blocks) - 1
		}
		for i := first; i < last; i++ {
			time := blocks[i].NormalizedTime * animation.Duration
			props := blocks[i].Properties.GetProperties()
			ids := sortedPropertyIds(props)
			for _, id := range ids {
				e.addAnimationKeyTime(id, resolve(id, props[id], i), time, animation.Tween)
			}
		}
		endTime := animation.Duration
		for _, id := range propertyIds {
			var p *Property
			if hasTo {
				p = blocks[len(blocks)-1].Properties.GetProperty(id)
			}
			e.addAnimationKeyTime(id, resolve(id, p, len(blocks)-1), endTime, animation.Tween)
		}
	}
}

type animationCompletion struct {
	params       map[string]Variant
	isTransition bool
}

func (e *Element) advanceAnimations() {
	if len(e.animations) == 0 {
		return
	}
	time := ClockGetElapsedTime()
	animations := e.animations
	for _, a := range animations {
		property := a.UpdateAndGetProperty(time, e)
		if property.Unit != UnitUNKNOWN {
			e.SetPropertyById(a.GetPropertyId(), property)
		}
	}
	running := []*ElementAnimation{}
	completed := []*ElementAnimation{}
	for _, a := range animations {
		if a.IsComplete() {
			completed = append(completed, a)
		} else {
			running = append(running, a)
		}
	}
	events := []animationCompletion{}
	for _, a := range completed {
		params := map[string]Variant{}
		params["property"] = VariantString(GetPropertyName(a.GetPropertyId()))
		events = append(events, animationCompletion{params, a.IsTransition()})
		if a.GetOrigin() != ElementAnimationOriginUser {
			e.RemovePropertyById(a.GetPropertyId())
		}
	}
	e.animations = running
	for _, ev := range events {
		if ev.isTransition {
			e.DispatchEventId(EventIdTransitionend, ev.params)
		} else {
			e.DispatchEventId(EventIdAnimationend, ev.params)
		}
	}
}

// GetElement lets a plain *Element satisfy ElementSubclass-style APIs.
func (e *Element) GetElement() *Element { return e }
