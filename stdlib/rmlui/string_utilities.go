// Port of RmlUi Source/Core/StringUtilities.cpp and the string half of
// Include/RmlUi/Core/TypeConverter.inl (C atof/atoi/strtof/sscanf and
// printf("%.3f") semantics, which the parsers and data bindings rely on).
package rmlui

import "strings"

// builderWriteByte stands in for strings.Builder.WriteByte, which this
// stdlib's Builder lacks.
func builderWriteByte(b *strings.Builder, c byte) {
	b.WriteString(string([]byte{c}))
}

func StringIsWhitespace(x byte) bool {
	return x == '\r' || x == '\n' || x == ' ' || x == '\t'
}

func StringToLower(s string) string {
	b := []byte(s)
	for i := 0; i < len(b); i++ {
		c := b[i]
		if c >= 'A' && c <= 'Z' {
			b[i] = c + ('a' - 'A')
		}
	}
	return string(b)
}

func StringToUpper(s string) string {
	b := []byte(s)
	for i := 0; i < len(b); i++ {
		c := b[i]
		if c >= 'a' && c <= 'z' {
			b[i] = c - ('a' - 'A')
		}
	}
	return string(b)
}

func StringEncodeRml(s string) string {
	var b strings.Builder
	for i := 0; i < len(s); i++ {
		c := s[i]
		switch c {
		case '<':
			b.WriteString("&lt;")
		case '>':
			b.WriteString("&gt;")
		case '&':
			b.WriteString("&amp;")
		case '"':
			b.WriteString("&quot;")
		default:
			builderWriteByte(&b, c)
		}
	}
	return b.String()
}

func byteAt(s string, i int) byte {
	if i < 0 || i >= len(s) {
		return 0
	}
	return s[i]
}

func isHexDigit(c byte) bool {
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')
}

func StringDecodeRml(s string) string {
	var b strings.Builder
	i := 0
	for i < len(s) {
		if s[i] == '&' {
			if byteAt(s, i+1) == 'l' && byteAt(s, i+2) == 't' && byteAt(s, i+3) == ';' {
				b.WriteString("<")
				i += 4
				continue
			}
			if byteAt(s, i+1) == 'g' && byteAt(s, i+2) == 't' && byteAt(s, i+3) == ';' {
				b.WriteString(">")
				i += 4
				continue
			}
			if byteAt(s, i+1) == 'a' && byteAt(s, i+2) == 'm' && byteAt(s, i+3) == 'p' && byteAt(s, i+4) == ';' {
				b.WriteString("&")
				i += 5
				continue
			}
			if byteAt(s, i+1) == 'q' && byteAt(s, i+2) == 'u' && byteAt(s, i+3) == 'o' && byteAt(s, i+4) == 't' && byteAt(s, i+5) == ';' {
				b.WriteString("\"")
				i += 6
				continue
			}
			if byteAt(s, i+1) == '#' {
				start := i + 2
				hex := byteAt(s, i+2) == 'x'
				if hex {
					start++
				}
				j := 0
				for j < 8 {
					c := byteAt(s, start+j)
					if hex && !isHexDigit(c) {
						break
					}
					if !hex && !(c >= '0' && c <= '9') {
						break
					}
					j++
				}
				if j > 0 && byteAt(s, start+j) == ';' {
					code := 0
					for k := 0; k < j; k++ {
						c := s[start+k]
						if hex {
							code = code*16 + MathHexToDecimal(c)
						} else {
							code = code*10 + int(c-'0')
						}
					}
					if code != 0 {
						b.WriteString(StringToUTF8(Character(code)))
						i = start + j + 1
						continue
					}
				}
			}
		}
		builderWriteByte(&b, s[i])
		i++
	}
	return b.String()
}

func StringReplace(subject string, search string, replace string) string {
	if search == "" {
		return subject
	}
	return strings.ReplaceAll(subject, search, replace)
}

func StringReplaceChar(subject string, search byte, replace byte) string {
	b := []byte(subject)
	for i := 0; i < len(b); i++ {
		if b[i] == search {
			b[i] = replace
		}
	}
	return string(b)
}

// StringExpand is StringUtilities::ExpandString with a single delimiter:
// splits on delimiter outside '...' / "..." quotes, trimming whitespace from
// each item. The quote characters themselves are dropped.
func StringExpand(s string, delimiter byte, ignoreRepeatedDelimiters bool) []string {
	list := []string{}
	var quote byte = 0
	lastCharDelimiter := true
	start := -1
	end := 0
	for i := 0; i < len(s); i++ {
		c := s[i]
		if lastCharDelimiter && quote == 0 && (c == '"' || c == '\'') {
			quote = c
		} else if quote != 0 && c == quote && byteAt(s, i-1) != '\\' {
			quote = 0
		} else if c == delimiter && quote == 0 {
			if start >= 0 {
				list = append(list, s[start:end+1])
			} else if !ignoreRepeatedDelimiters {
				list = append(list, "")
			}
			lastCharDelimiter = true
			start = -1
			continue
		} else if !StringIsWhitespace(c) || quote != 0 {
			if start < 0 {
				start = i
			}
			end = i
			lastCharDelimiter = false
		}
		// Opening and closing quotes fall in the first two branches and never
		// advance the item bounds, so quotes are stripped from the items.
	}
	if start >= 0 {
		list = append(list, s[start:end+1])
	}
	return list
}

// StringExpandList is ExpandString(list, string) with the default ','
// delimiter.
func StringExpandList(s string) []string { return StringExpand(s, ',', false) }

func isEscapedCharacter(s string, index int) bool {
	if index == 0 || index > len(s) {
		return false
	}
	n := 0
	i := index
	for i > 0 && s[i-1] == '\\' {
		n++
		i--
	}
	return n%2 == 1
}

// StringExpandNested is the ExpandString overload with quote/unquote
// characters that nest, e.g. '(' and ')' so commas inside function
// arguments do not split.
func StringExpandNested(s string, delimiter byte, quoteCharacter byte, unquoteCharacter byte, ignoreRepeatedDelimiters bool) []string {
	list := []string{}
	depth := 0
	start := -1
	end := 0
	for i := 0; i < len(s); i++ {
		c := s[i]
		escaped := isEscapedCharacter(s, i)
		if c == quoteCharacter && !escaped {
			depth++
		} else if c == unquoteCharacter && !escaped {
			depth--
		}
		if c == delimiter && depth == 0 && !escaped {
			if start >= 0 {
				list = append(list, s[start:end+1])
			} else if !ignoreRepeatedDelimiters {
				list = append(list, "")
			}
			start = -1
		} else if !StringIsWhitespace(c) || depth > 0 {
			if start < 0 {
				start = i
			}
			end = i
		}
	}
	if start >= 0 {
		list = append(list, s[start:end+1])
	}
	return list
}

func StringJoin(list []string, delimiter byte) string {
	var b strings.Builder
	for i := 0; i < len(list); i++ {
		b.WriteString(list[i])
		if delimiter != 0 && i < len(list)-1 {
			builderWriteByte(&b, delimiter)
		}
	}
	return b.String()
}

func StringStripWhitespace(s string) string {
	start := 0
	end := len(s)
	for start < end && StringIsWhitespace(s[start]) {
		start++
	}
	for end > start && StringIsWhitespace(s[end-1]) {
		end--
	}
	if start < end {
		return s[start:end]
	}
	return ""
}

func StringTrimTrailingDotZeros(s string) string {
	newSize := len(s)
	for i := len(s) - 1; i >= 0; i-- {
		if s[i] == '.' {
			newSize = i
			break
		} else if s[i] == '0' {
			newSize = i
		} else {
			break
		}
	}
	if newSize < len(s) {
		return s[:newSize]
	}
	return s
}

func StringStartsWith(s string, start string) bool {
	return len(s) >= len(start) && s[:len(start)] == start
}

func StringEndsWith(s string, end string) bool {
	return len(s) >= len(end) && s[len(s)-len(end):] == end
}

func StringCompareCaseInsensitive(lhs string, rhs string) bool {
	if len(lhs) != len(rhs) {
		return false
	}
	return StringToLower(lhs) == StringToLower(rhs)
}

// StringToCharacter decodes the UTF-8 code point starting at s[i]. Invalid
// sequences return CharacterNull, as in the C++.
func StringToCharacter(s string, i int) Character {
	if i >= len(s) {
		return CharacterNull
	}
	c := s[i]
	if c&0x80 == 0 {
		return Character(c)
	}
	numBytes := 0
	code := 0
	if c&0xE0 == 0xC0 {
		numBytes = 2
		code = int(c & 0x1F)
	} else if c&0xF0 == 0xE0 {
		numBytes = 3
		code = int(c & 0x0F)
	} else if c&0xF8 == 0xF0 {
		numBytes = 4
		code = int(c & 0x07)
	} else {
		return CharacterNull
	}
	if len(s)-i < numBytes {
		return CharacterNull
	}
	for k := 1; k < numBytes; k++ {
		b := s[i+k]
		if b&0xC0 != 0x80 {
			return CharacterNull
		}
		code = (code << 6) | int(b&0x3F)
	}
	return Character(code)
}

func StringBytesUTF8(ch Character) int {
	c := int(ch)
	if c < 0x80 {
		return 1
	} else if c < 0x800 {
		return 2
	} else if c < 0x10000 {
		return 3
	} else if c <= 0x10FFFF {
		return 4
	}
	return 0
}

func StringToUTF8(ch Character) string {
	c := int(ch)
	b := []byte{}
	if c < 0x80 {
		b = append(b, byte(c))
	} else if c < 0x800 {
		b = append(b, byte(((c>>6)&0x1F)|0xC0), byte((c&0x3F)|0x80))
	} else if c < 0x10000 {
		b = append(b, byte(((c>>12)&0x0F)|0xE0), byte(((c>>6)&0x3F)|0x80), byte((c&0x3F)|0x80))
	} else if c <= 0x10FFFF {
		b = append(b, byte(((c>>18)&0x07)|0xF0), byte(((c>>12)&0x3F)|0x80), byte(((c>>6)&0x3F)|0x80), byte((c&0x3F)|0x80))
	}
	return string(b)
}

func StringLengthUTF8(s string) int {
	n := 0
	for i := 0; i < len(s); i++ {
		if s[i]&0xC0 == 0x80 {
			n++
		}
	}
	return len(s) - n
}

// StringSeekForwardUTF8 skips continuation bytes starting at index p.
func StringSeekForwardUTF8(s string, p int) int {
	for p < len(s) && s[p]&0xC0 == 0x80 {
		p++
	}
	return p
}

// StringSeekBackwardUTF8 moves p back over continuation bytes.
func StringSeekBackwardUTF8(s string, p int) int {
	for p > 0 && p < len(s) && s[p]&0xC0 == 0x80 {
		p--
	}
	return p
}

// StringNextUTF8 returns the byte index of the character after the one at p.
func StringNextUTF8(s string, p int) int {
	if p >= len(s) {
		return len(s)
	}
	return StringSeekForwardUTF8(s, p+1)
}

// StringPrevUTF8 returns the byte index of the character before the one at p.
func StringPrevUTF8(s string, p int) int {
	if p <= 0 {
		return 0
	}
	return StringSeekBackwardUTF8(s, p-1)
}

func StringConvertCharacterOffsetToByteOffset(s string, characterOffset int) int {
	if characterOffset >= len(s) {
		return len(s)
	}
	count := 0
	p := 0
	for p < len(s) {
		count++
		if count > characterOffset {
			return p
		}
		p = StringNextUTF8(s, p)
	}
	return len(s)
}

func StringConvertByteOffsetToCharacterOffset(s string, byteOffset int) int {
	count := 0
	p := 0
	for p < len(s) {
		if p >= byteOffset {
			break
		}
		count++
		p = StringNextUTF8(s, p)
	}
	return count
}

// StringToCharacters decodes a whole UTF-8 string.
func StringToCharacters(s string) []Character {
	out := []Character{}
	p := 0
	for p < len(s) {
		out = append(out, StringToCharacter(s, p))
		p = StringNextUTF8(s, p)
	}
	return out
}

// ---- C numeric conversions ----

// parseFloatPrefix is strtod: skips leading whitespace, parses the longest
// numeric prefix, and returns the value and the index just past it (0 when
// nothing was parsed).
func parseFloatPrefix(s string) (float64, int) {
	i := 0
	for i < len(s) && (StringIsWhitespace(s[i]) || s[i] == '\f' || s[i] == '\v') {
		i++
	}
	start := i
	neg := false
	if i < len(s) && (s[i] == '+' || s[i] == '-') {
		neg = s[i] == '-'
		i++
	}
	mant := 0.0
	digits := 0
	for i < len(s) && s[i] >= '0' && s[i] <= '9' {
		mant = mant*10 + float64(s[i]-'0')
		i++
		digits++
	}
	scale := 0
	if i < len(s) && s[i] == '.' {
		j := i + 1
		fracDigits := 0
		for j < len(s) && s[j] >= '0' && s[j] <= '9' {
			mant = mant*10 + float64(s[j]-'0')
			scale--
			j++
			fracDigits++
		}
		if digits > 0 || fracDigits > 0 {
			i = j
			digits += fracDigits
		}
	}
	if digits == 0 {
		// "inf"/"nan" are not needed by any RmlUi parser path.
		_ = start
		return 0, 0
	}
	if i < len(s) && (s[i] == 'e' || s[i] == 'E') {
		j := i + 1
		eneg := false
		if j < len(s) && (s[j] == '+' || s[j] == '-') {
			eneg = s[j] == '-'
			j++
		}
		if j < len(s) && s[j] >= '0' && s[j] <= '9' {
			e := 0
			for j < len(s) && s[j] >= '0' && s[j] <= '9' {
				if e < 10000 {
					e = e*10 + int(s[j]-'0')
				}
				j++
			}
			if eneg {
				scale -= e
			} else {
				scale += e
			}
			i = j
		}
	}
	v := mant
	for scale > 0 {
		v = v * 10
		scale--
	}
	for scale < 0 {
		v = v / 10
		scale++
	}
	if neg {
		v = -v
	}
	return v, i
}

// Atof is C atof: the numeric prefix of s, or 0.
func Atof(s string) float64 {
	v, _ := parseFloatPrefix(s)
	return v
}

// Strtof is strtof: ok is false when no number was parsed; rest is the
// unparsed tail.
func Strtof(s string) (float32, string, bool) {
	v, n := parseFloatPrefix(s)
	if n == 0 {
		return 0, s, false
	}
	return float32(v), s[n:], true
}

// parseIntPrefix is strtol base 10.
func parseIntPrefix(s string) (int, int) {
	i := 0
	for i < len(s) && (StringIsWhitespace(s[i]) || s[i] == '\f' || s[i] == '\v') {
		i++
	}
	neg := false
	if i < len(s) && (s[i] == '+' || s[i] == '-') {
		neg = s[i] == '-'
		i++
	}
	digits := 0
	v := 0
	for i < len(s) && s[i] >= '0' && s[i] <= '9' {
		v = v*10 + int(s[i]-'0')
		i++
		digits++
	}
	if digits == 0 {
		return 0, 0
	}
	if neg {
		v = -v
	}
	return v, i
}

// Atoi is C atoi.
func Atoi(s string) int {
	v, _ := parseIntPrefix(s)
	return v
}

// ScanInt is sscanf(s, "%d"): ok is false when no integer prefix exists.
func ScanInt(s string) (int, bool) {
	v, n := parseIntPrefix(s)
	return v, n > 0
}

func FormatInt(v int) string {
	if v == 0 {
		return "0"
	}
	neg := v < 0
	u := uint64(v)
	if neg {
		u = uint64(-v)
	}
	buf := []byte{}
	for u > 0 {
		buf = append(buf, byte('0'+u%10))
		u = u / 10
	}
	if neg {
		buf = append(buf, '-')
	}
	out := make([]byte, len(buf))
	for i := 0; i < len(buf); i++ {
		out[i] = buf[len(buf)-1-i]
	}
	return string(out)
}

// FormatFixed3 is printf("%.3f").
func FormatFixed3(f float64) string {
	if f != f {
		return "nan"
	}
	neg := f < 0
	if neg {
		f = -f
	}
	if f > 1e15 {
		s := FormatInt(int(f))
		if neg {
			s = "-" + s
		}
		return s + ".000"
	}
	scaled := uint64(floor64(f*1000 + 0.5))
	ip := scaled / 1000
	fp := scaled % 1000
	s := FormatInt(int(ip)) + "."
	if fp < 100 {
		s = s + "0"
	}
	if fp < 10 {
		s = s + "0"
	}
	s = s + FormatInt(int(fp))
	if neg && scaled != 0 {
		s = "-" + s
	}
	return s
}

// FormatFloat is TypeConverter<float, String>: "%.3f" with trailing zeros
// and dot trimmed.
func FormatFloat(f float32) string { return StringTrimTrailingDotZeros(FormatFixed3(float64(f))) }

func FormatDouble(f float64) string { return StringTrimTrailingDotZeros(FormatFixed3(f)) }

func FormatBool(b bool) string {
	if b {
		return "1"
	}
	return "0"
}

const hexDigits = "0123456789abcdef"

func formatHexByte(b byte) string {
	return string([]byte{hexDigits[b>>4], hexDigits[b&15]})
}

// FormatColourb is TypeConverter<Colourb, String>: #rrggbb or #rrggbbaa.
func FormatColourb(c Colourb) string {
	s := "#" + formatHexByte(c.Red) + formatHexByte(c.Green) + formatHexByte(c.Blue)
	if c.Alpha != 255 {
		s = s + formatHexByte(c.Alpha)
	}
	return s
}

func FormatVector2f(v Vector2f) string { return FormatFloat(v.X) + ", " + FormatFloat(v.Y) }
func FormatVector2i(v Vector2i) string { return FormatInt(v.X) + ", " + FormatInt(v.Y) }
func FormatVector3f(v Vector3f) string {
	return FormatFloat(v.X) + ", " + FormatFloat(v.Y) + ", " + FormatFloat(v.Z)
}
func FormatVector4f(v Vector4f) string {
	return FormatFloat(v.X) + ", " + FormatFloat(v.Y) + ", " + FormatFloat(v.Z) + ", " + FormatFloat(v.W)
}
func FormatColourf(c Colourf) string {
	return FormatFloat(c.Red) + ", " + FormatFloat(c.Green) + ", " + FormatFloat(c.Blue) + ", " + FormatFloat(c.Alpha)
}

// ParseBool is TypeConverter<String, bool>.
func ParseBool(s string) (bool, bool) {
	l := StringToLower(s)
	if l == "1" || l == "true" {
		return true, true
	}
	if l == "0" || l == "false" {
		return false, true
	}
	return false, false
}

// ParseVector2f is the "x, y" string converter.
func ParseVector2f(s string) (Vector2f, bool) {
	parts := StringExpand(s, ',', false)
	if len(parts) != 2 {
		return Vector2f{}, false
	}
	return Vector2f{float32(Atof(parts[0])), float32(Atof(parts[1]))}, true
}

func ParseVector2i(s string) (Vector2i, bool) {
	parts := StringExpand(s, ',', false)
	if len(parts) != 2 {
		return Vector2i{}, false
	}
	return Vector2i{Atoi(parts[0]), Atoi(parts[1])}, true
}
