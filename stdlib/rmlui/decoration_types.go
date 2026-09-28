// Port of RmlUi Include/RmlUi/Core/DecorationTypes.h and Source/Core/
// PropertyParserColorStopList.cpp, PropertyParserBoxShadow.cpp, plus their
// TypeConverter<..., String> in TypeConverter.cpp.
package rmlui

// ColorStop is Rml::ColorStop.
type ColorStop struct {
	Color    ColourbPremultiplied
	Position NumericValue
}

func (a ColorStop) Equals(b ColorStop) bool {
	return a.Color.Equals(b.Color) && a.Position.Number == b.Position.Number && a.Position.Unit == b.Position.Unit
}

// ColorStopList is Rml::ColorStopList.
type ColorStopList struct {
	Stops []ColorStop
}

func (a *ColorStopList) variantEquals(other VariantPayload) bool {
	b, ok := other.(*ColorStopList)
	if !ok || len(a.Stops) != len(b.Stops) {
		return false
	}
	for i := 0; i < len(a.Stops); i++ {
		if !a.Stops[i].Equals(b.Stops[i]) {
			return false
		}
	}
	return true
}

func (a *ColorStopList) variantString() string {
	dest := ""
	for i := 0; i < len(a.Stops); i++ {
		stop := a.Stops[i]
		dest = dest + FormatColourb(stop.Color.ToNonPremultiplied())
		if AnyUnit(stop.Position.Unit & UnitNUMBER_LENGTH_PERCENT) {
			dest = dest + " " + FormatFloat(stop.Position.Number) + UnitSuffix(stop.Position.Unit)
		}
		if i < len(a.Stops)-1 {
			dest = dest + ", "
		}
	}
	return dest
}

// BoxShadow is Rml::BoxShadow.
type BoxShadow struct {
	Color          ColourbPremultiplied
	OffsetX        NumericValue
	OffsetY        NumericValue
	BlurRadius     NumericValue
	SpreadDistance NumericValue
	Inset          bool
}

func numericEqual(a NumericValue, b NumericValue) bool { return a.Number == b.Number && a.Unit == b.Unit }

func (a BoxShadow) Equals(b BoxShadow) bool {
	return a.Color.Equals(b.Color) && numericEqual(a.OffsetX, b.OffsetX) && numericEqual(a.OffsetY, b.OffsetY) &&
		numericEqual(a.BlurRadius, b.BlurRadius) && numericEqual(a.SpreadDistance, b.SpreadDistance) && a.Inset == b.Inset
}

// BoxShadowList is Rml::BoxShadowList.
type BoxShadowList struct {
	Shadows []BoxShadow
}

func (a *BoxShadowList) variantEquals(other VariantPayload) bool {
	b, ok := other.(*BoxShadowList)
	if !ok || len(a.Shadows) != len(b.Shadows) {
		return false
	}
	for i := 0; i < len(a.Shadows); i++ {
		if !a.Shadows[i].Equals(b.Shadows[i]) {
			return false
		}
	}
	return true
}

func (a *BoxShadowList) variantString() string {
	dest := ""
	temp := ""
	for i := 0; i < len(a.Shadows); i++ {
		shadow := a.Shadows[i]
		values := []NumericValue{shadow.OffsetX, shadow.OffsetY, shadow.BlurRadius, shadow.SpreadDistance}
		for _, v := range values {
			if suffix, ok := UnitToString(v.Unit); ok {
				temp = temp + " " + FormatFloat(v.Number) + suffix
			}
		}
		if shadow.Inset {
			temp = temp + " inset"
		}
		dest = dest + FormatColourb(shadow.Color.ToNonPremultiplied()) + temp
		if i < len(a.Shadows)-1 {
			dest = dest + ", "
			temp = ""
		}
	}
	return dest
}

// PropertyParserColorStopList is Rml::PropertyParserColorStopList.
type PropertyParserColorStopList struct {
	colour PropertyParser
}

var colorStopPositionParser = NewPropertyParserNumber(UnitLENGTH_PERCENT|UnitANGLE, UnitPERCENT)

func (p *PropertyParserColorStopList) ParseValue(property *Property, value string, parameters map[string]int) bool {
	if value == "" {
		return false
	}
	stopStrings := StringExpandNested(value, ',', '(', ')', false)
	if len(stopStrings) == 0 {
		return false
	}
	accepted := UnitLENGTH_PERCENT
	if _, ok := parameters["angle"]; ok {
		accepted = UnitANGLE | UnitPERCENT
	}
	list := &ColorStopList{}
	for _, stopStr := range stopStrings {
		values := StringExpandNested(stopStr, ' ', '(', ')', true)
		if len(values) == 0 || len(values) > 3 {
			return false
		}
		pColor := NewProperty()
		if !p.colour.ParseValue(&pColor, values[0], noParameters) {
			return false
		}
		stop := ColorStop{}
		stop.Color = pColor.Value.GetColourb().ToPremultiplied()
		if len(values) <= 1 {
			list.Stops = append(list.Stops, stop)
		}
		for i := 1; i < len(values); i++ {
			pPosition := PropertyKeyword(LengthPercentageAutoAuto)
			if !colorStopPositionParser.ParseValue(&pPosition, values[i], noParameters) {
				return false
			}
			if AnyUnit(pPosition.Unit & accepted) {
				stop.Position = NumericValue{pPosition.Value.GetFloat(), pPosition.Unit}
			} else if pPosition.Unit != UnitKEYWORD {
				return false
			}
			list.Stops = append(list.Stops, stop)
		}
	}
	property.Value = VariantPointer(VariantCOLORSTOPLIST, list)
	property.Unit = UnitCOLORSTOPLIST
	return true
}

// PropertyParserBoxShadow is Rml::PropertyParserBoxShadow.
type PropertyParserBoxShadow struct {
	colour PropertyParser
	length PropertyParser
}

func (p *PropertyParserBoxShadow) ParseValue(property *Property, value string, parameters map[string]int) bool {
	if value == "" || value == "none" {
		property.Unit = UnitBOXSHADOWLIST
		property.Value = VariantPointer(VariantBOXSHADOWLIST, &BoxShadowList{})
		return true
	}
	shadowStrings := StringExpandNested(StringToLower(value), ',', '(', ')', false)
	if len(shadowStrings) == 0 {
		return false
	}
	list := &BoxShadowList{}
	for _, shadowStr := range shadowStrings {
		arguments := StringExpandNested(shadowStr, ' ', '(', ')', false)
		if len(arguments) == 0 {
			return false
		}
		shadow := BoxShadow{}
		lengthIndex := 0
		for _, argument := range arguments {
			if argument == "" {
				continue
			}
			prop := NewProperty()
			if p.length.ParseValue(&prop, argument, noParameters) {
				switch lengthIndex {
				case 0:
					shadow.OffsetX = prop.GetNumericValue()
				case 1:
					shadow.OffsetY = prop.GetNumericValue()
				case 2:
					shadow.BlurRadius = prop.GetNumericValue()
				case 3:
					shadow.SpreadDistance = prop.GetNumericValue()
				default:
					return false
				}
				lengthIndex++
			} else if argument == "inset" {
				shadow.Inset = true
			} else if p.colour.ParseValue(&prop, argument, noParameters) {
				shadow.Color = prop.Value.GetColourb().ToPremultiplied()
			} else {
				return false
			}
		}
		if lengthIndex < 2 {
			return false
		}
		list.Shadows = append(list.Shadows, shadow)
	}
	property.Unit = UnitBOXSHADOWLIST
	property.Value = VariantPointer(VariantBOXSHADOWLIST, list)
	return true
}
