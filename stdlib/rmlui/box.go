// Port of RmlUi Source/Core/Box.cpp and Include/RmlUi/Core/Box.h, plus the
// BoxArea/BoxEdge/BoxDirection enums from Types.h.
package rmlui

// BoxArea is Rml::BoxArea.
type BoxArea = int

const (
	BoxAreaMargin  BoxArea = 0
	BoxAreaBorder  BoxArea = 1
	BoxAreaPadding BoxArea = 2
	BoxAreaContent BoxArea = 3
	BoxAreaAuto    BoxArea = 4
)

// BoxEdge is Rml::BoxEdge.
type BoxEdge = int

const (
	BoxEdgeTop    BoxEdge = 0
	BoxEdgeRight  BoxEdge = 1
	BoxEdgeBottom BoxEdge = 2
	BoxEdgeLeft   BoxEdge = 3
)

// BoxDirection is Rml::BoxDirection.
type BoxDirection = int

const (
	BoxDirectionVertical   BoxDirection = 0
	BoxDirectionHorizontal BoxDirection = 1
)

const boxNumAreas = 3
const boxNumEdges = 4

// Box is Rml::Box: content size plus margin, border, and padding edges.
type Box struct {
	content   Vector2f
	areaEdges [12]float32 // [area*4 + edge] for Margin, Border, Padding
}

func NewBox(content Vector2f) Box { return Box{content: content} }

func (b *Box) GetPosition(area BoxArea) Vector2f {
	pos := Vector2f{-b.GetEdge(BoxAreaMargin, BoxEdgeLeft), -b.GetEdge(BoxAreaMargin, BoxEdgeTop)}
	for i := 0; i < area; i++ {
		pos.X += b.areaEdges[i*4+BoxEdgeLeft]
		pos.Y += b.areaEdges[i*4+BoxEdgeTop]
	}
	return pos
}

// GetContentSize is Box::GetSize().
func (b *Box) GetContentSize() Vector2f { return b.content }

// GetSize is Box::GetSize(area).
func (b *Box) GetSize(area BoxArea) Vector2f {
	size := b.content
	for i := area; i <= BoxAreaPadding; i++ {
		size.X += b.areaEdges[i*4+BoxEdgeLeft] + b.areaEdges[i*4+BoxEdgeRight]
		size.Y += b.areaEdges[i*4+BoxEdgeTop] + b.areaEdges[i*4+BoxEdgeBottom]
	}
	return size
}

func (b *Box) SetContent(content Vector2f) { b.content = content }

func (b *Box) SetEdge(area BoxArea, edge BoxEdge, size float32) { b.areaEdges[area*4+edge] = size }

func (b *Box) GetEdge(area BoxArea, edge BoxEdge) float32 { return b.areaEdges[area*4+edge] }

func (b *Box) GetCumulativeEdge(area BoxArea, edge BoxEdge) float32 {
	var size float32 = 0
	maxArea := MathMinInt(area, BoxAreaPadding)
	for i := 0; i <= maxArea; i++ {
		size += b.areaEdges[i*4+edge]
	}
	return size
}

// GetSizeAcross sums the edges of areaOuter..areaInner across direction,
// plus the content when areaInner is Content.
func (b *Box) GetSizeAcross(direction BoxDirection, areaOuter BoxArea, areaInner BoxArea) float32 {
	var size float32 = 0
	if areaInner == BoxAreaContent {
		if direction == BoxDirectionHorizontal {
			size = b.content.X
		} else {
			size = b.content.Y
		}
	}
	for i := areaOuter; i <= areaInner && i < BoxAreaContent; i++ {
		size += b.areaEdges[i*4+BoxEdgeTop+direction] + b.areaEdges[i*4+BoxEdgeBottom+direction]
	}
	return size
}

func (b *Box) GetFrameSize(area BoxArea) Vector2f {
	if area == BoxAreaContent {
		return b.content
	}
	return Vector2f{
		b.areaEdges[area*4+BoxEdgeRight] + b.areaEdges[area*4+BoxEdgeLeft],
		b.areaEdges[area*4+BoxEdgeTop] + b.areaEdges[area*4+BoxEdgeBottom],
	}
}

func (b *Box) Equals(o *Box) bool {
	if !b.content.Equals(o.content) {
		return false
	}
	for i := 0; i < 12; i++ {
		if b.areaEdges[i] != o.areaEdges[i] {
			return false
		}
	}
	return true
}
