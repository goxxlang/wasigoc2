// Port of RmlUi Source/Core/Transform.cpp, TransformPrimitive.cpp,
// PropertyParserTransform.cpp, and Include/RmlUi/Core/Transform.h,
// TransformPrimitive.h.
package rmlui

// TransformPrimitiveType is TransformPrimitive::Type.
type TransformPrimitiveType = int

const (
	TransformMATRIX2D TransformPrimitiveType = iota
	TransformMATRIX3D
	TransformTRANSLATEX
	TransformTRANSLATEY
	TransformTRANSLATEZ
	TransformTRANSLATE2D
	TransformTRANSLATE3D
	TransformSCALEX
	TransformSCALEY
	TransformSCALEZ
	TransformSCALE2D
	TransformSCALE3D
	TransformROTATEX
	TransformROTATEY
	TransformROTATEZ
	TransformROTATE2D
	TransformROTATE3D
	TransformSKEWX
	TransformSKEWY
	TransformSKEW2D
	TransformPERSPECTIVE
	TransformDECOMPOSEDMATRIX4
)

// DecomposedMatrix4 is Transforms::DecomposedMatrix4.
type DecomposedMatrix4 struct {
	Perspective Vector4f
	Quaternion  Vector4f
	Translation Vector3f
	Scale       Vector3f
	Skew        Vector3f
}

// TransformPrimitive is Rml::TransformPrimitive. The C++ union is
// flattened: resolved primitives use Values, unresolved ones (translate*,
// perspective) use Unresolved, and DECOMPOSEDMATRIX4 uses Decomposed.
type TransformPrimitive struct {
	Type       TransformPrimitiveType
	Values     [16]float32
	Unresolved [3]NumericValue
	Decomposed DecomposedMatrix4
}

// transformPrimitiveSize is N for each primitive's value array.
func transformPrimitiveSize(t TransformPrimitiveType) int {
	switch t {
	case TransformMATRIX2D:
		return 6
	case TransformMATRIX3D:
		return 16
	case TransformTRANSLATEX, TransformTRANSLATEY, TransformTRANSLATEZ, TransformPERSPECTIVE:
		return 1
	case TransformTRANSLATE2D:
		return 2
	case TransformTRANSLATE3D:
		return 3
	case TransformSCALEX, TransformSCALEY, TransformSCALEZ:
		return 1
	case TransformSCALE2D:
		return 2
	case TransformSCALE3D:
		return 3
	case TransformROTATEX, TransformROTATEY, TransformROTATEZ, TransformROTATE2D:
		return 1
	case TransformROTATE3D:
		return 4
	case TransformSKEWX, TransformSKEWY:
		return 1
	case TransformSKEW2D:
		return 2
	}
	return 0
}

func transformIsUnresolved(t TransformPrimitiveType) bool {
	switch t {
	case TransformTRANSLATEX, TransformTRANSLATEY, TransformTRANSLATEZ, TransformTRANSLATE2D, TransformTRANSLATE3D, TransformPERSPECTIVE:
		return true
	}
	return false
}

// resolvePrimitiveAbsoluteValue converts to radians (baseUnit RAD) or checks
// for a plain number (baseUnit NUMBER).
func resolvePrimitiveAbsoluteValue(value NumericValue, baseUnit Unit) float32 {
	if baseUnit == UnitRAD {
		switch value.Unit {
		case UnitRAD:
			return value.Number
		case UnitDEG:
			return MathDegreesToRadians(value.Number)
		case UnitPERCENT:
			return value.Number * 0.01 * 2.0 * Pi
		default:
			LogMessage(LogWarning, "Trying to pass a non-angle unit to a property expecting an angle.")
		}
	} else if baseUnit == UnitNUMBER && value.Unit != UnitNUMBER {
		LogMessage(LogWarning, "A unit was passed to a property which expected a unit-less number.")
	}
	return value.Number
}

// NewTransformPrimitive builds a primitive from parsed arguments, applying
// the base-unit resolution each C++ constructor performs.
func NewTransformPrimitive(t TransformPrimitiveType, args []NumericValue) TransformPrimitive {
	p := TransformPrimitive{Type: t}
	n := transformPrimitiveSize(t)
	if transformIsUnresolved(t) {
		for i := 0; i < n; i++ {
			p.Unresolved[i] = args[i]
		}
		return p
	}
	switch t {
	case TransformROTATEX, TransformROTATEY, TransformROTATEZ, TransformROTATE2D, TransformSKEWX, TransformSKEWY, TransformSKEW2D:
		for i := 0; i < n; i++ {
			p.Values[i] = resolvePrimitiveAbsoluteValue(args[i], UnitRAD)
		}
	case TransformROTATE3D:
		for i := 0; i < 3; i++ {
			p.Values[i] = resolvePrimitiveAbsoluteValue(args[i], UnitNUMBER)
		}
		p.Values[3] = resolvePrimitiveAbsoluteValue(args[3], UnitRAD)
	default:
		for i := 0; i < n; i++ {
			p.Values[i] = args[i].Number
		}
	}
	return p
}

// Convenience constructors mirroring the C++ ones with defaults.
func TransformTranslate2D(x float32, y float32, unit Unit) TransformPrimitive {
	return NewTransformPrimitive(TransformTRANSLATE2D, []NumericValue{{x, unit}, {y, unit}})
}
func TransformScale2D(x float32, y float32) TransformPrimitive {
	return NewTransformPrimitive(TransformSCALE2D, []NumericValue{{x, UnitNUMBER}, {y, UnitNUMBER}})
}
func TransformRotate2D(angle float32, unit Unit) TransformPrimitive {
	return NewTransformPrimitive(TransformROTATE2D, []NumericValue{{angle, unit}})
}

func (p TransformPrimitive) Equals(o TransformPrimitive) bool {
	if p.Type != o.Type {
		return false
	}
	if p.Type == TransformDECOMPOSEDMATRIX4 {
		a := p.Decomposed
		b := o.Decomposed
		return vector4Equal(a.Perspective, b.Perspective) && vector4Equal(a.Quaternion, b.Quaternion) &&
			vector3Equal(a.Translation, b.Translation) && vector3Equal(a.Scale, b.Scale) && vector3Equal(a.Skew, b.Skew)
	}
	n := transformPrimitiveSize(p.Type)
	if transformIsUnresolved(p.Type) {
		for i := 0; i < n; i++ {
			if p.Unresolved[i].Number != o.Unresolved[i].Number || p.Unresolved[i].Unit != o.Unresolved[i].Unit {
				return false
			}
		}
		return true
	}
	for i := 0; i < n; i++ {
		if p.Values[i] != o.Values[i] {
			return false
		}
	}
	return true
}

func vector4Equal(a Vector4f, b Vector4f) bool {
	return a.X == b.X && a.Y == b.Y && a.Z == b.Z && a.W == b.W
}

func vector3Equal(a Vector3f, b Vector3f) bool { return a.X == b.X && a.Y == b.Y && a.Z == b.Z }

// Transform is Rml::Transform.
type Transform struct {
	primitives []TransformPrimitive
}

func NewTransform(primitives []TransformPrimitive) *Transform {
	return &Transform{primitives: primitives}
}

// TransformMakeProperty is Transform::MakeProperty.
func TransformMakeProperty(primitives []TransformPrimitive) Property {
	p := PropertyOf(VariantPointer(VariantTRANSFORMPTR, NewTransform(primitives)), UnitTRANSFORM)
	p.Definition = GetPropertyDefinition(PropertyIdTransform)
	return p
}

func (t *Transform) ClearPrimitives()                         { t.primitives = nil }
func (t *Transform) AddPrimitive(p TransformPrimitive)        { t.primitives = append(t.primitives, p) }
func (t *Transform) GetNumPrimitives() int                    { return len(t.primitives) }
func (t *Transform) GetPrimitive(i int) TransformPrimitive    { return t.primitives[i] }
func (t *Transform) GetPrimitives() []TransformPrimitive      { return t.primitives }
func (t *Transform) SetPrimitive(i int, p TransformPrimitive) { t.primitives[i] = p }

// Transform variants compare by pointer identity (SharedPtr ==).
func (t *Transform) variantEquals(other VariantPayload) bool {
	o, ok := other.(*Transform)
	return ok && o == t
}

func (t *Transform) variantString() string {
	dest := ""
	for i := 0; i < len(t.primitives); i++ {
		dest = dest + TransformPrimitiveToString(t.primitives[i])
		if i != len(t.primitives)-1 {
			dest = dest + " "
		}
	}
	return dest
}

// ---- PropertyParserTransform ----

// PropertyParserTransform is Rml::PropertyParserTransform.
type PropertyParserTransform struct{}

type transformRule struct {
	keyword        string
	parsers        []PropertyParser
	nargs          int
	typ            TransformPrimitiveType
	duplicateFirst bool
}

var (
	transformNumber    = NewPropertyParserNumber(UnitNUMBER, UnitUNKNOWN)
	transformLength    = NewPropertyParserNumber(UnitLENGTH, UnitPX)
	transformLengthPct = NewPropertyParserNumber(UnitLENGTH_PERCENT, UnitPX)
	transformAngle     = NewPropertyParserNumber(UnitANGLE, UnitRAD)
)

func (p *PropertyParserTransform) ParseValue(property *Property, value string, parameters map[string]int) bool {
	if value == "none" {
		property.Value = VariantPointer(VariantTRANSFORMPTR, nil)
		property.Unit = UnitTRANSFORM
		return true
	}
	transform := NewTransform(nil)
	number16 := []PropertyParser{}
	for i := 0; i < 16; i++ {
		number16 = append(number16, transformNumber)
	}
	lengthpct2Length1 := []PropertyParser{transformLengthPct, transformLengthPct, transformLength}
	number3Angle1 := []PropertyParser{transformNumber, transformNumber, transformNumber, transformAngle}
	angle2 := []PropertyParser{transformAngle, transformAngle}
	length1 := []PropertyParser{transformLength}

	// Order matters: keywords match by prefix, as in the C++ chain.
	rules := []transformRule{
		{"perspective", length1, 1, TransformPERSPECTIVE, false},
		{"matrix", number16, 6, TransformMATRIX2D, false},
		{"matrix3d", number16, 16, TransformMATRIX3D, false},
		{"translateX", lengthpct2Length1, 1, TransformTRANSLATEX, false},
		{"translateY", lengthpct2Length1, 1, TransformTRANSLATEY, false},
		{"translateZ", length1, 1, TransformTRANSLATEZ, false},
		{"translate", lengthpct2Length1, 2, TransformTRANSLATE2D, false},
		{"translate3d", lengthpct2Length1, 3, TransformTRANSLATE3D, false},
		{"scaleX", number16, 1, TransformSCALEX, false},
		{"scaleY", number16, 1, TransformSCALEY, false},
		{"scaleZ", number16, 1, TransformSCALEZ, false},
		{"scale", number16, 2, TransformSCALE2D, false},
		{"scale", number16, 1, TransformSCALE2D, true},
		{"scale3d", number16, 3, TransformSCALE3D, false},
		{"rotateX", angle2, 1, TransformROTATEX, false},
		{"rotateY", angle2, 1, TransformROTATEY, false},
		{"rotateZ", angle2, 1, TransformROTATEZ, false},
		{"rotate", angle2, 1, TransformROTATE2D, false},
		{"rotate3d", number3Angle1, 4, TransformROTATE3D, false},
		{"skewX", angle2, 1, TransformSKEWX, false},
		{"skewY", angle2, 1, TransformSKEWY, false},
		{"skew", angle2, 2, TransformSKEW2D, false},
	}
	next := value
	for next != "" {
		bytesRead := 0
		for _, rule := range rules {
			n, args, ok := transformScan(next, rule.keyword, rule.parsers, rule.nargs)
			if !ok {
				continue
			}
			if rule.duplicateFirst {
				args = append(args, args[0])
			}
			transform.AddPrimitive(NewTransformPrimitive(rule.typ, args))
			bytesRead = n
			break
		}
		if bytesRead > 0 {
			next = next[bytesRead:]
		} else {
			return false
		}
	}
	property.Value = VariantPointer(VariantTRANSFORMPTR, transform)
	property.Unit = UnitTRANSFORM
	return true
}

// isCSpace matches the whitespace set of a scanf " " directive.
func isCSpace(c byte) bool {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'
}

func skipCSpace(s string, i int) int {
	for i < len(s) && isCSpace(s[i]) {
		i++
	}
	return i
}

// transformScan is PropertyParserTransform::Scan: keyword, '(', nargs
// comma-separated arguments (each read as sscanf "%[^,)]", keeping trailing
// spaces), ')'. Returns the bytes consumed.
func transformScan(str string, keyword string, parsers []PropertyParser, nargs int) (int, []NumericValue, bool) {
	i := skipCSpace(str, 0)
	if !StringStartsWith(str[i:], keyword) {
		return 0, nil, false
	}
	i += len(keyword)
	i = skipCSpace(str, i)
	if i >= len(str) || str[i] != '(' {
		return 0, nil, false
	}
	i = skipCSpace(str, i+1)
	args := []NumericValue{}
	for a := 0; a < nargs; a++ {
		i = skipCSpace(str, i)
		start := i
		for i < len(str) && str[i] != ',' && str[i] != ')' {
			i++
		}
		if i == start {
			return 0, nil, false
		}
		arg := str[start:i]
		i = skipCSpace(str, i)
		prop := NewProperty()
		if !parsers[a].ParseValue(&prop, arg, noParameters) {
			return 0, nil, false
		}
		args = append(args, NumericValue{prop.Value.GetFloat(), prop.Unit})
		if a < nargs-1 {
			i = skipCSpace(str, i)
			if i >= len(str) || str[i] != ',' {
				return 0, nil, false
			}
			i = skipCSpace(str, i+1)
		}
	}
	i = skipCSpace(str, i)
	if i >= len(str) || str[i] != ')' {
		return 0, nil, false
	}
	i = skipCSpace(str, i+1)
	return i, args, i > 0
}
