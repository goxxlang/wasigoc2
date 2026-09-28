// Port of RmlUi Source/Core/MeshUtilities.cpp, GeometryBackgroundBorder.cpp
// and their headers.
package rmlui

// MeshGenerateQuad is MeshUtilities::GenerateQuad with default texcoords.
func MeshGenerateQuad(mesh *Mesh, origin Vector2f, dimensions Vector2f, colour ColourbPremultiplied) {
	MeshGenerateQuadTex(mesh, origin, dimensions, colour, Vector2f{0, 0}, Vector2f{1, 1})
}

// MeshGenerateQuadTex is MeshUtilities::GenerateQuad with texcoords.
func MeshGenerateQuadTex(mesh *Mesh, origin Vector2f, dimensions Vector2f, colour ColourbPremultiplied, topLeftTex Vector2f, bottomRightTex Vector2f) {
	v0 := len(mesh.Vertices)
	mesh.Vertices = append(mesh.Vertices,
		Vertex{origin, colour, topLeftTex},
		Vertex{Vector2f{origin.X + dimensions.X, origin.Y}, colour, Vector2f{bottomRightTex.X, topLeftTex.Y}},
		Vertex{origin.Add(dimensions), colour, bottomRightTex},
		Vertex{Vector2f{origin.X, origin.Y + dimensions.Y}, colour, Vector2f{topLeftTex.X, bottomRightTex.Y}})
	mesh.Indices = append(mesh.Indices, v0+0, v0+3, v0+1, v0+1, v0+3, v0+2)
}

// MeshGenerateLine is MeshUtilities::GenerateLine (pixel-snapped quad).
func MeshGenerateLine(mesh *Mesh, position Vector2f, size Vector2f, color ColourbPremultiplied) {
	position, size = MathSnapToPixelGridVector(position, size)
	MeshGenerateQuad(mesh, position, size, color)
}

// MeshGenerateBackgroundBorder is MeshUtilities::GenerateBackgroundBorder.
func MeshGenerateBackgroundBorder(mesh *Mesh, renderBox RenderBox, backgroundColor ColourbPremultiplied, borderColors []ColourbPremultiplied) {
	borderWidths := renderBox.BorderWidths
	numBorders := 0
	for i := 0; i < 4; i++ {
		if borderColors[i].Alpha > 0 && borderWidths[i] > 0 {
			numBorders++
		}
	}
	fillSize := renderBox.FillSize
	hasBackground := backgroundColor.Alpha > 0 && fillSize.X > 0 && fillSize.Y > 0
	hasBorder := numBorders > 0
	if !hasBackground && !hasBorder {
		return
	}
	g := &geometryBackgroundBorder{mesh: mesh}
	metrics := computeBorderMetrics(renderBox.BorderOffset, borderWidths, fillSize, renderBox.BorderRadius)
	if hasBackground {
		g.drawBackground(metrics, backgroundColor)
	}
	if hasBorder {
		g.drawBorder(metrics, borderWidths, borderColors)
	}
}

// MeshGenerateBackground is MeshUtilities::GenerateBackground.
func MeshGenerateBackground(mesh *Mesh, renderBox RenderBox, color ColourbPremultiplied) {
	fillSize := renderBox.FillSize
	if !(color.Alpha > 0 && fillSize.X > 0 && fillSize.Y > 0) {
		return
	}
	metrics := computeBorderMetrics(renderBox.BorderOffset, renderBox.BorderWidths, fillSize, renderBox.BorderRadius)
	g := &geometryBackgroundBorder{mesh: mesh}
	g.drawBackground(metrics, color)
}

const (
	edgeTop    = 0
	edgeRight  = 1
	edgeBottom = 2
	edgeLeft   = 3
)

const (
	cornerTopLeft     = 0
	cornerTopRight    = 1
	cornerBottomRight = 2
	cornerBottomLeft  = 3
)

// BorderMetrics is Rml::BorderMetrics: inner and outer rectangles with
// possibly rounded corners.
type BorderMetrics struct {
	PositionsOuter        [4]Vector2f
	PositionsInner        [4]Vector2f
	PositionsCircleCenter [4]Vector2f
	OuterRadii            [4]float32
	InnerRadii            [4]Vector2f
}

func computeBorderMetrics(outerPosition Vector2f, edgeSizes EdgeSizes, innerSize Vector2f, outerRadiiDef CornerSizes) BorderMetrics {
	m := BorderMetrics{}
	innerPosition := outerPosition.Add(Vector2f{edgeSizes[edgeLeft], edgeSizes[edgeTop]})
	outerSize := innerSize.Add(Vector2f{edgeSizes[edgeLeft] + edgeSizes[edgeRight], edgeSizes[edgeTop] + edgeSizes[edgeBottom]})
	m.PositionsOuter[0] = outerPosition
	m.PositionsOuter[1] = outerPosition.Add(Vector2f{outerSize.X, 0})
	m.PositionsOuter[2] = outerPosition.Add(outerSize)
	m.PositionsOuter[3] = outerPosition.Add(Vector2f{0, outerSize.Y})
	m.PositionsInner[0] = innerPosition
	m.PositionsInner[1] = innerPosition.Add(Vector2f{innerSize.X, 0})
	m.PositionsInner[2] = innerPosition.Add(innerSize)
	m.PositionsInner[3] = innerPosition.Add(Vector2f{0, innerSize.Y})
	sum := outerRadiiDef[0] + outerRadiiDef[1] + outerRadiiDef[2] + outerRadiiDef[3]
	if sum > 1 {
		r := outerRadiiDef
		scale := FltMax
		scale = MathMin(scale, innerSize.X/(r[cornerTopLeft]+r[cornerTopRight]))
		scale = MathMin(scale, innerSize.Y/(r[cornerTopRight]+r[cornerBottomRight]))
		scale = MathMin(scale, innerSize.X/(r[cornerBottomRight]+r[cornerBottomLeft]))
		scale = MathMin(scale, innerSize.Y/(r[cornerBottomLeft]+r[cornerTopLeft]))
		scale = MathMin(1.0, scale)
		for i := 0; i < 4; i++ {
			m.OuterRadii[i] = MathRound(r[i] * scale)
		}
		o := m.OuterRadii
		m.PositionsCircleCenter[0] = m.PositionsOuter[cornerTopLeft].Add(Vector2f{1, 1}.Mul(o[cornerTopLeft]))
		m.PositionsCircleCenter[1] = m.PositionsOuter[cornerTopRight].Add(Vector2f{-1, 1}.Mul(o[cornerTopRight]))
		m.PositionsCircleCenter[2] = m.PositionsOuter[cornerBottomRight].Add(Vector2f{-1, -1}.Mul(o[cornerBottomRight]))
		m.PositionsCircleCenter[3] = m.PositionsOuter[cornerBottomLeft].Add(Vector2f{1, -1}.Mul(o[cornerBottomLeft]))
		m.InnerRadii[0] = Vector2f{o[cornerTopLeft], o[cornerTopLeft]}.Sub(Vector2f{edgeSizes[edgeLeft], edgeSizes[edgeTop]})
		m.InnerRadii[1] = Vector2f{o[cornerTopRight], o[cornerTopRight]}.Sub(Vector2f{edgeSizes[edgeRight], edgeSizes[edgeTop]})
		m.InnerRadii[2] = Vector2f{o[cornerBottomRight], o[cornerBottomRight]}.Sub(Vector2f{edgeSizes[edgeRight], edgeSizes[edgeBottom]})
		m.InnerRadii[3] = Vector2f{o[cornerBottomLeft], o[cornerBottomLeft]}.Sub(Vector2f{edgeSizes[edgeLeft], edgeSizes[edgeBottom]})
	}
	return m
}

// ComputeBorderMetrics is GeometryBackgroundBorder::ComputeBorderMetrics.
func ComputeBorderMetrics(outerPosition Vector2f, edgeSizes EdgeSizes, innerSize Vector2f, outerRadii CornerSizes) BorderMetrics {
	return computeBorderMetrics(outerPosition, edgeSizes, innerSize, outerRadii)
}

// geometryBackgroundBorder is Rml::GeometryBackgroundBorder, drawing into
// a mesh.
type geometryBackgroundBorder struct {
	mesh *Mesh
}

func (g *geometryBackgroundBorder) drawBackground(m BorderMetrics, color ColourbPremultiplied) {
	offset := len(g.mesh.Vertices)
	for corner := 0; corner < 4; corner++ {
		g.drawBackgroundCorner(corner, m.PositionsInner[corner], m.PositionsCircleCenter[corner], m.OuterRadii[corner], m.InnerRadii[corner], color)
	}
	g.fillBackground(offset)
}

func (g *geometryBackgroundBorder) drawBorder(m BorderMetrics, edgeSizes EdgeSizes, borderColors []ColourbPremultiplied) {
	offset := len(g.mesh.Vertices)
	var drawEdge [4]bool
	for i := 0; i < 4; i++ {
		drawEdge[i] = edgeSizes[i] > 0 && borderColors[i].Alpha > 0
	}
	var drawCorner [4]bool
	drawCorner[0] = drawEdge[edgeTop] || drawEdge[edgeLeft]
	drawCorner[1] = drawEdge[edgeTop] || drawEdge[edgeRight]
	drawCorner[2] = drawEdge[edgeBottom] || drawEdge[edgeRight]
	drawCorner[3] = drawEdge[edgeBottom] || drawEdge[edgeLeft]
	for corner := 0; corner < 4; corner++ {
		edge0 := (corner + 3) % 4
		edge1 := corner
		if drawCorner[corner] {
			g.drawBorderCorner(corner, m.PositionsOuter[corner], m.PositionsInner[corner], m.PositionsCircleCenter[corner],
				m.OuterRadii[corner], m.InnerRadii[corner], borderColors[edge0], borderColors[edge1])
		}
		if drawEdge[edge1] {
			if edge1 == edgeLeft {
				g.fillEdge(offset)
			} else {
				g.fillEdge(len(g.mesh.Vertices))
			}
		}
	}
}

func (g *geometryBackgroundBorder) drawBackgroundCorner(corner int, posInner Vector2f, posCircleCenter Vector2f, R float32, r Vector2f, color ColourbPremultiplied) {
	if R == 0 || r.X <= 0 || r.Y <= 0 {
		g.drawPoint(posInner, color)
	} else if r.X > 0 && r.Y > 0 {
		a0 := float32(corner+2) * 0.5 * Pi
		a1 := float32(corner+3) * 0.5 * Pi
		g.drawArc(posCircleCenter, r, a0, a1, color, color, g.getNumPoints(R))
	}
}

func (g *geometryBackgroundBorder) drawPoint(pos Vector2f, color ColourbPremultiplied) {
	g.mesh.Vertices = append(g.mesh.Vertices, Vertex{Position: pos, Colour: color})
}

func (g *geometryBackgroundBorder) drawArc(center Vector2f, r Vector2f, a0 float32, a1 float32, color0 ColourbPremultiplied, color1 ColourbPremultiplied, numPoints int) {
	for i := 0; i < numPoints; i++ {
		t := float32(i) / float32(numPoints-1)
		a := MathLerp(t, a0, a1)
		unit := Vector2f{MathCos(a), MathSin(a)}
		g.mesh.Vertices = append(g.mesh.Vertices, Vertex{Position: unit.MulV(r).Add(center), Colour: MathRoundedLerp(t, color0, color1)})
	}
}

func (g *geometryBackgroundBorder) fillBackground(indexStart int) {
	added := len(g.mesh.Vertices) - indexStart
	numTriangles := added - 2
	for i := 0; i < numTriangles; i++ {
		g.mesh.Indices = append(g.mesh.Indices, indexStart, indexStart+i+2, indexStart+i+1)
	}
}

func (g *geometryBackgroundBorder) drawBorderCorner(corner int, posOuter Vector2f, posInner Vector2f, posCircleCenter Vector2f, R float32, r Vector2f, color0 ColourbPremultiplied, color1 ColourbPremultiplied) {
	a0 := float32(corner+2) * 0.5 * Pi
	a1 := float32(corner+3) * 0.5 * Pi
	if R == 0 {
		g.drawPointPoint(posOuter, posInner, color0, color1)
	} else if r.X > 0 && r.Y > 0 {
		g.drawArcArc(posCircleCenter, R, r, a0, a1, color0, color1, g.getNumPoints(R))
	} else {
		g.drawArcPoint(posCircleCenter, posInner, R, a0, a1, color0, color1, g.getNumPoints(R))
	}
}

func (g *geometryBackgroundBorder) drawPointPoint(posOuter Vector2f, posInner Vector2f, color0 ColourbPremultiplied, color1 ColourbPremultiplied) {
	g.drawPoint(posInner, color0)
	g.drawPoint(posOuter, color0)
	if !color0.Equals(color1) {
		g.drawPoint(posInner, color1)
		g.drawPoint(posOuter, color1)
	}
}

func (g *geometryBackgroundBorder) drawArcArc(center Vector2f, R float32, r Vector2f, a0 float32, a1 float32, color0 ColourbPremultiplied, color1 ColourbPremultiplied, numPoints int) {
	numTriangles := 2 * (numPoints - 1)
	offset := len(g.mesh.Vertices)
	for i := 0; i < numPoints; i++ {
		t := float32(i) / float32(numPoints-1)
		a := MathLerp(t, a0, a1)
		color := MathRoundedLerp(t, color0, color1)
		unit := Vector2f{MathCos(a), MathSin(a)}
		g.mesh.Vertices = append(g.mesh.Vertices,
			Vertex{Position: unit.MulV(r).Add(center), Colour: color},
			Vertex{Position: unit.Mul(R).Add(center), Colour: color})
	}
	for i := 0; i < numTriangles; i += 2 {
		g.mesh.Indices = append(g.mesh.Indices,
			offset+i+0, offset+i+2, offset+i+1,
			offset+i+1, offset+i+2, offset+i+3)
	}
}

func (g *geometryBackgroundBorder) drawArcPoint(center Vector2f, posInner Vector2f, R float32, a0 float32, a1 float32, color0 ColourbPremultiplied, color1 ColourbPremultiplied, numPoints int) {
	offset := len(g.mesh.Vertices)
	g.drawPoint(posInner, color0)
	g.drawArc(center, Vector2f{R, R}, a0, a1, color0, color1, numPoints)
	g.drawPoint(posInner, color1)
	last := len(g.mesh.Vertices) - 1
	// Swap positions so the outer edge vertex is last.
	tmp := g.mesh.Vertices[last-1].Position
	g.mesh.Vertices[last-1].Position = g.mesh.Vertices[last].Position
	g.mesh.Vertices[last].Position = tmp
	numTriangles := numPoints - 1
	inner0 := offset
	inner1 := last - 1
	offsetIndices := len(g.mesh.Indices)
	for i := 0; i < numTriangles; i++ {
		first := inner0
		if i > numTriangles/2 {
			first = inner1
		}
		g.mesh.Indices = append(g.mesh.Indices, first, offset+i+2, offset+i+1)
	}
	g.mesh.Indices[offsetIndices+3*(numTriangles-1)+1] = last
}

func (g *geometryBackgroundBorder) fillEdge(indexNextCorner int) {
	n := len(g.mesh.Vertices)
	g.mesh.Indices = append(g.mesh.Indices,
		n-2, indexNextCorner, n-1,
		n-1, indexNextCorner, indexNextCorner+1)
}

func (g *geometryBackgroundBorder) getNumPoints(R float32) int {
	return MathClampInt(3+MathRoundToInteger(R/6.0), 2, 100)
}
