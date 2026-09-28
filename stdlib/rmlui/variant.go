// Port of RmlUi Source/Core/Variant.cpp, Include/RmlUi/Core/Variant.h,
// Variant.inl, and the TypeConverter rules Variant::Get applies.
package rmlui

// VariantType is Variant::Type; values are the C++ type-code characters.
type VariantType = int

const (
	VariantNONE            VariantType = '-'
	VariantBOOL            VariantType = 'B'
	VariantBYTE            VariantType = 'b'
	VariantCHAR            VariantType = 'c'
	VariantFLOAT           VariantType = 'f'
	VariantDOUBLE          VariantType = 'd'
	VariantINT             VariantType = 'i'
	VariantINT64           VariantType = 'I'
	VariantUINT            VariantType = 'u'
	VariantUINT64          VariantType = 'U'
	VariantSTRING          VariantType = 's'
	VariantVECTOR2         VariantType = '2'
	VariantVECTOR3         VariantType = '3'
	VariantVECTOR4         VariantType = '4'
	VariantCOLOURF         VariantType = 'g'
	VariantCOLOURB         VariantType = 'h'
	VariantSCRIPTINTERFACE VariantType = 'p'
	VariantTRANSFORMPTR    VariantType = 't'
	VariantTRANSITIONLIST  VariantType = 'T'
	VariantANIMATIONLIST   VariantType = 'A'
	VariantDECORATORSPTR   VariantType = 'D'
	VariantFILTERSPTR      VariantType = 'F'
	VariantFONTEFFECTSPTR  VariantType = 'E'
	VariantCOLORSTOPLIST   VariantType = 'C'
	VariantBOXSHADOWLIST   VariantType = 'S'
	VariantVOIDPTR         VariantType = '*'
)

// VariantPayload is a complex Variant value (Transform, TransitionList,
// DecoratorDeclarationList, ...): its TypeConverter<T, String> and
// operator==. The payload is an interface rather than any because this
// compiler can only assert an interface value to a concrete type, not to
// another interface.
type VariantPayload interface {
	variantString() string
	variantEquals(other VariantPayload) bool
}

// VariantUserData wraps an arbitrary host value (VOIDPTR / SCRIPTINTERFACE).
type VariantUserData struct {
	Value any
}

func (u *VariantUserData) variantString() string {
	if u == nil {
		return "0x0"
	}
	return "0x1"
}

func (u *VariantUserData) variantEquals(other VariantPayload) bool {
	o, ok := other.(*VariantUserData)
	return ok && o == u
}

// Variant is Rml::Variant.
type Variant struct {
	typ VariantType
	num float64
	i64 int64
	str string
	vec Vector4f
	col Colourb
	ptr VariantPayload
}

func NewVariant() Variant { return Variant{typ: VariantNONE} }

func VariantBool(v bool) Variant {
	n := 0.0
	if v {
		n = 1
	}
	return Variant{typ: VariantBOOL, num: n}
}
func VariantByte(v byte) Variant      { return Variant{typ: VariantBYTE, num: float64(v)} }
func VariantChar(v byte) Variant      { return Variant{typ: VariantCHAR, num: float64(v)} }
func VariantFloat(v float32) Variant  { return Variant{typ: VariantFLOAT, num: float64(v)} }
func VariantDouble(v float64) Variant { return Variant{typ: VariantDOUBLE, num: v} }
func VariantInt(v int) Variant        { return Variant{typ: VariantINT, num: float64(v), i64: int64(v)} }
func VariantInt64(v int64) Variant    { return Variant{typ: VariantINT64, num: float64(v), i64: v} }
func VariantUint(v uint) Variant      { return Variant{typ: VariantUINT, num: float64(v), i64: int64(v)} }
func VariantUint64(v uint64) Variant  { return Variant{typ: VariantUINT64, num: float64(v), i64: int64(v)} }
func VariantString(v string) Variant  { return Variant{typ: VariantSTRING, str: v} }
func VariantVector2f(v Vector2f) Variant {
	return Variant{typ: VariantVECTOR2, vec: Vector4f{v.X, v.Y, 0, 0}}
}
func VariantVector3f(v Vector3f) Variant {
	return Variant{typ: VariantVECTOR3, vec: Vector4f{v.X, v.Y, v.Z, 0}}
}
func VariantVector4f(v Vector4f) Variant { return Variant{typ: VariantVECTOR4, vec: v} }
func VariantColourf(v Colourf) Variant {
	return Variant{typ: VariantCOLOURF, vec: Vector4f{v.Red, v.Green, v.Blue, v.Alpha}}
}
func VariantColourb(v Colourb) Variant { return Variant{typ: VariantCOLOURB, col: v} }

// VariantPointer stores a complex payload under the given type code
// (TRANSFORMPTR, TRANSITIONLIST, DECORATORSPTR, SCRIPTINTERFACE, VOIDPTR, ...).
func VariantPointer(typ VariantType, p VariantPayload) Variant { return Variant{typ: typ, ptr: p} }

// VariantKeyword stores an enum value; Variant::Set(enum) stores INT64.
func VariantKeyword(v int) Variant { return Variant{typ: VariantINT64, num: float64(v), i64: int64(v)} }

func (v Variant) GetType() VariantType {
	if v.typ == 0 {
		return VariantNONE
	}
	return v.typ
}

func (v *Variant) Clear() { *v = Variant{typ: VariantNONE} }

func (v Variant) IsNone() bool { return v.GetType() == VariantNONE }

func (v Variant) isNumeric() bool {
	switch v.typ {
	case VariantBOOL, VariantBYTE, VariantCHAR, VariantFLOAT, VariantDOUBLE, VariantINT, VariantINT64, VariantUINT, VariantUINT64:
		return true
	}
	return false
}

// Pointer returns the complex payload, or nil.
func (v Variant) Pointer() VariantPayload { return v.ptr }

// GetStringOk is GetInto<String>.
func (v Variant) GetStringOk() (string, bool) {
	switch v.typ {
	case VariantSTRING:
		return v.str, true
	case VariantBOOL:
		return FormatBool(v.num != 0), true
	case VariantBYTE, VariantINT, VariantUINT:
		return FormatInt(int(v.num)), true
	case VariantINT64, VariantUINT64:
		return FormatInt(int(v.i64)), true
	case VariantCHAR:
		return string([]byte{byte(v.num)}), true
	case VariantFLOAT:
		return FormatFloat(float32(v.num)), true
	case VariantDOUBLE:
		return FormatDouble(v.num), true
	case VariantVECTOR2:
		return FormatVector2f(Vector2f{v.vec.X, v.vec.Y}), true
	case VariantVECTOR3:
		return FormatVector3f(Vector3f{v.vec.X, v.vec.Y, v.vec.Z}), true
	case VariantVECTOR4:
		return FormatVector4f(v.vec), true
	case VariantCOLOURF:
		return FormatColourf(Colourf{v.vec.X, v.vec.Y, v.vec.Z, v.vec.W}), true
	case VariantCOLOURB:
		return FormatColourb(v.col), true
	case VariantSCRIPTINTERFACE, VariantVOIDPTR:
		if v.ptr == nil {
			return "0x0", true
		}
		return "0x1", true
	}
	if v.ptr != nil {
		return v.ptr.variantString(), true
	}
	switch v.typ {
	case VariantTRANSFORMPTR, VariantDECORATORSPTR, VariantFILTERSPTR, VariantFONTEFFECTSPTR:
		return "none", true
	case VariantTRANSITIONLIST:
		return "none", true
	case VariantANIMATIONLIST, VariantCOLORSTOPLIST, VariantBOXSHADOWLIST:
		return "", true
	}
	return "", false
}

// GetString is Get<String>().
func (v Variant) GetString() string {
	s, _ := v.GetStringOk()
	return s
}

// GetDoubleOk is GetInto<double>.
func (v Variant) GetDoubleOk() (float64, bool) {
	if v.typ == VariantINT64 || v.typ == VariantUINT64 {
		return float64(v.i64), true
	}
	if v.isNumeric() {
		return v.num, true
	}
	if v.typ == VariantSTRING {
		return Atof(v.str), true
	}
	return 0, false
}

func (v Variant) GetDouble() float64 {
	d, _ := v.GetDoubleOk()
	return d
}

// GetFloatOk is GetInto<float>.
func (v Variant) GetFloatOk() (float32, bool) {
	d, ok := v.GetDoubleOk()
	return float32(d), ok
}

// GetFloat is Get<float>().
func (v Variant) GetFloat() float32 {
	f, _ := v.GetFloatOk()
	return f
}

// GetFloatOr is Get<float>(default_value).
func (v Variant) GetFloatOr(def float32) float32 {
	if f, ok := v.GetFloatOk(); ok {
		return f
	}
	return def
}

// GetInt64Ok is GetInto<int64_t>; floats truncate as in a C cast.
func (v Variant) GetInt64Ok() (int64, bool) {
	switch v.typ {
	case VariantINT64, VariantUINT64:
		return v.i64, true
	case VariantFLOAT, VariantDOUBLE:
		return int64(trunc64(v.num)), true
	case VariantBOOL, VariantBYTE, VariantCHAR, VariantINT, VariantUINT:
		return int64(v.num), true
	case VariantSTRING:
		n, ok := ScanInt(v.str)
		return int64(n), ok
	}
	return 0, false
}

func (v Variant) GetIntOk() (int, bool) {
	n, ok := v.GetInt64Ok()
	return int(n), ok
}

// GetInt is Get<int>().
func (v Variant) GetInt() int {
	n, _ := v.GetIntOk()
	return n
}

func (v Variant) GetIntOr(def int) int {
	if n, ok := v.GetIntOk(); ok {
		return n
	}
	return def
}

// GetBoolOk is GetInto<bool>.
func (v Variant) GetBoolOk() (bool, bool) {
	switch v.typ {
	case VariantINT64, VariantUINT64:
		return v.i64 != 0, true
	case VariantBOOL, VariantBYTE, VariantINT, VariantUINT, VariantFLOAT, VariantDOUBLE:
		return v.num != 0, true
	case VariantSTRING:
		return ParseBool(v.str)
	}
	return false, false
}

// GetBool is Get<bool>().
func (v Variant) GetBool() bool {
	b, _ := v.GetBoolOk()
	return b
}

func (v Variant) GetVector2fOk() (Vector2f, bool) {
	switch v.typ {
	case VariantVECTOR2:
		return Vector2f{v.vec.X, v.vec.Y}, true
	case VariantSTRING:
		parts := StringExpandList(v.str)
		if len(parts) < 2 {
			return Vector2f{}, false
		}
		return Vector2f{float32(Atof(parts[0])), float32(Atof(parts[1]))}, true
	}
	return Vector2f{}, false
}

func (v Variant) GetVector2f() Vector2f {
	r, _ := v.GetVector2fOk()
	return r
}

func (v Variant) GetVector3f() Vector3f {
	if v.typ == VariantVECTOR3 {
		return Vector3f{v.vec.X, v.vec.Y, v.vec.Z}
	}
	if v.typ == VariantSTRING {
		parts := StringExpandList(v.str)
		if len(parts) >= 3 {
			return Vector3f{float32(Atof(parts[0])), float32(Atof(parts[1])), float32(Atof(parts[2]))}
		}
	}
	return Vector3f{}
}

func (v Variant) GetVector4f() Vector4f {
	if v.typ == VariantVECTOR4 {
		return v.vec
	}
	if v.typ == VariantSTRING {
		parts := StringExpandList(v.str)
		if len(parts) >= 4 {
			return Vector4f{float32(Atof(parts[0])), float32(Atof(parts[1])), float32(Atof(parts[2])), float32(Atof(parts[3]))}
		}
	}
	return Vector4f{}
}

func (v Variant) GetColourbOk() (Colourb, bool) {
	switch v.typ {
	case VariantCOLOURB:
		return v.col, true
	case VariantSTRING:
		return ParseColour(v.str)
	}
	return Colourb{}, false
}

// GetColourb is Get<Colourb>().
func (v Variant) GetColourb() Colourb {
	c, _ := v.GetColourbOk()
	return c
}

func (v Variant) GetColourf() Colourf {
	if v.typ == VariantCOLOURF {
		return Colourf{v.vec.X, v.vec.Y, v.vec.Z, v.vec.W}
	}
	return Colourf{}
}

// Equals is Variant::operator==.
func (v Variant) Equals(o Variant) bool {
	if v.GetType() != o.GetType() {
		return false
	}
	switch v.GetType() {
	case VariantNONE:
		return true
	case VariantINT64, VariantUINT64:
		return v.i64 == o.i64
	case VariantBOOL, VariantBYTE, VariantCHAR, VariantFLOAT, VariantDOUBLE, VariantINT, VariantUINT:
		return v.num == o.num
	case VariantSTRING:
		return v.str == o.str
	case VariantVECTOR2, VariantVECTOR3, VariantVECTOR4, VariantCOLOURF:
		return v.vec.X == o.vec.X && v.vec.Y == o.vec.Y && v.vec.Z == o.vec.Z && v.vec.W == o.vec.W
	case VariantCOLOURB:
		return v.col.Equals(o.col)
	}
	if v.ptr == nil || o.ptr == nil {
		return v.ptr == nil && o.ptr == nil
	}
	return v.ptr.variantEquals(o.ptr)
}

