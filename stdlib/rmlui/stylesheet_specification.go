// Port of RmlUi Source/Core/StyleSheetSpecification.cpp and
// Include/RmlUi/Core/StyleSheetSpecification.h.
package rmlui

// StyleSheetSpecification is Rml::StyleSheetSpecification, the registry of
// every RCSS property, shorthand, and value parser.
type StyleSheetSpecification struct {
	properties *PropertySpecification
	parsers    map[string]PropertyParser
}

var styleSheetSpecificationInstance *StyleSheetSpecification

func styleSpec() *StyleSheetSpecification { return styleSheetSpecificationInstance }

// StyleSheetSpecificationInitialise is StyleSheetSpecification::Initialise.
func StyleSheetSpecificationInitialise() {
	if styleSheetSpecificationInstance != nil {
		return
	}
	s := &StyleSheetSpecification{parsers: map[string]PropertyParser{}}
	s.properties = NewPropertySpecification(PropertyIdMaxNumIds, 2*ShorthandIdNumDefinedIds)
	styleSheetSpecificationInstance = s
	s.registerDefaultParsers()
	s.registerDefaultProperties()
}

// StyleSheetSpecificationShutdown is StyleSheetSpecification::Shutdown.
func StyleSheetSpecificationShutdown() { styleSheetSpecificationInstance = nil }

// RegisterPropertyParser is StyleSheetSpecification::RegisterParser.
func RegisterPropertyParser(name string, parser PropertyParser) bool {
	s := styleSpec()
	if _, ok := s.parsers[name]; ok {
		LogMessage(LogWarning, "Parser with name "+name+" already exists!")
		return false
	}
	s.parsers[name] = parser
	return true
}

// GetPropertyParser is StyleSheetSpecification::GetParser.
func GetPropertyParser(name string) PropertyParser {
	s := styleSpec()
	if s == nil {
		return nil
	}
	if p, ok := s.parsers[name]; ok {
		return p
	}
	return nil
}

// RegisterCustomProperty is the public StyleSheetSpecification::
// RegisterProperty(name, default, inherited, forces_layout).
func RegisterCustomProperty(propertyName string, defaultValue string, inherited bool, forcesLayout bool) *PropertyDefinition {
	return styleSpec().properties.RegisterProperty(propertyName, defaultValue, inherited, forcesLayout, PropertyIdInvalid)
}

// RegisterCustomShorthand is StyleSheetSpecification::RegisterShorthand.
func RegisterCustomShorthand(shorthandName string, propertyNames string, shType ShorthandType) ShorthandId {
	return styleSpec().properties.RegisterShorthand(shorthandName, propertyNames, shType, ShorthandIdInvalid)
}

func GetPropertyDefinition(id PropertyId) *PropertyDefinition { return styleSpec().properties.GetProperty(id) }
func GetPropertyDefinitionByName(name string) *PropertyDefinition {
	return styleSpec().properties.GetPropertyByName(name)
}
func GetShorthandDefinition(id ShorthandId) *ShorthandDefinition {
	return styleSpec().properties.GetShorthand(id)
}
func GetShorthandDefinitionByName(name string) *ShorthandDefinition {
	return styleSpec().properties.GetShorthandByName(name)
}
func GetRegisteredProperties() PropertyIdSet { return styleSpec().properties.GetRegisteredProperties() }
func GetRegisteredInheritedProperties() PropertyIdSet {
	return styleSpec().properties.GetRegisteredInheritedProperties()
}
func GetRegisteredPropertiesForcingLayout() PropertyIdSet {
	return styleSpec().properties.GetRegisteredPropertiesForcingLayout()
}

// StyleParsePropertyDeclaration is StyleSheetSpecification::
// ParsePropertyDeclaration.
func StyleParsePropertyDeclaration(dictionary *PropertyDictionary, propertyName string, propertyValue string) bool {
	return styleSpec().properties.ParsePropertyDeclaration(dictionary, propertyName, propertyValue)
}

func GetPropertyId(name string) PropertyId     { return styleSpec().properties.GetPropertyId(name) }
func GetShorthandId(name string) ShorthandId   { return styleSpec().properties.GetShorthandId(name) }
func GetPropertyName(id PropertyId) string     { return styleSpec().properties.GetPropertyName(id) }
func GetShorthandName(id ShorthandId) string   { return styleSpec().properties.GetShorthandName(id) }
func GetPropertySpecification() *PropertySpecification { return styleSpec().properties }

// GetShorthandUnderlyingProperties returns every property a shorthand sets,
// recursing through nested shorthands.
func GetShorthandUnderlyingProperties(id ShorthandId) PropertyIdSet {
	result := PropertyIdSet{}
	sh := styleSpec().properties.GetShorthand(id)
	if sh == nil {
		return result
	}
	rng1 := sh.Items
	for _, item := range rng1 {
		if item.Type == ShorthandItemTypeProperty {
			result.Insert(item.PropertyId)
		} else if item.Type == ShorthandItemTypeShorthand {
			result.UnionWith(GetShorthandUnderlyingProperties(item.ShorthandId))
		}
	}
	return result
}

func (s *StyleSheetSpecification) registerDefaultParsers() {
	colour := &PropertyParserColour{}
	length := NewPropertyParserNumber(UnitLENGTH, UnitPX)
	RegisterPropertyParser("number", NewPropertyParserNumber(UnitNUMBER, UnitUNKNOWN))
	RegisterPropertyParser("length", length)
	RegisterPropertyParser("length_percent", NewPropertyParserNumber(UnitLENGTH_PERCENT, UnitPX))
	RegisterPropertyParser("number_percent", NewPropertyParserNumber(UnitNUMBER_PERCENT, UnitUNKNOWN))
	RegisterPropertyParser("number_length_percent", NewPropertyParserNumber(UnitNUMBER_LENGTH_PERCENT, UnitPX))
	RegisterPropertyParser("angle", NewPropertyParserNumber(UnitANGLE, UnitRAD))
	RegisterPropertyParser("keyword", &PropertyParserKeyword{})
	RegisterPropertyParser("string", &PropertyParserString{})
	RegisterPropertyParser("animation", NewPropertyParserAnimation(animationParser))
	RegisterPropertyParser("transition", NewPropertyParserAnimation(transitionParser))
	RegisterPropertyParser("color", colour)
	RegisterPropertyParser("color_stop_list", &PropertyParserColorStopList{colour: colour})
	RegisterPropertyParser("decorator", &PropertyParserDecorator{})
	RegisterPropertyParser("filter", &PropertyParserFilter{})
	RegisterPropertyParser("font_effect", &PropertyParserFontEffect{})
	RegisterPropertyParser("transform", &PropertyParserTransform{})
	RegisterPropertyParser("ratio", &PropertyParserRatio{})
	RegisterPropertyParser("resolution", NewPropertyParserNumber(UnitX, UnitUNKNOWN))
	RegisterPropertyParser("box_shadow", &PropertyParserBoxShadow{colour: colour, length: length})
}

func (s *StyleSheetSpecification) reg(id PropertyId, name string, def string, inherited bool, forcesLayout bool) *PropertyDefinition {
	return s.properties.RegisterProperty(name, def, inherited, forcesLayout, id)
}

func (s *StyleSheetSpecification) shorthand(id ShorthandId, name string, props string, shType ShorthandType) {
	s.properties.RegisterShorthand(name, props, shType, id)
}

func (s *StyleSheetSpecification) registerDefaultProperties() {
	const cbWidth = RelativeTargetContainingBlockWidth
	const cbHeight = RelativeTargetContainingBlockHeight

	s.reg(PropertyIdMarginTop, "margin-top", "0px", false, true).AddParser("keyword", "auto").AddParser("length_percent", "").SetRelativeTarget(cbWidth)
	s.reg(PropertyIdMarginRight, "margin-right", "0px", false, true).AddParser("keyword", "auto").AddParser("length_percent", "").SetRelativeTarget(cbWidth)
	s.reg(PropertyIdMarginBottom, "margin-bottom", "0px", false, true).AddParser("keyword", "auto").AddParser("length_percent", "").SetRelativeTarget(cbWidth)
	s.reg(PropertyIdMarginLeft, "margin-left", "0px", false, true).AddParser("keyword", "auto").AddParser("length_percent", "").SetRelativeTarget(cbWidth)
	s.shorthand(ShorthandIdMargin, "margin", "margin-top, margin-right, margin-bottom, margin-left", ShorthandTypeBox)

	s.reg(PropertyIdPaddingTop, "padding-top", "0px", false, true).AddParser("length_percent", "").SetRelativeTarget(cbWidth)
	s.reg(PropertyIdPaddingRight, "padding-right", "0px", false, true).AddParser("length_percent", "").SetRelativeTarget(cbWidth)
	s.reg(PropertyIdPaddingBottom, "padding-bottom", "0px", false, true).AddParser("length_percent", "").SetRelativeTarget(cbWidth)
	s.reg(PropertyIdPaddingLeft, "padding-left", "0px", false, true).AddParser("length_percent", "").SetRelativeTarget(cbWidth)
	s.shorthand(ShorthandIdPadding, "padding", "padding-top, padding-right, padding-bottom, padding-left", ShorthandTypeBox)

	s.reg(PropertyIdBorderTopWidth, "border-top-width", "0px", false, true).AddParser("length", "")
	s.reg(PropertyIdBorderRightWidth, "border-right-width", "0px", false, true).AddParser("length", "")
	s.reg(PropertyIdBorderBottomWidth, "border-bottom-width", "0px", false, true).AddParser("length", "")
	s.reg(PropertyIdBorderLeftWidth, "border-left-width", "0px", false, true).AddParser("length", "")
	s.shorthand(ShorthandIdBorderWidth, "border-width", "border-top-width, border-right-width, border-bottom-width, border-left-width", ShorthandTypeBox)

	s.reg(PropertyIdBorderTopColor, "border-top-color", "black", false, false).AddParser("color", "")
	s.reg(PropertyIdBorderRightColor, "border-right-color", "black", false, false).AddParser("color", "")
	s.reg(PropertyIdBorderBottomColor, "border-bottom-color", "black", false, false).AddParser("color", "")
	s.reg(PropertyIdBorderLeftColor, "border-left-color", "black", false, false).AddParser("color", "")
	s.shorthand(ShorthandIdBorderColor, "border-color", "border-top-color, border-right-color, border-bottom-color, border-left-color", ShorthandTypeBox)

	s.shorthand(ShorthandIdBorderTop, "border-top", "border-top-width, border-top-color", ShorthandTypeFallThrough)
	s.shorthand(ShorthandIdBorderRight, "border-right", "border-right-width, border-right-color", ShorthandTypeFallThrough)
	s.shorthand(ShorthandIdBorderBottom, "border-bottom", "border-bottom-width, border-bottom-color", ShorthandTypeFallThrough)
	s.shorthand(ShorthandIdBorderLeft, "border-left", "border-left-width, border-left-color", ShorthandTypeFallThrough)
	s.shorthand(ShorthandIdBorder, "border", "border-top, border-right, border-bottom, border-left", ShorthandTypeRecursiveRepeat)

	s.reg(PropertyIdBorderTopLeftRadius, "border-top-left-radius", "0px", false, false).AddParser("length", "")
	s.reg(PropertyIdBorderTopRightRadius, "border-top-right-radius", "0px", false, false).AddParser("length", "")
	s.reg(PropertyIdBorderBottomRightRadius, "border-bottom-right-radius", "0px", false, false).AddParser("length", "")
	s.reg(PropertyIdBorderBottomLeftRadius, "border-bottom-left-radius", "0px", false, false).AddParser("length", "")
	s.shorthand(ShorthandIdBorderRadius, "border-radius", "border-top-left-radius, border-top-right-radius, border-bottom-right-radius, border-bottom-left-radius", ShorthandTypeBox)

	s.reg(PropertyIdDisplay, "display", "inline", false, true).AddParser("keyword", "none, block, inline, inline-block, flow-root, flex, inline-flex, table, inline-table, table-row, table-row-group, table-column, table-column-group, table-cell")
	s.reg(PropertyIdPosition, "position", "static", false, true).AddParser("keyword", "static, relative, absolute, fixed")
	s.reg(PropertyIdTop, "top", "auto", false, false).AddParser("keyword", "auto").AddParser("length_percent", "").SetRelativeTarget(cbHeight)
	s.reg(PropertyIdRight, "right", "auto", false, false).AddParser("keyword", "auto").AddParser("length_percent", "").SetRelativeTarget(cbWidth)
	s.reg(PropertyIdBottom, "bottom", "auto", false, false).AddParser("keyword", "auto").AddParser("length_percent", "").SetRelativeTarget(cbHeight)
	s.reg(PropertyIdLeft, "left", "auto", false, false).AddParser("keyword", "auto").AddParser("length_percent", "").SetRelativeTarget(cbWidth)
	s.shorthand(ShorthandIdInset, "inset", "top, right, bottom, left", ShorthandTypeBox)

	s.reg(PropertyIdFloat, "float", "none", false, true).AddParser("keyword", "none, left, right")
	s.reg(PropertyIdClear, "clear", "none", false, true).AddParser("keyword", "none, left, right, both")

	s.reg(PropertyIdBoxSizing, "box-sizing", "content-box", false, true).AddParser("keyword", "content-box, border-box")

	s.reg(PropertyIdZIndex, "z-index", "auto", false, false).AddParser("keyword", "auto").AddParser("number", "")

	s.reg(PropertyIdWidth, "width", "auto", false, true).AddParser("keyword", "auto").AddParser("length_percent", "").SetRelativeTarget(cbWidth)
	s.reg(PropertyIdMinWidth, "min-width", "0px", false, true).AddParser("length_percent", "").SetRelativeTarget(cbWidth)
	s.reg(PropertyIdMaxWidth, "max-width", "none", false, true).AddParser("keyword", "none").AddParser("length_percent", "").SetRelativeTarget(cbWidth)

	s.reg(PropertyIdHeight, "height", "auto", false, true).AddParser("keyword", "auto").AddParser("length_percent", "").SetRelativeTarget(cbHeight)
	s.reg(PropertyIdMinHeight, "min-height", "0px", false, true).AddParser("length_percent", "").SetRelativeTarget(cbHeight)
	s.reg(PropertyIdMaxHeight, "max-height", "none", false, true).AddParser("keyword", "none").AddParser("length_percent", "").SetRelativeTarget(cbHeight)

	s.reg(PropertyIdLineHeight, "line-height", "1.2", true, true).AddParser("number_length_percent", "").SetRelativeTarget(RelativeTargetFontSize)
	s.reg(PropertyIdVerticalAlign, "vertical-align", "baseline", false, true).AddParser("keyword", "baseline, middle, sub, super, text-top, text-bottom, top, center, bottom").AddParser("length_percent", "").SetRelativeTarget(RelativeTargetLineHeight)

	s.reg(PropertyIdOverflowX, "overflow-x", "visible", false, true).AddParser("keyword", "visible, hidden, auto, scroll")
	s.reg(PropertyIdOverflowY, "overflow-y", "visible", false, true).AddParser("keyword", "visible, hidden, auto, scroll")
	s.shorthand(ShorthandIdOverflow, "overflow", "overflow-x, overflow-y", ShorthandTypeReplicate)
	s.reg(PropertyIdClip, "clip", "auto", false, false).AddParser("keyword", "auto, none, always").AddParser("number", "")
	s.reg(PropertyIdVisibility, "visibility", "visible", false, false).AddParser("keyword", "visible, hidden")
	s.reg(PropertyIdTextOverflow, "text-overflow", "clip", false, false).AddParser("keyword", "clip, ellipsis").AddParser("string", "")

	s.reg(PropertyIdBackgroundColor, "background-color", "transparent", false, false).AddParser("color", "")
	s.shorthand(ShorthandIdBackground, "background", "background-color", ShorthandTypeFallThrough)

	s.reg(PropertyIdColor, "color", "white", true, false).AddParser("color", "")

	s.reg(PropertyIdCaretColor, "caret-color", "auto", true, false).AddParser("keyword", "auto").AddParser("color", "")

	s.reg(PropertyIdImageColor, "image-color", "white", false, false).AddParser("color", "")
	s.reg(PropertyIdOpacity, "opacity", "1", true, false).AddParser("number", "")

	s.reg(PropertyIdFontFamily, "font-family", "", true, true).AddParser("string", "")
	s.reg(PropertyIdFontStyle, "font-style", "normal", true, true).AddParser("keyword", "normal, italic")
	s.reg(PropertyIdFontWeight, "font-weight", "normal", true, true).AddParser("keyword", "normal=400, bold=700").AddParser("number", "")
	s.reg(PropertyIdFontSize, "font-size", "12px", true, true).AddParser("length", "").AddParser("length_percent", "").SetRelativeTarget(RelativeTargetParentFontSize)
	s.reg(PropertyIdFontKerning, "font-kerning", "auto", true, true).AddParser("keyword", "auto, normal, none")
	s.reg(PropertyIdLetterSpacing, "letter-spacing", "normal", true, true).AddParser("keyword", "normal").AddParser("length", "")
	s.shorthand(ShorthandIdFont, "font", "font-style, font-weight, font-size, font-family", ShorthandTypeFallThrough)

	s.reg(PropertyIdTextAlign, "text-align", "left", true, true).AddParser("keyword", "left, right, center, justify")
	s.reg(PropertyIdTextDecoration, "text-decoration", "none", true, false).AddParser("keyword", "none, underline, overline, line-through")
	s.reg(PropertyIdTextTransform, "text-transform", "none", true, true).AddParser("keyword", "none, capitalize, uppercase, lowercase")
	s.reg(PropertyIdWhiteSpace, "white-space", "normal", true, true).AddParser("keyword", "normal, pre, nowrap, pre-wrap, pre-line")
	s.reg(PropertyIdWordBreak, "word-break", "normal", true, true).AddParser("keyword", "normal, break-all, break-word")

	s.reg(PropertyIdRowGap, "row-gap", "0px", false, true).AddParser("length_percent", "").SetRelativeTarget(cbHeight)
	s.reg(PropertyIdColumnGap, "column-gap", "0px", false, true).AddParser("length_percent", "").SetRelativeTarget(cbHeight)
	s.shorthand(ShorthandIdGap, "gap", "row-gap, column-gap", ShorthandTypeReplicate)

	s.reg(PropertyIdCursor, "cursor", "", true, false).AddParser("string", "")

	s.reg(PropertyIdDrag, "drag", "none", false, false).AddParser("keyword", "none, drag, drag-drop, block, clone")
	s.reg(PropertyIdTabIndex, "tab-index", "none", false, false).AddParser("keyword", "none, auto")
	s.reg(PropertyIdFocus, "focus", "auto", true, false).AddParser("keyword", "none, auto")

	s.reg(PropertyIdNavUp, "nav-up", "none", false, false).AddParser("keyword", "none, auto, horizontal, vertical, tree-order").AddParser("string", "")
	s.reg(PropertyIdNavRight, "nav-right", "none", false, false).AddParser("keyword", "none, auto, horizontal, vertical, tree-order").AddParser("string", "")
	s.reg(PropertyIdNavDown, "nav-down", "none", false, false).AddParser("keyword", "none, auto, horizontal, vertical, tree-order").AddParser("string", "")
	s.reg(PropertyIdNavLeft, "nav-left", "none", false, false).AddParser("keyword", "none, auto, horizontal, vertical, tree-order").AddParser("string", "")
	s.shorthand(ShorthandIdNav, "nav", "nav-up, nav-right, nav-down, nav-left", ShorthandTypeBox)

	s.reg(PropertyIdScrollbarMargin, "scrollbar-margin", "0", false, false).AddParser("length", "")
	s.reg(PropertyIdOverscrollBehavior, "overscroll-behavior", "auto", false, false).AddParser("keyword", "auto, contain")
	s.reg(PropertyIdPointerEvents, "pointer-events", "auto", true, false).AddParser("keyword", "none, auto")

	s.reg(PropertyIdPerspective, "perspective", "none", false, false).AddParser("keyword", "none").AddParser("length", "")
	s.reg(PropertyIdPerspectiveOriginX, "perspective-origin-x", "50%", false, false).AddParser("keyword", "left, center, right").AddParser("length_percent", "")
	s.reg(PropertyIdPerspectiveOriginY, "perspective-origin-y", "50%", false, false).AddParser("keyword", "top, center, bottom").AddParser("length_percent", "")
	s.shorthand(ShorthandIdPerspectiveOrigin, "perspective-origin", "perspective-origin-x, perspective-origin-y", ShorthandTypeFallThrough)
	s.reg(PropertyIdTransform, "transform", "none", false, false).AddParser("transform", "")
	s.reg(PropertyIdTransformOriginX, "transform-origin-x", "50%", false, false).AddParser("keyword", "left, center, right").AddParser("length_percent", "")
	s.reg(PropertyIdTransformOriginY, "transform-origin-y", "50%", false, false).AddParser("keyword", "top, center, bottom").AddParser("length_percent", "")
	s.reg(PropertyIdTransformOriginZ, "transform-origin-z", "0", false, false).AddParser("length", "")
	s.shorthand(ShorthandIdTransformOrigin, "transform-origin", "transform-origin-x, transform-origin-y, transform-origin-z", ShorthandTypeFallThrough)

	s.reg(PropertyIdTransition, "transition", "none", false, false).AddParser("transition", "")
	s.reg(PropertyIdAnimation, "animation", "none", false, false).AddParser("animation", "")

	s.reg(PropertyIdDecorator, "decorator", "", false, false).AddParser("decorator", "")
	s.reg(PropertyIdMaskImage, "mask-image", "", false, false).AddParser("decorator", "")
	s.reg(PropertyIdFontEffect, "font-effect", "", true, false).AddParser("font_effect", "")

	s.reg(PropertyIdFilter, "filter", "", false, false).AddParser("filter", "filter")
	s.reg(PropertyIdBackdropFilter, "backdrop-filter", "", false, false).AddParser("filter", "")

	s.reg(PropertyIdBoxShadow, "box-shadow", "none", false, false).AddParser("box_shadow", "")

	s.reg(PropertyIdFillImage, "fill-image", "", false, false).AddParser("string", "")

	s.reg(PropertyIdAlignContent, "align-content", "stretch", false, true).AddParser("keyword", "flex-start, flex-end, center, space-between, space-around, space-evenly, stretch")
	s.reg(PropertyIdAlignItems, "align-items", "stretch", false, true).AddParser("keyword", "flex-start, flex-end, center, baseline, stretch")
	s.reg(PropertyIdAlignSelf, "align-self", "auto", false, true).AddParser("keyword", "auto, flex-start, flex-end, center, baseline, stretch")

	s.reg(PropertyIdFlexBasis, "flex-basis", "auto", false, true).AddParser("keyword", "auto").AddParser("length_percent", "")
	s.reg(PropertyIdFlexDirection, "flex-direction", "row", false, true).AddParser("keyword", "row, row-reverse, column, column-reverse")

	s.reg(PropertyIdFlexGrow, "flex-grow", "0", false, true).AddParser("number", "")
	s.reg(PropertyIdFlexShrink, "flex-shrink", "1", false, true).AddParser("number", "")
	s.reg(PropertyIdFlexWrap, "flex-wrap", "nowrap", false, true).AddParser("keyword", "nowrap, wrap, wrap-reverse")
	s.reg(PropertyIdJustifyContent, "justify-content", "flex-start", false, true).AddParser("keyword", "flex-start, flex-end, center, space-between, space-around, space-evenly")

	s.shorthand(ShorthandIdFlex, "flex", "flex-grow, flex-shrink, flex-basis", ShorthandTypeFlex)
	s.shorthand(ShorthandIdFlexFlow, "flex-flow", "flex-direction, flex-wrap", ShorthandTypeFallThrough)

	s.reg(PropertyIdRmlUi_Language, "-rmlui-language", "", true, true).AddParser("string", "")
	s.reg(PropertyIdRmlUi_Direction, "-rmlui-direction", "auto", true, true).AddParser("keyword", "auto, ltr, rtl")

	if !s.properties.shorthandMap.AssertAllInserted(ShorthandIdNumDefinedIds) {
		LogMessage(LogError, "Missing specification for one or more Shorthand IDs.")
	}
	if !s.properties.propertyMap.AssertAllInserted(PropertyIdNumDefinedIds) {
		LogMessage(LogError, "Missing specification for one or more Property IDs.")
	}
}
