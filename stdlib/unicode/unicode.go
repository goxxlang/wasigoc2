// Subset of unicode. IsLetter, IsNumber, IsDigit, IsUpper and IsLower use
// the full category tables (tables.go, from gen_tables.py); IsSpace is Go's
// definition. Case mapping and IsPunct are still ASCII + Latin-1.
package unicode

const (
	MaxRune         = 1114111
	ReplacementChar = 65533
	MaxASCII        = 127
	MaxLatin1       = 255
)

// in reports whether r falls in one of the sorted [lo, hi] pairs of t.
func in(r rune, t []int32) bool {
	lo := 0
	hi := len(t)/2 - 1
	for lo <= hi {
		m := (lo + hi) / 2
		if r < rune(t[2*m]) {
			hi = m - 1
		} else if r > rune(t[2*m+1]) {
			lo = m + 1
		} else {
			return true
		}
	}
	return false
}

// IsDigit reports a decimal digit (category Nd).
func IsDigit(r rune) bool {
	if r <= MaxLatin1 {
		return r >= 48 && r <= 57
	}
	return in(r, digitRanges)
}

// IsUpper reports an upper case letter (category Lu).
func IsUpper(r rune) bool {
	if r <= MaxASCII {
		return r >= 65 && r <= 90
	}
	return in(r, upperRanges)
}

// IsLower reports a lower case letter (category Ll).
func IsLower(r rune) bool {
	if r <= MaxASCII {
		return r >= 97 && r <= 122
	}
	return in(r, lowerRanges)
}

// IsLetter reports a letter (category L).
func IsLetter(r rune) bool {
	if r <= MaxASCII {
		return (r >= 65 && r <= 90) || (r >= 97 && r <= 122)
	}
	return in(r, letterRanges)
}

// IsSpace reports white space as Go defines it: in Latin-1 tab, LF, VT,
// FF, CR, space, U+0085 (NEL) and U+00A0 (NBSP); beyond it the White_Space
// property.
func IsSpace(r rune) bool {
	if r <= MaxLatin1 {
		return r == 32 || (r >= 9 && r <= 13) || r == 133 || r == 160
	}
	return r == 0x1680 || (r >= 0x2000 && r <= 0x200a) || r == 0x2028 || r == 0x2029 ||
		r == 0x202f || r == 0x205f || r == 0x3000
}

func IsPunct(r rune) bool {
	if r >= 33 && r <= 47 {
		return true
	}
	if r >= 58 && r <= 64 {
		return true
	}
	if r >= 91 && r <= 96 {
		return true
	}
	if r >= 123 && r <= 126 {
		return true
	}
	return false
}

func IsControl(r rune) bool {
	if r < 32 {
		return true
	}
	return r == 127
}

func IsPrint(r rune) bool {
	return !IsControl(r)
}

// IsNumber reports a number (category N).
func IsNumber(r rune) bool {
	if r <= MaxASCII {
		return r >= 48 && r <= 57
	}
	return in(r, numberRanges)
}

func ToUpper(r rune) rune {
	if r >= 97 && r <= 122 {
		return r - 32
	}
	if r >= 224 && r <= 254 && r != 247 {
		return r - 32
	}
	return r
}

func ToLower(r rune) rune {
	if r >= 65 && r <= 90 {
		return r + 32
	}
	if r >= 192 && r <= 222 && r != 215 {
		return r + 32
	}
	return r
}

func ToTitle(r rune) rune {
	return ToUpper(r)
}
