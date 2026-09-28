// Port of RmlUi Source/Core/ElementBackgroundBorder.cpp, BoxShadowCache.cpp,
// GeometryBoxShadow.cpp and their headers.
package rmlui

const (
	backgroundTypeBackgroundBorder = iota
	backgroundTypeBoxShadowAndBackgroundBorder
	backgroundTypeClipBorder
	backgroundTypeClipPadding
	backgroundTypeClipContent
	backgroundTypeCount
)

type elementBackground struct {
	geometry  Geometry
	texture   Texture
	boxShadow *BoxShadowRenderable
}

// ElementBackgroundBorder is Rml::ElementBackgroundBorder: an element's
// background, border, box-shadow, and clip-mask geometry.
type ElementBackgroundBorder struct {
	backgroundDirty bool
	borderDirty     bool
	backgrounds     [backgroundTypeCount]*elementBackground
}

func NewElementBackgroundBorder() *ElementBackgroundBorder { return &ElementBackgroundBorder{} }

func (b *ElementBackgroundBorder) Render(element *Element) {
	if b.backgroundDirty || b.borderDirty {
		for t := 0; t < backgroundTypeCount; t++ {
			if t != backgroundTypeBackgroundBorder && b.backgrounds[t] != nil {
				b.backgrounds[t].geometry.Release(GeometryReleaseReturnMesh)
			}
		}
		b.generateGeometry(element)
		b.backgroundDirty = false
		b.borderDirty = false
	}
	if shadow := b.backgrounds[backgroundTypeBoxShadowAndBackgroundBorder]; shadow != nil && shadow.boxShadow != nil {
		offset := element.GetAbsoluteOffset(BoxAreaBorder)
		tex := shadow.boxShadow.texture.GetTexture()
		shadow.boxShadow.geometry.Render(offset, tex, NewCompiledShader())
	} else if background := b.backgrounds[backgroundTypeBackgroundBorder]; background != nil {
		offset := element.GetAbsoluteOffset(BoxAreaBorder)
		background.geometry.RenderPlain(offset)
	}
}

func (b *ElementBackgroundBorder) DirtyBackground() { b.backgroundDirty = true }
func (b *ElementBackgroundBorder) DirtyBorder()     { b.borderDirty = true }

// GetClipGeometry returns the clip-mask geometry for clipArea.
func (b *ElementBackgroundBorder) GetClipGeometry(element *Element, clipArea BoxArea) *Geometry {
	t := 0
	switch clipArea {
	case BoxAreaBorder:
		t = backgroundTypeClipBorder
	case BoxAreaPadding:
		t = backgroundTypeClipPadding
	case BoxAreaContent:
		t = backgroundTypeClipContent
	default:
		return nil
	}
	rm := element.GetRenderManager()
	bg := b.getOrCreateBackground(t)
	if rm != nil && !bg.geometry.Valid() {
		mesh := bg.geometry.Release(GeometryReleaseClearMesh)
		MeshGenerateBackground(&mesh, element.GetRenderBox(clipArea, 0), ColourbPremultiplied{255, 255, 255, 255})
		bg.geometry = rm.MakeGeometry(mesh)
	}
	return &bg.geometry
}

func (b *ElementBackgroundBorder) getOrCreateBackground(t int) *elementBackground {
	if b.backgrounds[t] == nil {
		b.backgrounds[t] = &elementBackground{geometry: NewGeometry()}
	}
	return b.backgrounds[t]
}

func (b *ElementBackgroundBorder) eraseBackground(t int) {
	if bg := b.backgrounds[t]; bg != nil {
		bg.geometry.Release(GeometryReleaseReturnMesh)
		if bg.boxShadow != nil {
			bg.boxShadow.releaseReference()
		}
		b.backgrounds[t] = nil
	}
}

// Release frees every render resource (element destruction).
func (b *ElementBackgroundBorder) Release() {
	for t := 0; t < backgroundTypeCount; t++ {
		b.eraseBackground(t)
	}
}

func (b *ElementBackgroundBorder) generateGeometry(element *Element) {
	rm := element.GetRenderManager()
	if rm == nil {
		return
	}
	computed := element.GetComputedValues()
	if computed.HasBoxShadow() {
		b.eraseBackground(backgroundTypeBackgroundBorder)
		shadowBg := b.getOrCreateBackground(backgroundTypeBoxShadowAndBackgroundBorder)
		handle := BoxShadowCacheGetHandle(element, computed)
		if shadowBg.boxShadow != nil {
			shadowBg.boxShadow.releaseReference()
		}
		shadowBg.boxShadow = handle
		return
	}
	b.eraseBackground(backgroundTypeBoxShadowAndBackgroundBorder)
	opacity := computed.Opacity()
	backgroundColor := computed.BackgroundColor().ToPremultipliedOpacity(opacity)
	borderColors := []ColourbPremultiplied{
		computed.BorderTopColor().ToPremultipliedOpacity(opacity),
		computed.BorderRightColor().ToPremultipliedOpacity(opacity),
		computed.BorderBottomColor().ToPremultipliedOpacity(opacity),
		computed.BorderLeftColor().ToPremultipliedOpacity(opacity),
	}
	bg := b.getOrCreateBackground(backgroundTypeBackgroundBorder)
	mesh := bg.geometry.Release(GeometryReleaseClearMesh)
	for i := 0; i < element.GetNumBoxes(); i++ {
		MeshGenerateBackgroundBorder(&mesh, element.GetRenderBox(BoxAreaPadding, i), backgroundColor, borderColors)
	}
	bg.geometry = rm.MakeGeometry(mesh)
}

// ---- BoxShadowCache / GeometryBoxShadow ----

// BoxShadowGeometryInfo is Rml::BoxShadowGeometryInfo, the cache key.
type BoxShadowGeometryInfo struct {
	BackgroundColor        ColourbPremultiplied
	BorderColors           []ColourbPremultiplied
	BorderRadius           CornerSizes
	TextureDimensions      Vector2i
	ElementOffsetInTexture Vector2f
	PaddingRenderBoxes     []RenderBox
	BorderRenderBoxes      []RenderBox
	ShadowList             []BoxShadow
	Opacity                float32
}

func renderBoxListsEqual(a []RenderBox, b []RenderBox) bool {
	if len(a) != len(b) {
		return false
	}
	for i := 0; i < len(a); i++ {
		if !a[i].Equals(b[i]) {
			return false
		}
	}
	return true
}

func (a *BoxShadowGeometryInfo) Equals(b *BoxShadowGeometryInfo) bool {
	if !a.BackgroundColor.Equals(b.BackgroundColor) || len(a.BorderColors) != len(b.BorderColors) {
		return false
	}
	for i := 0; i < len(a.BorderColors); i++ {
		if !a.BorderColors[i].Equals(b.BorderColors[i]) {
			return false
		}
	}
	for i := 0; i < 4; i++ {
		if a.BorderRadius[i] != b.BorderRadius[i] {
			return false
		}
	}
	if !a.TextureDimensions.Equals(b.TextureDimensions) || !a.ElementOffsetInTexture.Equals(b.ElementOffsetInTexture) {
		return false
	}
	if !renderBoxListsEqual(a.PaddingRenderBoxes, b.PaddingRenderBoxes) || !renderBoxListsEqual(a.BorderRenderBoxes, b.BorderRenderBoxes) {
		return false
	}
	if len(a.ShadowList) != len(b.ShadowList) {
		return false
	}
	for i := 0; i < len(a.ShadowList); i++ {
		if !a.ShadowList[i].Equals(b.ShadowList[i]) {
			return false
		}
	}
	return a.Opacity == b.Opacity
}

// BoxShadowRenderable is Rml::BoxShadowRenderable, shared through the cache.
type BoxShadowRenderable struct {
	texture                  CallbackTexture
	geometry                 Geometry
	backgroundBorderGeometry Geometry
	cacheKey                 *BoxShadowGeometryInfo
	references               int
}

func (r *BoxShadowRenderable) releaseReference() {
	r.references--
	if r.references > 0 {
		return
	}
	r.texture.Release()
	r.geometry.Release(GeometryReleaseReturnMesh)
	r.backgroundBorderGeometry.Release(GeometryReleaseReturnMesh)
	kept := []*BoxShadowRenderable{}
	handles := boxShadowCacheHandles
	for _, h := range handles {
		if h != r {
			kept = append(kept, h)
		}
	}
	boxShadowCacheHandles = kept
}

var boxShadowCacheHandles []*BoxShadowRenderable

func BoxShadowCacheInitialize() { boxShadowCacheHandles = nil }
func BoxShadowCacheShutdown()   { boxShadowCacheHandles = nil }

// BoxShadowCacheGetHandle is BoxShadowCache::GetHandle.
func BoxShadowCacheGetHandle(element *Element, computed *ComputedValues) *BoxShadowRenderable {
	rm := element.GetRenderManager()
	if rm == nil {
		return nil
	}
	backgroundColor := computed.BackgroundColor().ToPremultiplied()
	borderColors := []ColourbPremultiplied{
		computed.BorderTopColor().ToPremultiplied(),
		computed.BorderRightColor().ToPremultiplied(),
		computed.BorderBottomColor().ToPremultiplied(),
		computed.BorderLeftColor().ToPremultiplied(),
	}
	info := geometryBoxShadowResolve(element, computed.BorderRadius(), backgroundColor, borderColors, computed.Opacity())
	handles := boxShadowCacheHandles
	for _, h := range handles {
		if h.cacheKey.Equals(info) {
			h.references++
			return h
		}
	}
	handle := &BoxShadowRenderable{texture: NewCallbackTexture(), geometry: NewGeometry(), backgroundBorderGeometry: NewGeometry(), cacheKey: info, references: 1}
	geometryBoxShadowGenerateTexture(handle, rm, info)
	mesh := Mesh{}
	alpha := byte(info.Opacity * 255.0)
	MeshGenerateQuad(&mesh, info.ElementOffsetInTexture.Neg(), info.TextureDimensions.ToFloat(), ColourbPremultiplied{alpha, alpha, alpha, alpha})
	handle.geometry = rm.MakeGeometry(mesh)
	boxShadowCacheHandles = append(boxShadowCacheHandles, handle)
	return handle
}

func geometryBoxShadowResolve(element *Element, borderRadius CornerSizes, backgroundColor ColourbPremultiplied, borderColors []ColourbPremultiplied, opacity float32) *BoxShadowGeometryInfo {
	shadowList := []BoxShadow{}
	p := element.GetStyle().GetLocalPropertyWithResolvedVariables(PropertyIdBoxShadow)
	if p != nil {
		if list, ok := p.Value.Pointer().(*BoxShadowList); ok {
			shadowList = append(shadowList, list.Shadows...)
		}
	}
	for i := 0; i < len(shadowList); i++ {
		s := shadowList[i]
		s.BlurRadius = NumericValue{element.ResolveLength(s.BlurRadius), UnitPX}
		s.SpreadDistance = NumericValue{element.ResolveLength(s.SpreadDistance), UnitPX}
		s.OffsetX = NumericValue{element.ResolveLength(s.OffsetX), UnitPX}
		s.OffsetY = NumericValue{element.ResolveLength(s.OffsetY), UnitPX}
		shadowList[i] = s
	}
	extendMin := Vector2f{}
	extendMax := Vector2f{}
	for _, s := range shadowList {
		if !s.Inset {
			extend := 1.5*s.BlurRadius.Number + s.SpreadDistance.Number
			offset := Vector2f{s.OffsetX.Number, s.OffsetY.Number}
			extendMin = MathMinVector(extendMin, offset.Sub(Vector2f{extend, extend}))
			extendMax = MathMaxVector(extendMax, offset.Add(Vector2f{extend, extend}))
		}
	}
	region := Rectanglef{}
	for i := 0; i < element.GetNumBoxes(); i++ {
		box := element.GetRenderBox(BoxAreaBorder, i)
		region = region.Join(RectanglefFromPositionSize(box.BorderOffset, box.FillSize))
	}
	region = region.ExtendCorners(extendMin.Neg(), extendMax)
	region = MathExpandToPixelGridRect(region)
	info := &BoxShadowGeometryInfo{
		BackgroundColor:        backgroundColor,
		BorderColors:           borderColors,
		BorderRadius:           borderRadius,
		TextureDimensions:      region.Size().ToInt(),
		ElementOffsetInTexture: region.TopLeft().Neg(),
		ShadowList:             shadowList,
		Opacity:                opacity,
	}
	for i := 0; i < element.GetNumBoxes(); i++ {
		info.PaddingRenderBoxes = append(info.PaddingRenderBoxes, element.GetRenderBox(BoxAreaPadding, i))
		info.BorderRenderBoxes = append(info.BorderRenderBoxes, element.GetRenderBox(BoxAreaBorder, i))
	}
	return info
}

// boxShadowTextureCallback renders the shadow into a layer and saves it.
type boxShadowTextureCallback struct {
	info       *BoxShadowGeometryInfo
	renderable *BoxShadowRenderable
}

func copyMesh(m Mesh) Mesh {
	return Mesh{Vertices: append([]Vertex{}, m.Vertices...), Indices: append([]int{}, m.Indices...)}
}

func (c *boxShadowTextureCallback) GenerateTexture(textureInterface *CallbackTextureInterface) bool {
	info := c.info
	numBoxes := len(info.BorderRenderBoxes)
	rm := textureInterface.GetRenderManager()
	meshPadding := Mesh{}
	meshPaddingBorder := Mesh{}
	hasInner := false
	hasOuter := false
	shadows := info.ShadowList
	for _, s := range shadows {
		if s.Inset {
			hasInner = true
		} else {
			hasOuter = true
		}
	}
	white := ColourbPremultiplied{255, 255, 255, 255}
	for i := 0; i < numBoxes; i++ {
		if hasInner {
			MeshGenerateBackground(&meshPadding, info.PaddingRenderBoxes[i], white)
		}
		if hasOuter {
			MeshGenerateBackground(&meshPaddingBorder, info.BorderRenderBoxes[i], white)
		}
	}
	initialState := rm.GetState()
	rm.ResetState()
	textureRect := RectangleiFromPositionSize(Vector2i{}, info.TextureDimensions)
	rm.SetScissorRegion(textureRect)
	scissor := rm.GetScissorRegion()
	if scissor.Width() <= 0 || scissor.Height() <= 0 {
		rm.SetState(initialState)
		return false
	}
	if !scissor.Equals(textureRect) {
		LogMessage(LogInfo, "The desired box-shadow texture dimensions ("+FormatInt(info.TextureDimensions.X)+", "+FormatInt(info.TextureDimensions.Y)+
			") are larger than the current window region ("+FormatInt(scissor.Width())+", "+FormatInt(scissor.Height())+"). Results may be clipped.")
	}
	rm.PushLayer()
	c.renderable.backgroundBorderGeometry.RenderPlain(info.ElementOffsetInTexture)
	for index := len(shadows) - 1; index >= 0; index-- {
		shadow := shadows[index]
		shadowOffset := Vector2f{shadow.OffsetX.Number, shadow.OffsetY.Number}
		inset := shadow.Inset
		spread := shadow.SpreadDistance.Number
		blurRadius := shadow.BlurRadius.Number
		spreadRadii := info.BorderRadius
		for i := 0; i < 4; i++ {
			radius := spreadRadii[i]
			var spreadFactor float32 = 1.0
			if inset {
				spreadFactor = -1.0
			}
			if radius < spread {
				ratioMinusOne := (radius / spread) - 1.0
				spreadFactor = spreadFactor * (1.0 + ratioMinusOne*ratioMinusOne*ratioMinusOne)
			}
			spreadRadii[i] = MathMax(radius+spreadFactor*spread, 0)
		}
		meshShadow := Mesh{}
		for i := 0; i < numBoxes; i++ {
			signedSpread := spread
			if inset {
				signedSpread = -spread
			}
			renderBox := info.BorderRenderBoxes[i]
			if inset {
				renderBox = info.PaddingRenderBoxes[i]
			}
			renderBox.FillSize = MathMaxVector(renderBox.FillSize.Add(Vector2f{2.0 * signedSpread, 2.0 * signedSpread}), Vector2f{0.001, 0.001})
			renderBox.BorderRadius = spreadRadii
			renderBox.BorderOffset = renderBox.BorderOffset.Sub(Vector2f{signedSpread, signedSpread})
			MeshGenerateBackground(&meshShadow, renderBox, shadow.Color)
		}
		blur := CompiledFilter{}
		if blurRadius >= 0.5 {
			params := map[string]Variant{}
			params["sigma"] = VariantFloat(0.5 * blurRadius)
			blur = rm.CompileFilter("blur", params)
			if blur.Valid() {
				rm.PushLayer()
			}
		}
		geometryShadow := rm.MakeGeometry(meshShadow)
		geometryPadding := NewGeometry()
		geometryPaddingBorder := NewGeometry()
		if inset {
			rm.SetClipMaskGeometry(ClipMaskOperationSetInverse, &geometryShadow, shadowOffset.Add(info.ElementOffsetInTexture))
			for v := 0; v < len(meshPadding.Vertices); v++ {
				meshPadding.Vertices[v].Colour = shadow.Color
			}
			geometryPadding = rm.MakeGeometry(copyMesh(meshPadding))
			geometryPadding.RenderPlain(info.ElementOffsetInTexture)
			rm.SetClipMaskGeometry(ClipMaskOperationSet, &geometryPadding, info.ElementOffsetInTexture)
		} else {
			geometryPaddingBorder = rm.MakeGeometry(copyMesh(meshPaddingBorder))
			rm.SetClipMaskGeometry(ClipMaskOperationSetInverse, &geometryPaddingBorder, info.ElementOffsetInTexture)
			geometryShadow.RenderPlain(shadowOffset.Add(info.ElementOffsetInTexture))
		}
		if blur.Valid() {
			filters := blur.AddHandleTo([]CompiledFilterHandle{})
			rm.CompositeLayers(rm.GetTopLayer(), rm.GetNextLayer(), BlendModeBlend, filters)
			rm.PopLayer()
			blur.Release()
		}
		// End of scope in the C++: the per-shadow geometry is released.
		geometryShadow.Release(GeometryReleaseReturnMesh)
		geometryPadding.Release(GeometryReleaseReturnMesh)
		geometryPaddingBorder.Release(GeometryReleaseReturnMesh)
	}
	textureInterface.SaveLayerAsTexture()
	rm.PopLayer()
	rm.SetState(initialState)
	return true
}

func geometryBoxShadowGenerateTexture(renderable *BoxShadowRenderable, rm *RenderManager, info *BoxShadowGeometryInfo) {
	mesh := renderable.backgroundBorderGeometry.Release(GeometryReleaseClearMesh)
	for i := 0; i < len(info.PaddingRenderBoxes); i++ {
		MeshGenerateBackgroundBorder(&mesh, info.PaddingRenderBoxes[i], info.BackgroundColor, info.BorderColors)
	}
	renderable.backgroundBorderGeometry = rm.MakeGeometry(mesh)
	callback := &boxShadowTextureCallback{info: info, renderable: renderable}
	renderable.texture = rm.MakeCallbackTexture(callback)
}
