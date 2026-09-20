package wazero

import "math"

func f32frombits(u uint32) float32 {
	if u == 0 || u == 2147483648 {
		return 0
	}
	sign := 1.0
	if u >= 2147483648 {
		sign = -1.0
		u = u - 2147483648
	}
	exp := int(u >> 23)
	frac := u & 8388607
	if exp == 255 {
		if frac == 0 {
			return float32(sign * math.Inf(1))
		}
		return float32(math.NaN())
	}
	f := float64(frac) / 8388608.0
	if exp == 0 {
		e := -126
		for e < 0 {
			f = f / 2.0
			e++
		}
		return float32(sign * f)
	}
	f = f + 1.0
	e := exp - 127
	if e > 0 {
		for i := 0; i < e; i++ {
			f = f * 2.0
		}
	} else if e < 0 {
		for i := 0; i < -e; i++ {
			f = f / 2.0
		}
	}
	return float32(sign * f)
}

func f32bits(f float32) uint32 {
	if f == 0 {
		return 0
	}
	if math.IsNaN(float64(f)) {
		return 2143289344
	}
	sign := uint32(0)
	x := float64(f)
	if x < 0 {
		sign = 2147483648
		x = -x
	}
	if math.IsInf(x, 1) {
		return sign | 2139095040
	}
	exp := 0
	if x >= 1 {
		for x >= 2 {
			x = x / 2
			exp++
		}
	} else {
		for x < 1 && exp > -126 {
			x = x * 2
			exp--
		}
	}
	if exp <= -127 {
		frac := uint32(x * 8388608.0)
		return sign | frac
	}
	e := uint32(exp + 127)
	frac := uint32((x - 1.0) * 8388608.0)
	return sign | (e << 23) | (frac & 8388607)
}

func f64frombits(u uint64) float64 {
	if u == 0 || u == 9223372036854775808 {
		return 0
	}
	sign := 1.0
	if u >= 9223372036854775808 {
		sign = -1.0
		u = u - 9223372036854775808
	}
	exp := int(u >> 52)
	frac := u & 4503599627370495
	if exp == 2047 {
		if frac == 0 {
			return sign * math.Inf(1)
		}
		return math.NaN()
	}
	f := float64(frac) / 4503599627370496.0
	if exp == 0 {
		e := -1022
		for e < 0 {
			f = f / 2.0
			e++
		}
		return sign * f
	}
	f = f + 1.0
	e := exp - 1023
	if e > 0 {
		for i := 0; i < e; i++ {
			f = f * 2.0
		}
	} else if e < 0 {
		for i := 0; i < -e; i++ {
			f = f / 2.0
		}
	}
	return sign * f
}

func f64bits(f float64) uint64 {
	if f == 0 {
		return 0
	}
	if math.IsNaN(f) {
		return 9221120237041090560
	}
	sign := uint64(0)
	x := f
	if x < 0 {
		sign = 9223372036854775808
		x = -x
	}
	if math.IsInf(x, 1) {
		return sign | 9218868437227405312
	}
	exp := 0
	if x >= 1 {
		for x >= 2 {
			x = x / 2
			exp++
		}
	} else {
		for x < 1 && exp > -1022 {
			x = x * 2
			exp--
		}
	}
	if exp <= -1023 {
		frac := uint64(x * 4503599627370496.0)
		return sign | frac
	}
	e := uint64(exp + 1023)
	frac := uint64((x - 1.0) * 4503599627370496.0)
	return sign | (e << 52) | (frac & 4503599627370495)
}
