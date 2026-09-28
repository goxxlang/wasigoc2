// Port of RmlUi Include/RmlUi/Core/Matrix4.h and Matrix4.inl, for the
// default ColumnMajorMatrix4f storage (RMLUI_MATRIX_ROW_MAJOR unset).
package rmlui

// Matrix4f is Rml::Matrix4f with column-major storage: M[c*4+r] is row r of
// column c, so At(i, j) matches the C++ `m[i][j]` (column i, component j).
type Matrix4f struct {
	M [16]float32
}

// At is m[i][j]: column i, row j.
func (m *Matrix4f) At(i int, j int) float32 { return m.M[i*4+j] }

// Set is m[i][j] = v.
func (m *Matrix4f) Set(i int, j int, v float32) { m.M[i*4+j] = v }

// Data is data(): the 16 components in storage (column-major) order.
func (m *Matrix4f) Data() []float32 {
	out := make([]float32, 16)
	for i := 0; i < 16; i++ {
		out[i] = m.M[i]
	}
	return out
}

func (m *Matrix4f) GetColumn(c int) Vector4f {
	return Vector4f{m.M[c*4], m.M[c*4+1], m.M[c*4+2], m.M[c*4+3]}
}

func (m *Matrix4f) GetRow(r int) Vector4f {
	return Vector4f{m.M[r], m.M[4+r], m.M[8+r], m.M[12+r]}
}

func (m *Matrix4f) SetColumn(c int, v Vector4f) {
	m.M[c*4] = v.X
	m.M[c*4+1] = v.Y
	m.M[c*4+2] = v.Z
	m.M[c*4+3] = v.W
}

func (m *Matrix4f) SetRow(r int, v Vector4f) {
	m.M[r] = v.X
	m.M[4+r] = v.Y
	m.M[8+r] = v.Z
	m.M[12+r] = v.W
}

func Matrix4FromRows(r0 Vector4f, r1 Vector4f, r2 Vector4f, r3 Vector4f) Matrix4f {
	m := Matrix4f{}
	m.SetRow(0, r0)
	m.SetRow(1, r1)
	m.SetRow(2, r2)
	m.SetRow(3, r3)
	return m
}

func Matrix4FromColumns(c0 Vector4f, c1 Vector4f, c2 Vector4f, c3 Vector4f) Matrix4f {
	m := Matrix4f{}
	m.SetColumn(0, c0)
	m.SetColumn(1, c1)
	m.SetColumn(2, c2)
	m.SetColumn(3, c3)
	return m
}

func Matrix4FromRowMajor(c []float32) Matrix4f {
	m := Matrix4f{}
	for i := 0; i < 4; i++ {
		for j := 0; j < 4; j++ {
			m.M[j*4+i] = c[i*4+j]
		}
	}
	return m
}

func Matrix4FromColumnMajor(c []float32) Matrix4f {
	m := Matrix4f{}
	for i := 0; i < 16; i++ {
		m.M[i] = c[i]
	}
	return m
}

func Matrix4Diag(a float32, b float32, c float32, d float32) Matrix4f {
	return Matrix4FromRows(Vector4f{a, 0, 0, 0}, Vector4f{0, b, 0, 0}, Vector4f{0, 0, c, 0}, Vector4f{0, 0, 0, d})
}

func Matrix4Identity() Matrix4f { return Matrix4Diag(1, 1, 1, 1) }

func Matrix4ProjectOrtho(l float32, r float32, b float32, t float32, n float32, f float32) Matrix4f {
	return Matrix4FromRows(
		Vector4f{2 / (r - l), 0, 0, -(r + l) / (r - l)},
		Vector4f{0, 2 / (t - b), 0, -(t + b) / (t - b)},
		Vector4f{0, 0, 2 / (f - n), -(f + n) / (f - n)},
		Vector4f{0, 0, 0, 1})
}

func Matrix4ProjectPerspective(l float32, r float32, b float32, t float32, n float32, f float32) Matrix4f {
	return Matrix4FromRows(
		Vector4f{2 * n / (r - l), 0, (r + l) / (r - l), 0},
		Vector4f{0, 2 * n / (t - b), (t + b) / (t - b), 0},
		Vector4f{0, 0, -(f + n) / (f - n), -(2 * f * n) / (f - n)},
		Vector4f{0, 0, -1, 0})
}

func Matrix4Perspective(d float32) Matrix4f {
	return Matrix4FromRows(Vector4f{1, 0, 0, 0}, Vector4f{0, 1, 0, 0}, Vector4f{0, 0, 1, 0}, Vector4f{0, 0, -1.0 / d, 1})
}

func Matrix4Translate(x float32, y float32, z float32) Matrix4f {
	return Matrix4FromRows(Vector4f{1, 0, 0, x}, Vector4f{0, 1, 0, y}, Vector4f{0, 0, 1, z}, Vector4f{0, 0, 0, 1})
}

func Matrix4TranslateV(v Vector3f) Matrix4f { return Matrix4Translate(v.X, v.Y, v.Z) }
func Matrix4TranslateX(x float32) Matrix4f  { return Matrix4Translate(x, 0, 0) }
func Matrix4TranslateY(y float32) Matrix4f  { return Matrix4Translate(0, y, 0) }
func Matrix4TranslateZ(z float32) Matrix4f  { return Matrix4Translate(0, 0, z) }

func Matrix4Scale(x float32, y float32, z float32) Matrix4f { return Matrix4Diag(x, y, z, 1) }
func Matrix4ScaleX(x float32) Matrix4f                     { return Matrix4Scale(x, 1, 1) }
func Matrix4ScaleY(y float32) Matrix4f                     { return Matrix4Scale(1, y, 1) }
func Matrix4ScaleZ(z float32) Matrix4f                     { return Matrix4Scale(1, 1, z) }

func Matrix4Rotate(v Vector3f, angle float32) Matrix4f {
	n := v.Normalise()
	s := MathSin(angle)
	c := MathCos(angle)
	return Matrix4FromRows(
		Vector4f{n.X*n.X*(1-c) + c, n.X*n.Y*(1-c) - n.Z*s, n.X*n.Z*(1-c) + n.Y*s, 0},
		Vector4f{n.Y*n.X*(1-c) + n.Z*s, n.Y*n.Y*(1-c) + c, n.Y*n.Z*(1-c) - n.X*s, 0},
		Vector4f{n.Z*n.X*(1-c) - n.Y*s, n.Z*n.Y*(1-c) + n.X*s, n.Z*n.Z*(1-c) + c, 0},
		Vector4f{0, 0, 0, 1})
}

func Matrix4RotateX(angle float32) Matrix4f {
	s := MathSin(angle)
	c := MathCos(angle)
	return Matrix4FromRows(Vector4f{1, 0, 0, 0}, Vector4f{0, c, -s, 0}, Vector4f{0, s, c, 0}, Vector4f{0, 0, 0, 1})
}

func Matrix4RotateY(angle float32) Matrix4f {
	s := MathSin(angle)
	c := MathCos(angle)
	return Matrix4FromRows(Vector4f{c, 0, s, 0}, Vector4f{0, 1, 0, 0}, Vector4f{-s, 0, c, 0}, Vector4f{0, 0, 0, 1})
}

func Matrix4RotateZ(angle float32) Matrix4f {
	s := MathSin(angle)
	c := MathCos(angle)
	return Matrix4FromRows(Vector4f{c, -s, 0, 0}, Vector4f{s, c, 0, 0}, Vector4f{0, 0, 1, 0}, Vector4f{0, 0, 0, 1})
}

func Matrix4Skew(angleX float32, angleY float32) Matrix4f {
	sx := MathTan(angleX)
	sy := MathTan(angleY)
	return Matrix4FromRows(Vector4f{1, sx, 0, 0}, Vector4f{sy, 1, 0, 0}, Vector4f{0, 0, 1, 0}, Vector4f{0, 0, 0, 1})
}

func Matrix4SkewX(angle float32) Matrix4f { return Matrix4Skew(angle, 0) }
func Matrix4SkewY(angle float32) Matrix4f { return Matrix4Skew(0, angle) }

// Matrix4Compose is Matrix4::Compose from decomposed parts.
func Matrix4Compose(translation Vector3f, scale Vector3f, skew Vector3f, perspective Vector4f, quaternion Vector4f) Matrix4f {
	matrix := Matrix4Identity()
	for i := 0; i < 4; i++ {
		matrix.Set(i, 3, perspective.Index(i))
	}
	tr := []float32{translation.X, translation.Y, translation.Z}
	for i := 0; i < 4; i++ {
		for j := 0; j < 3; j++ {
			matrix.Set(3, i, matrix.At(3, i)+tr[j]*matrix.At(j, i))
		}
	}
	x := quaternion.X
	y := quaternion.Y
	z := quaternion.Z
	w := quaternion.W
	rotation := Matrix4FromRows(
		Vector4f{1.0 - 2.0*(y*y+z*z), 2.0 * (x*y - z*w), 2.0 * (x*z + y*w), 0},
		Vector4f{2.0 * (x*y + z*w), 1.0 - 2.0*(x*x+z*z), 2.0 * (y*z - x*w), 0},
		Vector4f{2.0 * (x*z - y*w), 2.0 * (y*z + x*w), 1.0 - 2.0*(x*x+y*y), 0},
		Vector4f{0, 0, 0, 1})
	matrix = matrix.Mul(rotation)
	temp := Matrix4Identity()
	if skew.Z != 0 {
		temp.Set(2, 1, skew.Z)
		matrix = matrix.Mul(temp)
	}
	if skew.Y != 0 {
		temp.Set(2, 1, 0)
		temp.Set(2, 0, skew.Y)
		matrix = matrix.Mul(temp)
	}
	if skew.X != 0 {
		temp.Set(2, 0, 0)
		temp.Set(1, 0, skew.X)
		matrix = matrix.Mul(temp)
	}
	sc := []float32{scale.X, scale.Y, scale.Z}
	for i := 0; i < 3; i++ {
		for j := 0; j < 4; j++ {
			matrix.Set(i, j, matrix.At(i, j)*sc[i])
		}
	}
	return matrix
}

// Mul is matrix * matrix.
func (m Matrix4f) Mul(o Matrix4f) Matrix4f {
	r := Matrix4f{}
	for i := 0; i < 4; i++ {
		lhsRow := m.GetRow(i)
		for j := 0; j < 4; j++ {
			r.M[j*4+i] = o.GetColumn(j).DotProduct(lhsRow)
		}
	}
	return r
}

// MulVector is matrix * vector.
func (m Matrix4f) MulVector(v Vector4f) Vector4f {
	return Vector4f{v.DotProduct(m.GetRow(0)), v.DotProduct(m.GetRow(1)), v.DotProduct(m.GetRow(2)), v.DotProduct(m.GetRow(3))}
}

// Scaled is matrix * scalar.
func (m Matrix4f) Scaled(s float32) Matrix4f {
	r := m
	for i := 0; i < 16; i++ {
		r.M[i] = r.M[i] * s
	}
	return r
}

func (m Matrix4f) Negate() Matrix4f { return m.Scaled(-1) }

func (m Matrix4f) Add(o Matrix4f) Matrix4f {
	r := m
	for i := 0; i < 16; i++ {
		r.M[i] = r.M[i] + o.M[i]
	}
	return r
}

func (m Matrix4f) Sub(o Matrix4f) Matrix4f {
	r := m
	for i := 0; i < 16; i++ {
		r.M[i] = r.M[i] - o.M[i]
	}
	return r
}

func (m Matrix4f) Equals(o Matrix4f) bool {
	for i := 0; i < 16; i++ {
		if m.M[i] != o.M[i] {
			return false
		}
	}
	return true
}

// Transpose transposes in place and returns the result (the C++ returns
// *this by reference).
func (m *Matrix4f) Transpose() Matrix4f {
	t := Matrix4f{}
	for i := 0; i < 4; i++ {
		for j := 0; j < 4; j++ {
			t.M[i*4+j] = m.M[j*4+i]
		}
	}
	m.M = t.M
	return *m
}

// Invert inverts in place (MESA GLU algorithm); false if singular.
func (m *Matrix4f) Invert() bool {
	s := m.M
	var d [16]float32
	d[0] = s[5]*s[10]*s[15] - s[5]*s[11]*s[14] - s[9]*s[6]*s[15] + s[9]*s[7]*s[14] + s[13]*s[6]*s[11] - s[13]*s[7]*s[10]
	d[4] = -s[4]*s[10]*s[15] + s[4]*s[11]*s[14] + s[8]*s[6]*s[15] - s[8]*s[7]*s[14] - s[12]*s[6]*s[11] + s[12]*s[7]*s[10]
	d[8] = s[4]*s[9]*s[15] - s[4]*s[11]*s[13] - s[8]*s[5]*s[15] + s[8]*s[7]*s[13] + s[12]*s[5]*s[11] - s[12]*s[7]*s[9]
	d[12] = -s[4]*s[9]*s[14] + s[4]*s[10]*s[13] + s[8]*s[5]*s[14] - s[8]*s[6]*s[13] - s[12]*s[5]*s[10] + s[12]*s[6]*s[9]
	d[1] = -s[1]*s[10]*s[15] + s[1]*s[11]*s[14] + s[9]*s[2]*s[15] - s[9]*s[3]*s[14] - s[13]*s[2]*s[11] + s[13]*s[3]*s[10]
	d[5] = s[0]*s[10]*s[15] - s[0]*s[11]*s[14] - s[8]*s[2]*s[15] + s[8]*s[3]*s[14] + s[12]*s[2]*s[11] - s[12]*s[3]*s[10]
	d[9] = -s[0]*s[9]*s[15] + s[0]*s[11]*s[13] + s[8]*s[1]*s[15] - s[8]*s[3]*s[13] - s[12]*s[1]*s[11] + s[12]*s[3]*s[9]
	d[13] = s[0]*s[9]*s[14] - s[0]*s[10]*s[13] - s[8]*s[1]*s[14] + s[8]*s[2]*s[13] + s[12]*s[1]*s[10] - s[12]*s[2]*s[9]
	d[2] = s[1]*s[6]*s[15] - s[1]*s[7]*s[14] - s[5]*s[2]*s[15] + s[5]*s[3]*s[14] + s[13]*s[2]*s[7] - s[13]*s[3]*s[6]
	d[6] = -s[0]*s[6]*s[15] + s[0]*s[7]*s[14] + s[4]*s[2]*s[15] - s[4]*s[3]*s[14] - s[12]*s[2]*s[7] + s[12]*s[3]*s[6]
	d[10] = s[0]*s[5]*s[15] - s[0]*s[7]*s[13] - s[4]*s[1]*s[15] + s[4]*s[3]*s[13] + s[12]*s[1]*s[7] - s[12]*s[3]*s[5]
	d[14] = -s[0]*s[5]*s[14] + s[0]*s[6]*s[13] + s[4]*s[1]*s[14] - s[4]*s[2]*s[13] - s[12]*s[1]*s[6] + s[12]*s[2]*s[5]
	d[3] = -s[1]*s[6]*s[11] + s[1]*s[7]*s[10] + s[5]*s[2]*s[11] - s[5]*s[3]*s[10] - s[9]*s[2]*s[7] + s[9]*s[3]*s[6]
	d[7] = s[0]*s[6]*s[11] - s[0]*s[7]*s[10] - s[4]*s[2]*s[11] + s[4]*s[3]*s[10] + s[8]*s[2]*s[7] - s[8]*s[3]*s[6]
	d[11] = -s[0]*s[5]*s[11] + s[0]*s[7]*s[9] + s[4]*s[1]*s[11] - s[4]*s[3]*s[9] - s[8]*s[1]*s[7] + s[8]*s[3]*s[5]
	d[15] = s[0]*s[5]*s[10] - s[0]*s[6]*s[9] - s[4]*s[1]*s[10] + s[4]*s[2]*s[9] + s[8]*s[1]*s[6] - s[8]*s[2]*s[5]
	det := s[0]*d[0] + s[1]*d[4] + s[2]*d[8] + s[3]*d[12]
	if det == 0 {
		return false
	}
	inv := 1.0 / det
	for i := 0; i < 16; i++ {
		m.M[i] = d[i] * inv
	}
	return true
}

func (m *Matrix4f) Determinant() float32 {
	s := m.M
	d0 := s[5]*s[10]*s[15] - s[5]*s[11]*s[14] - s[9]*s[6]*s[15] + s[9]*s[7]*s[14] + s[13]*s[6]*s[11] - s[13]*s[7]*s[10]
	d1 := -s[4]*s[10]*s[15] + s[4]*s[11]*s[14] + s[8]*s[6]*s[15] - s[8]*s[7]*s[14] - s[12]*s[6]*s[11] + s[12]*s[7]*s[10]
	d2 := s[4]*s[9]*s[15] - s[4]*s[11]*s[13] - s[8]*s[5]*s[15] + s[8]*s[7]*s[13] + s[12]*s[5]*s[11] - s[12]*s[7]*s[9]
	d3 := -s[4]*s[9]*s[14] + s[4]*s[10]*s[13] + s[8]*s[5]*s[14] - s[8]*s[6]*s[13] - s[12]*s[5]*s[10] + s[12]*s[6]*s[9]
	return s[0]*d0 + s[1]*d1 + s[2]*d2 + s[3]*d3
}
