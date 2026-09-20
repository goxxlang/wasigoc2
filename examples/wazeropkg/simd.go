package wazero

import (
	"errors"
	"math"
)

func (m *Module) doSimd(fn *compiledFn, in Op, stack []uint64) ([]uint64, error) {
	if m == nil || m.rt == nil {
		return nil, errors.New("simd")
	}
	rt := m.rt
	sub := in.A
	if sub == 12 {
		lo := uint64(0)
		hi := uint64(0)
		i := int(in.B)
		if fn != nil && i >= 0 && i+1 < len(fn.simdc) {
			lo = fn.simdc[i]
			hi = fn.simdc[i+1]
		}
		stack = append(stack, rt.newV128(lo, hi))
		return stack, nil
	}
	if sub <= 10 || sub == 92 || sub == 93 {
		return simdLoad(m, in, stack)
	}
	if sub == 11 {
		return simdStore(m, in, stack)
	}
	if sub >= 84 && sub <= 87 {
		return simdLoadLane(m, in, stack)
	}
	if sub >= 88 && sub <= 91 {
		return simdStoreLane(m, in, stack)
	}
	if sub == 13 || sub == 14 {
		return simdShuffle(m, fn, in, stack)
	}
	if sub >= 15 && sub <= 20 {
		if len(stack) < 1 {
			return nil, errors.New("splat underflow")
		}
		v := stack[len(stack)-1]
		stack = stack[0 : len(stack)-1]
		lo, hi := simdSplat(sub, v)
		stack = append(stack, rt.newV128(lo, hi))
		return stack, nil
	}
	if sub >= 21 && sub <= 34 {
		return simdLane(m, in, stack)
	}
	if sub == 77 {
		if len(stack) < 1 {
			return nil, errors.New("v128.not underflow")
		}
		lo, hi, ok := rt.v128Bits(stack[len(stack)-1])
		if !ok {
			return nil, errors.New("v128.not")
		}
		stack[len(stack)-1] = rt.newV128(^lo, ^hi)
		return stack, nil
	}
	if sub == 78 || sub == 79 || sub == 80 || sub == 81 {
		return simdBit(m, sub, stack)
	}
	if sub == 82 {
		return simdBitselect(m, stack)
	}
	if sub == 83 {
		return simdAnyTrue(m, stack)
	}
	if simdIsUnop(sub) {
		return simdUnop(m, sub, stack)
	}
	if simdIsShift(sub) {
		return simdShift(m, sub, stack)
	}
	if simdIsBinop(sub) {
		return simdBin(m, sub, stack)
	}
	d := simdBump(sub)
	if d == 0 && len(stack) >= 1 {
		return stack, nil
	}
	if d == 1 {
		stack = append(stack, rt.newV128(0, 0))
		return stack, nil
	}
	if d < 0 {
		need := -d + 1
		if len(stack) < need {
			return nil, errors.New("simd 0xfd." + itoa(int(sub)))
		}
		keep := stack[len(stack)-need]
		stack = stack[0 : len(stack)-need+1]
		stack[len(stack)-1] = keep
		return stack, nil
	}
	return nil, errors.New("simd 0xfd." + itoa(int(sub)))
}

func simdLoad(m *Module, in Op, stack []uint64) ([]uint64, error) {
	if len(stack) < 1 {
		return nil, errors.New("v128.load underflow")
	}
	addr := int(stack[len(stack)-1]) + int(in.U)
	stack = stack[0 : len(stack)-1]
	sub := in.A
	n := 16
	if sub == 7 {
		n = 1
	} else if sub == 8 {
		n = 2
	} else if sub == 9 {
		n = 4
	} else if sub == 10 {
		n = 8
	} else if sub == 92 {
		n = 4
	} else if sub == 93 {
		n = 8
	}
	buf, ok := m.readBytes(addr, n)
	if !ok {
		return nil, errors.New("v128.load oob")
	}
	lo, hi := bytesToV128(buf)
	stack = append(stack, m.rt.newV128(lo, hi))
	return stack, nil
}

func simdStore(m *Module, in Op, stack []uint64) ([]uint64, error) {
	if len(stack) < 2 {
		return nil, errors.New("v128.store underflow")
	}
	h := stack[len(stack)-1]
	addr := int(stack[len(stack)-2]) + int(in.U)
	stack = stack[0 : len(stack)-2]
	lo, hi, ok := m.rt.v128Bits(h)
	if !ok {
		return nil, errors.New("v128.store")
	}
	b := v128To16(lo, hi)
	if !m.writeBytes(addr, b[:]) {
		return nil, errors.New("v128.store oob")
	}
	return stack, nil
}

func simdLoadLane(m *Module, in Op, stack []uint64) ([]uint64, error) {
	if len(stack) < 2 {
		return nil, errors.New("v128.load_lane underflow")
	}
	h := stack[len(stack)-1]
	addr := int(stack[len(stack)-2]) + int(in.U)
	stack = stack[0 : len(stack)-2]
	lo, hi, ok := m.rt.v128Bits(h)
	if !ok {
		return nil, errors.New("v128.load_lane")
	}
	b := v128To16(lo, hi)
	n := 1
	if in.A == 85 {
		n = 2
	} else if in.A == 86 {
		n = 4
	} else if in.A == 87 {
		n = 8
	}
	got, ok2 := m.readBytes(addr, n)
	if !ok2 {
		return nil, errors.New("v128.load_lane oob")
	}
	lane := int(in.B)
	off := lane * n
	for i := 0; i < n && off+i < 16; i++ {
		b[off+i] = got[i]
	}
	nlo, nhi := bytesToV128(b[:])
	stack = append(stack, m.rt.newV128(nlo, nhi))
	return stack, nil
}

func simdStoreLane(m *Module, in Op, stack []uint64) ([]uint64, error) {
	if len(stack) < 2 {
		return nil, errors.New("v128.store_lane underflow")
	}
	h := stack[len(stack)-1]
	addr := int(stack[len(stack)-2]) + int(in.U)
	stack = stack[0 : len(stack)-2]
	lo, hi, ok := m.rt.v128Bits(h)
	if !ok {
		return nil, errors.New("v128.store_lane")
	}
	b := v128To16(lo, hi)
	n := 1
	if in.A == 89 {
		n = 2
	} else if in.A == 90 {
		n = 4
	} else if in.A == 91 {
		n = 8
	}
	lane := int(in.B)
	off := lane * n
	if off < 0 {
		off = 0
	}
	end := off + n
	if end > 16 {
		end = 16
	}
	if !m.writeBytes(addr, b[off:end]) {
		return nil, errors.New("v128.store_lane oob")
	}
	return stack, nil
}

func simdShuffle(m *Module, fn *compiledFn, in Op, stack []uint64) ([]uint64, error) {
	if len(stack) < 2 {
		return nil, errors.New("shuffle underflow")
	}
	bLo, bHi, okb := m.rt.v128Bits(stack[len(stack)-1])
	aLo, aHi, oka := m.rt.v128Bits(stack[len(stack)-2])
	if !oka || !okb {
		return nil, errors.New("shuffle")
	}
	a := v128To16(aLo, aHi)
	b := v128To16(bLo, bHi)
	var src [32]byte
	for i := 0; i < 16; i++ {
		src[i] = a[i]
		src[i+16] = b[i]
	}
	var out [16]byte
	if in.A == 14 {
		for i := 0; i < 16; i++ {
			idx := int(b[i])
			if idx < 16 {
				out[i] = a[idx]
			}
		}
	} else {
		idxLo := uint64(0)
		idxHi := uint64(0)
		i0 := int(in.B)
		if fn != nil && i0 >= 0 && i0+1 < len(fn.simdc) {
			idxLo = fn.simdc[i0]
			idxHi = fn.simdc[i0+1]
		}
		idx := v128To16(idxLo, idxHi)
		for i := 0; i < 16; i++ {
			k := int(idx[i]) & 31
			out[i] = src[k]
		}
	}
	nlo, nhi := bytesToV128(out[:])
	stack = stack[0 : len(stack)-2]
	stack = append(stack, m.rt.newV128(nlo, nhi))
	return stack, nil
}

func simdLane(m *Module, in Op, stack []uint64) ([]uint64, error) {
	sub := in.A
	lane := int(in.B)
	replace := sub == 23 || sub == 26 || sub == 28 || sub == 30 || sub == 32 || sub == 34
	if replace {
		if len(stack) < 2 {
			return nil, errors.New("replace_lane underflow")
		}
		v := stack[len(stack)-1]
		h := stack[len(stack)-2]
		stack = stack[0 : len(stack)-2]
		lo, hi, ok := m.rt.v128Bits(h)
		if !ok {
			return nil, errors.New("replace_lane")
		}
		nlo, nhi := replaceLane(sub, lo, hi, lane, v)
		stack = append(stack, m.rt.newV128(nlo, nhi))
		return stack, nil
	}
	if len(stack) < 1 {
		return nil, errors.New("extract_lane underflow")
	}
	lo, hi, ok := m.rt.v128Bits(stack[len(stack)-1])
	if !ok {
		return nil, errors.New("extract_lane")
	}
	stack[len(stack)-1] = extractLane(sub, lo, hi, lane)
	return stack, nil
}

func simdBit(m *Module, sub uint32, stack []uint64) ([]uint64, error) {
	if len(stack) < 2 {
		return nil, errors.New("v128.bit underflow")
	}
	bLo, bHi, okb := m.rt.v128Bits(stack[len(stack)-1])
	aLo, aHi, oka := m.rt.v128Bits(stack[len(stack)-2])
	if !oka || !okb {
		return nil, errors.New("v128.bit")
	}
	var lo uint64
	var hi uint64
	if sub == 78 {
		lo = aLo & bLo
		hi = aHi & bHi
	} else if sub == 79 {
		lo = aLo &^ bLo
		hi = aHi &^ bHi
	} else if sub == 80 {
		lo = aLo | bLo
		hi = aHi | bHi
	} else {
		lo = aLo ^ bLo
		hi = aHi ^ bHi
	}
	stack = stack[0 : len(stack)-2]
	stack = append(stack, m.rt.newV128(lo, hi))
	return stack, nil
}

func simdBitselect(m *Module, stack []uint64) ([]uint64, error) {
	if len(stack) < 3 {
		return nil, errors.New("v128.bitselect underflow")
	}
	cLo, cHi, okc := m.rt.v128Bits(stack[len(stack)-1])
	bLo, bHi, okb := m.rt.v128Bits(stack[len(stack)-2])
	aLo, aHi, oka := m.rt.v128Bits(stack[len(stack)-3])
	if !oka || !okb || !okc {
		return nil, errors.New("v128.bitselect")
	}
	lo := (aLo & cLo) | (bLo &^ cLo)
	hi := (aHi & cHi) | (bHi &^ cHi)
	stack = stack[0 : len(stack)-3]
	stack = append(stack, m.rt.newV128(lo, hi))
	return stack, nil
}

func simdAnyTrue(m *Module, stack []uint64) ([]uint64, error) {
	if len(stack) < 1 {
		return nil, errors.New("v128.any_true underflow")
	}
	lo, hi, ok := m.rt.v128Bits(stack[len(stack)-1])
	if !ok {
		return nil, errors.New("v128.any_true")
	}
	v := uint64(0)
	if lo != 0 || hi != 0 {
		v = 1
	}
	stack[len(stack)-1] = v
	return stack, nil
}

func simdUnop(m *Module, sub uint32, stack []uint64) ([]uint64, error) {
	if len(stack) < 1 {
		return nil, errors.New("simd unop underflow")
	}
	lo, hi, ok := m.rt.v128Bits(stack[len(stack)-1])
	if !ok {
		return nil, errors.New("simd unop")
	}
	if sub == 100 || sub == 132 || sub == 164 || sub == 196 {
		stack[len(stack)-1] = bitmask(sub, lo, hi)
		return stack, nil
	}
	if sub == 99 || sub == 131 || sub == 163 || sub == 195 {
		v := uint64(1)
		b := v128To16(lo, hi)
		if sub == 99 {
			for i := 0; i < 16; i++ {
				if b[i] == 0 {
					v = 0
				}
			}
		} else if sub == 131 {
			for i := 0; i < 8; i++ {
				if u16at(lo, hi, i) == 0 {
					v = 0
				}
			}
		} else if sub == 163 {
			if wrap32(lo) == 0 || wrap32(lo>>32) == 0 || wrap32(hi) == 0 || wrap32(hi>>32) == 0 {
				v = 0
			}
		} else {
			if lo == 0 || hi == 0 {
				v = 0
			}
		}
		stack[len(stack)-1] = v
		return stack, nil
	}
	nlo, nhi := simdUnopBits(sub, lo, hi)
	stack[len(stack)-1] = m.rt.newV128(nlo, nhi)
	return stack, nil
}

func simdShift(m *Module, sub uint32, stack []uint64) ([]uint64, error) {
	if len(stack) < 2 {
		return nil, errors.New("simd shift underflow")
	}
	cnt := stack[len(stack)-1]
	lo, hi, ok := m.rt.v128Bits(stack[len(stack)-2])
	if !ok {
		return nil, errors.New("simd shift")
	}
	nlo, nhi := doSimdShift(sub, lo, hi, cnt)
	stack = stack[0 : len(stack)-2]
	stack = append(stack, m.rt.newV128(nlo, nhi))
	return stack, nil
}

func simdBin(m *Module, sub uint32, stack []uint64) ([]uint64, error) {
	if len(stack) < 2 {
		return nil, errors.New("simd binop underflow")
	}
	bLo, bHi, okb := m.rt.v128Bits(stack[len(stack)-1])
	aLo, aHi, oka := m.rt.v128Bits(stack[len(stack)-2])
	if !oka || !okb {
		return nil, errors.New("simd binop")
	}
	lo, hi := simdLaneBinop(sub, aLo, aHi, bLo, bHi)
	stack = stack[0 : len(stack)-2]
	stack = append(stack, m.rt.newV128(lo, hi))
	return stack, nil
}

func simdIsUnop(sub uint32) bool {
	if sub == 96 || sub == 97 || sub == 98 || sub == 99 || sub == 100 ||
		sub == 124 || sub == 125 || sub == 128 || sub == 129 || sub == 131 || sub == 132 ||
		sub == 160 || sub == 161 || sub == 163 || sub == 164 ||
		sub == 192 || sub == 193 || sub == 196 ||
		sub == 224 || sub == 225 || sub == 227 || sub == 236 || sub == 237 || sub == 239 ||
		sub == 248 || sub == 249 || sub == 250 || sub == 251 ||
		sub == 252 || sub == 253 || sub == 254 || sub == 255 ||
		sub == 124 || sub == 125 || sub == 126 || sub == 127 {
		return true
	}
	if sub >= 103 && sub <= 106 {
		return true
	}
	if sub == 195 {
		return true
	}
	if sub >= 135 && sub <= 138 {
		return true
	}
	if sub >= 167 && sub <= 170 {
		return true
	}
	if sub >= 199 && sub <= 202 {
		return true
	}
	return false
}

func simdIsShift(sub uint32) bool {
	return sub == 107 || sub == 108 || sub == 109 || sub == 139 || sub == 140 || sub == 141 ||
		sub == 171 || sub == 172 || sub == 173 || sub == 187 || sub == 188 || sub == 189
}

func simdIsBinop(sub uint32) bool {
	if sub == 101 || sub == 102 || sub == 133 || sub == 134 {
		return true
	}
	if sub >= 35 && sub <= 76 {
		return true
	}
	if sub >= 110 && sub <= 123 {
		return true
	}
	if sub >= 142 && sub <= 155 {
		return true
	}
	if sub >= 174 && sub <= 186 {
		return true
	}
	if sub >= 208 && sub <= 223 {
		return true
	}
	if sub >= 228 && sub <= 235 {
		return true
	}
	if sub >= 240 && sub <= 247 {
		return true
	}
	if sub == 123 || sub == 155 || sub == 130 || sub == 186 || (sub >= 214 && sub <= 219) {
		return true
	}
	if sub >= 156 && sub <= 159 {
		return true
	}
	if sub >= 188 && sub <= 191 {
		return true
	}
	if sub >= 220 && sub <= 223 {
		return true
	}
	return false
}

func simdSplat(sub uint32, v uint64) (uint64, uint64) {
	if sub == 15 {
		b := v & 255
		p := b | (b << 8) | (b << 16) | (b << 24)
		p = p | (p << 32)
		return p, p
	}
	if sub == 16 {
		w := v & 65535
		p := w | (w << 16) | (w << 32) | (w << 48)
		return p, p
	}
	if sub == 17 || sub == 19 {
		w := v & 4294967295
		p := w | (w << 32)
		return p, p
	}
	return v, v
}

func extractLane(sub uint32, lo uint64, hi uint64, lane int) uint64 {
	b := v128To16(lo, hi)
	if sub == 21 || sub == 22 {
		if lane < 0 || lane > 15 {
			lane = 0
		}
		v := uint64(b[lane])
		if sub == 21 && v >= 128 {
			v = v | 18446744073709551360
		}
		return wrap32(v)
	}
	if sub == 24 || sub == 25 {
		w := uint64(u16at(lo, hi, lane))
		if sub == 24 && w >= 32768 {
			w = w | 18446744073709518848
		}
		return wrap32(w)
	}
	if sub == 27 || sub == 31 {
		if lane&1 == 0 {
			return wrap32(lo >> uint((lane%2)*32))
		}
		return wrap32(hi >> uint((lane%2)*32))
	}
	if sub == 29 || sub == 33 {
		if lane == 0 {
			return lo
		}
		return hi
	}
	if lane == 0 {
		return wrap32(lo)
	}
	if lane == 1 {
		return wrap32(lo >> 32)
	}
	if lane == 2 {
		return wrap32(hi)
	}
	return wrap32(hi >> 32)
}

func replaceLane(sub uint32, lo uint64, hi uint64, lane int, v uint64) (uint64, uint64) {
	b := v128To16(lo, hi)
	if sub == 23 {
		if lane >= 0 && lane < 16 {
			b[lane] = byte(v)
		}
		return bytesToV128(b[:])
	}
	if sub == 26 {
		setU16(&lo, &hi, lane, uint16(v))
		return lo, hi
	}
	if sub == 28 || sub == 32 {
		if lane == 0 {
			lo = (lo &^ 4294967295) | wrap32(v)
		} else if lane == 1 {
			lo = wrap32(lo) | (wrap32(v) << 32)
		} else if lane == 2 {
			hi = (hi &^ 4294967295) | wrap32(v)
		} else {
			hi = wrap32(hi) | (wrap32(v) << 32)
		}
		return lo, hi
	}
	if lane == 0 {
		return v, hi
	}
	return lo, v
}

func simdLaneBinop(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	if sub >= 35 && sub <= 44 {
		return cmpI8(sub, aLo, aHi, bLo, bHi)
	}
	if sub >= 45 && sub <= 54 {
		return cmpI16(sub, aLo, aHi, bLo, bHi)
	}
	if sub >= 55 && sub <= 64 {
		return cmpI32(sub, aLo, aHi, bLo, bHi)
	}
	if sub >= 65 && sub <= 70 {
		return cmpF32x4(sub, aLo, aHi, bLo, bHi)
	}
	if sub >= 71 && sub <= 76 {
		return cmpF64x2(sub, aLo, aHi, bLo, bHi)
	}
	if sub == 110 || sub == 113 {
		return addSubI8(sub == 110, aLo, aHi, bLo, bHi)
	}
	if sub >= 111 && sub <= 115 && sub != 113 {
		return addSubSatI8(sub, aLo, aHi, bLo, bHi)
	}
	if sub >= 143 && sub <= 147 && sub != 145 {
		return addSubSatI16(sub, aLo, aHi, bLo, bHi)
	}
	if sub == 142 || sub == 145 || sub == 149 {
		return addSubMulI16(sub, aLo, aHi, bLo, bHi)
	}
	if sub == 174 {
		return addI32x2(aLo, bLo), addI32x2(aHi, bHi)
	}
	if sub == 177 {
		return subI32x2(aLo, bLo), subI32x2(aHi, bHi)
	}
	if sub == 181 {
		return mulI32x2(aLo, bLo), mulI32x2(aHi, bHi)
	}
	if sub >= 118 && sub <= 121 {
		return minMaxI8(sub, aLo, aHi, bLo, bHi)
	}
	if sub >= 150 && sub <= 153 {
		return minMaxI16(sub, aLo, aHi, bLo, bHi)
	}
	if sub >= 182 && sub <= 185 {
		return minMaxI32(sub, aLo, aHi, bLo, bHi)
	}
	if sub == 211 || sub == 221 {
		return aLo + bLo, aHi + bHi
	}
	if sub == 213 {
		return aLo * bLo, aHi * bHi
	}
	if sub == 222 {
		return aLo - bLo, aHi - bHi
	}
	if sub == 101 || sub == 102 {
		return narrowI16ToI8(sub == 101, aLo, aHi, bLo, bHi)
	}
	if sub == 133 || sub == 134 {
		return narrowI32ToI16(sub == 133, aLo, aHi, bLo, bHi)
	}
	if sub == 228 || sub == 229 || sub == 230 || sub == 231 || sub == 232 || sub == 233 || sub == 234 || sub == 235 {
		return f32x4Bin(sub, aLo, aHi, bLo, bHi)
	}
	if sub >= 214 && sub <= 219 {
		return cmpI64(sub, aLo, aHi, bLo, bHi)
	}
	if sub == 186 {
		return dotI16(aLo, aHi, bLo, bHi)
	}
	if (sub >= 156 && sub <= 159) || (sub >= 188 && sub <= 191) || (sub >= 220 && sub <= 223) {
		return extMul(sub, aLo, aHi, bLo, bHi)
	}
	if sub == 130 {
		return q15mulr(aLo, aHi, bLo, bHi)
	}
	if sub == 123 {
		return avgrI8(aLo, aHi, bLo, bHi)
	}
	if sub == 155 {
		return avgrI16(aLo, aHi, bLo, bHi)
	}
	if sub == 240 || sub == 241 || sub == 242 || sub == 243 || sub == 244 || sub == 245 || sub == 246 || sub == 247 {
		return f64x2Bin(sub, aLo, aHi, bLo, bHi)
	}
	return aLo, aHi
}

func cmpI8(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	a := v128To16(aLo, aHi)
	b := v128To16(bLo, bHi)
	var out [16]byte
	for i := 0; i < 16; i++ {
		av := int(a[i])
		bv := int(b[i])
		if sub == 37 || sub == 39 || sub == 41 || sub == 43 {
			if av >= 128 {
				av = av - 256
			}
			if bv >= 128 {
				bv = bv - 256
			}
		}
		ok := false
		if sub == 35 {
			ok = a[i] == b[i]
		} else if sub == 36 {
			ok = a[i] != b[i]
		} else if sub == 37 || sub == 38 {
			ok = av < bv
		} else if sub == 39 || sub == 40 {
			ok = av > bv
		} else if sub == 41 || sub == 42 {
			ok = av <= bv
		} else {
			ok = av >= bv
		}
		if ok {
			out[i] = 255
		}
	}
	return bytesToV128(out[:])
}

func cmpI16(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var out [8]uint16
	for i := 0; i < 8; i++ {
		av := int(u16at(aLo, aHi, i))
		bv := int(u16at(bLo, bHi, i))
		as := av
		bs := bv
		if as >= 32768 {
			as = as - 65536
		}
		if bs >= 32768 {
			bs = bs - 65536
		}
		ok := av == bv
		if sub == 46 {
			ok = av != bv
		} else if sub == 47 {
			ok = as < bs
		} else if sub == 48 {
			ok = av < bv
		} else if sub == 49 {
			ok = as > bs
		} else if sub == 50 {
			ok = av > bv
		} else if sub == 51 {
			ok = as <= bs
		} else if sub == 52 {
			ok = av <= bv
		} else if sub == 53 {
			ok = as >= bs
		} else if sub == 54 {
			ok = av >= bv
		}
		if ok {
			out[i] = 65535
		}
	}
	return packU16(out)
}

func cmpI32(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var vs [4]uint32
	vs[0] = uint32(aLo)
	vs[1] = uint32(aLo >> 32)
	vs[2] = uint32(aHi)
	vs[3] = uint32(aHi >> 32)
	var vt [4]uint32
	vt[0] = uint32(bLo)
	vt[1] = uint32(bLo >> 32)
	vt[2] = uint32(bHi)
	vt[3] = uint32(bHi >> 32)
	var o [4]uint32
	for i := 0; i < 4; i++ {
		ai := int64(vs[i])
		bi := int64(vt[i])
		if ai >= 2147483648 {
			ai = ai - 4294967296
		}
		if bi >= 2147483648 {
			bi = bi - 4294967296
		}
		ok := vs[i] == vt[i]
		if sub == 56 {
			ok = vs[i] != vt[i]
		} else if sub == 57 {
			ok = ai < bi
		} else if sub == 58 {
			ok = uint64(vs[i]) < uint64(vt[i])
		} else if sub == 59 {
			ok = ai > bi
		} else if sub == 60 {
			ok = vs[i] > vt[i]
		} else if sub == 61 {
			ok = ai <= bi
		} else if sub == 62 {
			ok = vs[i] <= vt[i]
		} else if sub == 63 {
			ok = ai >= bi
		} else if sub == 64 {
			ok = vs[i] >= vt[i]
		}
		if ok {
			o[i] = 4294967295
		}
	}
	return uint64(o[0]) | (uint64(o[1]) << 32), uint64(o[2]) | (uint64(o[3]) << 32)
}

func cmpF32x4(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var aa [4]float32
	var bb [4]float32
	aa[0] = DecodeF32(wrap32(aLo))
	aa[1] = DecodeF32(wrap32(aLo >> 32))
	aa[2] = DecodeF32(wrap32(aHi))
	aa[3] = DecodeF32(wrap32(aHi >> 32))
	bb[0] = DecodeF32(wrap32(bLo))
	bb[1] = DecodeF32(wrap32(bLo >> 32))
	bb[2] = DecodeF32(wrap32(bHi))
	bb[3] = DecodeF32(wrap32(bHi >> 32))
	var o [4]uint32
	for i := 0; i < 4; i++ {
		ok := aa[i] == bb[i]
		if sub == 66 {
			ok = aa[i] != bb[i]
		} else if sub == 67 {
			ok = aa[i] < bb[i]
		} else if sub == 68 {
			ok = aa[i] > bb[i]
		} else if sub == 69 {
			ok = aa[i] <= bb[i]
		} else if sub == 70 {
			ok = aa[i] >= bb[i]
		}
		if ok {
			o[i] = 4294967295
		}
	}
	return uint64(o[0]) | (uint64(o[1]) << 32), uint64(o[2]) | (uint64(o[3]) << 32)
}

func cmpF64x2(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	x0 := DecodeF64(aLo)
	x1 := DecodeF64(aHi)
	y0 := DecodeF64(bLo)
	y1 := DecodeF64(bHi)
	var o [2]uint64
	ok0 := x0 == y0
	ok1 := x1 == y1
	if sub == 72 {
		ok0 = x0 != y0
		ok1 = x1 != y1
	} else if sub == 73 {
		ok0 = x0 < y0
		ok1 = x1 < y1
	} else if sub == 74 {
		ok0 = x0 > y0
		ok1 = x1 > y1
	} else if sub == 75 {
		ok0 = x0 <= y0
		ok1 = x1 <= y1
	} else if sub == 76 {
		ok0 = x0 >= y0
		ok1 = x1 >= y1
	}
	if ok0 {
		o[0] = ^uint64(0)
	}
	if ok1 {
		o[1] = ^uint64(0)
	}
	return o[0], o[1]
}

func cmpI64(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var o [2]uint64
	var av [2]int64
	var bv [2]int64
	av[0] = int64(aLo)
	av[1] = int64(aHi)
	bv[0] = int64(bLo)
	bv[1] = int64(bHi)
	for i := 0; i < 2; i++ {
		ok := av[i] == bv[i]
		if sub == 215 {
			ok = av[i] != bv[i]
		} else if sub == 216 {
			ok = av[i] < bv[i]
		} else if sub == 217 {
			ok = av[i] > bv[i]
		} else if sub == 218 {
			ok = av[i] <= bv[i]
		} else if sub == 219 {
			ok = av[i] >= bv[i]
		}
		if ok {
			o[i] = ^uint64(0)
		}
	}
	return o[0], o[1]
}

func dotI16(aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var o [4]int64
	for i := 0; i < 4; i++ {
		a0 := int(u16at(aLo, aHi, i*2))
		a1 := int(u16at(aLo, aHi, i*2+1))
		b0 := int(u16at(bLo, bHi, i*2))
		b1 := int(u16at(bLo, bHi, i*2+1))
		if a0 >= 32768 {
			a0 = a0 - 65536
		}
		if a1 >= 32768 {
			a1 = a1 - 65536
		}
		if b0 >= 32768 {
			b0 = b0 - 65536
		}
		if b1 >= 32768 {
			b1 = b1 - 65536
		}
		o[i] = int64(a0*b0 + a1*b1)
	}
	return EncodeI32(o[0]) | (EncodeI32(o[1]) << 32), EncodeI32(o[2]) | (EncodeI32(o[3]) << 32)
}

func q15mulr(aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var out [8]uint16
	for i := 0; i < 8; i++ {
		a := int(u16at(aLo, aHi, i))
		b := int(u16at(bLo, bHi, i))
		if a >= 32768 {
			a = a - 65536
		}
		if b >= 32768 {
			b = b - 65536
		}
		v := (a*b + 16384) >> 15
		if v < -32768 {
			v = -32768
		}
		if v > 32767 {
			v = 32767
		}
		out[i] = uint16(v)
	}
	return packU16(out)
}

func addSubSatI8(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	a := v128To16(aLo, aHi)
	b := v128To16(bLo, bHi)
	var out [16]byte
	add := sub == 111 || sub == 112
	signed := sub == 111 || sub == 114
	for i := 0; i < 16; i++ {
		if signed {
			av := int(a[i])
			bv := int(b[i])
			if av >= 128 {
				av = av - 256
			}
			if bv >= 128 {
				bv = bv - 256
			}
			var s int
			if add {
				s = av + bv
			} else {
				s = av - bv
			}
			if s < -128 {
				s = -128
			}
			if s > 127 {
				s = 127
			}
			out[i] = byte(s)
		} else {
			av := int(a[i])
			bv := int(b[i])
			var s int
			if add {
				s = av + bv
			} else {
				s = av - bv
			}
			if s < 0 {
				s = 0
			}
			if s > 255 {
				s = 255
			}
			out[i] = byte(s)
		}
	}
	return bytesToV128(out[:])
}

func addSubSatI16(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	add := sub == 143 || sub == 144
	signed := sub == 143 || sub == 146
	var out [8]uint16
	for i := 0; i < 8; i++ {
		av := int(u16at(aLo, aHi, i))
		bv := int(u16at(bLo, bHi, i))
		if signed {
			if av >= 32768 {
				av = av - 65536
			}
			if bv >= 32768 {
				bv = bv - 65536
			}
		}
		var s int
		if add {
			s = av + bv
		} else {
			s = av - bv
		}
		if signed {
			if s < -32768 {
				s = -32768
			}
			if s > 32767 {
				s = 32767
			}
		} else {
			if s < 0 {
				s = 0
			}
			if s > 65535 {
				s = 65535
			}
		}
		out[i] = uint16(s)
	}
	return packU16(out)
}

func addSubI8(add bool, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	a := v128To16(aLo, aHi)
	b := v128To16(bLo, bHi)
	var out [16]byte
	for i := 0; i < 16; i++ {
		if add {
			out[i] = a[i] + b[i]
		} else {
			out[i] = a[i] - b[i]
		}
	}
	return bytesToV128(out[:])
}

func addSubMulI16(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var out [8]uint16
	for i := 0; i < 8; i++ {
		av := u16at(aLo, aHi, i)
		bv := u16at(bLo, bHi, i)
		if sub == 142 {
			out[i] = av + bv
		} else if sub == 145 {
			out[i] = av - bv
		} else {
			out[i] = av * bv
		}
	}
	return packU16(out)
}

func f32x4Bin(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var o [4]uint64
	var aa [4]uint64
	var bb [4]uint64
	aa[0] = wrap32(aLo)
	aa[1] = wrap32(aLo >> 32)
	aa[2] = wrap32(aHi)
	aa[3] = wrap32(aHi >> 32)
	bb[0] = wrap32(bLo)
	bb[1] = wrap32(bLo >> 32)
	bb[2] = wrap32(bHi)
	bb[3] = wrap32(bHi >> 32)
	for i := 0; i < 4; i++ {
		x := DecodeF32(aa[i])
		y := DecodeF32(bb[i])
		var z float32
		if sub == 228 {
			z = x + y
		} else if sub == 229 {
			z = x - y
		} else if sub == 230 {
			z = x * y
		} else if sub == 231 {
			z = x / y
		} else if sub == 232 || sub == 234 {
			if y < x {
				z = y
			} else {
				z = x
			}
		} else {
			if y > x {
				z = y
			} else {
				z = x
			}
		}
		o[i] = EncodeF32(z)
	}
	return o[0] | (o[1] << 32), o[2] | (o[3] << 32)
}

func f64x2Bin(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	x0 := DecodeF64(aLo)
	x1 := DecodeF64(aHi)
	y0 := DecodeF64(bLo)
	y1 := DecodeF64(bHi)
	if sub == 240 {
		return EncodeF64(x0 + y0), EncodeF64(x1 + y1)
	}
	if sub == 241 {
		return EncodeF64(x0 - y0), EncodeF64(x1 - y1)
	}
	if sub == 242 {
		return EncodeF64(x0 * y0), EncodeF64(x1 * y1)
	}
	if sub == 243 {
		return EncodeF64(x0 / y0), EncodeF64(x1 / y1)
	}
	if sub == 244 || sub == 246 {
		z0 := x0
		z1 := x1
		if y0 < x0 {
			z0 = y0
		}
		if y1 < x1 {
			z1 = y1
		}
		return EncodeF64(z0), EncodeF64(z1)
	}
	z0 := x0
	z1 := x1
	if y0 > x0 {
		z0 = y0
	}
	if y1 > x1 {
		z1 = y1
	}
	return EncodeF64(z0), EncodeF64(z1)
}

func avgrI8(aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	a := v128To16(aLo, aHi)
	b := v128To16(bLo, bHi)
	var out [16]byte
	for i := 0; i < 16; i++ {
		out[i] = byte((int(a[i]) + int(b[i]) + 1) / 2)
	}
	return bytesToV128(out[:])
}

func avgrI16(aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var out [8]uint16
	for i := 0; i < 8; i++ {
		out[i] = uint16((int(u16at(aLo, aHi, i)) + int(u16at(bLo, bHi, i)) + 1) / 2)
	}
	return packU16(out)
}

func popcntI8(lo uint64, hi uint64) (uint64, uint64) {
	b := v128To16(lo, hi)
	var out [16]byte
	for i := 0; i < 16; i++ {
		x := b[i]
		n := byte(0)
		for x != 0 {
			n = n + (x & 1)
			x = x >> 1
		}
		out[i] = n
	}
	return bytesToV128(out[:])
}

func simdUnopBits(sub uint32, lo uint64, hi uint64) (uint64, uint64) {
	if sub == 96 || sub == 97 {
		return absNegI8(sub == 96, lo, hi)
	}
	if sub == 128 || sub == 129 {
		return absNegI16(sub == 128, lo, hi)
	}
	if sub == 160 || sub == 161 {
		return absNegI32(sub == 160, lo, hi)
	}
	if sub == 192 {
		if int64(lo) < 0 {
			lo = uint64(-int64(lo))
		}
		if int64(hi) < 0 {
			hi = uint64(-int64(hi))
		}
		return lo, hi
	}
	if sub == 193 {
		return uint64(-int64(lo)), uint64(-int64(hi))
	}
	if sub == 227 {
		a0 := DecodeF32(wrap32(lo))
		a1 := DecodeF32(wrap32(lo >> 32))
		a2 := DecodeF32(wrap32(hi))
		a3 := DecodeF32(wrap32(hi >> 32))
		return EncodeF32(f32sqrt(a0)) | (EncodeF32(f32sqrt(a1)) << 32), EncodeF32(f32sqrt(a2)) | (EncodeF32(f32sqrt(a3)) << 32)
	}
	if sub == 224 || sub == 225 {
		return absNegF32x4(sub == 224, lo, hi)
	}
	if sub == 236 || sub == 237 {
		return absNegF64x2(sub == 236, lo, hi)
	}
	if sub == 195 {
		v := uint64(1)
		if lo == 0 || hi == 0 {
			v = 0
		}
		return v, 0
	}
	if sub == 239 {
		return EncodeF64(math.Sqrt(DecodeF64(lo))), EncodeF64(math.Sqrt(DecodeF64(hi)))
	}
	if sub == 98 {
		return popcntI8(lo, hi)
	}
	if sub >= 103 && sub <= 106 {
		return extendI8(sub-103+135, lo, hi)
	}
	if sub >= 135 && sub <= 138 {
		return extendI8(sub, lo, hi)
	}
	if sub >= 167 && sub <= 170 {
		return extendI16(sub, lo, hi)
	}
	if sub >= 199 && sub <= 202 {
		return extendI32(sub, lo, hi)
	}
	if sub == 248 || sub == 249 {
		return truncSatF32x4(sub == 248, lo, hi)
	}
	if sub == 250 || sub == 251 {
		return convertI32x4(sub == 250, lo, hi)
	}
	if sub == 252 || sub == 253 {
		return truncSatF64x2Zero(sub == 252, lo, hi)
	}
	if sub == 254 || sub == 255 {
		return convertLowI32x4(sub == 254, lo, hi)
	}
	if sub >= 124 && sub <= 127 {
		return extAddPairwise(sub, lo, hi)
	}
	return lo, hi
}

func extMul(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	high := sub == 157 || sub == 159 || sub == 189 || sub == 191 || sub == 221 || sub == 223
	signed := sub == 156 || sub == 157 || sub == 188 || sub == 189 || sub == 220 || sub == 221
	if sub >= 156 && sub <= 159 {
		b := v128To16(aLo, aHi)
		c := v128To16(bLo, bHi)
		base := 0
		if high {
			base = 8
		}
		var out [8]uint16
		for i := 0; i < 8; i++ {
			av := int(b[base+i])
			bv := int(c[base+i])
			if signed {
				if av >= 128 {
					av = av - 256
				}
				if bv >= 128 {
					bv = bv - 256
				}
			}
			out[i] = uint16(av * bv)
		}
		return packU16(out)
	}
	if sub >= 188 && sub <= 191 {
		base := 0
		if high {
			base = 4
		}
		var o [4]uint32
		for i := 0; i < 4; i++ {
			av := int(u16at(aLo, aHi, base+i))
			bv := int(u16at(bLo, bHi, base+i))
			if signed {
				if av >= 32768 {
					av = av - 65536
				}
				if bv >= 32768 {
					bv = bv - 65536
				}
			}
			o[i] = uint32(av * bv)
		}
		return uint64(o[0]) | (uint64(o[1]) << 32), uint64(o[2]) | (uint64(o[3]) << 32)
	}
	base := 0
	if high {
		base = 2
	}
	var p [2]uint64
	var aa [4]uint32
	var bb [4]uint32
	aa[0] = uint32(aLo)
	aa[1] = uint32(aLo >> 32)
	aa[2] = uint32(aHi)
	aa[3] = uint32(aHi >> 32)
	bb[0] = uint32(bLo)
	bb[1] = uint32(bLo >> 32)
	bb[2] = uint32(bHi)
	bb[3] = uint32(bHi >> 32)
	for i := 0; i < 2; i++ {
		av := int64(aa[base+i])
		bv := int64(bb[base+i])
		if signed {
			if av >= 2147483648 {
				av = av - 4294967296
			}
			if bv >= 2147483648 {
				bv = bv - 4294967296
			}
		}
		p[i] = uint64(av * bv)
	}
	return p[0], p[1]
}

func extAddPairwise(sub uint32, lo uint64, hi uint64) (uint64, uint64) {
	if sub == 124 || sub == 125 {
		b := v128To16(lo, hi)
		var out [8]uint16
		for i := 0; i < 8; i++ {
			a0 := int(b[i*2])
			a1 := int(b[i*2+1])
			if sub == 124 {
				if a0 >= 128 {
					a0 = a0 - 256
				}
				if a1 >= 128 {
					a1 = a1 - 256
				}
			}
			out[i] = uint16(a0 + a1)
		}
		return packU16(out)
	}
	var o [4]uint32
	for i := 0; i < 4; i++ {
		a0 := int(u16at(lo, hi, i*2))
		a1 := int(u16at(lo, hi, i*2+1))
		if sub == 126 {
			if a0 >= 32768 {
				a0 = a0 - 65536
			}
			if a1 >= 32768 {
				a1 = a1 - 65536
			}
		}
		o[i] = uint32(a0 + a1)
	}
	return uint64(o[0]) | (uint64(o[1]) << 32), uint64(o[2]) | (uint64(o[3]) << 32)
}

func truncSatF64x2Zero(signed bool, lo uint64, hi uint64) (uint64, uint64) {
	v0, _ := truncI32(DecodeF64(lo), signed)
	v1, _ := truncI32(DecodeF64(hi), signed)
	return wrap32(v0) | (wrap32(v1) << 32), 0
}

func convertLowI32x4(signed bool, lo uint64, hi uint64) (uint64, uint64) {
	a0 := wrap32(lo)
	a1 := wrap32(lo >> 32)
	if signed {
		return EncodeF64(float64(asI32(a0))), EncodeF64(float64(asI32(a1)))
	}
	return EncodeF64(float64(uint32(a0))), EncodeF64(float64(uint32(a1)))
}

func absNegI8(abs bool, lo uint64, hi uint64) (uint64, uint64) {
	b := v128To16(lo, hi)
	var out [16]byte
	for i := 0; i < 16; i++ {
		v := int(b[i])
		if v >= 128 {
			v = v - 256
		}
		if abs {
			if v < 0 {
				v = -v
			}
		} else {
			v = -v
		}
		out[i] = byte(v)
	}
	return bytesToV128(out[:])
}

func absNegI16(abs bool, lo uint64, hi uint64) (uint64, uint64) {
	var out [8]uint16
	for i := 0; i < 8; i++ {
		v := int(u16at(lo, hi, i))
		if v >= 32768 {
			v = v - 65536
		}
		if abs {
			if v < 0 {
				v = -v
			}
		} else {
			v = -v
		}
		out[i] = uint16(v)
	}
	return packU16(out)
}

func absNegI32(abs bool, lo uint64, hi uint64) (uint64, uint64) {
	var a [4]int64
	a[0] = asI32(lo)
	a[1] = asI32(lo >> 32)
	a[2] = asI32(hi)
	a[3] = asI32(hi >> 32)
	for i := 0; i < 4; i++ {
		if abs {
			if a[i] < 0 {
				a[i] = -a[i]
			}
		} else {
			a[i] = -a[i]
		}
	}
	return EncodeI32(a[0]) | (EncodeI32(a[1]) << 32), EncodeI32(a[2]) | (EncodeI32(a[3]) << 32)
}

func minMaxI8(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	a := v128To16(aLo, aHi)
	b := v128To16(bLo, bHi)
	var out [16]byte
	for i := 0; i < 16; i++ {
		av := int(a[i])
		bv := int(b[i])
		if sub == 118 || sub == 120 {
			if av >= 128 {
				av = av - 256
			}
			if bv >= 128 {
				bv = bv - 256
			}
		}
		v := av
		if sub == 118 || sub == 119 {
			if bv < av {
				v = bv
			}
		} else {
			if bv > av {
				v = bv
			}
		}
		out[i] = byte(v)
	}
	return bytesToV128(out[:])
}

func minMaxI16(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var out [8]uint16
	for i := 0; i < 8; i++ {
		av := int(u16at(aLo, aHi, i))
		bv := int(u16at(bLo, bHi, i))
		if sub == 150 || sub == 152 {
			if av >= 32768 {
				av = av - 65536
			}
			if bv >= 32768 {
				bv = bv - 65536
			}
		}
		v := av
		if sub == 150 || sub == 151 {
			if bv < av {
				v = bv
			}
		} else {
			if bv > av {
				v = bv
			}
		}
		out[i] = uint16(v)
	}
	return packU16(out)
}

func minMaxI32(sub uint32, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var a [4]int64
	var b [4]int64
	a[0] = asI32(aLo)
	a[1] = asI32(aLo >> 32)
	a[2] = asI32(aHi)
	a[3] = asI32(aHi >> 32)
	b[0] = asI32(bLo)
	b[1] = asI32(bLo >> 32)
	b[2] = asI32(bHi)
	b[3] = asI32(bHi >> 32)
	if sub == 183 || sub == 185 {
		a[0] = int64(uint32(aLo))
		a[1] = int64(uint32(aLo >> 32))
		a[2] = int64(uint32(aHi))
		a[3] = int64(uint32(aHi >> 32))
		b[0] = int64(uint32(bLo))
		b[1] = int64(uint32(bLo >> 32))
		b[2] = int64(uint32(bHi))
		b[3] = int64(uint32(bHi >> 32))
	}
	var o [4]int64
	for i := 0; i < 4; i++ {
		o[i] = a[i]
		if sub == 182 || sub == 183 {
			if b[i] < a[i] {
				o[i] = b[i]
			}
		} else {
			if b[i] > a[i] {
				o[i] = b[i]
			}
		}
	}
	return EncodeI32(o[0]) | (EncodeI32(o[1]) << 32), EncodeI32(o[2]) | (EncodeI32(o[3]) << 32)
}

func truncSatF32x4(signed bool, lo uint64, hi uint64) (uint64, uint64) {
	var o [4]uint64
	var src [4]uint64
	src[0] = wrap32(lo)
	src[1] = wrap32(lo >> 32)
	src[2] = wrap32(hi)
	src[3] = wrap32(hi >> 32)
	for i := 0; i < 4; i++ {
		v, _ := truncSat(0, src[i])
		if !signed {
			v, _ = truncSat(1, src[i])
		}
		o[i] = wrap32(v)
	}
	return o[0] | (o[1] << 32), o[2] | (o[3] << 32)
}

func convertI32x4(signed bool, lo uint64, hi uint64) (uint64, uint64) {
	var o [4]uint64
	var src [4]uint64
	src[0] = wrap32(lo)
	src[1] = wrap32(lo >> 32)
	src[2] = wrap32(hi)
	src[3] = wrap32(hi >> 32)
	for i := 0; i < 4; i++ {
		if signed {
			o[i] = EncodeF32(float32(asI32(src[i])))
		} else {
			o[i] = EncodeF32(float32(uint32(src[i])))
		}
	}
	return o[0] | (o[1] << 32), o[2] | (o[3] << 32)
}

func f32sqrt(x float32) float32 {
	return float32(math.Sqrt(float64(x)))
}

func absNegF32x4(abs bool, lo uint64, hi uint64) (uint64, uint64) {
	var a [4]float32
	a[0] = DecodeF32(wrap32(lo))
	a[1] = DecodeF32(wrap32(lo >> 32))
	a[2] = DecodeF32(wrap32(hi))
	a[3] = DecodeF32(wrap32(hi >> 32))
	for i := 0; i < 4; i++ {
		if abs {
			if a[i] < 0 {
				a[i] = -a[i]
			}
		} else {
			a[i] = -a[i]
		}
	}
	return EncodeF32(a[0]) | (EncodeF32(a[1]) << 32), EncodeF32(a[2]) | (EncodeF32(a[3]) << 32)
}

func absNegF64x2(abs bool, lo uint64, hi uint64) (uint64, uint64) {
	x := DecodeF64(lo)
	y := DecodeF64(hi)
	if abs {
		if x < 0 {
			x = -x
		}
		if y < 0 {
			y = -y
		}
	} else {
		x = -x
		y = -y
	}
	return EncodeF64(x), EncodeF64(y)
}

func extendI8(sub uint32, lo uint64, hi uint64) (uint64, uint64) {
	b := v128To16(lo, hi)
	high := sub == 136 || sub == 138
	signed := sub == 135 || sub == 136
	base := 0
	if high {
		base = 8
	}
	var out [8]uint16
	for i := 0; i < 8; i++ {
		v := int(b[base+i])
		if signed && v >= 128 {
			v = v - 256
		}
		out[i] = uint16(v)
	}
	return packU16(out)
}

func extendI32(sub uint32, lo uint64, hi uint64) (uint64, uint64) {
	high := sub == 200 || sub == 202
	signed := sub == 199 || sub == 200
	base := 0
	if high {
		base = 2
	}
	var a [4]uint32
	a[0] = uint32(lo)
	a[1] = uint32(lo >> 32)
	a[2] = uint32(hi)
	a[3] = uint32(hi >> 32)
	var p [2]uint64
	for i := 0; i < 2; i++ {
		v := int64(a[base+i])
		if signed && v >= 2147483648 {
			v = v - 4294967296
		}
		p[i] = uint64(v)
	}
	return p[0], p[1]
}

func extendI16(sub uint32, lo uint64, hi uint64) (uint64, uint64) {
	high := sub == 168 || sub == 170
	signed := sub == 167 || sub == 168
	base := 0
	if high {
		base = 4
	}
	var o [4]uint32
	for i := 0; i < 4; i++ {
		v := int(u16at(lo, hi, base+i))
		if signed && v >= 32768 {
			v = v - 65536
		}
		o[i] = uint32(v)
	}
	return uint64(o[0]) | (uint64(o[1]) << 32), uint64(o[2]) | (uint64(o[3]) << 32)
}

func narrowI16ToI8(signed bool, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var out [16]byte
	for i := 0; i < 8; i++ {
		out[i] = satI8(int(u16at(aLo, aHi, i)), signed)
		out[i+8] = satI8(int(u16at(bLo, bHi, i)), signed)
	}
	return bytesToV128(out[:])
}

func narrowI32ToI16(signed bool, aLo uint64, aHi uint64, bLo uint64, bHi uint64) (uint64, uint64) {
	var out [8]uint16
	var src [8]int64
	src[0] = asI32(aLo)
	src[1] = asI32(aLo >> 32)
	src[2] = asI32(aHi)
	src[3] = asI32(aHi >> 32)
	src[4] = asI32(bLo)
	src[5] = asI32(bLo >> 32)
	src[6] = asI32(bHi)
	src[7] = asI32(bHi >> 32)
	if !signed {
		src[0] = int64(uint32(aLo))
		src[1] = int64(uint32(aLo >> 32))
		src[2] = int64(uint32(aHi))
		src[3] = int64(uint32(aHi >> 32))
		src[4] = int64(uint32(bLo))
		src[5] = int64(uint32(bLo >> 32))
		src[6] = int64(uint32(bHi))
		src[7] = int64(uint32(bHi >> 32))
	}
	for i := 0; i < 8; i++ {
		v := src[i]
		if signed {
			if v < -32768 {
				v = -32768
			}
			if v > 32767 {
				v = 32767
			}
		} else {
			if v < 0 {
				v = 0
			}
			if v > 65535 {
				v = 65535
			}
		}
		out[i] = uint16(v)
	}
	return packU16(out)
}

func satI8(v int, signed bool) byte {
	if signed {
		if v >= 32768 {
			v = v - 65536
		}
		if v < -128 {
			v = -128
		}
		if v > 127 {
			v = 127
		}
		return byte(v)
	}
	if v < 0 {
		v = 0
	}
	if v > 255 {
		v = 255
	}
	return byte(v)
}

func bitmask(sub uint32, lo uint64, hi uint64) uint64 {
	b := v128To16(lo, hi)
	m := uint64(0)
	if sub == 100 {
		for i := 0; i < 16; i++ {
			if b[i]&128 != 0 {
				m = m | (1 << uint(i))
			}
		}
		return m
	}
	if sub == 132 {
		for i := 0; i < 8; i++ {
			w := u16at(lo, hi, i)
			if w&32768 != 0 {
				m = m | (1 << uint(i))
			}
		}
		return m
	}
	if wrap32(lo)&2147483648 != 0 {
		m = m | 1
	}
	if wrap32(lo>>32)&2147483648 != 0 {
		m = m | 2
	}
	if wrap32(hi)&2147483648 != 0 {
		m = m | 4
	}
	if wrap32(hi>>32)&2147483648 != 0 {
		m = m | 8
	}
	return m
}

func doSimdShift(sub uint32, lo uint64, hi uint64, cnt uint64) (uint64, uint64) {
	if sub == 171 {
		k := cnt & 31
		return wrap32(lo)<<k | (wrap32(lo>>32)<<k)<<32, wrap32(hi)<<k | (wrap32(hi>>32)<<k)<<32
	}
	if sub == 172 || sub == 173 {
		k := cnt & 31
		if sub == 173 {
			return wrap32(lo)>>k | (wrap32(lo>>32)>>k)<<32, wrap32(hi)>>k | (wrap32(hi>>32)>>k)<<32
		}
		return EncodeI32(asI32(lo)>>k) | (EncodeI32(asI32(lo>>32)>>k) << 32), EncodeI32(asI32(hi)>>k) | (EncodeI32(asI32(hi>>32)>>k) << 32)
	}
	if sub == 187 {
		k := cnt & 63
		return lo << k, hi << k
	}
	if sub == 188 {
		k := cnt & 63
		return uint64(int64(lo) >> k), uint64(int64(hi) >> k)
	}
	if sub == 189 {
		k := cnt & 63
		return lo >> k, hi >> k
	}
	k := cnt & 7
	a := v128To16(lo, hi)
	var out [16]byte
	for i := 0; i < 16; i++ {
		if sub == 107 {
			out[i] = a[i] << k
		} else {
			out[i] = a[i] >> k
		}
	}
	return bytesToV128(out[:])
}

func addI32x2(a uint64, b uint64) uint64 {
	l := wrap32(a) + wrap32(b)
	h := wrap32(a>>32) + wrap32(b>>32)
	return wrap32(l) | (wrap32(h) << 32)
}

func subI32x2(a uint64, b uint64) uint64 {
	l := wrap32(a) - wrap32(b)
	h := wrap32(a>>32) - wrap32(b>>32)
	return wrap32(l) | (wrap32(h) << 32)
}

func mulI32x2(a uint64, b uint64) uint64 {
	l := wrap32(a) * wrap32(b)
	h := wrap32(a>>32) * wrap32(b>>32)
	return wrap32(l) | (wrap32(h) << 32)
}

func v128To16(lo uint64, hi uint64) [16]byte {
	var b [16]byte
	for i := 0; i < 8; i++ {
		b[i] = byte(lo >> uint(i*8))
		b[i+8] = byte(hi >> uint(i*8))
	}
	return b
}

func bytesToV128(b []byte) (uint64, uint64) {
	lo := uint64(0)
	hi := uint64(0)
	n := len(b)
	if n > 16 {
		n = 16
	}
	for i := 0; i < n && i < 8; i++ {
		lo = lo | (uint64(b[i]) << uint(i*8))
	}
	for i := 8; i < n; i++ {
		hi = hi | (uint64(b[i]) << uint((i-8)*8))
	}
	return lo, hi
}

func u16at(lo uint64, hi uint64, i int) uint16 {
	if i < 0 {
		i = 0
	}
	if i < 4 {
		return uint16(lo >> uint(i*16))
	}
	return uint16(hi >> uint((i-4)*16))
}

func setU16(lo *uint64, hi *uint64, i int, v uint16) {
	if i < 4 {
		sh := uint(i * 16)
		mask := uint64(65535) << sh
		*lo = (*lo &^ mask) | (uint64(v) << sh)
		return
	}
	sh := uint((i - 4) * 16)
	mask := uint64(65535) << sh
	*hi = (*hi &^ mask) | (uint64(v) << sh)
}

func packU16(w [8]uint16) (uint64, uint64) {
	lo := uint64(w[0]) | (uint64(w[1]) << 16) | (uint64(w[2]) << 32) | (uint64(w[3]) << 48)
	hi := uint64(w[4]) | (uint64(w[5]) << 16) | (uint64(w[6]) << 32) | (uint64(w[7]) << 48)
	return lo, hi
}
