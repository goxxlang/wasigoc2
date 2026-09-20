package wasmbin

import (
	"fmt"
	"io"
)

func appendU32(dst []byte, v uint32) []byte {
	for {
		b := byte(v & 0x7f)
		v >>= 7
		if v != 0 {
			b |= 0x80
		}
		dst = append(dst, b)
		if v == 0 {
			break
		}
	}
	return dst
}

func appendI32(dst []byte, v int32) []byte {
	for {
		b := byte(v & 0x7f)
		v >>= 7
		sign := (b & 0x40) != 0
		done := (v == 0 && !sign) || (v == -1 && sign)
		if !done {
			b |= 0x80
		}
		dst = append(dst, b)
		if done {
			break
		}
	}
	return dst
}

type reader struct {
	b   []byte
	off int
}

func (r *reader) remaining() int { return len(r.b) - r.off }

func (r *reader) u8() (byte, error) {
	if r.off >= len(r.b) {
		return 0, io.ErrUnexpectedEOF
	}
	v := r.b[r.off]
	r.off++
	return v, nil
}

func (r *reader) raw(n int) ([]byte, error) {
	if n < 0 || r.off+n > len(r.b) {
		return nil, io.ErrUnexpectedEOF
	}
	out := r.b[r.off : r.off+n]
	r.off += n
	return out, nil
}

func (r *reader) u32() (uint32, error) {
	var result uint32
	var shift uint
	for i := 0; i < 5; i++ {
		b, err := r.u8()
		if err != nil {
			return 0, err
		}
		result |= uint32(b&0x7f) << shift
		if b&0x80 == 0 {
			return result, nil
		}
		shift += 7
	}
	return 0, fmt.Errorf("uleb128 overflow")
}

func (r *reader) i32() (int32, error) {
	var result int32
	var shift uint
	var b byte
	var err error
	for {
		b, err = r.u8()
		if err != nil {
			return 0, err
		}
		result |= int32(b&0x7f) << shift
		shift += 7
		if b&0x80 == 0 {
			break
		}
		if shift >= 32 {
			return 0, fmt.Errorf("sleb128 overflow")
		}
	}
	if shift < 32 && b&0x40 != 0 {
		result |= ^int32(0) << shift
	}
	return result, nil
}

func (r *reader) i64() (int64, error) {
	var result int64
	var shift uint
	var b byte
	var err error
	for {
		b, err = r.u8()
		if err != nil {
			return 0, err
		}
		result |= int64(b&0x7f) << shift
		shift += 7
		if b&0x80 == 0 {
			break
		}
		if shift >= 64 {
			return 0, fmt.Errorf("sleb128 overflow")
		}
	}
	if shift < 64 && b&0x40 != 0 {
		result |= ^int64(0) << shift
	}
	return result, nil
}

func (r *reader) name() (string, error) {
	n, err := r.u32()
	if err != nil {
		return "", err
	}
	raw, err := r.raw(int(n))
	if err != nil {
		return "", err
	}
	return string(raw), nil
}
