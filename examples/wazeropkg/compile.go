package wazero

import (
	"errors"

	"../wasmbinpkg"
)

const (
	opUnreachable uint16 = 1
	opBr          uint16 = 2
	opBrIfz       uint16 = 3
	opCall        uint16 = 4
	opCallIndirect uint16 = 5
	opDrop        uint16 = 6
	opSelect      uint16 = 7
	opLocalGet    uint16 = 8
	opLocalSet    uint16 = 9
	opLocalTee    uint16 = 10
	opGlobalGet   uint16 = 11
	opGlobalSet   uint16 = 12
	opLoad        uint16 = 13
	opStore       uint16 = 14
	opMemSize     uint16 = 15
	opMemGrow     uint16 = 16
	opConst       uint16 = 17
	opUnop        uint16 = 18
	opBinop       uint16 = 19
	opReturn      uint16 = 20
	opMemCopy     uint16 = 21
	opMemFill     uint16 = 22
	opBrTable     uint16 = 23
	opBrIf        uint16 = 24
	opThrow       uint16 = 25
	opThrowRef    uint16 = 26
	opTableGet    uint16 = 27
	opTableSet    uint16 = 28
	opRefNull     uint16 = 29
	opRefIsNull   uint16 = 30
	opRefFunc     uint16 = 31
	opLabelPush   uint16 = 32
	opLabelPop    uint16 = 33
	opMemInit     uint16 = 34
	opDataDrop    uint16 = 35
	opTableInit   uint16 = 36
	opElemDrop    uint16 = 37
	opTableCopy   uint16 = 38
	opTableGrow   uint16 = 39
	opTableSize   uint16 = 40
	opTableFill   uint16 = 41
	opSimd        uint16 = 42
	opAtomic      uint16 = 43
	opReturnCall  uint16 = 44
	opReturnCallI uint16 = 45
	opRefEq       uint16 = 46
	opRefAsNN     uint16 = 47
	opBrOnNull    uint16 = 48
	opBrOnNonNull uint16 = 49
)

type Catch struct {
	Start  uint32
	End    uint32
	Kind   byte
	Tag    uint32
	Target uint32
	U      uint64
}

type Op struct {
	Kind uint16
	A    uint32
	B    uint32
	U    uint64
}

type cframe struct {
	kind    int
	start   int
	height  int
	keep    int
	patches []int
	brtSlots []int
	catchSlots []int
	brIfz   int
	hasElse bool
}

type crd struct {
	b   []byte
	pos int
}

func (r *crd) remain() int {
	if r.pos >= len(r.b) {
		return 0
	}
	return len(r.b) - r.pos
}

func (r *crd) u8() (byte, bool) {
	if r.pos >= len(r.b) {
		return 0, false
	}
	v := r.b[r.pos]
	r.pos = r.pos + 1
	return v, true
}

func (r *crd) u32() (uint32, bool) {
	var result uint32
	var shift uint
	for i := 0; i < 5; i++ {
		b, ok := r.u8()
		if !ok {
			return 0, false
		}
		result |= uint32(b&0x7f) << shift
		if b&0x80 == 0 {
			return result, true
		}
		shift += 7
	}
	return 0, false
}

func (r *crd) s32() (int64, bool) {
	var result int64
	var shift uint
	var b byte
	ok := false
	for {
		b, ok = r.u8()
		if !ok {
			return 0, false
		}
		result |= int64(b&0x7f) << shift
		shift += 7
		if b&0x80 == 0 {
			break
		}
		if shift >= 32 {
			return 0, false
		}
	}
	if shift < 32 && b&0x40 != 0 {
		result |= ^int64(0) << shift
	}
	return result, true
}

func (r *crd) s64() (int64, bool) {
	var result int64
	var shift uint
	var b byte
	ok := false
	for {
		b, ok = r.u8()
		if !ok {
			return 0, false
		}
		result |= int64(b&0x7f) << shift
		shift += 7
		if b&0x80 == 0 {
			break
		}
		if shift >= 64 {
			return 0, false
		}
	}
	if shift < 64 && b&0x40 != 0 {
		result |= ^int64(0) << shift
	}
	return result, true
}

func (r *crd) raw(n int) ([]byte, bool) {
	if n < 0 || r.pos+n > len(r.b) {
		return nil, false
	}
	out := r.b[r.pos : r.pos+n]
	r.pos += n
	return out, true
}

func packHB(height int, keep int) uint64 {
	if height < 0 {
		height = 0
	}
	if keep < 0 {
		keep = 0
	}
	return (uint64(height) << 32) | uint64(keep)
}

func unpackH(u uint64) int {
	return int(u >> 32)
}

func unpackK(u uint64) int {
	return int(u & 4294967295)
}

type compiler struct {
	img      *wasmbin.Image
	r        crd
	ops      []Op
	frames   []cframe
	height   int
	nresults int
	catches  []Catch
	brt      []uint32
	brtU     []uint64
	simdc    []uint64
}

func compileBody(img *wasmbin.Image, code []byte, nparams int, nresults int) ([]Op, []Catch, []uint32, []uint64, []uint64, error) {
	var c compiler
	c.img = img
	c.r = crd{b: code, pos: 0}
	c.nresults = nresults
	c.height = nparams
	var f0 cframe
	f0.kind = 0
	f0.start = 0
	f0.height = 0
	f0.keep = nresults
	f0.brIfz = -1
	c.frames = append(c.frames, f0)
	ops, err := c.run()
	return ops, c.catches, c.brt, c.brtU, c.simdc, err
}

func (c *compiler) emit(kind uint16, a uint32, b uint32, u uint64) {
	var o Op
	o.Kind = kind
	o.A = a
	o.B = b
	o.U = u
	c.ops = append(c.ops, o)
}

func (c *compiler) emitBr(depth int) error {
	fi := len(c.frames) - 1 - depth
	if fi < 0 || fi >= len(c.frames) {
		return errors.New("bad br")
	}
	if c.frames[fi].kind == 2 {
		c.emit(opBr, uint32(c.frames[fi].start), uint32(depth), 0)
		return nil
	}
	c.emit(opBr, 0, uint32(depth), 0)
	c.frames[fi].patches = append(c.frames[fi].patches, len(c.ops)-1)
	return nil
}

type blockArity struct {
	np  int
	nr  int
	err error
}

func (c *compiler) readBlockType() blockArity {
	var a blockArity
	v, ok := c.r.s32()
	if !ok {
		a.err = errors.New("blocktype")
		return a
	}
	if v == -64 {
		return a
	}
	if v < 0 {
		a.nr = 1
		return a
	}
	if int(v) >= len(c.img.Types) {
		a.err = errors.New("block type idx")
		return a
	}
	ft := c.img.Types[v]
	a.np = len(ft.Params)
	a.nr = len(ft.Results)
	return a
}

func (c *compiler) pushFrame(kind int, np int, nr int, loop bool) {
	var f cframe
	f.kind = kind
	f.height = c.height - np
	if loop {
		f.keep = np
		f.start = len(c.ops)
	} else {
		f.keep = nr
		f.start = len(c.ops)
	}
	f.brIfz = -1
	c.frames = append(c.frames, f)
	extra := uint64(0)
	if kind == 3 {
		extra = 1
	}
	c.emit(opLabelPush, uint32(f.keep), uint32(np), extra)
	if loop {
		c.frames[len(c.frames)-1].start = len(c.ops)
	}
}

func (c *compiler) bump(n int) {
	c.height = c.height + n
	if c.height < 0 {
		c.height = 0
	}
}

func (c *compiler) run() ([]Op, error) {
	for c.r.remain() > 0 {
		op, ok := c.r.u8()
		if !ok {
			return nil, errors.New("truncated op")
		}
		if op == 0x0b {
			if len(c.frames) == 0 {
				return nil, errors.New("end underflow")
			}
			fi := len(c.frames) - 1
			f := c.frames[fi]
			endPC := uint32(len(c.ops))
			if f.kind == 3 && !f.hasElse && f.brIfz >= 0 {
				c.ops[f.brIfz].A = endPC
			}
			for i := 0; i < len(f.patches); i++ {
				c.ops[f.patches[i]].A = endPC
			}
			for i := 0; i < len(f.brtSlots); i++ {
				c.brt[f.brtSlots[i]] = endPC
			}
			for i := 0; i < len(f.catchSlots); i++ {
				si := f.catchSlots[i]
				if si >= 0 && si < len(c.catches) {
					c.catches[si].Target = endPC
				}
			}
			c.height = f.height + f.keep
			if f.kind == 4 {
				for ci := 0; ci < len(c.catches); ci++ {
					if c.catches[ci].Start == uint32(f.start) && c.catches[ci].End == 0 {
						c.catches[ci].End = endPC
					}
				}
			}
			if f.kind != 0 {
				c.emit(opLabelPop, 0, 0, 0)
			}
			c.frames = c.frames[0:fi]
			if f.kind == 0 {
				break
			}
			continue
		}
		if op == 0x05 {
			if len(c.frames) == 0 || c.frames[len(c.frames)-1].kind != 3 {
				return nil, errors.New("else without if")
			}
			fi := len(c.frames) - 1
			_ = c.emitBr(0)
			if c.frames[fi].brIfz >= 0 {
				c.ops[c.frames[fi].brIfz].A = uint32(len(c.ops))
			}
			c.frames[fi].hasElse = true
			c.height = c.frames[fi].height
			continue
		}
		if op == 0x02 || op == 0x03 || op == 0x04 || op == 0x1f {
			ar := c.readBlockType()
			if ar.err != nil {
				return nil, ar.err
			}
			if op == 0x02 {
				c.pushFrame(1, ar.np, ar.nr, false)
			} else if op == 0x03 {
				c.pushFrame(2, ar.np, ar.nr, true)
			} else if op == 0x1f {
				c.pushFrame(4, ar.np, ar.nr, false)
				nc, okc := c.r.u32()
				if !okc {
					return nil, errors.New("try_table")
				}
				for ci := uint32(0); ci < nc; ci++ {
					kind, okk := c.r.u8()
					if !okk {
						return nil, errors.New("try_table catch")
					}
					var ch Catch
					ch.Kind = kind
					ch.Start = uint32(c.frames[len(c.frames)-1].start)
					if kind == 0 || kind == 1 {
						tag, okt := c.r.u32()
						if !okt {
							return nil, errors.New("try_table tag")
						}
						ch.Tag = tag
					}
					lab, okl := c.r.u32()
					if !okl {
						return nil, errors.New("try_table label")
					}
					fi := len(c.frames) - 1 - int(lab)
					if fi < 0 {
						fi = 0
					}
					if fi >= len(c.frames) {
						fi = len(c.frames) - 1
					}
					ch.U = uint64(lab)
					if c.frames[fi].kind == 2 {
						ch.Target = uint32(c.frames[fi].start)
					} else {
						c.frames[fi].catchSlots = append(c.frames[fi].catchSlots, len(c.catches))
					}
					c.catches = append(c.catches, ch)
				}
			} else {
				c.pushFrame(3, ar.np, ar.nr, false)
				c.emit(opBrIfz, 0, 0, 0)
				c.frames[len(c.frames)-1].brIfz = len(c.ops) - 1
				c.bump(-1)
			}
			continue
		}
		if op == 0x08 {
			tag, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("throw")
			}
			c.emit(opThrow, tag, 0, 0)
			continue
		}
		if op == 0x0a {
			c.emit(opThrowRef, 0, 0, 0)
			c.bump(-1)
			continue
		}
		if op == 0x00 {
			c.emit(opUnreachable, 0, 0, 0)
			continue
		}
		if op == 0x06 {
			ar := c.readBlockType()
			if ar.err != nil {
				return nil, ar.err
			}
			c.pushFrame(1, ar.np, ar.nr, false)
			continue
		}
		if op == 0x07 {
			_, _ = c.r.u32()
			_ = c.emitBr(0)
			if len(c.frames) > 0 {
				fi := len(c.frames) - 1
				c.height = c.frames[fi].height
			}
			continue
		}
		if op == 0x09 {
			_ = c.emitBr(0)
			if len(c.frames) > 0 {
				fi := len(c.frames) - 1
				c.height = c.frames[fi].height
			}
			continue
		}
		if op == 0x18 {
			d, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("delegate")
			}
			if err := c.emitBr(int(d)); err != nil {
				return nil, err
			}
			continue
		}
		if op == 0x01 {
			continue
		}
		if op == 0x0c {
			d, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("br")
			}
			if err := c.emitBr(int(d)); err != nil {
				return nil, err
			}
			continue
		}
		if op == 0x0d {
			d, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("br_if")
			}
			fi := len(c.frames) - 1 - int(d)
			if fi < 0 || fi >= len(c.frames) {
				return nil, errors.New("bad br_if")
			}
			u := packHB(c.frames[fi].height, c.frames[fi].keep)
			target := uint32(0)
			if c.frames[fi].kind == 2 {
				target = uint32(c.frames[fi].start)
			}
			c.emit(opBrIf, target, uint32(int(d)), 0)
			idx := len(c.ops) - 1
			if c.frames[fi].kind != 2 {
				c.frames[fi].patches = append(c.frames[fi].patches, idx)
			}
			c.bump(-1)
			continue
		}
		if op == 0x0e {
			n, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("br_table")
			}
			start := uint32(len(c.brt))
			for i := uint32(0); i < n+1; i++ {
				d, ok2 := c.r.u32()
				if !ok2 {
					return nil, errors.New("br_table label")
				}
				fi := len(c.frames) - 1 - int(d)
				if fi < 0 || fi >= len(c.frames) {
					return nil, errors.New("bad br_table")
				}
				c.brt = append(c.brt, 0)
				c.brtU = append(c.brtU, uint64(int(d)))
				if c.frames[fi].kind == 2 {
					c.brt[len(c.brt)-1] = uint32(c.frames[fi].start)
				} else {
					c.frames[fi].brtSlots = append(c.frames[fi].brtSlots, len(c.brt)-1)
				}
			}
			c.emit(opBrTable, start, n+1, 0)
			c.bump(-1)
			continue
		}
		if op == 0x0f {
			c.emit(opReturn, 0, 0, packHB(0, c.nresults))
			continue
		}
		if op == 0x10 {
			idx, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("call")
			}
			np := 0
			nr := 0
			nimp := c.img.NImpFunc()
			if int(idx) < nimp {
				imp, ok2 := c.img.ImpFunc(int(idx))
				if ok2 && int(imp.TypeIdx) < len(c.img.Types) {
					np = len(c.img.Types[imp.TypeIdx].Params)
					nr = len(c.img.Types[imp.TypeIdx].Results)
				}
			} else {
				li := int(idx) - nimp
				if li >= 0 && li < len(c.img.FuncTypes) && int(c.img.FuncTypes[li]) < len(c.img.Types) {
					np = len(c.img.Types[c.img.FuncTypes[li]].Params)
					nr = len(c.img.Types[c.img.FuncTypes[li]].Results)
				}
			}
			c.emit(opCall, idx, 0, 0)
			c.bump(nr - np)
			continue
		}
		if op == 0x11 {
			ti, ok1 := c.r.u32()
			tbl, ok2 := c.r.u32()
			if !ok1 || !ok2 {
				return nil, errors.New("call_indirect")
			}
			np := 0
			nr := 0
			if int(ti) < len(c.img.Types) {
				np = len(c.img.Types[ti].Params)
				nr = len(c.img.Types[ti].Results)
			}
			c.emit(opCallIndirect, ti, tbl, 0)
			c.bump(nr - np - 1)
			continue
		}
		if op == 0x12 {
			idx, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("return_call")
			}
			c.emit(opReturnCall, idx, 0, 0)
			continue
		}
		if op == 0x13 {
			ti, ok1 := c.r.u32()
			tbl, ok2 := c.r.u32()
			if !ok1 || !ok2 {
				return nil, errors.New("return_call_indirect")
			}
			c.emit(opReturnCallI, ti, tbl, 0)
			continue
		}
		if op == 0x14 {
			ti, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("call_ref")
			}
			np := 0
			nr := 0
			if int(ti) < len(c.img.Types) {
				np = len(c.img.Types[ti].Params)
				nr = len(c.img.Types[ti].Results)
			}
			c.emit(opCallIndirect, ti, 0, 1)
			c.bump(nr - np - 1)
			continue
		}
		if op == 0x1a {
			c.emit(opDrop, 0, 0, 0)
			c.bump(-1)
			continue
		}
		if op == 0x1b {
			c.emit(opSelect, 0, 0, 0)
			c.bump(-2)
			continue
		}
		if op == 0x1c {
			n, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("select t")
			}
			for i := uint32(0); i < n; i++ {
				_, _ = c.r.u8()
			}
			c.emit(opSelect, 0, 0, 0)
			c.bump(-2)
			continue
		}
		if op == 0x20 {
			i, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("local.get")
			}
			c.emit(opLocalGet, i, 0, 0)
			c.bump(1)
			continue
		}
		if op == 0x21 {
			i, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("local.set")
			}
			c.emit(opLocalSet, i, 0, 0)
			c.bump(-1)
			continue
		}
		if op == 0x22 {
			i, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("local.tee")
			}
			c.emit(opLocalTee, i, 0, 0)
			continue
		}
		if op == 0x23 {
			i, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("global.get")
			}
			c.emit(opGlobalGet, i, 0, 0)
			c.bump(1)
			continue
		}
		if op == 0x24 {
			i, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("global.set")
			}
			c.emit(opGlobalSet, i, 0, 0)
			c.bump(-1)
			continue
		}
		if op >= 0x28 && op <= 0x35 {
			align, okA := c.r.u32()
			off, okB := c.r.u32()
			if !okA || !okB {
				return nil, errors.New("load memarg")
			}
			c.emit(opLoad, uint32(op), off, uint64(align))
			continue
		}
		if op >= 0x36 && op <= 0x3e {
			align, okA := c.r.u32()
			off, okB := c.r.u32()
			if !okA || !okB {
				return nil, errors.New("store memarg")
			}
			c.emit(opStore, uint32(op), off, uint64(align))
			c.bump(-2)
			continue
		}
		if op == 0x3f {
			_, _ = c.r.u8()
			c.emit(opMemSize, 0, 0, 0)
			c.bump(1)
			continue
		}
		if op == 0x40 {
			_, _ = c.r.u8()
			c.emit(opMemGrow, 0, 0, 0)
			continue
		}
		if op == 0x41 {
			v, ok1 := c.r.s32()
			if !ok1 {
				return nil, errors.New("i32.const")
			}
			c.emit(opConst, 0, 0, uint64(uint32(v)))
			c.bump(1)
			continue
		}
		if op == 0x42 {
			v, ok1 := c.r.s64()
			if !ok1 {
				return nil, errors.New("i64.const")
			}
			c.emit(opConst, 0, 0, uint64(v))
			c.bump(1)
			continue
		}
		if op == 0x43 {
			b, ok1 := c.r.raw(4)
			if !ok1 {
				return nil, errors.New("f32.const")
			}
			u := uint64(b[0]) | uint64(b[1])<<8 | uint64(b[2])<<16 | uint64(b[3])<<24
			c.emit(opConst, 0, 0, u)
			c.bump(1)
			continue
		}
		if op == 0x44 {
			b, ok1 := c.r.raw(8)
			if !ok1 {
				return nil, errors.New("f64.const")
			}
			u := uint64(b[0]) | uint64(b[1])<<8 | uint64(b[2])<<16 | uint64(b[3])<<24 |
				uint64(b[4])<<32 | uint64(b[5])<<40 | uint64(b[6])<<48 | uint64(b[7])<<56
			c.emit(opConst, 0, 0, u)
			c.bump(1)
			continue
		}
		if op == 0x25 {
			i, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("table.get")
			}
			c.emit(opTableGet, i, 0, 0)
			continue
		}
		if op == 0x26 {
			i, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("table.set")
			}
			c.emit(opTableSet, i, 0, 0)
			c.bump(-2)
			continue
		}
		if op == 0xd0 {
			_, _ = c.r.u8()
			c.emit(opRefNull, 0, 0, 0)
			c.bump(1)
			continue
		}
		if op == 0xd1 {
			c.emit(opRefIsNull, 0, 0, 0)
			continue
		}
		if op == 0xd2 {
			i, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("ref.func")
			}
			c.emit(opRefFunc, i, 0, 0)
			c.bump(1)
			continue
		}
		if op == 0xd3 {
			c.emit(opRefEq, 0, 0, 0)
			c.bump(-1)
			continue
		}
		if op == 0xd4 {
			c.emit(opRefAsNN, 0, 0, 0)
			continue
		}
		if op == 0xd5 || op == 0xd6 {
			d, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("br_on_null")
			}
			fi := len(c.frames) - 1 - int(d)
			if fi < 0 || fi >= len(c.frames) {
				return nil, errors.New("bad br_on_null")
			}
			target := uint32(0)
			if c.frames[fi].kind == 2 {
				target = uint32(c.frames[fi].start)
			}
			kind := opBrOnNull
			if op == 0xd6 {
				kind = opBrOnNonNull
				c.bump(-1)
			}
			c.emit(kind, target, uint32(int(d)), packHB(c.frames[fi].height, c.frames[fi].keep))
			idx := len(c.ops) - 1
			if c.frames[fi].kind != 2 {
				c.frames[fi].patches = append(c.frames[fi].patches, idx)
			}
			continue
		}
		if op == 0xfc {
			sub, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("0xfc")
			}
			if sub <= 7 {
				c.emit(opUnop, 0x100+sub, 0, 0)
				continue
			}
			if sub == 8 {
				dataidx, _ := c.r.u32()
				_, _ = c.r.u32()
				c.emit(opMemInit, dataidx, 0, 0)
				c.bump(-3)
			} else if sub == 9 {
				dataidx, _ := c.r.u32()
				c.emit(opDataDrop, dataidx, 0, 0)
			} else if sub == 10 {
				_, _ = c.r.u32()
				_, _ = c.r.u32()
				c.emit(opMemCopy, 0, 0, 0)
				c.bump(-3)
			} else if sub == 11 {
				_, _ = c.r.u32()
				c.emit(opMemFill, 0, 0, 0)
				c.bump(-3)
			} else if sub == 12 {
				elem, _ := c.r.u32()
				table, _ := c.r.u32()
				c.emit(opTableInit, elem, table, 0)
				c.bump(-3)
			} else if sub == 13 {
				elem, _ := c.r.u32()
				c.emit(opElemDrop, elem, 0, 0)
			} else if sub == 14 {
				dst, _ := c.r.u32()
				src, _ := c.r.u32()
				c.emit(opTableCopy, dst, src, 0)
				c.bump(-3)
			} else if sub == 15 {
				table, _ := c.r.u32()
				c.emit(opTableGrow, table, 0, 0)
				c.bump(-1)
			} else if sub == 16 {
				table, _ := c.r.u32()
				c.emit(opTableSize, table, 0, 0)
				c.bump(1)
			} else if sub == 17 {
				table, _ := c.r.u32()
				c.emit(opTableFill, table, 0, 0)
				c.bump(-3)
			} else if sub == 18 {
				_, _ = c.r.u32()
			} else {
				_, _ = c.r.u32()
			}
			continue
		}
		if op == 0xfe {
			sub, ok1 := c.r.u32()
			if !ok1 {
				return nil, errors.New("0xfe")
			}
			off := uint64(0)
			if sub == 3 {
				_, _ = c.r.u8()
			} else {
				_, _ = c.r.u32()
				o, _ := c.r.u32()
				off = uint64(o)
			}
			c.emit(opAtomic, sub, 0, off)
			if sub == 3 {
			} else if sub <= 2 {
				c.bump(-1)
			} else if sub >= 16 && sub <= 22 {
			} else if sub >= 23 && sub <= 29 {
				c.bump(-2)
			} else {
				c.bump(-1)
			}
			continue
		}
		if op == 0xfb {
			return nil, errors.New("gc is oilpan")
		}
		if op == 0xfd {
			if err := c.compileSimd(); err != nil {
				return nil, err
			}
			continue
		}
		if isUnop(op) {
			c.emit(opUnop, uint32(op), 0, 0)
			continue
		}
		if isBinop(op) {
			c.emit(opBinop, uint32(op), 0, 0)
			c.bump(-1)
			continue
		}
		return nil, errors.New("unsupported opcode 0x" + hex2(op))
	}
	return c.ops, nil
}

func hex2(op byte) string {
	d := "0123456789abcdef"
	return string([]byte{d[op>>4], d[op&15]})
}

func isUnop(op byte) bool {
	if op == 0x45 || op == 0x50 {
		return true
	}
	if op >= 0x67 && op <= 0x69 {
		return true
	}
	if op >= 0x79 && op <= 0x7b {
		return true
	}
	if op >= 0x8b && op <= 0x91 {
		return true
	}
	if op >= 0x99 && op <= 0x9f {
		return true
	}
	if op >= 0xa7 && op <= 0xc4 {
		return true
	}
	return false
}

func isBinop(op byte) bool {
	if op >= 0x46 && op <= 0x4f {
		return true
	}
	if op >= 0x51 && op <= 0x5a {
		return true
	}
	if op >= 0x5b && op <= 0x66 {
		return true
	}
	if op >= 0x6a && op <= 0x78 {
		return true
	}
	if op >= 0x7c && op <= 0x8a {
		return true
	}
	if op >= 0x92 && op <= 0x98 {
		return true
	}
	if op >= 0xa0 && op <= 0xa6 {
		return true
	}
	return false
}

func (c *compiler) compileSimd() error {
	sub, ok := c.r.u32()
	if !ok {
		return errors.New("0xfd")
	}
	lane := uint32(0)
	off := uint64(0)
	if sub <= 11 || sub == 92 || sub == 93 {
		_, _ = c.r.u32()
		o, _ := c.r.u32()
		off = uint64(o)
	} else if sub == 12 {
		b, ok1 := c.r.raw(16)
		if !ok1 {
			return errors.New("v128.const")
		}
		lo := uint64(b[0]) | uint64(b[1])<<8 | uint64(b[2])<<16 | uint64(b[3])<<24 |
			uint64(b[4])<<32 | uint64(b[5])<<40 | uint64(b[6])<<48 | uint64(b[7])<<56
		hi := uint64(b[8]) | uint64(b[9])<<8 | uint64(b[10])<<16 | uint64(b[11])<<24 |
			uint64(b[12])<<32 | uint64(b[13])<<40 | uint64(b[14])<<48 | uint64(b[15])<<56
		lane = uint32(len(c.simdc))
		c.simdc = append(c.simdc, lo)
		c.simdc = append(c.simdc, hi)
	} else if sub == 13 {
		b, ok1 := c.r.raw(16)
		if !ok1 {
			return errors.New("i8x16.shuffle")
		}
		lane = uint32(len(c.simdc))
		lo := uint64(0)
		hi := uint64(0)
		for i := 0; i < 8; i++ {
			lo = lo | (uint64(b[i]) << uint(i*8))
			hi = hi | (uint64(b[i+8]) << uint(i*8))
		}
		c.simdc = append(c.simdc, lo)
		c.simdc = append(c.simdc, hi)
	} else if sub >= 21 && sub <= 34 {
		ln, _ := c.r.u8()
		lane = uint32(ln)
	} else if sub >= 84 && sub <= 91 {
		_, _ = c.r.u32()
		o, _ := c.r.u32()
		off = uint64(o)
		ln, _ := c.r.u8()
		lane = uint32(ln)
	}
	c.emit(opSimd, sub, lane, off)
	c.bump(simdBump(sub))
	return nil
}

func simdBump(sub uint32) int {
	if sub <= 10 || sub == 92 || sub == 93 {
		return 0
	}
	if sub == 11 {
		return -2
	}
	if sub == 12 {
		return 1
	}
	if sub == 13 || sub == 14 {
		return -1
	}
	if sub >= 15 && sub <= 20 {
		return 0
	}
	if sub == 23 || sub == 26 || sub == 28 || sub == 30 || sub == 32 || sub == 34 {
		return -1
	}
	if sub >= 21 && sub <= 34 {
		return 0
	}
	if sub >= 84 && sub <= 87 {
		return -1
	}
	if sub >= 88 && sub <= 91 {
		return -2
	}
	if sub == 82 {
		return -2
	}
	if sub == 77 || sub == 83 || sub == 100 {
		return 0
	}
	return -1
}
