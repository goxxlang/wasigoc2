// Port of RmlUi Include/RmlUi/Core/StyleTypes.h, ComputedValues.h,
// Source/Core/ComputedValues.cpp, and Source/Core/ComputeProperty.cpp.
package rmlui

// ---- Style:: types ----

const (
	LengthPercentageAutoAuto       = 0
	LengthPercentageAutoLength     = 1
	LengthPercentageAutoPercentage = 2
)

// LengthPercentageAuto is Style::LengthPercentageAuto.
type LengthPercentageAuto struct {
	Type  int
	Value float32
}

const (
	LengthPercentageLength     = 0
	LengthPercentagePercentage = 1
)

// LengthPercentage is Style::LengthPercentage.
type LengthPercentage struct {
	Type  int
	Value float32
}

const (
	NumberAutoAuto   = 0
	NumberAutoNumber = 1
)

// NumberAuto is Style::NumberAuto.
type NumberAuto struct {
	Type  int
	Value float32
}

const (
	DisplayNone = iota
	DisplayBlock
	DisplayInline
	DisplayInlineBlock
	DisplayFlowRoot
	DisplayFlex
	DisplayInlineFlex
	DisplayTable
	DisplayInlineTable
	DisplayTableRow
	DisplayTableRowGroup
	DisplayTableColumn
	DisplayTableColumnGroup
	DisplayTableCell
)

const (
	PositionStatic = iota
	PositionRelative
	PositionAbsolute
	PositionFixed
)

const (
	FloatNone = iota
	FloatLeft
	FloatRight
)

const (
	ClearNone = iota
	ClearLeft
	ClearRight
	ClearBoth
)

const (
	BoxSizingContentBox = iota
	BoxSizingBorderBox
)

const (
	LineHeightNumber = 0
	LineHeightLength = 1
)

// StyleLineHeight is Style::LineHeight.
type StyleLineHeight struct {
	Value        float32
	InheritType  int
	InheritValue float32
}

const (
	VerticalAlignBaseline = iota
	VerticalAlignMiddle
	VerticalAlignSub
	VerticalAlignSuper
	VerticalAlignTextTop
	VerticalAlignTextBottom
	VerticalAlignTop
	VerticalAlignCenter
	VerticalAlignBottom
	VerticalAlignLength
)

// StyleVerticalAlign is Style::VerticalAlign.
type StyleVerticalAlign struct {
	Type  int
	Value float32
}

const (
	OverflowVisible = iota
	OverflowHidden
	OverflowAuto
	OverflowScroll
)

const (
	ClipTypeAuto = iota
	ClipTypeNone
	ClipTypeAlways
	ClipTypeNumber
)

// StyleClip is Style::Clip, stored as in the C++: 0 auto, -1 none, -2 always,
// otherwise a positive number.
type StyleClip struct {
	value int
}

func NewClip(t int, number int) StyleClip {
	switch t {
	case ClipTypeAuto:
		return StyleClip{0}
	case ClipTypeNone:
		return StyleClip{-1}
	case ClipTypeAlways:
		return StyleClip{-2}
	}
	return StyleClip{number}
}

func (c StyleClip) GetNumber() int {
	if c.value < 0 {
		return 0
	}
	return c.value
}

func (c StyleClip) GetType() int {
	switch c.value {
	case 0:
		return ClipTypeAuto
	case -1:
		return ClipTypeNone
	case -2:
		return ClipTypeAlways
	}
	return ClipTypeNumber
}

const (
	VisibilityVisible = iota
	VisibilityHidden
)

const (
	TextOverflowClip = iota
	TextOverflowEllipsis
	TextOverflowString
)

const (
	FontStyleNormal = iota
	FontStyleItalic
)

const (
	FontWeightAuto   = 0
	FontWeightNormal = 400
	FontWeightBold   = 700
)

const (
	FontKerningAuto = iota
	FontKerningNormal
	FontKerningNone
)

const (
	TextAlignLeft = iota
	TextAlignRight
	TextAlignCenter
	TextAlignJustify
)

const (
	TextDecorationNone = iota
	TextDecorationUnderline
	TextDecorationOverline
	TextDecorationLineThrough
)

const (
	TextTransformNone = iota
	TextTransformCapitalize
	TextTransformUppercase
	TextTransformLowercase
)

const (
	WhiteSpaceNormal = iota
	WhiteSpacePre
	WhiteSpaceNowrap
	WhiteSpacePrewrap
	WhiteSpacePreline
)

const (
	WordBreakNormal = iota
	WordBreakBreakAll
	WordBreakBreakWord
)

const (
	DragNone = iota
	DragDrag
	DragDragDrop
	DragBlock
	DragClone
)

const (
	TabIndexNone = iota
	TabIndexAuto
)

const (
	FocusNone = iota
	FocusAuto
)

const (
	OverscrollBehaviorAuto = iota
	OverscrollBehaviorContain
)

const (
	PointerEventsNone = iota
	PointerEventsAuto
)

const (
	OriginXLeft = iota
	OriginXCenter
	OriginXRight
)

const (
	AlignContentFlexStart = iota
	AlignContentFlexEnd
	AlignContentCenter
	AlignContentSpaceBetween
	AlignContentSpaceAround
	AlignContentSpaceEvenly
	AlignContentStretch
)

const (
	AlignItemsFlexStart = iota
	AlignItemsFlexEnd
	AlignItemsCenter
	AlignItemsBaseline
	AlignItemsStretch
)

const (
	AlignSelfAuto = iota
	AlignSelfFlexStart
	AlignSelfFlexEnd
	AlignSelfCenter
	AlignSelfBaseline
	AlignSelfStretch
)

const (
	FlexDirectionRow = iota
	FlexDirectionRowReverse
	FlexDirectionColumn
	FlexDirectionColumnReverse
)

const (
	FlexWrapNowrap = iota
	FlexWrapWrap
	FlexWrapWrapReverse
)

const (
	JustifyContentFlexStart = iota
	JustifyContentFlexEnd
	JustifyContentCenter
	JustifyContentSpaceBetween
	JustifyContentSpaceAround
	JustifyContentSpaceEvenly
)

const (
	NavNone = iota
	NavAuto
	NavHorizontal
	NavVertical
	NavTreeOrder
)

const (
	DirectionAuto = iota
	DirectionLtr
	DirectionRtl
)

// FontFaceHandle is Rml::FontFaceHandle.
type FontFaceHandle = int

// ---- ComputedValues ----

type commonValues struct {
	display          int
	position         int
	float_           int
	clear            int
	overflowX        int
	overflowY        int
	visibility       int
	hasDecorator     bool
	boxSizing        int
	widthType        int
	heightType       int
	marginTopType    int
	marginRightType  int
	marginBottomType int
	marginLeftType   int
	paddingTopType    int
	paddingRightType  int
	paddingBottomType int
	paddingLeftType   int
	topType          int
	rightType        int
	bottomType       int
	leftType         int
	zIndexType       int
	widthValue         float32
	heightValue        float32
	marginTopValue     float32
	marginRightValue   float32
	marginBottomValue  float32
	marginLeftValue    float32
	paddingTopValue    float32
	paddingRightValue  float32
	paddingBottomValue float32
	paddingLeftValue   float32
	topValue           float32
	rightValue         float32
	bottomValue        float32
	leftValue          float32
	zIndexValue        float32
	borderTopWidth    int
	borderRightWidth  int
	borderBottomWidth int
	borderLeftWidth   int
	borderTopColor    Colourb
	borderRightColor  Colourb
	borderBottomColor Colourb
	borderLeftColor   Colourb
	backgroundColor   Colourb
}

func defaultCommonValues() commonValues {
	c := commonValues{}
	c.display = DisplayInline
	c.widthType = LengthPercentageAutoAuto
	c.heightType = LengthPercentageAutoAuto
	c.marginTopType = LengthPercentageAutoLength
	c.marginRightType = LengthPercentageAutoLength
	c.marginBottomType = LengthPercentageAutoLength
	c.marginLeftType = LengthPercentageAutoLength
	c.topType = LengthPercentageAutoAuto
	c.rightType = LengthPercentageAutoAuto
	c.bottomType = LengthPercentageAutoAuto
	c.leftType = LengthPercentageAutoAuto
	c.zIndexType = NumberAutoAuto
	white := Colourb{255, 255, 255, 255}
	c.borderTopColor = white
	c.borderRightColor = white
	c.borderBottomColor = white
	c.borderLeftColor = white
	c.backgroundColor = Colourb{0, 0, 0, 0}
	return c
}

type inheritedValues struct {
	fontFaceHandle        FontFaceHandle
	fontSize              float32
	opacity               float32
	color                 Colourb
	fontWeight            int
	fontKerning           int
	hasLetterSpacing      bool
	fontStyle             int
	hasFontEffect         bool
	pointerEvents         int
	focus                 int
	textAlign             int
	textDecoration        int
	textTransform         int
	whiteSpace            int
	wordBreak             int
	direction             int
	lineHeightInheritType int
	lineHeight            float32
	lineHeightInherit     float32
	language              string
}

func defaultInheritedValues() inheritedValues {
	return inheritedValues{
		fontSize:          12,
		opacity:           1,
		color:             Colourb{255, 255, 255, 255},
		fontWeight:        FontWeightNormal,
		pointerEvents:     PointerEventsAuto,
		focus:             FocusAuto,
		lineHeight:        12.0 * 1.2,
		lineHeightInherit: 1.2,
	}
}

type rareValues struct {
	minWidthType           int
	maxWidthType           int
	minHeightType          int
	maxHeightType          int
	perspectiveOriginXType int
	perspectiveOriginYType int
	transformOriginXType   int
	transformOriginYType   int
	hasLocalTransform      bool
	hasLocalPerspective    bool
	flexBasisType          int
	rowGapType             int
	columnGapType          int
	verticalAlignType      int
	drag                   int
	tabIndex               int
	overscrollBehavior     int
	hasMaskImage           bool
	hasFilter              bool
	hasBackdropFilter      bool
	hasBoxShadow           bool
	textOverflow           int
	clip                   StyleClip
	minWidth               float32
	maxWidth               float32
	minHeight              float32
	maxHeight              float32
	verticalAlignLength    float32
	perspective            float32
	perspectiveOriginX     float32
	perspectiveOriginY     float32
	transformOriginX       float32
	transformOriginY       float32
	transformOriginZ       float32
	flexBasis              float32
	rowGap                 float32
	columnGap              float32
	borderTopLeftRadius     int
	borderTopRightRadius    int
	borderBottomRightRadius int
	borderBottomLeftRadius  int
	imageColor             Colourb
	scrollbarMargin        float32
}

func defaultRareValues() rareValues {
	r := rareValues{}
	r.perspectiveOriginXType = LengthPercentagePercentage
	r.perspectiveOriginYType = LengthPercentagePercentage
	r.transformOriginXType = LengthPercentagePercentage
	r.transformOriginYType = LengthPercentagePercentage
	r.flexBasisType = LengthPercentageAutoAuto
	r.maxWidth = FltMax
	r.maxHeight = FltMax
	r.perspectiveOriginX = 50
	r.perspectiveOriginY = 50
	r.transformOriginX = 50
	r.transformOriginY = 50
	r.imageColor = Colourb{255, 255, 255, 255}
	return r
}

// ComputedValues is Style::ComputedValues.
type ComputedValues struct {
	element   *Element
	common    commonValues
	inherited inheritedValues
	rare      rareValues
}

func NewComputedValues(element *Element) *ComputedValues {
	return &ComputedValues{element: element, common: defaultCommonValues(), inherited: defaultInheritedValues(), rare: defaultRareValues()}
}

var defaultComputedValues = NewComputedValues(nil)

// DefaultComputedValues is Rml::DefaultComputedValues().
func DefaultComputedValues() *ComputedValues { return defaultComputedValues }

func (v *ComputedValues) Width() LengthPercentageAuto {
	return LengthPercentageAuto{v.common.widthType, v.common.widthValue}
}
func (v *ComputedValues) Height() LengthPercentageAuto {
	return LengthPercentageAuto{v.common.heightType, v.common.heightValue}
}
func (v *ComputedValues) MarginTop() LengthPercentageAuto {
	return LengthPercentageAuto{v.common.marginTopType, v.common.marginTopValue}
}
func (v *ComputedValues) MarginRight() LengthPercentageAuto {
	return LengthPercentageAuto{v.common.marginRightType, v.common.marginRightValue}
}
func (v *ComputedValues) MarginBottom() LengthPercentageAuto {
	return LengthPercentageAuto{v.common.marginBottomType, v.common.marginBottomValue}
}
func (v *ComputedValues) MarginLeft() LengthPercentageAuto {
	return LengthPercentageAuto{v.common.marginLeftType, v.common.marginLeftValue}
}
func (v *ComputedValues) PaddingTop() LengthPercentage {
	return LengthPercentage{v.common.paddingTopType, v.common.paddingTopValue}
}
func (v *ComputedValues) PaddingRight() LengthPercentage {
	return LengthPercentage{v.common.paddingRightType, v.common.paddingRightValue}
}
func (v *ComputedValues) PaddingBottom() LengthPercentage {
	return LengthPercentage{v.common.paddingBottomType, v.common.paddingBottomValue}
}
func (v *ComputedValues) PaddingLeft() LengthPercentage {
	return LengthPercentage{v.common.paddingLeftType, v.common.paddingLeftValue}
}
func (v *ComputedValues) Top() LengthPercentageAuto {
	return LengthPercentageAuto{v.common.topType, v.common.topValue}
}
func (v *ComputedValues) Right() LengthPercentageAuto {
	return LengthPercentageAuto{v.common.rightType, v.common.rightValue}
}
func (v *ComputedValues) Bottom() LengthPercentageAuto {
	return LengthPercentageAuto{v.common.bottomType, v.common.bottomValue}
}
func (v *ComputedValues) Left() LengthPercentageAuto {
	return LengthPercentageAuto{v.common.leftType, v.common.leftValue}
}
func (v *ComputedValues) ZIndex() NumberAuto { return NumberAuto{v.common.zIndexType, v.common.zIndexValue} }
func (v *ComputedValues) BorderTopWidth() float32    { return float32(v.common.borderTopWidth) }
func (v *ComputedValues) BorderRightWidth() float32  { return float32(v.common.borderRightWidth) }
func (v *ComputedValues) BorderBottomWidth() float32 { return float32(v.common.borderBottomWidth) }
func (v *ComputedValues) BorderLeftWidth() float32   { return float32(v.common.borderLeftWidth) }
func (v *ComputedValues) BoxSizing() int             { return v.common.boxSizing }
func (v *ComputedValues) Display() int               { return v.common.display }
func (v *ComputedValues) Position() int              { return v.common.position }
func (v *ComputedValues) Float() int                 { return v.common.float_ }
func (v *ComputedValues) Clear() int                 { return v.common.clear }
func (v *ComputedValues) OverflowX() int             { return v.common.overflowX }
func (v *ComputedValues) OverflowY() int             { return v.common.overflowY }
func (v *ComputedValues) Visibility() int            { return v.common.visibility }
func (v *ComputedValues) BackgroundColor() Colourb   { return v.common.backgroundColor }
func (v *ComputedValues) BorderTopColor() Colourb    { return v.common.borderTopColor }
func (v *ComputedValues) BorderRightColor() Colourb  { return v.common.borderRightColor }
func (v *ComputedValues) BorderBottomColor() Colourb { return v.common.borderBottomColor }
func (v *ComputedValues) BorderLeftColor() Colourb   { return v.common.borderLeftColor }
func (v *ComputedValues) HasDecorator() bool         { return v.common.hasDecorator }

func (v *ComputedValues) FontFaceHandle() int { return v.inherited.fontFaceHandle }
func (v *ComputedValues) FontSize() float32              { return v.inherited.fontSize }
func (v *ComputedValues) HasFontEffect() bool            { return v.inherited.hasFontEffect }
func (v *ComputedValues) FontStyle() int                 { return v.inherited.fontStyle }
func (v *ComputedValues) FontWeight() int                { return v.inherited.fontWeight }
func (v *ComputedValues) FontKerning() int               { return v.inherited.fontKerning }
func (v *ComputedValues) PointerEvents() int             { return v.inherited.pointerEvents }
func (v *ComputedValues) Focus() int                     { return v.inherited.focus }
func (v *ComputedValues) TextAlign() int                 { return v.inherited.textAlign }
func (v *ComputedValues) TextDecoration() int            { return v.inherited.textDecoration }
func (v *ComputedValues) TextTransform() int             { return v.inherited.textTransform }
func (v *ComputedValues) WhiteSpace() int                { return v.inherited.whiteSpace }
func (v *ComputedValues) WordBreak() int                 { return v.inherited.wordBreak }
func (v *ComputedValues) Color() Colourb                 { return v.inherited.color }
func (v *ComputedValues) Opacity() float32               { return v.inherited.opacity }
func (v *ComputedValues) LineHeight() StyleLineHeight {
	return StyleLineHeight{v.inherited.lineHeight, v.inherited.lineHeightInheritType, v.inherited.lineHeightInherit}
}
func (v *ComputedValues) Language() string { return v.inherited.language }
func (v *ComputedValues) Direction() int   { return v.inherited.direction }

func (v *ComputedValues) MinWidth() LengthPercentage {
	return LengthPercentage{v.rare.minWidthType, v.rare.minWidth}
}
func (v *ComputedValues) MaxWidth() LengthPercentage {
	return LengthPercentage{v.rare.maxWidthType, v.rare.maxWidth}
}
func (v *ComputedValues) MinHeight() LengthPercentage {
	return LengthPercentage{v.rare.minHeightType, v.rare.minHeight}
}
func (v *ComputedValues) MaxHeight() LengthPercentage {
	return LengthPercentage{v.rare.maxHeightType, v.rare.maxHeight}
}
func (v *ComputedValues) VerticalAlign() StyleVerticalAlign {
	return StyleVerticalAlign{v.rare.verticalAlignType, v.rare.verticalAlignLength}
}
func (v *ComputedValues) Perspective() float32 { return v.rare.perspective }
func (v *ComputedValues) PerspectiveOriginX() LengthPercentage {
	return LengthPercentage{v.rare.perspectiveOriginXType, v.rare.perspectiveOriginX}
}
func (v *ComputedValues) PerspectiveOriginY() LengthPercentage {
	return LengthPercentage{v.rare.perspectiveOriginYType, v.rare.perspectiveOriginY}
}
func (v *ComputedValues) TransformOriginX() LengthPercentage {
	return LengthPercentage{v.rare.transformOriginXType, v.rare.transformOriginX}
}
func (v *ComputedValues) TransformOriginY() LengthPercentage {
	return LengthPercentage{v.rare.transformOriginYType, v.rare.transformOriginY}
}
func (v *ComputedValues) TransformOriginZ() float32   { return v.rare.transformOriginZ }
func (v *ComputedValues) HasLocalTransform() bool     { return v.rare.hasLocalTransform }
func (v *ComputedValues) HasLocalPerspective() bool   { return v.rare.hasLocalPerspective }
func (v *ComputedValues) FlexBasis() LengthPercentageAuto {
	return LengthPercentageAuto{v.rare.flexBasisType, v.rare.flexBasis}
}
func (v *ComputedValues) BorderTopLeftRadius() float32     { return float32(v.rare.borderTopLeftRadius) }
func (v *ComputedValues) BorderTopRightRadius() float32    { return float32(v.rare.borderTopRightRadius) }
func (v *ComputedValues) BorderBottomRightRadius() float32 { return float32(v.rare.borderBottomRightRadius) }
func (v *ComputedValues) BorderBottomLeftRadius() float32  { return float32(v.rare.borderBottomLeftRadius) }
func (v *ComputedValues) BorderRadius() CornerSizes {
	return NewCornerSizes(float32(v.rare.borderTopLeftRadius), float32(v.rare.borderTopRightRadius),
		float32(v.rare.borderBottomRightRadius), float32(v.rare.borderBottomLeftRadius))
}
func (v *ComputedValues) TextOverflow() int       { return v.rare.textOverflow }
func (v *ComputedValues) Clip() StyleClip { return v.rare.clip }
func (v *ComputedValues) Drag() int               { return v.rare.drag }
func (v *ComputedValues) TabIndex() int           { return v.rare.tabIndex }
func (v *ComputedValues) ImageColor() Colourb     { return v.rare.imageColor }
func (v *ComputedValues) RowGap() LengthPercentage {
	return LengthPercentage{v.rare.rowGapType, v.rare.rowGap}
}
func (v *ComputedValues) ColumnGap() LengthPercentage {
	return LengthPercentage{v.rare.columnGapType, v.rare.columnGap}
}
func (v *ComputedValues) OverscrollBehavior() int { return v.rare.overscrollBehavior }
func (v *ComputedValues) ScrollbarMargin() float32 { return v.rare.scrollbarMargin }
func (v *ComputedValues) HasMaskImage() bool      { return v.rare.hasMaskImage }
func (v *ComputedValues) HasFilter() bool         { return v.rare.hasFilter }
func (v *ComputedValues) HasBackdropFilter() bool { return v.rare.hasBackdropFilter }
func (v *ComputedValues) HasBoxShadow() bool      { return v.rare.hasBoxShadow }

// Local (non-computed) properties read through the element's style.

func (v *ComputedValues) localProperty(id PropertyId) *Property {
	if v.element == nil {
		return nil
	}
	return v.element.GetStyle().GetLocalPropertyWithResolvedVariables(id)
}

func (v *ComputedValues) localKeyword(id PropertyId, def int) int {
	if p := v.localProperty(id); p != nil {
		return p.Value.GetInt()
	}
	return def
}

// TransformPtr is ComputedValues::transform().
func (v *ComputedValues) TransformPtr() *Transform {
	if p := v.localProperty(PropertyIdTransform); p != nil {
		if t, ok := p.Value.Pointer().(*Transform); ok {
			return t
		}
	}
	return nil
}

func (v *ComputedValues) AlignContent() int {
	return v.localKeyword(PropertyIdAlignContent, AlignContentStretch)
}
func (v *ComputedValues) AlignItems() int { return v.localKeyword(PropertyIdAlignItems, AlignItemsStretch) }
func (v *ComputedValues) AlignSelf() int  { return v.localKeyword(PropertyIdAlignSelf, AlignSelfAuto) }
func (v *ComputedValues) FlexDirection() int {
	return v.localKeyword(PropertyIdFlexDirection, FlexDirectionRow)
}
func (v *ComputedValues) FlexWrap() int { return v.localKeyword(PropertyIdFlexWrap, FlexWrapNowrap) }
func (v *ComputedValues) JustifyContent() int {
	return v.localKeyword(PropertyIdJustifyContent, JustifyContentFlexStart)
}
func (v *ComputedValues) FlexGrow() float32 {
	if p := v.localProperty(PropertyIdFlexGrow); p != nil {
		return p.Value.GetFloat()
	}
	return 0
}
func (v *ComputedValues) FlexShrink() float32 {
	if p := v.localProperty(PropertyIdFlexShrink); p != nil {
		return p.Value.GetFloat()
	}
	return 1
}
func (v *ComputedValues) TextOverflowString() string {
	if p := v.localProperty(PropertyIdTextOverflow); p != nil {
		return p.Value.GetString()
	}
	return ""
}

// Animations is ComputedValues::animation().
func (v *ComputedValues) Animations() *AnimationList {
	if p := v.localProperty(PropertyIdAnimation); p != nil && p.Unit == UnitANIMATION {
		if a, ok := p.Value.Pointer().(*AnimationList); ok {
			return a
		}
	}
	return nil
}

// Transitions is ComputedValues::transition().
func (v *ComputedValues) Transitions() *TransitionList {
	if p := v.localProperty(PropertyIdTransition); p != nil && p.Unit == UnitTRANSITION {
		if t, ok := p.Value.Pointer().(*TransitionList); ok {
			return t
		}
	}
	return nil
}

func (v *ComputedValues) FontFamily() string {
	if p := v.element.GetProperty(PropertyIdFontFamily); p != nil {
		return ComputeFontFamily(p.Value.GetString())
	}
	return ""
}

func (v *ComputedValues) Cursor() string {
	if p := v.element.GetProperty(PropertyIdCursor); p != nil {
		return p.Value.GetString()
	}
	return ""
}

func (v *ComputedValues) LetterSpacing() float32 {
	if v.inherited.hasLetterSpacing {
		if p := v.element.GetProperty(PropertyIdLetterSpacing); p != nil {
			return v.element.ResolveLength(p.GetNumericValue())
		}
	}
	return 0
}

// Setters.

func (v *ComputedValues) SetWidth(x LengthPercentageAuto) {
	v.common.widthType = x.Type
	v.common.widthValue = x.Value
}
func (v *ComputedValues) SetHeight(x LengthPercentageAuto) {
	v.common.heightType = x.Type
	v.common.heightValue = x.Value
}
func (v *ComputedValues) SetMarginTop(x LengthPercentageAuto) {
	v.common.marginTopType = x.Type
	v.common.marginTopValue = x.Value
}
func (v *ComputedValues) SetMarginRight(x LengthPercentageAuto) {
	v.common.marginRightType = x.Type
	v.common.marginRightValue = x.Value
}
func (v *ComputedValues) SetMarginBottom(x LengthPercentageAuto) {
	v.common.marginBottomType = x.Type
	v.common.marginBottomValue = x.Value
}
func (v *ComputedValues) SetMarginLeft(x LengthPercentageAuto) {
	v.common.marginLeftType = x.Type
	v.common.marginLeftValue = x.Value
}
func (v *ComputedValues) SetPaddingTop(x LengthPercentage) {
	v.common.paddingTopType = x.Type
	v.common.paddingTopValue = x.Value
}
func (v *ComputedValues) SetPaddingRight(x LengthPercentage) {
	v.common.paddingRightType = x.Type
	v.common.paddingRightValue = x.Value
}
func (v *ComputedValues) SetPaddingBottom(x LengthPercentage) {
	v.common.paddingBottomType = x.Type
	v.common.paddingBottomValue = x.Value
}
func (v *ComputedValues) SetPaddingLeft(x LengthPercentage) {
	v.common.paddingLeftType = x.Type
	v.common.paddingLeftValue = x.Value
}
func (v *ComputedValues) SetTop(x LengthPercentageAuto) {
	v.common.topType = x.Type
	v.common.topValue = x.Value
}
func (v *ComputedValues) SetRight(x LengthPercentageAuto) {
	v.common.rightType = x.Type
	v.common.rightValue = x.Value
}
func (v *ComputedValues) SetBottom(x LengthPercentageAuto) {
	v.common.bottomType = x.Type
	v.common.bottomValue = x.Value
}
func (v *ComputedValues) SetLeft(x LengthPercentageAuto) {
	v.common.leftType = x.Type
	v.common.leftValue = x.Value
}
func (v *ComputedValues) SetZIndex(x NumberAuto) {
	v.common.zIndexType = x.Type
	v.common.zIndexValue = x.Value
}
func (v *ComputedValues) SetBorderTopWidth(x int)      { v.common.borderTopWidth = x }
func (v *ComputedValues) SetBorderRightWidth(x int)    { v.common.borderRightWidth = x }
func (v *ComputedValues) SetBorderBottomWidth(x int)   { v.common.borderBottomWidth = x }
func (v *ComputedValues) SetBorderLeftWidth(x int)     { v.common.borderLeftWidth = x }
func (v *ComputedValues) SetBoxSizing(x int)           { v.common.boxSizing = x }
func (v *ComputedValues) SetDisplay(x int)             { v.common.display = x }
func (v *ComputedValues) SetPosition(x int)            { v.common.position = x }
func (v *ComputedValues) SetFloat(x int)               { v.common.float_ = x }
func (v *ComputedValues) SetClear(x int)               { v.common.clear = x }
func (v *ComputedValues) SetOverflowX(x int)           { v.common.overflowX = x }
func (v *ComputedValues) SetOverflowY(x int)           { v.common.overflowY = x }
func (v *ComputedValues) SetVisibility(x int)          { v.common.visibility = x }
func (v *ComputedValues) SetBackgroundColor(x Colourb) { v.common.backgroundColor = x }
func (v *ComputedValues) SetBorderTopColor(x Colourb)    { v.common.borderTopColor = x }
func (v *ComputedValues) SetBorderRightColor(x Colourb)  { v.common.borderRightColor = x }
func (v *ComputedValues) SetBorderBottomColor(x Colourb) { v.common.borderBottomColor = x }
func (v *ComputedValues) SetBorderLeftColor(x Colourb)   { v.common.borderLeftColor = x }
func (v *ComputedValues) SetHasDecorator(x bool)         { v.common.hasDecorator = x }

func (v *ComputedValues) SetFontFaceHandle(x int) { v.inherited.fontFaceHandle = x }
func (v *ComputedValues) SetFontSize(x float32)              { v.inherited.fontSize = x }
func (v *ComputedValues) SetHasLetterSpacing(x bool)         { v.inherited.hasLetterSpacing = x }
func (v *ComputedValues) SetHasFontEffect(x bool)            { v.inherited.hasFontEffect = x }
func (v *ComputedValues) SetFontStyle(x int)                 { v.inherited.fontStyle = x }
func (v *ComputedValues) SetFontWeight(x int)                { v.inherited.fontWeight = x }
func (v *ComputedValues) SetFontKerning(x int)               { v.inherited.fontKerning = x }
func (v *ComputedValues) SetPointerEvents(x int)             { v.inherited.pointerEvents = x }
func (v *ComputedValues) SetFocus(x int)                     { v.inherited.focus = x }
func (v *ComputedValues) SetTextAlign(x int)                 { v.inherited.textAlign = x }
func (v *ComputedValues) SetTextDecoration(x int)            { v.inherited.textDecoration = x }
func (v *ComputedValues) SetTextTransform(x int)             { v.inherited.textTransform = x }
func (v *ComputedValues) SetWhiteSpace(x int)                { v.inherited.whiteSpace = x }
func (v *ComputedValues) SetWordBreak(x int)                 { v.inherited.wordBreak = x }
func (v *ComputedValues) SetColor(x Colourb)                 { v.inherited.color = x }
func (v *ComputedValues) SetOpacity(x float32)               { v.inherited.opacity = x }
func (v *ComputedValues) SetLineHeight(x StyleLineHeight) {
	v.inherited.lineHeight = x.Value
	v.inherited.lineHeightInheritType = x.InheritType
	v.inherited.lineHeightInherit = x.InheritValue
}
func (v *ComputedValues) SetLanguage(x string) { v.inherited.language = x }
func (v *ComputedValues) SetDirection(x int)   { v.inherited.direction = x }

func (v *ComputedValues) SetMinWidth(x LengthPercentage) {
	v.rare.minWidthType = x.Type
	v.rare.minWidth = x.Value
}
func (v *ComputedValues) SetMaxWidth(x LengthPercentage) {
	v.rare.maxWidthType = x.Type
	v.rare.maxWidth = x.Value
}
func (v *ComputedValues) SetMinHeight(x LengthPercentage) {
	v.rare.minHeightType = x.Type
	v.rare.minHeight = x.Value
}
func (v *ComputedValues) SetMaxHeight(x LengthPercentage) {
	v.rare.maxHeightType = x.Type
	v.rare.maxHeight = x.Value
}
func (v *ComputedValues) SetVerticalAlign(x StyleVerticalAlign) {
	v.rare.verticalAlignType = x.Type
	v.rare.verticalAlignLength = x.Value
}
func (v *ComputedValues) SetPerspectiveOriginX(x LengthPercentage) {
	v.rare.perspectiveOriginXType = x.Type
	v.rare.perspectiveOriginX = x.Value
}
func (v *ComputedValues) SetPerspectiveOriginY(x LengthPercentage) {
	v.rare.perspectiveOriginYType = x.Type
	v.rare.perspectiveOriginY = x.Value
}
func (v *ComputedValues) SetTransformOriginX(x LengthPercentage) {
	v.rare.transformOriginXType = x.Type
	v.rare.transformOriginX = x.Value
}
func (v *ComputedValues) SetTransformOriginY(x LengthPercentage) {
	v.rare.transformOriginYType = x.Type
	v.rare.transformOriginY = x.Value
}
func (v *ComputedValues) SetRowGap(x LengthPercentage) {
	v.rare.rowGapType = x.Type
	v.rare.rowGap = x.Value
}
func (v *ComputedValues) SetColumnGap(x LengthPercentage) {
	v.rare.columnGapType = x.Type
	v.rare.columnGap = x.Value
}
func (v *ComputedValues) SetFlexBasis(x LengthPercentageAuto) {
	v.rare.flexBasisType = x.Type
	v.rare.flexBasis = x.Value
}
func (v *ComputedValues) SetTransformOriginZ(x float32)       { v.rare.transformOriginZ = x }
func (v *ComputedValues) SetPerspective(x float32)            { v.rare.perspective = x }
func (v *ComputedValues) SetHasLocalPerspective(x bool)       { v.rare.hasLocalPerspective = x }
func (v *ComputedValues) SetHasLocalTransform(x bool)         { v.rare.hasLocalTransform = x }
func (v *ComputedValues) SetBorderTopLeftRadius(x float32)     { v.rare.borderTopLeftRadius = int(int16(x)) }
func (v *ComputedValues) SetBorderTopRightRadius(x float32)    { v.rare.borderTopRightRadius = int(int16(x)) }
func (v *ComputedValues) SetBorderBottomRightRadius(x float32) { v.rare.borderBottomRightRadius = int(int16(x)) }
func (v *ComputedValues) SetBorderBottomLeftRadius(x float32)  { v.rare.borderBottomLeftRadius = int(int16(x)) }
func (v *ComputedValues) SetTextOverflow(x int)               { v.rare.textOverflow = x }
func (v *ComputedValues) SetClip(x StyleClip)                      { v.rare.clip = x }
func (v *ComputedValues) SetDrag(x int)                       { v.rare.drag = x }
func (v *ComputedValues) SetTabIndex(x int)                   { v.rare.tabIndex = x }
func (v *ComputedValues) SetImageColor(x Colourb)             { v.rare.imageColor = x }
func (v *ComputedValues) SetOverscrollBehavior(x int)         { v.rare.overscrollBehavior = x }
func (v *ComputedValues) SetScrollbarMargin(x float32)        { v.rare.scrollbarMargin = x }
func (v *ComputedValues) SetHasMaskImage(x bool)              { v.rare.hasMaskImage = x }
func (v *ComputedValues) SetHasFilter(x bool)                 { v.rare.hasFilter = x }
func (v *ComputedValues) SetHasBackdropFilter(x bool)         { v.rare.hasBackdropFilter = x }
func (v *ComputedValues) SetHasBoxShadow(x bool)              { v.rare.hasBoxShadow = x }

// CopyNonInherited copies the common and rare groups.
func (v *ComputedValues) CopyNonInherited(o *ComputedValues) {
	v.common = o.common
	v.rare = o.rare
}

// CopyInherited copies the inherited group from the parent.
func (v *ComputedValues) CopyInherited(parent *ComputedValues) { v.inherited = parent.inherited }

// ResolveValueOrAuto is ResolveValueOr(LengthPercentageAuto, base, default).
func ResolveValueOrAuto(length LengthPercentageAuto, baseValue float32, defaultValue float32) float32 {
	if length.Type == LengthPercentageAutoLength {
		return length.Value
	} else if length.Type == LengthPercentageAutoPercentage && baseValue >= 0 {
		return length.Value * 0.01 * baseValue
	}
	return defaultValue
}

// ResolveValueOr is ResolveValueOr(LengthPercentage, base, default).
func ResolveValueOr(length LengthPercentage, baseValue float32, defaultValue float32) float32 {
	if length.Type == LengthPercentageLength {
		return length.Value
	} else if length.Type == LengthPercentagePercentage && baseValue >= 0 {
		return length.Value * 0.01 * baseValue
	}
	return defaultValue
}

func ResolveValueAuto(length LengthPercentageAuto, baseValue float32) float32 {
	return ResolveValueOrAuto(length, baseValue, 0)
}

func ResolveValue(length LengthPercentage, baseValue float32) float32 {
	return ResolveValueOr(length, baseValue, 0)
}

// ---- ComputeProperty.cpp ----

const pixelsPerInch float32 = 96.0

func computePPILength(value NumericValue, dpRatio float32) float32 {
	inch := value.Number * pixelsPerInch * dpRatio
	switch value.Unit {
	case UnitINCH:
		return inch
	case UnitCM:
		return inch * (1.0 / 2.54)
	case UnitMM:
		return inch * (1.0 / 25.4)
	case UnitPT:
		return inch * (1.0 / 72.0)
	case UnitPC:
		return inch * (1.0 / 6.0)
	}
	return 0
}

// ComputeLength resolves an absolute or font/viewport-relative length.
func ComputeLength(value NumericValue, fontSize float32, documentFontSize float32, dpRatio float32, vpDimensions Vector2f) float32 {
	if AnyUnit(value.Unit & UnitPPI_UNIT) {
		return computePPILength(value, dpRatio)
	}
	switch value.Unit {
	case UnitPX:
		return value.Number
	case UnitEM:
		return value.Number * fontSize
	case UnitREM:
		return value.Number * documentFontSize
	case UnitDP:
		return value.Number * dpRatio
	case UnitVW:
		return value.Number * vpDimensions.X * 0.01
	case UnitVH:
		return value.Number * vpDimensions.Y * 0.01
	}
	return 0
}

// ComputeAngle resolves an angle to radians.
func ComputeAngle(value NumericValue) float32 {
	switch value.Unit {
	case UnitNUMBER, UnitRAD:
		return value.Number
	case UnitDEG:
		return MathDegreesToRadians(value.Number)
	}
	return 0
}

// ComputeFontsize resolves font-size relative to the parent or document.
func ComputeFontsize(value NumericValue, values *ComputedValues, parentValues *ComputedValues, documentValues *ComputedValues, dpRatio float32, vpDimensions Vector2f) float32 {
	if AnyUnit(value.Unit & (UnitPERCENT | UnitEM | UnitREM)) {
		var multiplier float32 = 1.0
		switch value.Unit {
		case UnitPERCENT, UnitEM:
			if value.Unit == UnitPERCENT {
				multiplier = 0.01
			}
			if parentValues == nil {
				return 0
			}
			return value.Number * multiplier * parentValues.FontSize()
		case UnitREM:
			if documentValues == nil || values == documentValues {
				return value.Number * DefaultComputedValues().FontSize()
			}
			return value.Number * documentValues.FontSize()
		}
	}
	return ComputeLength(value, 0, 0, dpRatio, vpDimensions)
}

func ComputeFontFamily(fontFamily string) string { return StringToLower(fontFamily) }

func ComputeClip(property *Property) StyleClip {
	value := property.Value.GetInt()
	if property.Unit == UnitKEYWORD {
		return NewClip(value, 0)
	} else if property.Unit == UnitNUMBER {
		return NewClip(ClipTypeNumber, int(int8(value)))
	}
	return StyleClip{}
}

func ComputeLineHeight(property *Property, fontSize float32, documentFontSize float32, dpRatio float32, vpDimensions Vector2f) StyleLineHeight {
	if AnyUnit(property.Unit & UnitLENGTH) {
		value := ComputeLength(property.GetNumericValue(), fontSize, documentFontSize, dpRatio, vpDimensions)
		return StyleLineHeight{value, LineHeightLength, value}
	}
	var scaleFactor float32 = 1.0
	switch property.Unit {
	case UnitNUMBER:
		scaleFactor = property.Value.GetFloat()
	case UnitPERCENT:
		scaleFactor = property.Value.GetFloat() * 0.01
	}
	value := fontSize * scaleFactor
	return StyleLineHeight{value, LineHeightNumber, scaleFactor}
}

func ComputeVerticalAlign(property *Property, lineHeight float32, fontSize float32, documentFontSize float32, dpRatio float32, vpDimensions Vector2f) StyleVerticalAlign {
	if AnyUnit(property.Unit & UnitLENGTH) {
		value := ComputeLength(property.GetNumericValue(), fontSize, documentFontSize, dpRatio, vpDimensions)
		return StyleVerticalAlign{VerticalAlignLength, value}
	} else if property.Unit == UnitPERCENT {
		return StyleVerticalAlign{VerticalAlignLength, property.Value.GetFloat() * lineHeight * 0.01}
	}
	return StyleVerticalAlign{property.Value.GetInt(), 0}
}

func ComputeLengthPercentage(property *Property, fontSize float32, documentFontSize float32, dpRatio float32, vpDimensions Vector2f) LengthPercentage {
	if property.Unit == UnitPERCENT {
		return LengthPercentage{LengthPercentagePercentage, property.Value.GetFloat()}
	}
	return LengthPercentage{LengthPercentageLength, ComputeLength(property.GetNumericValue(), fontSize, documentFontSize, dpRatio, vpDimensions)}
}

func ComputeLengthPercentageAuto(property *Property, fontSize float32, documentFontSize float32, dpRatio float32, vpDimensions Vector2f) LengthPercentageAuto {
	if property.Unit == UnitPERCENT {
		return LengthPercentageAuto{LengthPercentageAutoPercentage, property.Value.GetFloat()}
	} else if property.Unit == UnitKEYWORD {
		return LengthPercentageAuto{LengthPercentageAutoAuto, 0}
	}
	return LengthPercentageAuto{LengthPercentageAutoLength, ComputeLength(property.GetNumericValue(), fontSize, documentFontSize, dpRatio, vpDimensions)}
}

func ComputeOrigin(property *Property, fontSize float32, documentFontSize float32, dpRatio float32, vpDimensions Vector2f) LengthPercentage {
	if property.Unit == UnitKEYWORD {
		var percent float32 = 0
		switch property.Value.GetInt() {
		case OriginXLeft:
			percent = 0
		case OriginXCenter:
			percent = 50
		case OriginXRight:
			percent = 100
		}
		return LengthPercentage{LengthPercentagePercentage, percent}
	} else if property.Unit == UnitPERCENT {
		return LengthPercentage{LengthPercentagePercentage, property.Value.GetFloat()}
	}
	return LengthPercentage{LengthPercentageLength, ComputeLength(property.GetNumericValue(), fontSize, documentFontSize, dpRatio, vpDimensions)}
}

func ComputeMaxSize(property *Property, fontSize float32, documentFontSize float32, dpRatio float32, vpDimensions Vector2f) LengthPercentage {
	if AnyUnit(property.Unit & UnitKEYWORD) {
		return LengthPercentage{LengthPercentageLength, FltMax}
	} else if AnyUnit(property.Unit & UnitPERCENT) {
		return LengthPercentage{LengthPercentagePercentage, property.Value.GetFloat()}
	}
	length := ComputeLength(property.GetNumericValue(), fontSize, documentFontSize, dpRatio, vpDimensions)
	if length < 0 {
		length = FltMax
	}
	return LengthPercentage{LengthPercentageLength, length}
}

func ComputeBorderWidth(computedLength float32) int {
	if computedLength <= 0 {
		return 0
	}
	if computedLength <= 1 {
		return 1
	}
	return int(computedLength + 0.5)
}

func GetFontFaceDescription(fontFamily string, style int, weight int) string {
	attrs := ""
	if style == FontStyleItalic {
		attrs = attrs + "italic, "
	}
	if weight == FontWeightBold {
		attrs = attrs + "bold, "
	} else if weight != FontWeightAuto && weight != FontWeightNormal {
		attrs = attrs + "weight=" + FormatInt(weight) + ", "
	}
	if attrs == "" {
		attrs = "regular"
	} else {
		attrs = attrs[:len(attrs)-2]
	}
	return "'" + fontFamily + "' [" + attrs + "]"
}
