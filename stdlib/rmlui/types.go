// Port of RmlUi Include/RmlUi/Core/Vector2.h, Vector3.h, Vector4.h,
// Colour.h, Rectangle.h, and the aliases in Types.h.
package rmlui

// Vector2f is Rml::Vector2f.
type Vector2f struct {
	X float32
	Y float32
}

func (v Vector2f) Add(o Vector2f) Vector2f      { return Vector2f{v.X + o.X, v.Y + o.Y} }
func (v Vector2f) Sub(o Vector2f) Vector2f      { return Vector2f{v.X - o.X, v.Y - o.Y} }
func (v Vector2f) Mul(s float32) Vector2f       { return Vector2f{v.X * s, v.Y * s} }
func (v Vector2f) MulV(o Vector2f) Vector2f     { return Vector2f{v.X * o.X, v.Y * o.Y} }
func (v Vector2f) Div(s float32) Vector2f       { return Vector2f{v.X / s, v.Y / s} }
func (v Vector2f) DivV(o Vector2f) Vector2f     { return Vector2f{v.X / o.X, v.Y / o.Y} }
func (v Vector2f) Neg() Vector2f                { return Vector2f{-v.X, -v.Y} }
func (v Vector2f) Equals(o Vector2f) bool       { return v.X == o.X && v.Y == o.Y }
func (v Vector2f) DotProduct(o Vector2f) float32 { return v.X*o.X + v.Y*o.Y }
func (v Vector2f) SquaredMagnitude() float32    { return v.X*v.X + v.Y*v.Y }
func (v Vector2f) Magnitude() float32           { return MathSquareRoot(v.SquaredMagnitude()) }
func (v Vector2f) CrossProduct(o Vector2f) float32 { return v.X*o.Y - v.Y*o.X }

func (v Vector2f) Normalise() Vector2f {
	m := v.Magnitude()
	if MathIsCloseToZero(m) {
		return v
	}
	return v.Div(m)
}

func (v Vector2f) Round() Vector2f { return Vector2f{MathRound(v.X), MathRound(v.Y)} }

// Normal returns the vector rotated 90 degrees clockwise (Vector2::Normal).
func (v Vector2f) Normal() Vector2f { return Vector2f{v.Y, -v.X} }

func (v Vector2f) Rotate(theta float32) Vector2f {
	c := MathCos(theta)
	s := MathSin(theta)
	return Vector2f{c*v.X - s*v.Y, s*v.X + c*v.Y}
}

func (v Vector2f) ToInt() Vector2i { return Vector2i{int(v.X), int(v.Y)} }

func (v Vector2i) Add(o Vector2i) Vector2i  { return Vector2i{v.X + o.X, v.Y + o.Y} }
func (v Vector2i) Sub(o Vector2i) Vector2i  { return Vector2i{v.X - o.X, v.Y - o.Y} }
func (v Vector2i) Mul(s int) Vector2i       { return Vector2i{v.X * s, v.Y * s} }
func (v Vector2i) Equals(o Vector2i) bool   { return v.X == o.X && v.Y == o.Y }
func (v Vector2i) ToFloat() Vector2f        { return Vector2f{float32(v.X), float32(v.Y)} }

// Vector3f is Rml::Vector3f.
type Vector3f struct {
	X float32
	Y float32
	Z float32
}

func (v Vector3f) Add(o Vector3f) Vector3f  { return Vector3f{v.X + o.X, v.Y + o.Y, v.Z + o.Z} }
func (v Vector3f) Sub(o Vector3f) Vector3f  { return Vector3f{v.X - o.X, v.Y - o.Y, v.Z - o.Z} }
func (v Vector3f) Mul(s float32) Vector3f   { return Vector3f{v.X * s, v.Y * s, v.Z * s} }
func (v Vector3f) DotProduct(o Vector3f) float32 { return v.X*o.X + v.Y*o.Y + v.Z*o.Z }

func (v Vector3f) CrossProduct(o Vector3f) Vector3f {
	return Vector3f{v.Y*o.Z - v.Z*o.Y, v.Z*o.X - v.X*o.Z, v.X*o.Y - v.Y*o.X}
}

func (v Vector3f) Magnitude() float32 { return MathSquareRoot(v.DotProduct(v)) }

func (v Vector3f) Normalise() Vector3f {
	m := v.Magnitude()
	if MathIsCloseToZero(m) {
		return v
	}
	return v.Mul(1.0 / m)
}

// Vector4f is Rml::Vector4f.
type Vector4f struct {
	X float32
	Y float32
	Z float32
	W float32
}

func (v Vector4f) Add(o Vector4f) Vector4f { return Vector4f{v.X + o.X, v.Y + o.Y, v.Z + o.Z, v.W + o.W} }
func (v Vector4f) Sub(o Vector4f) Vector4f { return Vector4f{v.X - o.X, v.Y - o.Y, v.Z - o.Z, v.W - o.W} }
func (v Vector4f) Mul(s float32) Vector4f  { return Vector4f{v.X * s, v.Y * s, v.Z * s, v.W * s} }
func (v Vector4f) DotProduct(o Vector4f) float32 {
	return v.X*o.X + v.Y*o.Y + v.Z*o.Z + v.W*o.W
}

func (v Vector4f) Index(i int) float32 {
	switch i {
	case 0:
		return v.X
	case 1:
		return v.Y
	case 2:
		return v.Z
	}
	return v.W
}

func (v Vector4f) PerspectiveDivide() Vector3f {
	return Vector3f{v.X / v.W, v.Y / v.W, v.Z / v.W}
}

// Colourb is Rml::Colourb: straight (non-premultiplied) alpha bytes.
type Colourb struct {
	Red   byte
	Green byte
	Blue  byte
	Alpha byte
}

func NewColourb(r byte, g byte, b byte) Colourb { return Colourb{r, g, b, 255} }

func (c Colourb) Equals(o Colourb) bool {
	return c.Red == o.Red && c.Green == o.Green && c.Blue == o.Blue && c.Alpha == o.Alpha
}

func (c Colourb) Index(i int) byte {
	switch i {
	case 0:
		return c.Red
	case 1:
		return c.Green
	case 2:
		return c.Blue
	}
	return c.Alpha
}

func (c *Colourb) SetIndex(i int, v byte) {
	switch i {
	case 0:
		c.Red = v
	case 1:
		c.Green = v
	case 2:
		c.Blue = v
	default:
		c.Alpha = v
	}
}

// Mul is Colourb::operator*(float): channel-wise truncating scale.
func (c Colourb) Mul(s float32) Colourb {
	return Colourb{byte(float32(c.Red) * s), byte(float32(c.Green) * s), byte(float32(c.Blue) * s), byte(float32(c.Alpha) * s)}
}

func (c Colourb) ToPremultiplied() ColourbPremultiplied {
	a := int(c.Alpha)
	return ColourbPremultiplied{byte(int(c.Red) * a / 255), byte(int(c.Green) * a / 255), byte(int(c.Blue) * a / 255), c.Alpha}
}

func (c Colourb) ToPremultipliedOpacity(opacity float32) ColourbPremultiplied {
	newAlpha := float32(c.Alpha) * opacity
	return ColourbPremultiplied{
		byte(float32(c.Red) * (newAlpha / 255.0)),
		byte(float32(c.Green) * (newAlpha / 255.0)),
		byte(float32(c.Blue) * (newAlpha / 255.0)),
		byte(newAlpha),
	}
}

// ColourbPremultiplied is Rml::ColourbPremultiplied, the vertex colour type.
type ColourbPremultiplied struct {
	Red   byte
	Green byte
	Blue  byte
	Alpha byte
}

func (c ColourbPremultiplied) Equals(o ColourbPremultiplied) bool {
	return c.Red == o.Red && c.Green == o.Green && c.Blue == o.Blue && c.Alpha == o.Alpha
}

func (c ColourbPremultiplied) Mul(s float32) ColourbPremultiplied {
	return ColourbPremultiplied{byte(float32(c.Red) * s), byte(float32(c.Green) * s), byte(float32(c.Blue) * s), byte(float32(c.Alpha) * s)}
}

func (c ColourbPremultiplied) ToNonPremultiplied() Colourb {
	a := int(c.Alpha)
	if a == 0 {
		return Colourb{0, 0, 0, 0}
	}
	return Colourb{byte(int(c.Red) * 255 / a), byte(int(c.Green) * 255 / a), byte(int(c.Blue) * 255 / a), c.Alpha}
}

// Colourf is Rml::Colourf.
type Colourf struct {
	Red   float32
	Green float32
	Blue  float32
	Alpha float32
}

func (c Colourf) Add(o Colourf) Colourf {
	return Colourf{c.Red + o.Red, c.Green + o.Green, c.Blue + o.Blue, c.Alpha + o.Alpha}
}

func (c Colourf) Mul(s float32) Colourf { return Colourf{c.Red * s, c.Green * s, c.Blue * s, c.Alpha * s} }

// Rectanglef is Rml::Rectanglef. P0 is the top-left (minimum) corner, P1
// the bottom-right (maximum) corner.
type Rectanglef struct {
	P0 Vector2f
	P1 Vector2f
}

func RectanglefFromPosition(pos Vector2f) Rectanglef { return Rectanglef{pos, pos} }
func RectanglefFromPositionSize(pos Vector2f, size Vector2f) Rectanglef {
	return Rectanglef{pos, pos.Add(size)}
}
func RectanglefFromSize(size Vector2f) Rectanglef { return Rectanglef{Vector2f{}, size} }
func RectanglefFromCorners(topLeft Vector2f, bottomRight Vector2f) Rectanglef {
	return Rectanglef{topLeft, bottomRight}
}
func RectanglefMakeInvalid() Rectanglef { return Rectanglef{Vector2f{0, 0}, Vector2f{-1, -1}} }

func (r Rectanglef) Position() Vector2f    { return r.P0 }
func (r Rectanglef) Size() Vector2f        { return r.P1.Sub(r.P0) }
func (r Rectanglef) TopLeft() Vector2f     { return r.P0 }
func (r Rectanglef) TopRight() Vector2f    { return Vector2f{r.P1.X, r.P0.Y} }
func (r Rectanglef) BottomRight() Vector2f { return r.P1 }
func (r Rectanglef) BottomLeft() Vector2f  { return Vector2f{r.P0.X, r.P1.Y} }
func (r Rectanglef) Center() Vector2f      { return r.P0.Add(r.P1).Div(2) }
func (r Rectanglef) Left() float32         { return r.P0.X }
func (r Rectanglef) Right() float32        { return r.P1.X }
func (r Rectanglef) Top() float32          { return r.P0.Y }
func (r Rectanglef) Bottom() float32       { return r.P1.Y }
func (r Rectanglef) Width() float32        { return r.P1.X - r.P0.X }
func (r Rectanglef) Height() float32       { return r.P1.Y - r.P0.Y }

func (r Rectanglef) Extend(v float32) Rectanglef {
	return Rectanglef{r.P0.Sub(Vector2f{v, v}), r.P1.Add(Vector2f{v, v})}
}
func (r Rectanglef) ExtendV(v Vector2f) Rectanglef { return Rectanglef{r.P0.Sub(v), r.P1.Add(v)} }
func (r Rectanglef) ExtendCorners(topLeft Vector2f, bottomRight Vector2f) Rectanglef {
	return Rectanglef{r.P0.Sub(topLeft), r.P1.Add(bottomRight)}
}
func (r Rectanglef) Translate(v Vector2f) Rectanglef { return Rectanglef{r.P0.Add(v), r.P1.Add(v)} }
func (r Rectanglef) JoinPoint(p Vector2f) Rectanglef {
	return Rectanglef{MathMinVector(r.P0, p), MathMaxVector(r.P1, p)}
}
func (r Rectanglef) Join(o Rectanglef) Rectanglef {
	return Rectanglef{MathMinVector(r.P0, o.P0), MathMaxVector(r.P1, o.P1)}
}

func (r Rectanglef) Intersect(o Rectanglef) Rectanglef {
	result := Rectanglef{MathMaxVector(r.P0, o.P0), MathMinVector(r.P1, o.P1)}
	result.P1 = MathMaxVector(result.P0, result.P1)
	return result
}

func (r Rectanglef) IntersectIfValid(o Rectanglef) Rectanglef {
	if !r.Valid() || !o.Valid() {
		return r
	}
	return r.Intersect(o)
}

func (r Rectanglef) Intersects(o Rectanglef) bool {
	return r.P0.X < o.P1.X && r.P1.X > o.P0.X && r.P0.Y < o.P1.Y && r.P1.Y > o.P0.Y
}

func (r Rectanglef) Contains(p Vector2f) bool {
	return p.X >= r.P0.X && p.X <= r.P1.X && p.Y >= r.P0.Y && p.Y <= r.P1.Y
}

func (r Rectanglef) Valid() bool { return r.P0.X <= r.P1.X && r.P0.Y <= r.P1.Y }

func (r Rectanglef) Equals(o Rectanglef) bool { return r.P0.Equals(o.P0) && r.P1.Equals(o.P1) }

func (r Rectanglef) ToInt() Rectanglei { return Rectanglei{r.P0.ToInt(), r.P1.ToInt()} }

// Rectanglei is Rml::Rectanglei.
type Rectanglei struct {
	P0 Vector2i
	P1 Vector2i
}

func RectangleiFromPositionSize(pos Vector2i, size Vector2i) Rectanglei {
	return Rectanglei{pos, pos.Add(size)}
}
func RectangleiFromCorners(topLeft Vector2i, bottomRight Vector2i) Rectanglei {
	return Rectanglei{topLeft, bottomRight}
}
func RectangleiMakeInvalid() Rectanglei { return Rectanglei{Vector2i{0, 0}, Vector2i{-1, -1}} }

func (r Rectanglei) Position() Vector2i { return r.P0 }
func (r Rectanglei) Size() Vector2i     { return r.P1.Sub(r.P0) }
func (r Rectanglei) Left() int          { return r.P0.X }
func (r Rectanglei) Right() int         { return r.P1.X }
func (r Rectanglei) Top() int           { return r.P0.Y }
func (r Rectanglei) Bottom() int        { return r.P1.Y }
func (r Rectanglei) Width() int         { return r.P1.X - r.P0.X }
func (r Rectanglei) Height() int        { return r.P1.Y - r.P0.Y }
func (r Rectanglei) Valid() bool        { return r.P0.X <= r.P1.X && r.P0.Y <= r.P1.Y }
func (r Rectanglei) Equals(o Rectanglei) bool {
	return r.P0.Equals(o.P0) && r.P1.Equals(o.P1)
}

func (r Rectanglei) Intersect(o Rectanglei) Rectanglei {
	p0 := Vector2i{MathMaxInt(r.P0.X, o.P0.X), MathMaxInt(r.P0.Y, o.P0.Y)}
	p1 := Vector2i{MathMinInt(r.P1.X, o.P1.X), MathMinInt(r.P1.Y, o.P1.Y)}
	p1 = Vector2i{MathMaxInt(p0.X, p1.X), MathMaxInt(p0.Y, p1.Y)}
	return Rectanglei{p0, p1}
}

func (r Rectanglei) IntersectIfValid(o Rectanglei) Rectanglei {
	if !r.Valid() || !o.Valid() {
		return r
	}
	return r.Intersect(o)
}

func (r Rectanglei) ToFloat() Rectanglef { return Rectanglef{r.P0.ToFloat(), r.P1.ToFloat()} }

// CornerSizes is Rml::CornerSizes: top-left, top-right, bottom-right,
// bottom-left radii.
type CornerSizes = [4]float32

// NewCornerSizes builds a CornerSizes; wasigoc has no composite literal for
// a named array type.
func NewCornerSizes(topLeft float32, topRight float32, bottomRight float32, bottomLeft float32) CornerSizes {
	var c CornerSizes
	c[0] = topLeft
	c[1] = topRight
	c[2] = bottomRight
	c[3] = bottomLeft
	return c
}

const (
	CornerTopLeft     = 0
	CornerTopRight    = 1
	CornerBottomRight = 2
	CornerBottomLeft  = 3
)

// Character is Rml::Character: a Unicode code point.
type Character = rune

const CharacterNull Character = 0
