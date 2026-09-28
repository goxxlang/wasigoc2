// Port of RmlUi Source/Core/ElementEffects.cpp, Decorator.cpp, Filter.cpp,
// EffectSpecification.cpp, PropertyParserDecorator.cpp,
// PropertyParserFilter.cpp, PropertyParserFontEffect.cpp and the headers
// Decorator.h, Filter.h, EffectSpecification.h, StyleSheetTypes.h (the
// declaration lists).
package rmlui

// DecoratorData is the per-element payload a decorator stores.
type DecoratorData struct {
	Value any
}

// DecoratorDataHandle is Rml::DecoratorDataHandle. Nil means generation failed.
type DecoratorDataHandle = *DecoratorData

// Decorator is Rml::Decorator.
type Decorator interface {
	GenerateElementData(element *Element, paintArea BoxArea) DecoratorDataHandle
	ReleaseElementData(data DecoratorDataHandle)
	RenderElement(element *Element, data DecoratorDataHandle)
}

// DecoratorTextures is the texture bookkeeping of the Decorator base class
// (AddTexture/GetNumTextures/GetTexture), for decorators to hold.
type DecoratorTextures struct {
	first      Texture
	additional []Texture
}

func (d *DecoratorTextures) AddTexture(texture Texture) int {
	if !texture.Valid() {
		return -1
	}
	if !d.first.Valid() {
		d.first = texture
	}
	if d.first.Equals(texture) {
		return 0
	}
	for i := 0; i < len(d.additional); i++ {
		if d.additional[i].Equals(texture) {
			return i + 1
		}
	}
	d.additional = append(d.additional, texture)
	return len(d.additional)
}

func (d *DecoratorTextures) GetNumTextures() int {
	n := len(d.additional)
	if d.first.Valid() {
		n++
	}
	return n
}

func (d *DecoratorTextures) GetTexture(index int) Texture {
	if index == 0 {
		return d.first
	}
	index--
	if index < 0 || index >= len(d.additional) {
		return Texture{}
	}
	return d.additional[index]
}

// EffectSpecification is Rml::EffectSpecification: the property
// specification of a decorator, filter, or font-effect type.
type EffectSpecification struct {
	properties *PropertySpecification
}

func NewEffectSpecification() *EffectSpecification {
	return &EffectSpecification{properties: NewPropertySpecification(10, 10)}
}

func (e *EffectSpecification) GetPropertySpecification() *PropertySpecification { return e.properties }

func (e *EffectSpecification) RegisterProperty(name string, defaultValue string) *PropertyDefinition {
	return e.properties.RegisterProperty(name, defaultValue, false, false, PropertyIdInvalid)
}

func (e *EffectSpecification) RegisterShorthand(name string, propertyNames string, shType ShorthandType) ShorthandId {
	return e.properties.RegisterShorthand(name, propertyNames, shType, ShorthandIdInvalid)
}

// DecoratorInstancer is Rml::DecoratorInstancer.
type DecoratorInstancer interface {
	GetPropertySpecification() *PropertySpecification
	InstanceDecorator(name string, properties *PropertyDictionary, iface *DecoratorInstancerInterface) Decorator
}

// DecoratorInstancerInterface is Rml::DecoratorInstancerInterface.
type DecoratorInstancerInterface struct {
	renderManager  *RenderManager
	styleSheet     *StyleSheet
	propertySource *PropertySource
}

func (d *DecoratorInstancerInterface) GetSprite(name string) *Sprite { return d.styleSheet.GetSprite(name) }

func (d *DecoratorInstancerInterface) GetTexture(filename string) Texture {
	if d.propertySource == nil {
		LogMessage(LogWarning, "Texture name '"+filename+"' in decorator could not be loaded, no property source available.")
		return Texture{}
	}
	return d.renderManager.LoadTexture(filename, d.propertySource.Path)
}

func (d *DecoratorInstancerInterface) GetRenderManager() *RenderManager { return d.renderManager }

// Filter is Rml::Filter.
type Filter interface {
	CompileFilter(element *Element) CompiledFilter
	ExtendInkOverflow(element *Element, overflow *Rectanglef)
}

// FilterInstancer is Rml::FilterInstancer.
type FilterInstancer interface {
	GetPropertySpecification() *PropertySpecification
	InstanceFilter(name string, properties *PropertyDictionary) Filter
}

// DecoratorDeclaration is Rml::DecoratorDeclaration.
type DecoratorDeclaration struct {
	Type       string
	Instancer  DecoratorInstancer
	Properties *PropertyDictionary
	PaintArea  BoxArea
}

// DecoratorDeclarationList is Rml::DecoratorDeclarationList.
type DecoratorDeclarationList struct {
	List  []DecoratorDeclaration
	Value string
}

func (d *DecoratorDeclarationList) variantString() string { return effectListToString(d.Value, d.decls(), ", ") }

func (d *DecoratorDeclarationList) variantEquals(other VariantPayload) bool {
	o, ok := other.(*DecoratorDeclarationList)
	return ok && o == d
}

type effectDeclarationView struct {
	typ        string
	spec       *PropertySpecification
	properties *PropertyDictionary
	paintArea  BoxArea
	isDecorator bool
}

func (d *DecoratorDeclarationList) decls() []effectDeclarationView {
	out := []effectDeclarationView{}
	list := d.List
	for _, decl := range list {
		var spec *PropertySpecification
		if decl.Instancer != nil {
			spec = decl.Instancer.GetPropertySpecification()
		}
		out = append(out, effectDeclarationView{decl.Type, spec, decl.Properties, decl.PaintArea, true})
	}
	return out
}

// FilterDeclaration is Rml::FilterDeclaration.
type FilterDeclaration struct {
	Type       string
	Instancer  FilterInstancer
	Properties *PropertyDictionary
}

// FilterDeclarationList is Rml::FilterDeclarationList.
type FilterDeclarationList struct {
	List  []FilterDeclaration
	Value string
}

func (f *FilterDeclarationList) variantString() string {
	out := []effectDeclarationView{}
	list := f.List
	for _, decl := range list {
		var spec *PropertySpecification
		if decl.Instancer != nil {
			spec = decl.Instancer.GetPropertySpecification()
		}
		out = append(out, effectDeclarationView{decl.Type, spec, decl.Properties, BoxAreaAuto, false})
	}
	return effectListToString(f.Value, out, " ")
}

func (f *FilterDeclarationList) variantEquals(other VariantPayload) bool {
	o, ok := other.(*FilterDeclarationList)
	return ok && o == f
}

func areaToString(area BoxArea) string {
	switch area {
	case BoxAreaBorder:
		return "border-box"
	case BoxAreaPadding:
		return "padding-box"
	case BoxAreaContent:
		return "content-box"
	}
	return ""
}

// effectListToString is ConvertEffectToString in TypeConverter.cpp.
func effectListToString(value string, list []effectDeclarationView, separator string) string {
	if len(list) == 0 {
		return "none"
	}
	if value != "" {
		return value
	}
	dest := ""
	for i := 0; i < len(list); i++ {
		d := list[i]
		dest = dest + d.typ
		if d.spec != nil {
			dest = dest + "(" + d.spec.PropertiesToString(d.properties, false, ' ') + ")"
		}
		if d.isDecorator && d.paintArea >= BoxAreaBorder && d.paintArea <= BoxAreaPadding {
			dest = dest + " " + areaToString(d.paintArea)
		}
		if i < len(list)-1 {
			dest = dest + separator
		}
	}
	return dest
}

// FontEffects is Rml::FontEffects.
type FontEffects struct {
	List  []FontEffect
	Value string
}

func (f *FontEffects) variantString() string {
	if len(f.List) == 0 {
		return "none"
	}
	return f.Value
}

func (f *FontEffects) variantEquals(other VariantPayload) bool {
	o, ok := other.(*FontEffects)
	return ok && o == f
}

// ---- property parsers ----

var decoratorAreaKeywords = map[string]BoxArea{
	"border-box":  BoxAreaBorder,
	"padding-box": BoxAreaPadding,
	"content-box": BoxAreaContent,
}

// PropertyParserDecorator is Rml::PropertyParserDecorator.
type PropertyParserDecorator struct{}

func (p *PropertyParserDecorator) ParseValue(property *Property, value string, parameters map[string]int) bool {
	if value == "" || value == "none" {
		property.Value = VariantPointer(VariantDECORATORSPTR, nil)
		property.Unit = UnitDECORATOR
		return true
	}
	decoratorStrings := StringExpandNested(value, ',', '(', ')', false)
	decorators := &DecoratorDeclarationList{Value: value}
	for _, decoratorString := range decoratorStrings {
		open := indexByte(decoratorString, '(')
		close := lastIndexByte(decoratorString, ')')
		invalidParenthesis := open < 0 || close < 0 || open >= close
		keywordsBegin := close + 1
		if invalidParenthesis {
			keywordsBegin = indexByte(decoratorString, ' ')
		}
		paintArea := BoxAreaAuto
		if keywordsBegin >= 0 && keywordsBegin < len(decoratorString) {
			keywords := StringExpand(decoratorString[keywordsBegin:], ' ', false)
			for _, keyword := range keywords {
				if keyword == "" {
					continue
				}
				area, ok := decoratorAreaKeywords[StringToLower(keyword)]
				if !ok {
					return false
				}
				paintArea = area
			}
		}
		if invalidParenthesis {
			name := decoratorString
			if keywordsBegin >= 0 {
				name = decoratorString[:keywordsBegin]
			}
			decorators.List = append(decorators.List, DecoratorDeclaration{Type: name, Properties: NewPropertyDictionary(), PaintArea: paintArea})
		} else {
			typ := StringStripWhitespace(decoratorString[:open])
			instancer := FactoryGetDecoratorInstancer(typ)
			if instancer == nil {
				LogMessage(LogWarning, "Decorator type '"+typ+"' not found.")
				return false
			}
			shorthand := decoratorString[open+1 : close]
			spec := instancer.GetPropertySpecification()
			properties := NewPropertyDictionary()
			if !spec.ParsePropertyDeclaration(properties, "decorator", shorthand) {
				if StringStripWhitespace(shorthand) != "" {
					return false
				}
			}
			spec.SetPropertyDefaults(properties)
			decorators.List = append(decorators.List, DecoratorDeclaration{Type: typ, Instancer: instancer, Properties: properties, PaintArea: paintArea})
		}
	}
	if len(decorators.List) == 0 {
		return false
	}
	property.Value = VariantPointer(VariantDECORATORSPTR, decorators)
	property.Unit = UnitDECORATOR
	return true
}

// PropertyParserFilter is Rml::PropertyParserFilter.
type PropertyParserFilter struct{}

func (p *PropertyParserFilter) ParseValue(property *Property, value string, parameters map[string]int) bool {
	if value == "" || value == "none" {
		property.Value = VariantPointer(VariantFILTERSPTR, nil)
		property.Unit = UnitFILTER
		return true
	}
	filterStrings := StringExpandNested(value, ' ', '(', ')', true)
	filters := &FilterDeclarationList{Value: value}
	for _, filterString := range filterStrings {
		open := indexByte(filterString, '(')
		close := lastIndexByte(filterString, ')')
		if open < 0 || close < 0 || open >= close {
			LogMessage(LogWarning, "Invalid syntax for font-effect '"+filterString+"'.")
			return false
		}
		typ := StringStripWhitespace(filterString[:open])
		instancer := FactoryGetFilterInstancer(typ)
		if instancer == nil {
			LogMessage(LogWarning, "Filter type '"+typ+"' not found.")
			return false
		}
		shorthand := filterString[open+1 : close]
		spec := instancer.GetPropertySpecification()
		properties := NewPropertyDictionary()
		if !spec.ParsePropertyDeclaration(properties, "filter", shorthand) {
			if StringStripWhitespace(shorthand) != "" {
				return false
			}
		}
		spec.SetPropertyDefaults(properties)
		filters.List = append(filters.List, FilterDeclaration{Type: typ, Instancer: instancer, Properties: properties})
	}
	if len(filters.List) == 0 {
		return false
	}
	property.Value = VariantPointer(VariantFILTERSPTR, filters)
	property.Unit = UnitFILTER
	return true
}

// PropertyParserFontEffect is Rml::PropertyParserFontEffect.
type PropertyParserFontEffect struct{}

func (p *PropertyParserFontEffect) ParseValue(property *Property, value string, parameters map[string]int) bool {
	if value == "" || value == "none" {
		property.Value = NewVariant()
		property.Unit = UnitUNKNOWN
		return true
	}
	effects := &FontEffects{Value: value}
	effectStrings := StringExpandNested(value, ',', '(', ')', false)
	for _, effectString := range effectStrings {
		open := indexByte(effectString, '(')
		close := lastIndexByte(effectString, ')')
		if open < 0 || close < 0 || open >= close {
			LogMessage(LogWarning, "Invalid syntax for font-effect '"+effectString+"'.")
			return false
		}
		typ := StringStripWhitespace(effectString[:open])
		instancer := FactoryGetFontEffectInstancer(typ)
		if instancer == nil {
			LogMessage(LogWarning, "Font-effect type '"+typ+"' not found.")
			return false
		}
		shorthand := effectString[open+1 : close]
		spec := instancer.GetPropertySpecification()
		properties := NewPropertyDictionary()
		if !spec.ParsePropertyDeclaration(properties, "font-effect", shorthand) {
			if StringStripWhitespace(shorthand) != "" {
				LogMessage(LogWarning, "Could not parse font-effect value '"+effectString+"'.")
				return false
			}
		}
		spec.SetPropertyDefaults(properties)
		effect := instancer.InstanceFontEffect(typ, properties)
		if effect == nil {
			LogMessage(LogWarning, "Font-effect '"+effectString+"' could not be instanced.")
			return false
		}
		fingerprint := typ
		props := properties.GetProperties()
		ids := sortedPropertyIds(props)
		for _, id := range ids {
			fingerprint = fingerprint + "|" + props[id].ToString()
		}
		effect.SetFingerprint(fingerprint)
		effects.List = append(effects.List, effect)
	}
	if len(effects.List) == 0 {
		return false
	}
	// Stable partition: back-layer effects before front-layer effects.
	back := []FontEffect{}
	front := []FontEffect{}
	list := effects.List
	for _, e := range list {
		if e.GetLayer() == FontEffectLayerBack {
			back = append(back, e)
		} else {
			front = append(front, e)
		}
	}
	effects.List = append(back, front...)
	property.Value = VariantPointer(VariantFONTEFFECTSPTR, effects)
	property.Unit = UnitFONTEFFECT
	return true
}

// ---- ElementEffects ----

// RenderStage is Rml::RenderStage.
type RenderStage = int

const (
	RenderStageEnter RenderStage = iota
	RenderStageDecoration
	RenderStageExit
)

type decoratorEntry struct {
	decorator     Decorator
	decoratorData DecoratorDataHandle
	paintArea     BoxArea
}

type filterEntry struct {
	filter   Filter
	compiled CompiledFilter
}

// ElementEffects is Rml::ElementEffects: decorators, mask images,
// filters, and backdrop filters of an element.
type ElementEffects struct {
	element          *Element
	decorators       []*decoratorEntry
	maskImages       []*decoratorEntry
	filters          []*filterEntry
	backdropFilters  []*filterEntry
	effectsDirty     bool
	effectsDataDirty bool
}

func NewElementEffects(element *Element) *ElementEffects { return &ElementEffects{element: element} }

func (x *ElementEffects) InstanceEffects() {
	if !x.effectsDirty {
		return
	}
	x.effectsDirty = false
	x.effectsDataDirty = true
	x.releaseEffects()
	rm := x.element.GetRenderManager()
	if rm == nil {
		return
	}
	computed := x.element.GetComputedValues()
	if computed.HasDecorator() || computed.HasMaskImage() {
		sheet := x.element.GetStyleSheet()
		if sheet == nil {
			return
		}
		ids := []PropertyId{PropertyIdDecorator, PropertyIdMaskImage}
		for _, id := range ids {
			property := x.element.GetStyle().GetLocalPropertyWithResolvedVariables(id)
			if property == nil || property.Unit != UnitDECORATOR {
				continue
			}
			declarations, ok := property.Value.Pointer().(*DecoratorDeclarationList)
			if !ok || declarations == nil {
				continue
			}
			source := property.Source
			if source == nil {
				if doc := x.element.GetOwnerDocument(); doc != nil {
					source = &PropertySource{Path: doc.GetSourceURL()}
				}
			}
			list := sheet.InstanceDecorators(rm, declarations, source)
			for i := 0; i < len(list) && i < len(declarations.List); i++ {
				decorator := list[i]
				if decorator == nil {
					continue
				}
				area := declarations.List[i].PaintArea
				if area == BoxAreaAuto {
					if id == PropertyIdDecorator {
						area = BoxAreaPadding
					} else {
						area = BoxAreaBorder
					}
				}
				entry := &decoratorEntry{decorator: decorator, paintArea: area}
				if id == PropertyIdDecorator {
					x.decorators = append(x.decorators, entry)
				} else {
					x.maskImages = append(x.maskImages, entry)
				}
			}
		}
	}
	if computed.HasFilter() || computed.HasBackdropFilter() {
		ids := []PropertyId{PropertyIdFilter, PropertyIdBackdropFilter}
		for _, id := range ids {
			property := x.element.GetStyle().GetLocalPropertyWithResolvedVariables(id)
			if property == nil || property.Unit != UnitFILTER {
				continue
			}
			declarations, ok := property.Value.Pointer().(*FilterDeclarationList)
			if !ok || declarations == nil {
				continue
			}
			list := declarations.List
			for _, declaration := range list {
				filter := declaration.Instancer.InstanceFilter(declaration.Type, declaration.Properties)
				if filter != nil {
					entry := &filterEntry{filter: filter}
					if id == PropertyIdFilter {
						x.filters = append(x.filters, entry)
					} else {
						x.backdropFilters = append(x.backdropFilters, entry)
					}
				} else {
					path := ""
					line := -1
					if property.Source != nil {
						path = property.Source.Path
						line = property.Source.LineNumber
					}
					LogMessage(LogWarning, "Filter '"+declaration.Type+"' in '"+declarations.Value+"' could not be instanced, declared at "+path+":"+FormatInt(line))
				}
			}
		}
	}
}

func (x *ElementEffects) reloadEffectsData() {
	if !x.effectsDataDirty {
		return
	}
	x.effectsDataDirty = false
	failed := false
	lists := [][]*decoratorEntry{x.decorators, x.maskImages}
	for _, list := range lists {
		for _, d := range list {
			old := d.decoratorData
			d.decoratorData = d.decorator.GenerateElementData(x.element, d.paintArea)
			if d.decoratorData == nil {
				failed = true
			}
			if old != nil {
				d.decorator.ReleaseElementData(old)
			}
		}
	}
	if failed {
		LogMessage(LogWarning, "Could not generate decorator element data: "+x.element.GetAddress(false, true))
	}
	compileFailed := false
	filterLists := [][]*filterEntry{x.filters, x.backdropFilters}
	for _, list := range filterLists {
		for _, f := range list {
			f.compiled.Release()
			f.compiled = f.filter.CompileFilter(x.element)
			if !f.compiled.Valid() {
				compileFailed = true
			}
		}
	}
	if compileFailed {
		LogMessage(LogWarning, "Could not compile filter on element: "+x.element.GetAddress(false, true))
	}
}

func (x *ElementEffects) releaseEffects() {
	lists := [][]*decoratorEntry{x.decorators, x.maskImages}
	for _, list := range lists {
		for _, d := range list {
			if d.decoratorData != nil {
				d.decorator.ReleaseElementData(d.decoratorData)
			}
		}
	}
	x.decorators = nil
	x.maskImages = nil
	filterLists := [][]*filterEntry{x.filters, x.backdropFilters}
	for _, list := range filterLists {
		for _, f := range list {
			f.compiled.Release()
		}
	}
	x.filters = nil
	x.backdropFilters = nil
}

// Release frees all effects (~ElementEffects).
func (x *ElementEffects) Release() { x.releaseEffects() }

func (x *ElementEffects) applyClippingRegion(rm *RenderManager, filterId PropertyId) {
	forceBorder := filterId == PropertyIdBackdropFilter
	ElementUtilitiesSetClippingRegion(x.element, forceBorder)
	area := BoxAreaAuto
	if forceBorder {
		area = BoxAreaBorder
	}
	region := ElementUtilitiesGetBoundingBox(x.element, area)
	if filterId == PropertyIdFilter {
		filters := x.filters
		for _, f := range filters {
			f.filter.ExtendInkOverflow(x.element, &region)
		}
	}
	region = MathExpandToPixelGridRect(region)
	scissor := region.ToInt().IntersectIfValid(rm.GetScissorRegion())
	rm.SetScissorRegion(scissor)
}

func (x *ElementEffects) RenderEffects(stage RenderStage) {
	x.InstanceEffects()
	x.reloadEffectsData()
	if len(x.decorators) > 0 && stage == RenderStageDecoration {
		for i := len(x.decorators) - 1; i >= 0; i-- {
			d := x.decorators[i]
			if d.decoratorData != nil {
				d.decorator.RenderElement(x.element, d.decoratorData)
			}
		}
	}
	if len(x.filters) == 0 && len(x.backdropFilters) == 0 && len(x.maskImages) == 0 {
		return
	}
	rm := x.element.GetRenderManager()
	if rm == nil {
		return
	}
	initialScissor := rm.GetScissorRegion()
	if stage == RenderStageEnter {
		backdropSource := rm.GetTopLayer()
		if len(x.filters) > 0 || len(x.maskImages) > 0 {
			rm.PushLayer()
		}
		if len(x.backdropFilters) > 0 {
			backdropDestination := rm.GetTopLayer()
			region := ElementUtilitiesGetBoundingBox(x.element, BoxAreaBorder)
			backdrops := x.backdropFilters
			for _, f := range backdrops {
				f.filter.ExtendInkOverflow(x.element, &region)
			}
			region = MathExpandToPixelGridRect(region)
			rm.SetScissorRegion(region.ToInt())
			rm.PushLayer()
			backdropTemp := rm.GetTopLayer()
			handles := []CompiledFilterHandle{}
			for _, f := range backdrops {
				handles = f.compiled.AddHandleTo(handles)
			}
			rm.CompositeLayers(backdropSource, backdropTemp, BlendModeBlend, handles)
			x.applyClippingRegion(rm, PropertyIdBackdropFilter)
			rm.CompositeLayers(backdropTemp, backdropDestination, BlendModeBlend, []CompiledFilterHandle{})
			rm.PopLayer()
			rm.SetScissorRegion(initialScissor)
		}
	} else if stage == RenderStageExit {
		if len(x.filters) > 0 || len(x.maskImages) > 0 {
			x.applyClippingRegion(rm, PropertyIdFilter)
			maskFilter := CompiledFilter{}
			handles := []CompiledFilterHandle{}
			filters := x.filters
			for _, f := range filters {
				handles = f.compiled.AddHandleTo(handles)
			}
			if len(x.maskImages) > 0 {
				rm.PushLayer()
				for i := len(x.maskImages) - 1; i >= 0; i-- {
					m := x.maskImages[i]
					if m.decoratorData != nil {
						m.decorator.RenderElement(x.element, m.decoratorData)
					}
				}
				maskFilter = rm.SaveLayerAsMaskImage()
				handles = maskFilter.AddHandleTo(handles)
				rm.PopLayer()
			}
			rm.CompositeLayers(rm.GetTopLayer(), rm.GetNextLayer(), BlendModeBlend, handles)
			rm.PopLayer()
			rm.SetScissorRegion(initialScissor)
			maskFilter.Release()
		}
	}
}

func (x *ElementEffects) DirtyEffects()     { x.effectsDirty = true }
func (x *ElementEffects) DirtyEffectsData() { x.effectsDataDirty = true }
