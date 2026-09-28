// Port of RmlUi Source/Core/ElementScroll.cpp, WidgetScroll.cpp,
// TransformState.cpp and Include/RmlUi/Core/ElementScroll.h,
// Source/Core/WidgetScroll.h, TransformState.h.
package rmlui

// TransformState is Rml::TransformState: an element's accumulated
// transform and local perspective.
type TransformState struct {
	haveTransform         bool
	havePerspective       bool
	transform             Matrix4f
	localPerspective      Matrix4f
	dirtyInverseTransform bool
	haveInverseTransform  bool
	inverseTransform      Matrix4f
}

func NewTransformState() *TransformState { return &TransformState{} }

// SetTransform sets (or clears, with nil) the transform; returns true if
// it changed.
func (t *TransformState) SetTransform(in *Matrix4f) bool {
	changed := t.haveTransform != (in != nil)
	if in != nil {
		changed = changed || (t.haveTransform && !t.transform.Equals(*in))
		t.transform = *in
		t.haveTransform = true
	} else {
		t.haveTransform = false
	}
	if changed {
		t.dirtyInverseTransform = true
	}
	return changed
}

func (t *TransformState) SetLocalPerspective(in *Matrix4f) bool {
	changed := t.havePerspective != (in != nil)
	if in != nil {
		changed = changed || (t.havePerspective && !t.localPerspective.Equals(*in))
		t.localPerspective = *in
		t.havePerspective = true
	} else {
		t.havePerspective = false
	}
	return changed
}

func (t *TransformState) GetTransform() *Matrix4f {
	if t.haveTransform {
		return &t.transform
	}
	return nil
}

func (t *TransformState) GetLocalPerspective() *Matrix4f {
	if t.havePerspective {
		return &t.localPerspective
	}
	return nil
}

func (t *TransformState) GetInverseTransform() *Matrix4f {
	if !t.haveTransform {
		return nil
	}
	if t.dirtyInverseTransform {
		t.inverseTransform = t.transform
		t.haveInverseTransform = t.inverseTransform.Invert()
		t.dirtyInverseTransform = false
	}
	if t.haveInverseTransform {
		return &t.inverseTransform
	}
	return nil
}

// ElementScroll orientations (ElementScroll::Orientation).
const (
	ScrollbarVertical   = 0
	ScrollbarHorizontal = 1
)

func vecAxis(v Vector2f, axis int) float32 {
	if axis == 0 {
		return v.X
	}
	return v.Y
}

func vecSetAxis(v *Vector2f, axis int, value float32) {
	if axis == 0 {
		v.X = value
	} else {
		v.Y = value
	}
}

type scrollbar struct {
	element *Element
	widget  *WidgetScroll
	enabled bool
	size    float32
}

// ElementScroll is Rml::ElementScroll: an element's scrollbars.
type ElementScroll struct {
	element    *Element
	scrollbars [2]*scrollbar
	corner     *Element
}

func NewElementScroll(element *Element) *ElementScroll {
	s := &ElementScroll{element: element}
	s.scrollbars[0] = &scrollbar{}
	s.scrollbars[1] = &scrollbar{}
	return s
}

func (s *ElementScroll) Update() {
	for i := 0; i < 2; i++ {
		if s.scrollbars[i].widget != nil {
			s.scrollbars[i].widget.Update()
		}
	}
}

func (s *ElementScroll) EnableScrollbar(orientation int, elementWidth float32) {
	bar := s.scrollbars[orientation]
	if !bar.enabled {
		s.createScrollbar(orientation)
		bar.element.SetPropertyById(PropertyIdVisibility, PropertyKeyword(VisibilityVisible))
		bar.enabled = true
	}
	box := LayoutDetailsBuildBox(Vector2f{elementWidth, elementWidth}, bar.element, BuildBoxModeBlock)
	if orientation == ScrollbarVertical {
		bar.size = box.GetSize(BoxAreaMargin).X
	}
	if orientation == ScrollbarHorizontal {
		if box.GetContentSize().Y < 0 {
			bar.size = box.GetCumulativeEdge(BoxAreaContent, BoxEdgeLeft) + box.GetCumulativeEdge(BoxAreaContent, BoxEdgeRight) +
				ResolveValueAuto(bar.element.GetComputedValues().Height(), elementWidth)
		} else {
			bar.size = box.GetSize(BoxAreaMargin).Y
		}
	}
}

func (s *ElementScroll) DisableScrollbar(orientation int) {
	bar := s.scrollbars[orientation]
	if bar.enabled {
		bar.element.SetPropertyById(PropertyIdVisibility, PropertyKeyword(VisibilityHidden))
		bar.enabled = false
		if s.corner != nil {
			s.corner.SetPropertyById(PropertyIdVisibility, PropertyKeyword(VisibilityHidden))
		}
	}
}

func (s *ElementScroll) UpdateScrollbar(orientation int) {
	var barPosition float32 = 0
	var traversable float32 = 0
	if orientation == ScrollbarVertical {
		barPosition = s.element.GetScrollTop()
		traversable = s.element.GetScrollHeight() - s.element.GetClientHeight()
	} else {
		barPosition = s.element.GetScrollLeft()
		traversable = s.element.GetScrollWidth() - s.element.GetClientWidth()
	}
	if traversable <= 0 {
		barPosition = 0
	} else {
		barPosition = barPosition / traversable
	}
	if w := s.scrollbars[orientation].widget; w != nil {
		barPosition = MathClamp(barPosition, 0, 1)
		if w.GetBarPosition() != barPosition {
			w.SetBarPosition(barPosition)
		}
	}
}

func (s *ElementScroll) GetScrollbar(orientation int) *Element { return s.scrollbars[orientation].element }

func (s *ElementScroll) GetScrollbarSize(orientation int) float32 {
	if !s.scrollbars[orientation].enabled {
		return 0
	}
	return s.scrollbars[orientation].size
}

// FormatScrollbars positions the scrollbars after layout.
func (s *ElementScroll) FormatScrollbars() {
	elementBox := s.element.GetBox()
	block := elementBox.GetSize(BoxAreaPadding)
	for i := 0; i < 2; i++ {
		bar := s.scrollbars[i]
		if !bar.enabled {
			continue
		}
		if i == ScrollbarVertical {
			bar.widget.SetBarLength(s.element.GetClientHeight())
			bar.widget.SetTrackLength(s.element.GetScrollHeight())
			traversable := s.element.GetScrollHeight() - s.element.GetClientHeight()
			if traversable > 0 {
				bar.widget.SetBarPosition(s.element.GetScrollTop() / traversable)
			} else {
				bar.widget.SetBarPosition(0)
			}
		} else {
			bar.widget.SetBarLength(s.element.GetClientWidth())
			bar.widget.SetTrackLength(s.element.GetScrollWidth())
			traversable := s.element.GetScrollWidth() - s.element.GetClientWidth()
			if traversable > 0 {
				bar.widget.SetBarPosition(s.element.GetScrollLeft() / traversable)
			} else {
				bar.widget.SetBarPosition(0)
			}
		}
		sliderLength := vecAxis(block, 1-i)
		userMargin := bar.element.GetComputedValues().ScrollbarMargin()
		other := ScrollbarVertical
		if i == ScrollbarVertical {
			other = ScrollbarHorizontal
		}
		minMargin := s.GetScrollbarSize(other)
		sliderLength -= MathMax(userMargin, minMargin)
		bar.widget.FormatElements(block, sliderLength)
		variableAxis := 1
		if i == ScrollbarVertical {
			variableAxis = 0
		}
		offset := elementBox.GetPosition(BoxAreaPadding)
		marginEdge := BoxEdgeBottom
		if i == ScrollbarVertical {
			marginEdge = BoxEdgeRight
		}
		barBox := bar.element.GetBox()
		vecSetAxis(&offset, variableAxis, vecAxis(offset, variableAxis)+vecAxis(block, variableAxis)-
			(vecAxis(barBox.GetSize(BoxAreaBorder), variableAxis)+barBox.GetEdge(BoxAreaMargin, marginEdge)))
		leadEdge := BoxEdgeLeft
		if i == ScrollbarVertical {
			leadEdge = BoxEdgeTop
		}
		vecSetAxis(&offset, 1-variableAxis, vecAxis(offset, 1-variableAxis)+barBox.GetEdge(BoxAreaMargin, leadEdge))
		bar.element.SetOffset(offset, s.element, true)
	}
	if s.scrollbars[0].enabled && s.scrollbars[1].enabled {
		s.createCorner()
		cornerBox := LayoutDetailsBuildBox(Vector2f{block.X, block.X}, s.corner, BuildBoxModeBlock)
		cornerBox.SetContent(Vector2f{s.scrollbars[ScrollbarVertical].size, s.scrollbars[ScrollbarHorizontal].size})
		s.corner.SetBox(cornerBox)
		sizes := Vector2f{s.scrollbars[ScrollbarVertical].size, s.scrollbars[ScrollbarHorizontal].size}
		s.corner.SetOffset(block.Add(elementBox.GetPosition(BoxAreaPadding)).Sub(sizes).Sub(cornerBox.GetPosition(BoxAreaMargin)), s.element, true)
		s.corner.SetPropertyById(PropertyIdVisibility, PropertyKeyword(VisibilityVisible))
	}
}

func (s *ElementScroll) UpdateProperties() {
	if e := s.scrollbars[ScrollbarVertical].element; e != nil {
		s.updateScrollElementProperties(e)
	}
	if e := s.scrollbars[ScrollbarHorizontal].element; e != nil {
		s.updateScrollElementProperties(e)
	}
	if s.corner != nil {
		s.updateScrollElementProperties(s.corner)
	}
}

func (s *ElementScroll) createScrollbar(orientation int) bool {
	bar := s.scrollbars[orientation]
	if bar.element != nil && bar.widget != nil {
		return true
	}
	tag := "scrollbarhorizontal"
	if orientation == ScrollbarVertical {
		tag = "scrollbarvertical"
	}
	element := FactoryInstanceElement(s.element, "*", tag, map[string]Variant{})
	bar.element = element
	element.SetPropertyById(PropertyIdClip, PropertyFloat(1, UnitNUMBER))
	element.SetPropertyById(PropertyIdDrag, PropertyKeyword(DragBlock))
	bar.widget = NewWidgetScroll(element)
	widgetOrientation := WidgetScrollHorizontal
	if orientation == ScrollbarVertical {
		widgetOrientation = WidgetScrollVertical
	}
	bar.widget.Initialise(widgetOrientation)
	child := s.element.AppendChild(element, false)
	s.updateScrollElementProperties(child)
	return true
}

func (s *ElementScroll) createCorner() bool {
	if s.corner != nil {
		return true
	}
	corner := FactoryInstanceElement(s.element, "*", "scrollbarcorner", map[string]Variant{})
	s.corner = corner
	corner.SetPropertyById(PropertyIdClip, PropertyFloat(1, UnitNUMBER))
	corner.SetPropertyById(PropertyIdDrag, PropertyKeyword(DragBlock))
	child := s.element.AppendChild(corner, false)
	s.updateScrollElementProperties(child)
	return true
}

func (s *ElementScroll) updateScrollElementProperties(scrollElement *Element) {
	ctx := s.element.GetContext()
	var dpRatio float32 = 1
	vp := Vector2f{1, 1}
	if ctx != nil {
		dpRatio = ctx.GetDensityIndependentPixelRatio()
		vp = ctx.GetDimensions().ToFloat()
	}
	scrollElement.Update(dpRatio, vp)
}

// ---- WidgetScroll ----

const (
	scrollDefaultRepeatDelay  float32 = 0.5
	scrollDefaultRepeatPeriod float32 = 0.1
	scrollLineLength          float32 = 30 // dp
	scrollPageFactor          float32 = 0.8
)

// WidgetScroll orientations.
const (
	WidgetScrollUnknown = iota
	WidgetScrollVertical
	WidgetScrollHorizontal
)

// WidgetScroll is Rml::WidgetScroll: track, bar, and arrow elements of a
// scrollbar, and the event handling that scrolls the parent element.
type WidgetScroll struct {
	parent         *Element
	orientation    int
	track          *Element
	bar            *Element
	arrows         [2]*Element
	barPosition    float32
	barDragAnchor  float32
	arrowTimers    [2]float32
	lastUpdateTime float64
	trackLength    float32
	barLength      float32
}

func NewWidgetScroll(parent *Element) *WidgetScroll {
	w := &WidgetScroll{parent: parent, orientation: WidgetScrollUnknown}
	w.arrowTimers[0] = -1
	w.arrowTimers[1] = -1
	return w
}

// Destroy detaches the widget's listeners (~WidgetScroll).
func (w *WidgetScroll) Destroy() {
	if w.bar != nil {
		w.bar.RemoveEventListenerId(EventIdDrag, w, false)
		w.bar.RemoveEventListenerId(EventIdDragstart, w, false)
	}
	if w.track != nil {
		w.track.RemoveEventListenerId(EventIdClick, w, false)
	}
	for i := 0; i < 2; i++ {
		if w.arrows[i] != nil {
			w.arrows[i].RemoveEventListenerId(EventIdMousedown, w, false)
			w.arrows[i].RemoveEventListenerId(EventIdMouseup, w, false)
			w.arrows[i].RemoveEventListenerId(EventIdMouseout, w, false)
		}
	}
}

func (w *WidgetScroll) Initialise(orientation int) bool {
	if w.orientation != WidgetScrollUnknown {
		return false
	}
	if orientation != WidgetScrollHorizontal && orientation != WidgetScrollVertical {
		return false
	}
	w.orientation = orientation
	noAttributes := map[string]Variant{}
	track := FactoryInstanceElement(w.parent, "*", "slidertrack", noAttributes)
	bar := FactoryInstanceElement(w.parent, "*", "sliderbar", noAttributes)
	arrow0 := FactoryInstanceElement(w.parent, "*", "sliderarrowdec", noAttributes)
	arrow1 := FactoryInstanceElement(w.parent, "*", "sliderarrowinc", noAttributes)
	if track == nil || bar == nil || arrow0 == nil || arrow1 == nil {
		return false
	}
	w.track = w.parent.AppendChild(track, false)
	w.bar = w.parent.AppendChild(bar, false)
	w.arrows[0] = w.parent.AppendChild(arrow0, false)
	w.arrows[1] = w.parent.AppendChild(arrow1, false)
	w.bar.SetPropertyById(PropertyIdDrag, PropertyKeyword(DragDrag))
	w.bar.AddEventListenerId(EventIdDrag, w, false)
	w.bar.AddEventListenerId(EventIdDragstart, w, false)
	w.track.AddEventListenerId(EventIdClick, w, false)
	for i := 0; i < 2; i++ {
		w.arrows[i].AddEventListenerId(EventIdMousedown, w, false)
		w.arrows[i].AddEventListenerId(EventIdMouseup, w, false)
		w.arrows[i].AddEventListenerId(EventIdMouseout, w, false)
	}
	return true
}

func (w *WidgetScroll) Update() {
	if !(w.arrowTimers[0] > 0 || w.arrowTimers[1] > 0) {
		return
	}
	now := ClockGetElapsedTime()
	delta := float32(now - w.lastUpdateTime)
	w.lastUpdateTime = now
	for i := 0; i < 2; i++ {
		if w.arrowTimers[i] > 0 {
			w.arrowTimers[i] -= delta
			for w.arrowTimers[i] <= 0 {
				w.arrowTimers[i] += scrollDefaultRepeatPeriod
				if i == 0 {
					w.scrollLineUp()
				} else {
					w.scrollLineDown()
				}
			}
			if ctx := w.parent.GetContext(); ctx != nil {
				ctx.RequestNextUpdate(float64(w.arrowTimers[i]))
			}
		}
	}
}

func (w *WidgetScroll) SetBarPosition(position float32) {
	w.barPosition = MathClamp(position, 0, 1)
	w.positionBar()
}

func (w *WidgetScroll) GetBarPosition() float32 { return w.barPosition }
func (w *WidgetScroll) GetOrientation() int     { return w.orientation }

func (w *WidgetScroll) GetDimensions() Vector2f {
	switch w.orientation {
	case WidgetScrollVertical:
		return Vector2f{16, 256}
	case WidgetScrollHorizontal:
		return Vector2f{256, 16}
	}
	return Vector2f{}
}

func (w *WidgetScroll) SetTrackLength(length float32) { w.trackLength = length }
func (w *WidgetScroll) SetBarLength(length float32)   { w.barLength = length }

func (w *WidgetScroll) FormatElements(containingBlock Vector2f, sliderLength float32) {
	lengthAxis := 0
	if w.orientation == WidgetScrollVertical {
		lengthAxis = 1
	}
	parentBox := LayoutDetailsBuildBox(containingBlock, w.parent, BuildBoxModeBlock)
	if w.orientation == WidgetScrollVertical {
		sliderLength -= parentBox.GetCumulativeEdge(BoxAreaContent, BoxEdgeTop) + parentBox.GetCumulativeEdge(BoxAreaContent, BoxEdgeBottom)
	} else {
		sliderLength -= parentBox.GetCumulativeEdge(BoxAreaContent, BoxEdgeLeft) + parentBox.GetCumulativeEdge(BoxAreaContent, BoxEdgeRight)
	}
	content := parentBox.GetContentSize()
	vecSetAxis(&content, lengthAxis, sliderLength)
	parentBox.SetContent(content)
	w.parent.SetBox(parentBox)

	trackBox := LayoutDetailsBuildBox(parentBox.GetContentSize(), w.track, BuildBoxModeBlock)
	content = trackBox.GetContentSize()
	if w.orientation == WidgetScrollVertical {
		sliderLength -= trackBox.GetCumulativeEdge(BoxAreaContent, BoxEdgeTop) + trackBox.GetCumulativeEdge(BoxAreaContent, BoxEdgeBottom)
	} else {
		sliderLength -= trackBox.GetCumulativeEdge(BoxAreaContent, BoxEdgeLeft) + trackBox.GetCumulativeEdge(BoxAreaContent, BoxEdgeRight)
	}
	vecSetAxis(&content, lengthAxis, sliderLength)
	if w.orientation == WidgetScrollHorizontal && content.Y < 0 {
		content.Y = parentBox.GetContentSize().Y
	}
	for i := 0; i < 2; i++ {
		arrowBox := LayoutDetailsBuildBox(parentBox.GetContentSize(), w.arrows[i], BuildBoxModeBlock)
		size := arrowBox.GetContentSize()
		if size.X < 0 || size.Y < 0 {
			arrowBox.SetContent(Vector2f{})
		}
		w.arrows[i].SetBox(arrowBox)
		vecSetAxis(&content, lengthAxis, vecAxis(content, lengthAxis)-vecAxis(arrowBox.GetSize(BoxAreaMargin), lengthAxis))
	}
	trackBox.SetContent(content)
	w.track.SetBox(trackBox)

	a0 := w.arrows[0].GetBox()
	a1 := w.arrows[1].GetBox()
	tb := w.track.GetBox()
	if w.orientation == WidgetScrollVertical {
		offset := Vector2f{a0.GetEdge(BoxAreaMargin, BoxEdgeLeft), a0.GetEdge(BoxAreaMargin, BoxEdgeTop)}
		w.arrows[0].SetOffset(offset, w.parent, false)
		offset.X = tb.GetEdge(BoxAreaMargin, BoxEdgeLeft)
		offset.Y += a0.GetSize(BoxAreaBorder).Y + a0.GetEdge(BoxAreaMargin, BoxEdgeBottom) + tb.GetEdge(BoxAreaMargin, BoxEdgeTop)
		w.track.SetOffset(offset, w.parent, false)
		offset.X = a1.GetEdge(BoxAreaMargin, BoxEdgeLeft)
		offset.Y += tb.GetSize(BoxAreaBorder).Y + tb.GetEdge(BoxAreaMargin, BoxEdgeBottom) + a1.GetEdge(BoxAreaMargin, BoxEdgeTop)
		w.arrows[1].SetOffset(offset, w.parent, false)
	} else {
		offset := Vector2f{a0.GetEdge(BoxAreaMargin, BoxEdgeLeft), a0.GetEdge(BoxAreaMargin, BoxEdgeTop)}
		w.arrows[0].SetOffset(offset, w.parent, false)
		offset.X += a0.GetSize(BoxAreaBorder).X + a0.GetEdge(BoxAreaMargin, BoxEdgeRight) + tb.GetEdge(BoxAreaMargin, BoxEdgeLeft)
		offset.Y = tb.GetEdge(BoxAreaMargin, BoxEdgeTop)
		w.track.SetOffset(offset, w.parent, false)
		offset.X += tb.GetSize(BoxAreaBorder).X + tb.GetEdge(BoxAreaMargin, BoxEdgeRight) + a1.GetEdge(BoxAreaMargin, BoxEdgeLeft)
		offset.Y = a1.GetEdge(BoxAreaMargin, BoxEdgeTop)
		w.arrows[1].SetOffset(offset, w.parent, false)
	}
	w.formatBar()
}

func (w *WidgetScroll) formatBar() {
	barBox := LayoutDetailsBuildBox(w.parent.GetBox().GetContentSize(), w.bar, BuildBoxModeBlock)
	computed := w.bar.GetComputedValues()
	width := computed.Width()
	height := computed.Height()
	content := barBox.GetContentSize()
	if w.orientation == WidgetScrollHorizontal && height.Type == LengthPercentageAutoAuto {
		content.Y = w.parent.GetBox().GetContentSize().Y
	}
	var relative float32 = 0
	if w.trackLength <= 0 {
		relative = 1
	} else if w.barLength <= 0 {
		relative = 0
	} else {
		relative = w.barLength / w.trackLength
	}
	trackSize := w.track.GetBox().GetContentSize()
	if w.orientation == WidgetScrollVertical {
		trackLength := trackSize.Y - barBox.GetSizeAcross(BoxDirectionVertical, BoxAreaMargin, BoxAreaPadding)
		if height.Type == LengthPercentageAutoAuto {
			content.Y = trackLength * relative
			content.Y = MathMax(ResolveValue(computed.MinHeight(), trackLength), content.Y)
			content.Y = MathMin(ResolveValue(computed.MaxHeight(), trackLength), content.Y)
		}
		content.Y = MathMin(content.Y, trackLength)
	} else {
		trackLength := trackSize.X - barBox.GetSizeAcross(BoxDirectionHorizontal, BoxAreaMargin, BoxAreaPadding)
		if width.Type == LengthPercentageAutoAuto {
			content.X = trackLength * relative
			content.X = MathMax(ResolveValue(computed.MinWidth(), trackLength), content.X)
			content.X = MathMin(ResolveValue(computed.MaxWidth(), trackLength), content.X)
		}
		content.X = MathMin(content.X, trackLength)
	}
	barBox.SetContent(content.Round())
	w.bar.SetBox(barBox)
	w.positionBar()
}

// ProcessEvent handles bar dragging, track clicks, and arrow repeat.
func (w *WidgetScroll) ProcessEvent(event *Event) {
	target := event.GetTargetElement()
	if target == w.bar {
		if event.GetId() == EventIdDrag {
			var newPosition float32 = 0
			if w.orientation == WidgetScrollHorizontal {
				traversable := w.track.GetBox().GetContentSize().X - w.bar.GetBox().GetContentSize().X
				if traversable > 0 {
					origin := w.track.GetAbsoluteOffset(BoxAreaContent).X + w.barDragAnchor
					newPosition = (event.GetParameterFloat("mouse_x", 0) - origin) / traversable
				}
			} else {
				traversable := w.track.GetBox().GetContentSize().Y - w.bar.GetBox().GetContentSize().Y
				if traversable > 0 {
					origin := w.track.GetAbsoluteOffset(BoxAreaContent).Y + w.barDragAnchor
					newPosition = (event.GetParameterFloat("mouse_y", 0) - origin) / traversable
				}
			}
			w.SetBarPosition(newPosition)
			w.scroll(0, ScrollBehaviorInstant)
		} else if event.GetId() == EventIdDragstart {
			if w.orientation == WidgetScrollHorizontal {
				w.barDragAnchor = event.GetParameterFloat("mouse_x", 0) - w.bar.GetAbsoluteOffset(BoxAreaContent).X
			} else {
				w.barDragAnchor = event.GetParameterFloat("mouse_y", 0) - w.bar.GetAbsoluteOffset(BoxAreaContent).Y
			}
		}
	} else if target == w.track {
		if event.GetId() == EventIdClick {
			var clickPosition float32 = 0
			if w.orientation == WidgetScrollHorizontal {
				clickPosition = (event.GetParameterFloat("mouse_x", 0) - w.track.GetAbsoluteOffset(BoxAreaContent).X) / w.track.GetBox().GetContentSize().X
			} else {
				clickPosition = (event.GetParameterFloat("mouse_y", 0) - w.track.GetAbsoluteOffset(BoxAreaContent).Y) / w.track.GetBox().GetContentSize().Y
			}
			if clickPosition <= w.barPosition {
				w.scrollPageUp()
			} else {
				w.scrollPageDown()
			}
		}
	}
	if event.GetId() == EventIdMousedown {
		if target == w.arrows[0] {
			w.arrowTimers[0] = scrollDefaultRepeatDelay
			w.lastUpdateTime = ClockGetElapsedTime()
			w.scrollLineUp()
		} else if target == w.arrows[1] {
			w.arrowTimers[1] = scrollDefaultRepeatDelay
			w.lastUpdateTime = ClockGetElapsedTime()
			w.scrollLineDown()
		}
	} else if event.GetId() == EventIdMouseup || event.GetId() == EventIdMouseout {
		if target == w.arrows[0] {
			w.arrowTimers[0] = -1
		} else if target == w.arrows[1] {
			w.arrowTimers[1] = -1
		}
	}
}

func (w *WidgetScroll) OnAttach(element *Element) {}
func (w *WidgetScroll) OnDetach(element *Element) {}

func (w *WidgetScroll) positionBar() {
	trackDims := w.track.GetBox().GetContentSize()
	barDims := w.bar.GetBox().GetSize(BoxAreaBorder)
	if w.orientation == WidgetScrollVertical {
		traversable := trackDims.Y - barDims.Y
		offset := Vector2f{w.bar.GetBox().GetEdge(BoxAreaMargin, BoxEdgeLeft), w.track.GetRelativeOffset(BoxAreaContent).Y + traversable*w.barPosition}
		w.bar.SetOffset(offset.Round(), w.parent, false)
	} else {
		traversable := trackDims.X - barDims.X
		offset := Vector2f{w.track.GetRelativeOffset(BoxAreaContent).X + traversable*w.barPosition, w.bar.GetBox().GetEdge(BoxAreaMargin, BoxEdgeTop)}
		w.bar.SetOffset(offset.Round(), w.parent, false)
	}
}

func (w *WidgetScroll) scrollLineDown() {
	w.scroll(scrollLineLength*ElementUtilitiesGetDensityIndependentPixelRatio(w.parent), ScrollBehaviorAuto)
}

func (w *WidgetScroll) scrollLineUp() {
	w.scroll(-scrollLineLength*ElementUtilitiesGetDensityIndependentPixelRatio(w.parent), ScrollBehaviorAuto)
}

func (w *WidgetScroll) scrollPageDown() { w.scroll(scrollPageFactor*w.barLength, ScrollBehaviorAuto) }
func (w *WidgetScroll) scrollPageUp()   { w.scroll(-scrollPageFactor*w.barLength, ScrollBehaviorAuto) }

func (w *WidgetScroll) scroll(distance float32, behavior ScrollBehavior) {
	traversable := w.trackLength - w.barLength
	newPosition := w.barPosition
	if traversable > 0 {
		newPosition = MathClamp((w.barPosition*traversable+distance)/traversable, 0, 1)
	}
	target := w.parent.GetParentNode()
	if target == nil {
		return
	}
	offset := Vector2f{target.GetScrollLeft(), target.GetScrollTop()}
	if w.orientation == WidgetScrollHorizontal {
		offset.X = newPosition * (target.GetScrollWidth() - target.GetClientWidth())
	} else {
		offset.Y = newPosition * (target.GetScrollHeight() - target.GetClientHeight())
	}
	target.ScrollTo(offset, behavior)
}
