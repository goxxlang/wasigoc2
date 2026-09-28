// Port of RmlUi Include/RmlUi/Core/Unit.h, NumericValue.h, and the unit
// half of Source/Core/TypeConverter.cpp.
package rmlui

// Unit is Rml::Unit, a bit set so several units can be accepted at once.
type Unit = int

const (
	UnitUNKNOWN  Unit = 0
	UnitKEYWORD  Unit = 1 << 0
	UnitSTRING   Unit = 1 << 1
	UnitCOLOUR   Unit = 1 << 2
	UnitRATIO    Unit = 1 << 3
	UnitNUMBER   Unit = 1 << 4
	UnitPERCENT  Unit = 1 << 5
	UnitPX       Unit = 1 << 6
	UnitDP       Unit = 1 << 7
	UnitVW       Unit = 1 << 8
	UnitVH       Unit = 1 << 9
	UnitX        Unit = 1 << 10
	UnitEM       Unit = 1 << 11
	UnitREM      Unit = 1 << 12
	UnitINCH     Unit = 1 << 13
	UnitCM       Unit = 1 << 14
	UnitMM       Unit = 1 << 15
	UnitPT       Unit = 1 << 16
	UnitPC       Unit = 1 << 17
	UnitDEG      Unit = 1 << 18
	UnitRAD      Unit = 1 << 19
	UnitTRANSFORM     Unit = 1 << 20
	UnitTRANSITION    Unit = 1 << 21
	UnitANIMATION     Unit = 1 << 22
	UnitDECORATOR     Unit = 1 << 23
	UnitFILTER        Unit = 1 << 24
	UnitFONTEFFECT    Unit = 1 << 25
	UnitCOLORSTOPLIST Unit = 1 << 26
	UnitBOXSHADOWLIST Unit = 1 << 27
	UnitVAR_EXPRESSION        Unit = 1 << 28
	UnitSHORTHAND_PLACEHOLDER Unit = 1 << 29

	UnitPPI_UNIT              Unit = UnitINCH | UnitCM | UnitMM | UnitPT | UnitPC
	UnitLENGTH                Unit = UnitPX | UnitDP | UnitVW | UnitVH | UnitEM | UnitREM | UnitPPI_UNIT
	UnitLENGTH_PERCENT        Unit = UnitLENGTH | UnitPERCENT
	UnitNUMBER_PERCENT        Unit = UnitNUMBER | UnitPERCENT
	UnitNUMBER_LENGTH_PERCENT Unit = UnitNUMBER | UnitLENGTH | UnitPERCENT
	UnitDP_SCALABLE_LENGTH    Unit = UnitDP | UnitPPI_UNIT
	UnitANGLE                 Unit = UnitDEG | UnitRAD
	UnitNUMERIC               Unit = UnitNUMBER_LENGTH_PERCENT | UnitANGLE | UnitX
)

// AnyUnit is Rml::Any(Units): true if any unit bit is set.
func AnyUnit(units Unit) bool { return units != UnitUNKNOWN }

// UnitToString is TypeConverter<Unit, String>. ok is false for units with
// no suffix spelling (keyword, string, colour, ...).
func UnitToString(unit Unit) (string, bool) {
	switch unit {
	case UnitNUMBER:
		return "", true
	case UnitPERCENT:
		return "%", true
	case UnitPX:
		return "px", true
	case UnitDP:
		return "dp", true
	case UnitVW:
		return "vw", true
	case UnitVH:
		return "vh", true
	case UnitX:
		return "x", true
	case UnitEM:
		return "em", true
	case UnitREM:
		return "rem", true
	case UnitINCH:
		return "in", true
	case UnitCM:
		return "cm", true
	case UnitMM:
		return "mm", true
	case UnitPT:
		return "pt", true
	case UnitPC:
		return "pc", true
	case UnitDEG:
		return "deg", true
	case UnitRAD:
		return "rad", true
	}
	return "", false
}

// UnitSuffix is Rml::ToString(Unit): the suffix, or "" when there is none.
func UnitSuffix(unit Unit) string {
	s, _ := UnitToString(unit)
	return s
}

// NumericValue is Rml::NumericValue: a number together with its unit.
type NumericValue struct {
	Number float32
	Unit   Unit
}
