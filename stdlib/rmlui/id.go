// Port of RmlUi Include/RmlUi/Core/ID.h.
package rmlui

type ShorthandId = int

const (
	ShorthandIdInvalid ShorthandId = iota
	ShorthandIdMargin
	ShorthandIdPadding
	ShorthandIdBorderWidth
	ShorthandIdBorderColor
	ShorthandIdBorderTop
	ShorthandIdBorderRight
	ShorthandIdBorderBottom
	ShorthandIdBorderLeft
	ShorthandIdBorder
	ShorthandIdBorderRadius
	ShorthandIdInset
	ShorthandIdOverflow
	ShorthandIdBackground
	ShorthandIdFont
	ShorthandIdGap
	ShorthandIdPerspectiveOrigin
	ShorthandIdTransformOrigin
	ShorthandIdFlex
	ShorthandIdFlexFlow
	ShorthandIdNav
	ShorthandIdNumDefinedIds
)

const ShorthandIdFirstCustomId ShorthandId = ShorthandIdNumDefinedIds
const ShorthandIdMaxNumIds ShorthandId = 0xff

type PropertyId = int

const (
	PropertyIdInvalid PropertyId = iota
	PropertyIdMarginTop
	PropertyIdMarginRight
	PropertyIdMarginBottom
	PropertyIdMarginLeft
	PropertyIdPaddingTop
	PropertyIdPaddingRight
	PropertyIdPaddingBottom
	PropertyIdPaddingLeft
	PropertyIdBorderTopWidth
	PropertyIdBorderRightWidth
	PropertyIdBorderBottomWidth
	PropertyIdBorderLeftWidth
	PropertyIdBorderTopColor
	PropertyIdBorderRightColor
	PropertyIdBorderBottomColor
	PropertyIdBorderLeftColor
	PropertyIdBorderTopLeftRadius
	PropertyIdBorderTopRightRadius
	PropertyIdBorderBottomRightRadius
	PropertyIdBorderBottomLeftRadius
	PropertyIdDisplay
	PropertyIdPosition
	PropertyIdTop
	PropertyIdRight
	PropertyIdBottom
	PropertyIdLeft
	PropertyIdFloat
	PropertyIdClear
	PropertyIdBoxSizing
	PropertyIdZIndex
	PropertyIdWidth
	PropertyIdMinWidth
	PropertyIdMaxWidth
	PropertyIdHeight
	PropertyIdMinHeight
	PropertyIdMaxHeight
	PropertyIdLineHeight
	PropertyIdVerticalAlign
	PropertyIdOverflowX
	PropertyIdOverflowY
	PropertyIdClip
	PropertyIdVisibility
	PropertyIdTextOverflow
	PropertyIdBackgroundColor
	PropertyIdColor
	PropertyIdCaretColor
	PropertyIdImageColor
	PropertyIdFontFamily
	PropertyIdFontStyle
	PropertyIdFontWeight
	PropertyIdFontSize
	PropertyIdFontKerning
	PropertyIdLetterSpacing
	PropertyIdTextAlign
	PropertyIdTextDecoration
	PropertyIdTextTransform
	PropertyIdWhiteSpace
	PropertyIdWordBreak
	PropertyIdRowGap
	PropertyIdColumnGap
	PropertyIdCursor
	PropertyIdDrag
	PropertyIdTabIndex
	PropertyIdScrollbarMargin
	PropertyIdOverscrollBehavior
	PropertyIdPerspective
	PropertyIdPerspectiveOriginX
	PropertyIdPerspectiveOriginY
	PropertyIdTransform
	PropertyIdTransformOriginX
	PropertyIdTransformOriginY
	PropertyIdTransformOriginZ
	PropertyIdTransition
	PropertyIdAnimation
	PropertyIdOpacity
	PropertyIdPointerEvents
	PropertyIdFocus
	PropertyIdDecorator
	PropertyIdMaskImage
	PropertyIdFontEffect
	PropertyIdFilter
	PropertyIdBackdropFilter
	PropertyIdBoxShadow
	PropertyIdFillImage
	PropertyIdAlignContent
	PropertyIdAlignItems
	PropertyIdAlignSelf
	PropertyIdFlexBasis
	PropertyIdFlexDirection
	PropertyIdFlexGrow
	PropertyIdFlexShrink
	PropertyIdFlexWrap
	PropertyIdJustifyContent
	PropertyIdNavUp
	PropertyIdNavRight
	PropertyIdNavDown
	PropertyIdNavLeft
	PropertyIdRmlUi_Language
	PropertyIdRmlUi_Direction
	PropertyIdNumDefinedIds
)

const PropertyIdFirstCustomId PropertyId = PropertyIdNumDefinedIds
const PropertyIdMaxNumIds PropertyId = 128

type MediaQueryId = int

const (
	MediaQueryIdInvalid MediaQueryId = iota
	MediaQueryIdWidth
	MediaQueryIdMinWidth
	MediaQueryIdMaxWidth
	MediaQueryIdHeight
	MediaQueryIdMinHeight
	MediaQueryIdMaxHeight
	MediaQueryIdAspectRatio
	MediaQueryIdMinAspectRatio
	MediaQueryIdMaxAspectRatio
	MediaQueryIdResolution
	MediaQueryIdMinResolution
	MediaQueryIdMaxResolution
	MediaQueryIdOrientation
	MediaQueryIdTheme
	MediaQueryIdNumDefinedIds
)

type FontFaceId = int

const (
	FontFaceIdInvalid FontFaceId = iota
	FontFaceIdFontFamily
	FontFaceIdFontWeight
	FontFaceIdFontStyle
	FontFaceIdSrc
	FontFaceIdFallbackFace
	FontFaceIdFaceIndex
	FontFaceIdNumDefinedIds
)

type EventId = int

const (
	EventIdInvalid EventId = iota
	EventIdMousedown
	EventIdMousescroll
	EventIdMouseover
	EventIdMouseout
	EventIdFocus
	EventIdBlur
	EventIdKeydown
	EventIdKeyup
	EventIdTextinput
	EventIdMouseup
	EventIdClick
	EventIdDblclick
	EventIdLoad
	EventIdUnload
	EventIdShow
	EventIdHide
	EventIdMousemove
	EventIdDragmove
	EventIdDrag
	EventIdDragstart
	EventIdDragover
	EventIdDragdrop
	EventIdDragout
	EventIdDragend
	EventIdHandledrag
	EventIdResize
	EventIdScroll
	EventIdAnimationend
	EventIdTransitionend
	EventIdChange
	EventIdSubmit
	EventIdTabchange
	EventIdNumDefinedIds
)

const EventIdFirstCustomId EventId = EventIdNumDefinedIds
const EventIdMaxNumIds EventId = 0xffff

// IdNameMap is Rml::IdNameMap: a two-way map between names and ids that can
// hand out new ids for custom names.
type IdNameMap struct {
	nameMap     []string
	reverseMap  map[string]int
	nextFreeId  int
}

func NewIdNameMap(numIdsToReserve int) *IdNameMap {
	m := &IdNameMap{nameMap: make([]string, numIdsToReserve), reverseMap: map[string]int{}, nextFreeId: numIdsToReserve}
	return m
}

func (m *IdNameMap) AddPair(id int, name string) {
	for id >= len(m.nameMap) {
		m.nameMap = append(m.nameMap, "")
	}
	m.nameMap[id] = name
	m.reverseMap[name] = id
	if id >= m.nextFreeId {
		m.nextFreeId = id + 1
	}
}

// GetId returns the id for name, or 0 (Invalid) when unknown.
func (m *IdNameMap) GetId(name string) int {
	if id, ok := m.reverseMap[name]; ok {
		return id
	}
	return 0
}

func (m *IdNameMap) GetName(id int) string {
	if id >= 0 && id < len(m.nameMap) {
		return m.nameMap[id]
	}
	return ""
}

func (m *IdNameMap) GetOrCreateId(name string) int {
	if id, ok := m.reverseMap[name]; ok {
		return id
	}
	id := m.nextFreeId
	m.nextFreeId++
	m.AddPair(id, name)
	return id
}

// AssertAllInserted reports whether every id below numValidIds has a name.
func (m *IdNameMap) AssertAllInserted(numValidIds int) bool {
	count := 0
	rng1 := m.reverseMap
	for _, v := range rng1 {
		if v >= 0 && v < numValidIds {
			count++
		}
	}
	return count+1 == numValidIds
}
