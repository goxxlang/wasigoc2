// Port of RmlUi Source/Core/TransformUtilities.cpp.
package rmlui

func combine3(a Vector3f, b Vector3f, aScale float32, bScale float32) Vector3f {
	return Vector3f{aScale*a.X + bScale*b.X, aScale*a.Y + bScale*b.Y, aScale*a.Z + bScale*b.Z}
}

// quaternionSlerp interpolates two quaternions with weight alpha in [0, 1].
func quaternionSlerp(a Vector4f, b Vector4f, alpha float32) Vector4f {
	var eps float32 = 0.9995
	dot := MathClamp(a.DotProduct(b), -1, 1)
	if dot > eps {
		return a
	}
	theta := MathACos(dot)
	w := MathSin(alpha*theta) / MathSquareRoot(1.0-dot*dot)
	aScale := MathCos(alpha*theta) - dot*w
	return Vector4f{a.X*aScale + b.X*w, a.Y*aScale + b.Y*w, a.Z*aScale + b.Z*w, a.W*aScale + b.W*w}
}

func transformResolveWidth(value NumericValue, e *Element) float32 {
	if value.Unit == UnitPX || value.Unit == UnitNUMBER {
		return value.Number
	}
	return e.ResolveNumericValue(value, e.GetBox().GetSize(BoxAreaBorder).X)
}

func transformResolveHeight(value NumericValue, e *Element) float32 {
	if value.Unit == UnitPX || value.Unit == UnitNUMBER {
		return value.Number
	}
	return e.ResolveNumericValue(value, e.GetBox().GetSize(BoxAreaBorder).Y)
}

func transformResolveLength(value NumericValue, e *Element) float32 {
	if value.Unit == UnitPX || value.Unit == UnitNUMBER {
		return value.Number
	}
	return e.ResolveLength(value)
}

// TransformSetIdentity is TransformUtilities::SetIdentity.
func TransformSetIdentity(p *TransformPrimitive) {
	switch p.Type {
	case TransformMATRIX2D:
		for i := 0; i < 6; i++ {
			if i == 0 || i == 3 {
				p.Values[i] = 1
			} else {
				p.Values[i] = 0
			}
		}
	case TransformMATRIX3D:
		for i := 0; i < 16; i++ {
			if i%5 == 0 {
				p.Values[i] = 1
			} else {
				p.Values[i] = 0
			}
		}
	case TransformSCALEX, TransformSCALEY, TransformSCALEZ:
		p.Values[0] = 1
	case TransformSCALE2D:
		p.Values[0] = 1
		p.Values[1] = 1
	case TransformSCALE3D:
		p.Values[0] = 1
		p.Values[1] = 1
		p.Values[2] = 1
	case TransformROTATE3D:
		// Keep the rotation axis so interpolation with a matching axis works.
		p.Values[3] = 0
	case TransformDECOMPOSEDMATRIX4:
		p.Decomposed = DecomposedMatrix4{Vector4f{0, 0, 0, 1}, Vector4f{0, 0, 0, 1}, Vector3f{0, 0, 0}, Vector3f{1, 1, 1}, Vector3f{0, 0, 0}}
	default:
		if transformIsUnresolved(p.Type) {
			for i := 0; i < 3; i++ {
				p.Unresolved[i].Number = 0
			}
		} else {
			for i := 0; i < 16; i++ {
				p.Values[i] = 0
			}
		}
	}
}

// TransformResolve is TransformUtilities::ResolveTransform.
func TransformResolve(p TransformPrimitive, e *Element) Matrix4f {
	v := p.Values
	u := p.Unresolved
	switch p.Type {
	case TransformMATRIX2D:
		return Matrix4FromRows(Vector4f{v[0], v[2], 0, v[4]}, Vector4f{v[1], v[3], 0, v[5]}, Vector4f{0, 0, 1, 0}, Vector4f{0, 0, 0, 1})
	case TransformMATRIX3D:
		return Matrix4FromColumns(Vector4f{v[0], v[1], v[2], v[3]}, Vector4f{v[4], v[5], v[6], v[7]}, Vector4f{v[8], v[9], v[10], v[11]}, Vector4f{v[12], v[13], v[14], v[15]})
	case TransformTRANSLATEX:
		return Matrix4TranslateX(transformResolveWidth(u[0], e))
	case TransformTRANSLATEY:
		return Matrix4TranslateY(transformResolveHeight(u[0], e))
	case TransformTRANSLATEZ:
		return Matrix4TranslateZ(transformResolveLength(u[0], e))
	case TransformTRANSLATE2D:
		return Matrix4Translate(transformResolveWidth(u[0], e), transformResolveHeight(u[1], e), 0)
	case TransformTRANSLATE3D:
		return Matrix4Translate(transformResolveWidth(u[0], e), transformResolveHeight(u[1], e), transformResolveLength(u[2], e))
	case TransformSCALEX:
		return Matrix4ScaleX(v[0])
	case TransformSCALEY:
		return Matrix4ScaleY(v[0])
	case TransformSCALEZ:
		return Matrix4ScaleZ(v[0])
	case TransformSCALE2D:
		return Matrix4Scale(v[0], v[1], 1)
	case TransformSCALE3D:
		return Matrix4Scale(v[0], v[1], v[2])
	case TransformROTATEX:
		return Matrix4RotateX(v[0])
	case TransformROTATEY:
		return Matrix4RotateY(v[0])
	case TransformROTATEZ, TransformROTATE2D:
		return Matrix4RotateZ(v[0])
	case TransformROTATE3D:
		return Matrix4Rotate(Vector3f{v[0], v[1], v[2]}, v[3])
	case TransformSKEWX:
		return Matrix4SkewX(v[0])
	case TransformSKEWY:
		return Matrix4SkewY(v[0])
	case TransformSKEW2D:
		return Matrix4Skew(v[0], v[1])
	case TransformDECOMPOSEDMATRIX4:
		d := p.Decomposed
		return Matrix4Compose(d.Translation, d.Scale, d.Skew, d.Perspective, d.Quaternion)
	case TransformPERSPECTIVE:
		return Matrix4Perspective(transformResolveLength(u[0], e))
	}
	return Matrix4f{}
}

// TransformPrepareForInterpolation is
// TransformUtilities::PrepareForInterpolation: resolves relative units to
// pixels, and returns false for primitives that must be decomposed.
func TransformPrepareForInterpolation(p *TransformPrimitive, e *Element) bool {
	switch p.Type {
	case TransformTRANSLATEX:
		p.Unresolved[0] = NumericValue{transformResolveWidth(p.Unresolved[0], e), UnitPX}
		return true
	case TransformTRANSLATEY:
		p.Unresolved[0] = NumericValue{transformResolveHeight(p.Unresolved[0], e), UnitPX}
		return true
	case TransformTRANSLATEZ:
		p.Unresolved[0] = NumericValue{transformResolveLength(p.Unresolved[0], e), UnitPX}
		return true
	case TransformTRANSLATE2D:
		p.Unresolved[0] = NumericValue{transformResolveWidth(p.Unresolved[0], e), UnitPX}
		p.Unresolved[1] = NumericValue{transformResolveHeight(p.Unresolved[1], e), UnitPX}
		return true
	case TransformTRANSLATE3D:
		p.Unresolved[0] = NumericValue{transformResolveWidth(p.Unresolved[0], e), UnitPX}
		p.Unresolved[1] = NumericValue{transformResolveHeight(p.Unresolved[1], e), UnitPX}
		p.Unresolved[2] = NumericValue{transformResolveLength(p.Unresolved[2], e), UnitPX}
		return true
	case TransformROTATE3D:
		vec := Vector3f{p.Values[0], p.Values[1], p.Values[2]}.Normalise()
		p.Values[0] = vec.X
		p.Values[1] = vec.Y
		p.Values[2] = vec.Z
		return true
	case TransformMATRIX3D, TransformMATRIX2D, TransformPERSPECTIVE:
		return false
	}
	return true
}

const (
	genericNone = iota
	genericScale3D
	genericTranslate3D
	genericRotate3D
)

func transformGenericType(p TransformPrimitive) int {
	switch p.Type {
	case TransformTRANSLATEX, TransformTRANSLATEY, TransformTRANSLATEZ, TransformTRANSLATE2D, TransformTRANSLATE3D:
		return genericTranslate3D
	case TransformSCALEX, TransformSCALEY, TransformSCALEZ, TransformSCALE2D, TransformSCALE3D:
		return genericScale3D
	case TransformROTATEX, TransformROTATEY, TransformROTATEZ, TransformROTATE2D, TransformROTATE3D:
		return genericRotate3D
	}
	return genericNone
}

// zeroValues16 clears a primitive's value array; wasigoc has no array literal.
var zeroValues16 [16]float32

func transformConvertToGeneric(p TransformPrimitive) TransformPrimitive {
	r := p
	px0 := NumericValue{0, UnitPX}
	switch p.Type {
	case TransformTRANSLATEX:
		r.Type = TransformTRANSLATE3D
		r.Unresolved[0] = p.Unresolved[0]
		r.Unresolved[1] = px0
		r.Unresolved[2] = px0
	case TransformTRANSLATEY:
		r.Type = TransformTRANSLATE3D
		r.Unresolved[1] = p.Unresolved[0]
		r.Unresolved[0] = px0
		r.Unresolved[2] = px0
	case TransformTRANSLATEZ:
		r.Type = TransformTRANSLATE3D
		r.Unresolved[2] = p.Unresolved[0]
		r.Unresolved[0] = px0
		r.Unresolved[1] = px0
	case TransformTRANSLATE2D:
		r.Type = TransformTRANSLATE3D
		r.Unresolved[2] = px0
	case TransformSCALEX:
		r.Type = TransformSCALE3D
		r.Values[1] = 1
		r.Values[2] = 1
	case TransformSCALEY:
		r.Type = TransformSCALE3D
		r.Values[1] = p.Values[0]
		r.Values[0] = 1
		r.Values[2] = 1
	case TransformSCALEZ:
		r.Type = TransformSCALE3D
		r.Values[2] = p.Values[0]
		r.Values[0] = 1
		r.Values[1] = 1
	case TransformSCALE2D:
		r.Type = TransformSCALE3D
		r.Values[2] = 1
	case TransformROTATEX:
		r.Type = TransformROTATE3D
		r.Values = zeroValues16
		r.Values[0] = 1
		r.Values[3] = p.Values[0]
	case TransformROTATEY:
		r.Type = TransformROTATE3D
		r.Values = zeroValues16
		r.Values[1] = 1
		r.Values[3] = p.Values[0]
	case TransformROTATEZ, TransformROTATE2D:
		r.Type = TransformROTATE3D
		r.Values = zeroValues16
		r.Values[2] = 1
		r.Values[3] = p.Values[0]
	}
	return r
}

func canInterpolateRotate3D(p0 TransformPrimitive, p1 TransformPrimitive) bool {
	return p0.Values[0] == p1.Values[0] && p0.Values[1] == p1.Values[1] && p0.Values[2] == p1.Values[2]
}

// TransformTryConvertToMatchingGenericType is
// TransformUtilities::TryConvertToMatchingGenericType.
func TransformTryConvertToMatchingGenericType(p0 *TransformPrimitive, p1 *TransformPrimitive) bool {
	if p0.Type == p1.Type {
		if p0.Type == TransformROTATE3D && !canInterpolateRotate3D(*p0, *p1) {
			return false
		}
		return true
	}
	c0 := transformGenericType(*p0)
	c1 := transformGenericType(*p1)
	if c0 == c1 && c0 != genericNone {
		n0 := transformConvertToGeneric(*p0)
		n1 := transformConvertToGeneric(*p1)
		if n0.Type == TransformROTATE3D && !canInterpolateRotate3D(n0, n1) {
			return false
		}
		*p0 = n0
		*p1 = n1
		return true
	}
	return false
}

// TransformInterpolateWith is TransformUtilities::InterpolateWith.
func TransformInterpolateWith(target *TransformPrimitive, other TransformPrimitive, alpha float32) bool {
	if target.Type != other.Type {
		return false
	}
	switch target.Type {
	case TransformMATRIX2D, TransformMATRIX3D, TransformPERSPECTIVE:
		return false
	case TransformROTATE3D:
		target.Values[3] = target.Values[3]*(1.0-alpha) + other.Values[3]*alpha
		return true
	case TransformDECOMPOSEDMATRIX4:
		d0 := target.Decomposed
		d1 := other.Decomposed
		d0.Perspective = d0.Perspective.Mul(1.0 - alpha).Add(d1.Perspective.Mul(alpha))
		d0.Quaternion = quaternionSlerp(d0.Quaternion, d1.Quaternion, alpha)
		d0.Translation = d0.Translation.Mul(1.0 - alpha).Add(d1.Translation.Mul(alpha))
		d0.Scale = d0.Scale.Mul(1.0 - alpha).Add(d1.Scale.Mul(alpha))
		d0.Skew = d0.Skew.Mul(1.0 - alpha).Add(d1.Skew.Mul(alpha))
		target.Decomposed = d0
		return true
	}
	n := transformPrimitiveSize(target.Type)
	if transformIsUnresolved(target.Type) {
		for i := 0; i < n; i++ {
			target.Unresolved[i].Number = target.Unresolved[i].Number*(1.0-alpha) + other.Unresolved[i].Number*alpha
		}
		return true
	}
	for i := 0; i < n; i++ {
		target.Values[i] = target.Values[i]*(1.0-alpha) + other.Values[i]*alpha
	}
	return true
}

func resolvedToString(values [16]float32, n int, unit string, radToDeg bool, onlyUnitOnLastValue bool) string {
	var multiplier float32 = 1.0
	result := "("
	for i := 0; i < n; i++ {
		if onlyUnitOnLastValue && i < n-1 {
			multiplier = 1.0
		} else if radToDeg {
			multiplier = 180.0 / Pi
		}
		result = result + FormatFloat(values[i]*multiplier)
		if unit != "" && (!onlyUnitOnLastValue || i == n-1) {
			result = result + unit
		}
		if i < n-1 {
			result = result + ", "
		}
	}
	return result + ")"
}

func unresolvedToString(values [3]NumericValue, n int) string {
	result := "("
	for i := 0; i < n; i++ {
		result = result + FormatFloat(values[i].Number) + UnitSuffix(values[i].Unit)
		if i != n-1 {
			result = result + ", "
		}
	}
	return result + ")"
}

func decomposedToString(p DecomposedMatrix4) string {
	result := ""
	if !vector4Equal(p.Perspective, (Vector4f{0, 0, 0, 1})) {
		result = result + "perspective(" + FormatVector4f(p.Perspective) + "), "
	}
	if !vector4Equal(p.Quaternion, (Vector4f{0, 0, 0, 1})) {
		result = result + "quaternion(" + FormatVector4f(p.Quaternion) + "), "
	}
	if !vector3Equal(p.Translation, (Vector3f{0, 0, 0})) {
		result = result + "translation(" + FormatVector3f(p.Translation) + "), "
	}
	if !vector3Equal(p.Scale, (Vector3f{1, 1, 1})) {
		result = result + "scale(" + FormatVector3f(p.Scale) + "), "
	}
	if !vector3Equal(p.Skew, (Vector3f{0, 0, 0})) {
		result = result + "skew(" + FormatVector3f(p.Skew) + "), "
	}
	if len(result) > 2 {
		result = result[:len(result)-2]
	}
	return "decomposedMatrix3d{ " + result + " }"
}

// TransformPrimitiveToString is TransformUtilities::ToString.
func TransformPrimitiveToString(p TransformPrimitive) string {
	v := p.Values
	u := p.Unresolved
	switch p.Type {
	case TransformMATRIX2D:
		return "matrix" + resolvedToString(v, 6, "", false, false)
	case TransformMATRIX3D:
		return "matrix3d" + resolvedToString(v, 16, "", false, false)
	case TransformTRANSLATEX:
		return "translateX" + unresolvedToString(u, 1)
	case TransformTRANSLATEY:
		return "translateY" + unresolvedToString(u, 1)
	case TransformTRANSLATEZ:
		return "translateZ" + unresolvedToString(u, 1)
	case TransformTRANSLATE2D:
		return "translate" + unresolvedToString(u, 2)
	case TransformTRANSLATE3D:
		return "translate3d" + unresolvedToString(u, 3)
	case TransformSCALEX:
		return "scaleX" + resolvedToString(v, 1, "", false, false)
	case TransformSCALEY:
		return "scaleY" + resolvedToString(v, 1, "", false, false)
	case TransformSCALEZ:
		return "scaleZ" + resolvedToString(v, 1, "", false, false)
	case TransformSCALE2D:
		return "scale" + resolvedToString(v, 2, "", false, false)
	case TransformSCALE3D:
		return "scale3d" + resolvedToString(v, 3, "", false, false)
	case TransformROTATEX:
		return "rotateX" + resolvedToString(v, 1, "deg", true, false)
	case TransformROTATEY:
		return "rotateY" + resolvedToString(v, 1, "deg", true, false)
	case TransformROTATEZ:
		return "rotateZ" + resolvedToString(v, 1, "deg", true, false)
	case TransformROTATE2D:
		return "rotate" + resolvedToString(v, 1, "deg", true, false)
	case TransformROTATE3D:
		return "rotate3d" + resolvedToString(v, 4, "deg", true, true)
	case TransformSKEWX:
		return "skewX" + resolvedToString(v, 1, "deg", true, false)
	case TransformSKEWY:
		return "skewY" + resolvedToString(v, 1, "deg", true, false)
	case TransformSKEW2D:
		return "skew" + resolvedToString(v, 2, "deg", true, false)
	case TransformPERSPECTIVE:
		return "perspective" + unresolvedToString(u, 1)
	case TransformDECOMPOSEDMATRIX4:
		return decomposedToString(p.Decomposed)
	}
	return ""
}

// TransformDecompose is TransformUtilities::Decompose, following
// https://drafts.csswg.org/css-transforms-2/#interpolation-of-3d-matrices.
// As in RmlUi, the perspective term multiplies by the transposed
// perspective matrix (p), not the transposed inverse the spec names.
func TransformDecompose(m Matrix4f) (DecomposedMatrix4, bool) {
	d := DecomposedMatrix4{}
	var eps float32 = 0.0005
	if MathAbsolute(m.At(3, 3)) < eps {
		return d, false
	}
	p := m
	for i := 0; i < 3; i++ {
		p.Set(i, 3, 0)
	}
	p.Set(3, 3, 1)
	if MathAbsolute(p.Determinant()) < eps {
		return d, false
	}
	if m.At(0, 3) != 0 || m.At(1, 3) != 0 || m.At(2, 3) != 0 {
		rhs := m.GetColumn(3)
		pInv := p
		if !pInv.Invert() {
			return d, false
		}
		pInvTrans := p.Transpose()
		d.Perspective = pInvTrans.MulVector(rhs)
	} else {
		d.Perspective = Vector4f{0, 0, 0, 1}
	}
	d.Translation = Vector3f{m.At(3, 0), m.At(3, 1), m.At(3, 2)}
	row0 := Vector3f{m.At(0, 0), m.At(0, 1), m.At(0, 2)}
	row1 := Vector3f{m.At(1, 0), m.At(1, 1), m.At(1, 2)}
	row2 := Vector3f{m.At(2, 0), m.At(2, 1), m.At(2, 2)}

	d.Scale.X = row0.Magnitude()
	row0 = row0.Normalise()
	d.Skew.X = row0.DotProduct(row1)
	row1 = combine3(row1, row0, 1, -d.Skew.X)
	d.Scale.Y = row1.Magnitude()
	row1 = row1.Normalise()
	d.Skew.X = d.Skew.X / d.Scale.Y
	d.Skew.Y = row0.DotProduct(row2)
	row2 = combine3(row2, row0, 1, -d.Skew.Y)
	d.Skew.Z = row1.DotProduct(row2)
	row2 = combine3(row2, row1, 1, -d.Skew.Z)
	d.Scale.Z = row2.Magnitude()
	row2 = row2.Normalise()
	d.Skew.Z = d.Skew.Z / d.Scale.Z
	d.Skew.Y = d.Skew.Y / d.Scale.Z

	pdum3 := row1.CrossProduct(row2)
	if row0.DotProduct(pdum3) < 0 {
		d.Scale = d.Scale.Mul(-1)
		row0 = row0.Mul(-1)
		row1 = row1.Mul(-1)
		row2 = row2.Mul(-1)
	}
	d.Quaternion.X = 0.5 * MathSquareRoot(MathMax(1.0+row0.X-row1.Y-row2.Z, 0))
	d.Quaternion.Y = 0.5 * MathSquareRoot(MathMax(1.0-row0.X+row1.Y-row2.Z, 0))
	d.Quaternion.Z = 0.5 * MathSquareRoot(MathMax(1.0-row0.X-row1.Y+row2.Z, 0))
	d.Quaternion.W = 0.5 * MathSquareRoot(MathMax(1.0+row0.X+row1.Y+row2.Z, 0))
	if row2.Y > row1.Z {
		d.Quaternion.X = -d.Quaternion.X
	}
	if row0.Z > row2.X {
		d.Quaternion.Y = -d.Quaternion.Y
	}
	if row1.X > row0.Y {
		d.Quaternion.Z = -d.Quaternion.Z
	}
	return d, true
}
