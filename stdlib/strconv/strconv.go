// Tiny subset of Go's strconv package.
package strconv

import (
	"errors"
	"fmt"
)

func Itoa(i int) string {
	return fmt.Sprintf("%d", i)
}

const digits = "0123456789abcdefghijklmnopqrstuvwxyz"

func FormatInt(i int64, base int) string {
	if i == 0 {
		return "0"
	}
	neg := i < 0
	n := i
	if neg {
		n = -n
	}
	var rev []byte
	for n > 0 {
		d := n % int64(base)
		rev = append(rev, digits[d])
		n = n / int64(base)
	}
	out := make([]byte, len(rev))
	for k := 0; k < len(rev); k++ {
		out[k] = rev[len(rev)-1-k]
	}
	if neg {
		return "-" + string(out)
	}
	return string(out)
}

func FormatUint(i uint64, base int) string {
	if i == 0 {
		return "0"
	}
	n := i
	var rev []byte
	for n > 0 {
		d := n % uint64(base)
		rev = append(rev, digits[d])
		n = n / uint64(base)
	}
	out := make([]byte, len(rev))
	for k := 0; k < len(rev); k++ {
		out[k] = rev[len(rev)-1-k]
	}
	return string(out)
}

func digitVal(c byte) int {
	ci := int(c)
	if ci >= 48 && ci <= 57 {
		return ci - 48
	}
	if ci >= 97 && ci <= 122 {
		return ci - 97 + 10
	}
	if ci >= 65 && ci <= 90 {
		return ci - 65 + 10
	}
	return -1
}

func has0xPrefix(s string) bool {
	return len(s) >= 2 && s[0:1] == "0" && (s[1:2] == "x" || s[1:2] == "X")
}

func has0oPrefix(s string) bool {
	return len(s) >= 2 && s[0:1] == "0" && (s[1:2] == "o" || s[1:2] == "O")
}

func has0bPrefix(s string) bool {
	return len(s) >= 2 && s[0:1] == "0" && (s[1:2] == "b" || s[1:2] == "B")
}

func ParseUint(s string, base int, bitSize int) (uint64, error) {
	if len(s) == 0 {
		return 0, errors.New("strconv.ParseUint: invalid syntax")
	}
	rest := s
	b := base
	if b == 0 {
		if has0xPrefix(rest) {
			b = 16
			rest = rest[2:]
		} else if has0oPrefix(rest) {
			b = 8
			rest = rest[2:]
		} else if has0bPrefix(rest) {
			b = 2
			rest = rest[2:]
		} else if len(rest) > 1 && rest[0:1] == "0" {
			b = 8
			rest = rest[1:]
		} else {
			b = 10
		}
	} else if b == 16 && has0xPrefix(rest) {
		rest = rest[2:]
	}
	if len(rest) == 0 {
		return 0, errors.New("strconv.ParseUint: invalid syntax")
	}
	var n uint64
	for k := 0; k < len(rest); k++ {
		if rest[k] == 95 {
			continue
		}
		d := digitVal(rest[k])
		if d < 0 || d >= b {
			return 0, errors.New("strconv.ParseUint: invalid syntax")
		}
		n = n*uint64(b) + uint64(d)
	}
	return n, nil
}

func ParseInt(s string, base int, bitSize int) (int64, error) {
	if len(s) == 0 {
		return 0, errors.New("strconv.ParseInt: invalid syntax")
	}
	neg := false
	rest := s
	if s[0:1] == "-" {
		neg = true
		rest = s[1:]
	} else if s[0:1] == "+" {
		rest = s[1:]
	}
	u, err := ParseUint(rest, base, bitSize)
	if err != nil {
		return 0, errors.New("strconv.ParseInt: invalid syntax")
	}
	n := int64(u)
	if neg {
		n = -n
	}
	return n, nil
}

func Quote(s string) string {
	out := "\""
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c == 34 {
			out = out + "\\\""
		} else if c == 92 {
			out = out + "\\\\"
		} else if c == 10 {
			out = out + "\\n"
		} else if c == 9 {
			out = out + "\\t"
		} else if c == 13 {
			out = out + "\\r"
		} else {
			out = out + string(c)
		}
	}
	return out + "\""
}

func Atoi(s string) (int, error) {
	if len(s) == 0 {
		return 0, errors.New("strconv.Atoi: invalid syntax")
	}
	sign := 1
	i := 0
	if s[0:1] == "-" {
		sign = -1
		i = 1
	}
	if i >= len(s) {
		return 0, errors.New("strconv.Atoi: invalid syntax")
	}
	n := 0
	for ; i < len(s); i++ {
		ci := int(s[i])
		if ci < 48 || ci > 57 {
			return 0, errors.New("strconv.Atoi: invalid syntax")
		}
		n = n*10 + ci - 48
	}
	return n * sign, nil
}

func FormatBool(b bool) string {
	if b {
		return "true"
	}
	return "false"
}

func ParseBool(s string) (bool, error) {
	if s == "1" || s == "t" || s == "T" || s == "true" || s == "True" || s == "TRUE" {
		return true, nil
	}
	if s == "0" || s == "f" || s == "F" || s == "false" || s == "False" || s == "FALSE" {
		return false, nil
	}
	return false, errors.New("strconv.ParseBool: invalid syntax")
}

// pow10 is 10**n by squaring; exact for n <= 22.
func pow10(n int) float64 {
	r := 1.0
	b := 10.0
	for n > 0 {
		if n&1 == 1 {
			r = r * b
		}
		b = b * b
		n = n >> 1
	}
	return r
}

// ParseFloat parses a decimal float: [+-]digits[.digits][(e|E)[+-]digits].
// The digits are read as an integer mantissa and a decimal exponent, then
// scaled once, so a number of up to 19 significant digits and an exponent
// of at most 22 comes out exact.
func ParseFloat(s string) (float64, error) {
	bad := errors.New("strconv.ParseFloat: parsing \"" + s + "\": invalid syntax")
	i := 0
	neg := false
	if i < len(s) && (s[i] == 43 || s[i] == 45) {
		neg = s[i] == 45
		i++
	}
	var mant uint64
	digits := 0
	exp := 0
	seen := false
	dot := false
	for ; i < len(s); i++ {
		c := s[i]
		if c >= 48 && c <= 57 {
			seen = true
			if digits < 19 {
				mant = mant*10 + uint64(c-48)
				if mant != 0 {
					digits++
				}
				if dot {
					exp--
				}
			} else if !dot {
				exp++
			}
			continue
		}
		if c == 46 && !dot {
			dot = true
			continue
		}
		break
	}
	if !seen {
		return 0, bad
	}
	if i < len(s) && (s[i] == 101 || s[i] == 69) {
		i++
		eneg := false
		if i < len(s) && (s[i] == 43 || s[i] == 45) {
			eneg = s[i] == 45
			i++
		}
		e := 0
		eseen := false
		for ; i < len(s) && s[i] >= 48 && s[i] <= 57; i++ {
			if e < 100000 {
				e = e*10 + int(s[i]-48)
			}
			eseen = true
		}
		if !eseen {
			return 0, bad
		}
		if eneg {
			e = -e
		}
		exp += e
	}
	if i != len(s) {
		return 0, bad
	}
	f := float64(mant)
	if exp > 0 {
		for exp > 22 {
			f = f * 1e22
			exp -= 22
		}
		f = f * pow10(exp)
	} else if exp < 0 {
		for exp < -22 {
			f = f / 1e22
			exp += 22
		}
		f = f / pow10(-exp)
	}
	if neg {
		f = -f
	}
	return f, nil
}
