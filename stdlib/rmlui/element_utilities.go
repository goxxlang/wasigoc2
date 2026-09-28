// Port of the RmlUi ElementUtilities.h entry points the element and
// effects code call. Clipping and transforms forward to the render
// manager when one is attached; without one, a visible element still
// reports that it should be drawn.
package rmlui

// ElementUtilitiesGetDensityIndependentPixelRatio is
// ElementUtilities::GetDensityIndependentPixelRatio.
func ElementUtilitiesGetDensityIndependentPixelRatio(element *Element) float32 {
	if element != nil {
		if ctx := element.GetContext(); ctx != nil {
			return ctx.GetDensityIndependentPixelRatio()
		}
	}
	return 1
}

// ElementUtilitiesGetElementById is ElementUtilities::GetElementById.
func ElementUtilitiesGetElementById(root *Element, id string) *Element {
	if root == nil {
		return nil
	}
	if root.GetId() == id {
		return root
	}
	n := root.GetNumChildren(true)
	for i := 0; i < n; i++ {
		if found := ElementUtilitiesGetElementById(root.GetChild(i), id); found != nil {
			return found
		}
	}
	return nil
}

// ElementUtilitiesGetElementsByTagName is ElementUtilities::GetElementsByTagName.
func ElementUtilitiesGetElementsByTagName(root *Element, tag string) []*Element {
	out := []*Element{}
	if root == nil {
		return out
	}
	if root.GetTagName() == tag {
		out = append(out, root)
	}
	n := root.GetNumChildren(true)
	for i := 0; i < n; i++ {
		out = append(out, ElementUtilitiesGetElementsByTagName(root.GetChild(i), tag)...)
	}
	return out
}

// ElementUtilitiesGetElementsByClassName is ElementUtilities::GetElementsByClassName.
func ElementUtilitiesGetElementsByClassName(root *Element, className string) []*Element {
	out := []*Element{}
	if root == nil {
		return out
	}
	if root.IsClassSet(className) {
		out = append(out, root)
	}
	n := root.GetNumChildren(true)
	for i := 0; i < n; i++ {
		out = append(out, ElementUtilitiesGetElementsByClassName(root.GetChild(i), className)...)
	}
	return out
}

// ElementUtilitiesGetBoundingBox is the border (or other area) rectangle
// in absolute pixel coordinates.
func ElementUtilitiesGetBoundingBox(element *Element, area BoxArea) Rectanglef {
	if element == nil {
		return Rectanglef{}
	}
	origin := element.GetAbsoluteOffset(area)
	size := element.GetBox().GetSize(area)
	return RectanglefFromPositionSize(origin, size)
}

// ElementUtilitiesApplyTransform pushes the element's transform to the
// render manager. With no render manager the call is a no-op.
func ElementUtilitiesApplyTransform(element *Element) {
	if element == nil {
		return
	}
	rm := element.GetRenderManager()
	if rm == nil {
		return
	}
	state := element.GetTransformState()
	if state == nil {
		rm.SetTransform(nil)
		return
	}
	rm.SetTransform(state.GetTransform())
}

// ElementUtilitiesSetClippingRegion applies the element's clip. It
// returns false when the element is not visible.
func ElementUtilitiesSetClippingRegion(element *Element, forceClip bool) bool {
	if element == nil || !element.IsVisible(true) {
		return false
	}
	rm := element.GetRenderManager()
	if rm == nil {
		return true
	}
	if !forceClip && element.GetClipArea() == BoxAreaAuto {
		return true
	}
	box := ElementUtilitiesGetBoundingBox(element, element.GetClipArea())
	rm.SetScissorRegion(box.ToInt())
	return true
}

// ElementUtilitiesApplyDataViewsControllers binds {{ }} text on element
// to its data model. Descendants are visited by setDataModel itself.
func ElementUtilitiesApplyDataViewsControllers(element *Element) {
	if element == nil {
		return
	}
	model := element.GetDataModel()
	text := ElementTextOf(element)
	if model == nil || text == nil {
		return
	}
	raw := text.GetTemplate()
	if indexFrom(raw, "{{", 0) < 0 {
		return
	}
	model.BindText(element, raw)
}
