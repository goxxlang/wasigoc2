// Port of RmlUi Include/RmlUi/Core/PropertyParser.h and Source/Core/
// PropertyParserNumber.cpp, PropertyParserKeyword.cpp,
// PropertyParserString.cpp, PropertyParserRatio.cpp, PropertyParserColour.cpp.
package rmlui

// map[string]int is Rml::map[string]int: keyword name to value.

// PropertyParser is Rml::PropertyParser.
type PropertyParser interface {
	ParseValue(property *Property, value string, parameters map[string]int) bool
}

// PropertyParserNumber is Rml::PropertyParserNumber.
type PropertyParserNumber struct {
	units    Unit
	zeroUnit Unit
}

func NewPropertyParserNumber(units Unit, zeroUnit Unit) *PropertyParserNumber {
	return &PropertyParserNumber{units: units, zeroUnit: zeroUnit}
}

var numberUnitStringMap = map[string]Unit{
	"":    UnitNUMBER,
	"%":   UnitPERCENT,
	"px":  UnitPX,
	"dp":  UnitDP,
	"x":   UnitX,
	"vw":  UnitVW,
	"vh":  UnitVH,
	"em":  UnitEM,
	"rem": UnitREM,
	"in":  UnitINCH,
	"cm":  UnitCM,
	"mm":  UnitMM,
	"pt":  UnitPT,
	"pc":  UnitPC,
	"deg": UnitDEG,
	"rad": UnitRAD,
}

func (p *PropertyParserNumber) ParseValue(property *Property, value string, parameters map[string]int) bool {
	unitPos := 0
	for i := len(value) - 1; i >= 0; i-- {
		c := value[i]
		if (c >= '0' && c <= '9') || StringIsWhitespace(c) {
			unitPos = i + 1
			break
		}
	}
	strNumber := value[:unitPos]
	strUnit := StringToLower(value[unitPos:])
	f, _, ok := Strtof(strNumber)
	if !ok {
		return false
	}
	unit, found := numberUnitStringMap[strUnit]
	if !found {
		return false
	}
	if AnyUnit(unit & p.units) {
		property.Value = VariantFloat(f)
		property.Unit = unit
		return true
	}
	if unit == UnitNUMBER {
		if p.zeroUnit != UnitUNKNOWN && f == 0 {
			property.Unit = p.zeroUnit
			property.Value = VariantFloat(0)
			return true
		}
	}
	return false
}

// PropertyParserKeyword is Rml::PropertyParserKeyword.
type PropertyParserKeyword struct{}

func (p *PropertyParserKeyword) ParseValue(property *Property, value string, parameters map[string]int) bool {
	v, ok := parameters[StringToLower(value)]
	if !ok {
		return false
	}
	property.Value = VariantInt(v)
	property.Unit = UnitKEYWORD
	return true
}

// PropertyParserString is Rml::PropertyParserString.
type PropertyParserString struct{}

func (p *PropertyParserString) ParseValue(property *Property, value string, parameters map[string]int) bool {
	property.Value = VariantString(value)
	property.Unit = UnitSTRING
	return true
}

// PropertyParserRatio is Rml::PropertyParserRatio.
type PropertyParserRatio struct{}

func (p *PropertyParserRatio) ParseValue(property *Property, value string, parameters map[string]int) bool {
	parts := StringExpand(value, '/', false)
	if len(parts) != 2 {
		return false
	}
	property.Value = VariantVector2f(Vector2f{float32(Atof(parts[0])), float32(Atof(parts[1]))})
	property.Unit = UnitRATIO
	return true
}

// PropertyParserColour is Rml::PropertyParserColour.
type PropertyParserColour struct{}

func (p *PropertyParserColour) ParseValue(property *Property, value string, parameters map[string]int) bool {
	c, ok := ParseColour(value)
	if !ok {
		return false
	}
	property.Value = VariantColourb(c)
	property.Unit = UnitCOLOUR
	return true
}

var htmlColours = map[string]Colourb{
	"black":       {0, 0, 0, 255},
	"silver":      {192, 192, 192, 255},
	"gray":        {128, 128, 128, 255},
	"grey":        {128, 128, 128, 255},
	"white":       {255, 255, 255, 255},
	"maroon":      {128, 0, 0, 255},
	"red":         {255, 0, 0, 255},
	"orange":      {255, 165, 0, 255},
	"purple":      {128, 0, 128, 255},
	"fuchsia":     {255, 0, 255, 255},
	"green":       {0, 128, 0, 255},
	"lime":        {0, 255, 0, 255},
	"olive":       {128, 128, 0, 255},
	"yellow":      {255, 255, 0, 255},
	"navy":        {0, 0, 128, 255},
	"blue":        {0, 0, 255, 255},
	"teal":        {0, 128, 128, 255},
	"aqua":        {0, 255, 255, 255},
	"transparent": {0, 0, 0, 0},
}

func strPrefix(s string, n int) string {
	if len(s) < n {
		return s
	}
	return s[:n]
}

// ParseColour is PropertyParserColour::ParseColour.
func ParseColour(value string) (Colourb, bool) {
	if value == "" {
		return Colourb{}, false
	}
	if value[0] == '#' {
		return parseHexColour(value)
	}
	p3 := strPrefix(value, 3)
	p5 := strPrefix(value, 5)
	if p3 == "rgb" {
		return parseRGBColour(value)
	}
	if p3 == "hsl" {
		return parseHSLColour(value)
	}
	if p3 == "lab" || p3 == "lch" {
		return parseCIELABColour(value)
	}
	if p5 == "oklab" || p5 == "oklch" {
		return parseOklabColour(value)
	}
	c, ok := htmlColours[StringToLower(value)]
	return c, ok
}

func parseHexColour(value string) (Colourb, bool) {
	hex := []byte("ffffffff")
	switch len(value) {
	case 5:
		hex[6] = value[4]
		hex[7] = value[4]
		hex[0] = value[1]
		hex[1] = value[1]
		hex[2] = value[2]
		hex[3] = value[2]
		hex[4] = value[3]
		hex[5] = value[3]
	case 4:
		hex[0] = value[1]
		hex[1] = value[1]
		hex[2] = value[2]
		hex[3] = value[2]
		hex[4] = value[3]
		hex[5] = value[3]
	case 9:
		hex[6] = value[7]
		hex[7] = value[8]
		for i := 0; i < 6; i++ {
			hex[i] = value[1+i]
		}
	case 7:
		for i := 0; i < 6; i++ {
			hex[i] = value[1+i]
		}
	default:
		return Colourb{}, false
	}
	c := Colourb{}
	for i := 0; i < 4; i++ {
		tens := MathHexToDecimal(hex[2*i])
		ones := MathHexToDecimal(hex[2*i+1])
		if tens == -1 || ones == -1 {
			return Colourb{}, false
		}
		c.SetIndex(i, byte(tens*16+ones))
	}
	return c, true
}

func getColourFunctionValues(value string, isCommaSeparated bool) ([]string, bool) {
	find := indexByte(value, '(')
	if find < 0 {
		return nil, false
	}
	begin := find + 1
	end := lastIndexByte(value, ')')
	inner := ""
	if end < 0 || end < begin {
		inner = value[begin:]
	} else {
		inner = value[begin:end]
	}
	var delim byte = ' '
	if isCommaSeparated {
		delim = ','
	}
	return StringExpand(inner, delim, !isCommaSeparated), true
}

func endsWithPercent(s string) bool { return len(s) > 0 && s[len(s)-1] == '%' }

func withoutLast(s string) string { return s[:len(s)-1] }

func parseRGBColour(value string) (Colourb, bool) {
	values, ok := getColourFunctionValues(value, true)
	if !ok {
		return Colourb{}, false
	}
	if len(value) > 3 && value[3] == 'a' {
		if len(values) != 4 {
			return Colourb{}, false
		}
	} else {
		if len(values) != 3 {
			return Colourb{}, false
		}
		values = append(values, "255")
	}
	c := Colourb{}
	for i := 0; i < 4; i++ {
		component := 0
		if endsWithPercent(values[i]) {
			component = int(float32(Atof(withoutLast(values[i]))) * (255.0 / 100.0))
		} else {
			component = Atoi(values[i])
		}
		c.SetIndex(i, byte(MathClampInt(component, 0, 255)))
	}
	return c, true
}

func hslF(h float32, s float32, l float32, n float32) float32 {
	k := float32(fmod64(float64(n+h*(1.0/30.0)), 12.0))
	a := s * MathMin(l, 1.0-l)
	inner := MathMin(MathMin(k-3.0, 9.0-k), 1.0)
	return l - a*MathMax(-1.0, inner)
}

func hslaToRGBA(vals [4]float32) [4]float32 {
	if vals[1] == 0 {
		vals[0] = vals[2]
		vals[1] = vals[2]
		return vals
	}
	h := float32(fmod64(float64(vals[0]), 360.0))
	if h < 0 {
		h += 360.0
	}
	s := vals[1]
	l := vals[2]
	vals[0] = hslF(h, s, l, 0.0)
	vals[1] = hslF(h, s, l, 8.0)
	vals[2] = hslF(h, s, l, 4.0)
	return vals
}

func parseHSLColour(value string) (Colourb, bool) {
	values, ok := getColourFunctionValues(value, true)
	if !ok {
		return Colourb{}, false
	}
	if len(value) > 3 && value[3] == 'a' {
		if len(values) != 4 {
			return Colourb{}, false
		}
	} else {
		if len(values) != 3 {
			return Colourb{}, false
		}
		values = append(values, "1.0")
	}
	var vals [4]float32
	vals[0] = float32(Atof(values[0]))
	vals[3] = float32(Atof(values[3]))
	for i := 1; i <= 2; i++ {
		if !endsWithPercent(values[i]) {
			return Colourb{}, false
		}
		vals[i] = float32(Atof(withoutLast(values[i]))) * (1.0 / 100.0)
	}
	vals = hslaToRGBA(vals)
	c := Colourb{}
	for i := 0; i < 4; i++ {
		c.SetIndex(i, byte(MathClampInt(int(vals[i]*255.0), 0, 255)))
	}
	return c, true
}

func inverseSRGBNonlinearTransfer(channel float32) float32 {
	if channel > 0.0031308 {
		return 1.055*float32(pow64(float64(channel), 1.0/2.4)) - 0.055
	}
	return 12.92 * channel
}

func cielabToRGBA(values [4]float32) [4]float32 {
	yp := (values[0] + 16.0) / 116.0
	xp := (values[1] / 500.0) + yp
	zp := yp - (values[2] / 200.0)
	cube := func(v float32) float32 {
		if v*v*v > 0.008856 {
			return v * v * v
		}
		return (v - (16.0 / 116.0)) / 7.787
	}
	x := cube(xp) * 0.95047
	y := cube(yp) * 1.0
	z := cube(zp) * 1.08883
	r := 3.2404548*x - 1.5371389*y - 0.4985315*z
	g := -0.9692664*x + 1.8760109*y + 0.0415561*z
	b := 0.0556434*x - 0.2040259*y + 1.0572252*z
	values[0] = MathClamp(inverseSRGBNonlinearTransfer(r), 0, 1)
	values[1] = MathClamp(inverseSRGBNonlinearTransfer(g), 0, 1)
	values[2] = MathClamp(inverseSRGBNonlinearTransfer(b), 0, 1)
	return values
}

func oklabToRGBA(values [4]float32) [4]float32 {
	lightness := values[0]
	aAxis := values[1]
	bAxis := values[2]
	lp := 1.0*lightness + 0.3963377774*aAxis + 0.2158037573*bAxis
	mp := 1.0*lightness - 0.1055613458*aAxis - 0.0638541728*bAxis
	sp := 1.0*lightness - 0.0894841775*aAxis - 1.2914855480*bAxis
	l := lp * lp * lp
	m := mp * mp * mp
	s := sp * sp * sp
	r := 4.0767416621*l - 3.3077115913*m + 0.2309699292*s
	g := -1.2684380046*l + 2.6097574011*m - 0.3413193965*s
	b := -0.0041960863*l - 0.7034186147*m + 1.7076147010*s
	values[0] = MathClamp(inverseSRGBNonlinearTransfer(r), 0, 1)
	values[1] = MathClamp(inverseSRGBNonlinearTransfer(g), 0, 1)
	values[2] = MathClamp(inverseSRGBNonlinearTransfer(b), 0, 1)
	return values
}

// splitSlashAlpha handles the "L a b / alpha" form shared by lab/lch/oklab.
func splitSlashAlpha(values []string) ([]string, bool) {
	if len(values) == 5 {
		if values[3] != "/" {
			return nil, false
		}
		return []string{values[0], values[1], values[2], values[4]}, true
	}
	if len(values) != 3 {
		return nil, false
	}
	return []string{values[0], values[1], values[2], "1.0"}, true
}

func parseCIELABColour(value string) (Colourb, bool) {
	raw, ok := getColourFunctionValues(value, false)
	if !ok {
		return Colourb{}, false
	}
	values, ok2 := splitSlashAlpha(raw)
	if !ok2 {
		return Colourb{}, false
	}
	var lab [4]float32
	for _, i := range lightnessAlphaIndices {
		if values[i] == "none" {
			lab[i] = 0
		} else if endsWithPercent(values[i]) {
			lab[i] = float32(Atof(withoutLast(values[i])))
			if i == 3 {
				lab[i] = lab[i] / 100.0
			}
		} else {
			lab[i] = float32(Atof(values[i]))
		}
		var hi float32 = 1.0
		if i == 0 {
			hi = 100.0
		}
		lab[i] = MathClamp(lab[i], 0, hi)
	}
	if strPrefix(value, 3) == "lab" {
		for i := 1; i <= 2; i++ {
			if values[i] == "none" {
				lab[i] = 0
			} else if endsWithPercent(values[i]) {
				lab[i] = float32(Atof(withoutLast(values[i]))) / 100.0 * 125.0
			} else {
				lab[i] = float32(Atof(values[i]))
			}
			lab[i] = MathClamp(lab[i], -160.0, 160.0)
		}
	} else {
		var chroma float32 = 0
		if values[1] == "none" {
			chroma = 0
		} else if endsWithPercent(values[1]) {
			chroma = float32(Atof(withoutLast(values[1]))) / 100.0 * 150.0
		} else {
			chroma = float32(Atof(values[1]))
		}
		chroma = MathClamp(chroma, 0, 230.0)
		var hue float32 = 0
		if values[2] != "none" {
			hue = float32(Atof(values[2]))
		}
		lab[1] = chroma * MathCos(MathDegreesToRadians(hue))
		lab[2] = chroma * MathSin(MathDegreesToRadians(hue))
	}
	lab = cielabToRGBA(lab)
	c := Colourb{}
	for i := 0; i < 4; i++ {
		c.SetIndex(i, byte(MathClampInt(int(lab[i]*255.0), 0, 255)))
	}
	return c, true
}

func parseOklabColour(value string) (Colourb, bool) {
	raw, ok := getColourFunctionValues(value, false)
	if !ok {
		return Colourb{}, false
	}
	values, ok2 := splitSlashAlpha(raw)
	if !ok2 {
		return Colourb{}, false
	}
	var lab [4]float32
	for _, i := range lightnessAlphaIndices {
		if values[i] == "none" {
			lab[i] = 0
		} else if endsWithPercent(values[i]) {
			lab[i] = float32(Atof(withoutLast(values[i]))) / 100.0
		} else {
			lab[i] = float32(Atof(values[i]))
		}
		lab[i] = MathClamp(lab[i], 0, 1)
	}
	if strPrefix(value, 5) == "oklab" {
		for i := 1; i <= 2; i++ {
			if values[i] == "none" {
				lab[i] = 0
			} else if endsWithPercent(values[i]) {
				lab[i] = float32(Atof(withoutLast(values[i]))) / 100.0 * 0.4
			} else {
				lab[i] = float32(Atof(values[i]))
			}
			lab[i] = MathClamp(lab[i], -0.5, 0.5)
		}
	} else {
		var chroma float32 = 0
		if values[1] == "none" {
			chroma = 0
		} else if endsWithPercent(values[1]) {
			chroma = float32(Atof(withoutLast(values[1]))) / 100.0 * 0.4
		} else {
			chroma = float32(Atof(values[1]))
		}
		chroma = MathClamp(chroma, 0, 0.5)
		var hue float32 = 0
		if values[2] != "none" {
			hue = float32(Atof(values[2]))
		}
		lab[1] = chroma * MathCos(MathDegreesToRadians(hue))
		lab[2] = chroma * MathSin(MathDegreesToRadians(hue))
	}
	lab = oklabToRGBA(lab)
	c := Colourb{}
	for i := 0; i < 4; i++ {
		c.SetIndex(i, byte(MathClampInt(int(lab[i]*255.0), 0, 255)))
	}
	return c, true
}

// noParameters stands in for an empty map[string]int{} argument; wasigoc does
// not parse a composite literal inside an if-condition call.
var noParameters = map[string]int{}

// lightnessAlphaIndices are the lab/lch/oklab channels parsed alike.
var lightnessAlphaIndices = []int{0, 3}
