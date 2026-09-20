// Package wteng is a clean-room Wasmtime-shaped embedding API that runs
// as a wasigocvm guest (wasigocvm.bat + toolchain\sysroot).
//
// Specs used (public documents, not bytecodealliance/wasmtime source):
//   - Wasm binary format: https://webassembly.github.io/spec/core/binary/modules.html
//   - Wasmtime embedding shape (docs.wasmtime.dev): Engine, Store,
//     Module from binary, Instance, call a named export
//   - WASMLoader CLI shape (inspect / run / call): load a compiled
//     .wasm and either dump it, run _start, or invoke an export
//
// Code pages live in a WASMSafeSpace cage. Host imports that WASMLoader
// also provides: wasi_snapshot_preview1.fd_write / proc_exit, env.log /
// env.abort. Product load/run/call is the ported ~/WASMLoader packages
// (examples/wasmloaderpkg), not this embedding-API smoke and not w2g.
package wteng

import (
	"errors"
	"fmt"
	"os"
)

type Engine struct {
	Name string
}

type Store struct {
	Engine *Engine
}

type FuncType struct {
	Params  int
	Results int
}

type FuncDef struct {
	Type   int
	Locals int
	Code   []byte
}

type Export struct {
	Name string
	Kind byte
	Idx  int
}

type Import struct {
	Module string
	Name   string
	Kind   byte
	Type   int
}

type Section struct {
	ID   byte
	Name string
	Size int
	Off  int
}

type DataSeg struct {
	Off  int
	Data []byte
}

type Module struct {
	Types    []FuncType
	Imports  []Import
	Funcs    []FuncDef
	Exports  []Export
	Sections []Section
	Data     []DataSeg
	MemPages int
	Size     int
	Version  int
	HasStart bool
	Start    int
}

type Instance struct {
	Mod     *Module
	Mem     []byte
	Exited  bool
	ExitCode int
}

type rdr struct {
	b   []byte
	pos int
}

func NewEngine() *Engine {
	e := &Engine{}
	e.Name = "wasigocvm"
	return e
}

func NewStore(e *Engine) *Store {
	s := &Store{}
	s.Engine = e
	return s
}

func SectionName(id byte) string {
	if id == 0 {
		return "custom"
	}
	if id == 1 {
		return "type"
	}
	if id == 2 {
		return "import"
	}
	if id == 3 {
		return "function"
	}
	if id == 4 {
		return "table"
	}
	if id == 5 {
		return "memory"
	}
	if id == 6 {
		return "global"
	}
	if id == 7 {
		return "export"
	}
	if id == 8 {
		return "start"
	}
	if id == 9 {
		return "element"
	}
	if id == 10 {
		return "code"
	}
	if id == 11 {
		return "data"
	}
	if id == 12 {
		return "datacount"
	}
	return "section"
}

func KindName(k byte) string {
	if k == 0 {
		return "func"
	}
	if k == 1 {
		return "table"
	}
	if k == 2 {
		return "memory"
	}
	if k == 3 {
		return "global"
	}
	return "kind"
}

func (r *rdr) remain() int {
	if r.pos >= len(r.b) {
		return 0
	}
	return len(r.b) - r.pos
}

func (r *rdr) u8() (byte, bool) {
	if r.pos >= len(r.b) {
		return 0, false
	}
	v := r.b[r.pos]
	r.pos = r.pos + 1
	return v, true
}

func (r *rdr) u32() (int, bool) {
	result := 0
	shift := 0
	for {
		b, ok := r.u8()
		if !ok {
			return 0, false
		}
		result = result | (int(b&127) << shift)
		if (b & 128) == 0 {
			return result, true
		}
		shift = shift + 7
		if shift > 28 {
			return 0, false
		}
	}
}

func (r *rdr) s32() (int64, bool) {
	result := int64(0)
	shift := 0
	var b byte
	ok := false
	for {
		b, ok = r.u8()
		if !ok {
			return 0, false
		}
		result = result | (int64(b&127) << shift)
		shift = shift + 7
		if (b & 128) == 0 {
			break
		}
		if shift > 28 {
			return 0, false
		}
	}
	if shift < 32 && (b&64) != 0 {
		result = result | (int64(-1) << shift)
	}
	return result, true
}

func (r *rdr) bytes(n int) ([]byte, bool) {
	if n < 0 || r.pos+n > len(r.b) {
		return nil, false
	}
	out := r.b[r.pos : r.pos+n]
	r.pos = r.pos + n
	return out, true
}

func (r *rdr) name() (string, bool) {
	n, ok := r.u32()
	if !ok {
		return "", false
	}
	bs, ok2 := r.bytes(n)
	if !ok2 {
		return "", false
	}
	return string(bs), true
}

func wrap32(v int64) int64 {
	n := v & 4294967295
	if n >= 2147483648 {
		return n - 4294967296
	}
	return n
}

func parseTypes(payload []byte) ([]FuncType, error) {
	r := rdr{b: payload, pos: 0}
	n, ok := r.u32()
	if !ok {
		return nil, errors.New("wteng: bad type count")
	}
	var types []FuncType
	for i := 0; i < n; i++ {
		form, ok1 := r.u8()
		if !ok1 || form != 0x60 {
			return nil, errors.New("wteng: bad functype")
		}
		np, ok2 := r.u32()
		if !ok2 {
			return nil, errors.New("wteng: bad param count")
		}
		for p := 0; p < np; p++ {
			_, ok3 := r.u8()
			if !ok3 {
				return nil, errors.New("wteng: bad param type")
			}
		}
		nr, ok4 := r.u32()
		if !ok4 {
			return nil, errors.New("wteng: bad result count")
		}
		for p := 0; p < nr; p++ {
			_, ok5 := r.u8()
			if !ok5 {
				return nil, errors.New("wteng: bad result type")
			}
		}
		var ft FuncType
		ft.Params = np
		ft.Results = nr
		types = append(types, ft)
	}
	return types, nil
}

func parseFuncSec(payload []byte) ([]int, error) {
	r := rdr{b: payload, pos: 0}
	n, ok := r.u32()
	if !ok {
		return nil, errors.New("wteng: bad func count")
	}
	var idx []int
	for i := 0; i < n; i++ {
		t, ok1 := r.u32()
		if !ok1 {
			return nil, errors.New("wteng: bad typeidx")
		}
		idx = append(idx, t)
	}
	return idx, nil
}

func parseExports(payload []byte) ([]Export, error) {
	r := rdr{b: payload, pos: 0}
	n, ok := r.u32()
	if !ok {
		return nil, errors.New("wteng: bad export count")
	}
	var ex []Export
	for i := 0; i < n; i++ {
		name, ok1 := r.name()
		if !ok1 {
			return nil, errors.New("wteng: bad export name")
		}
		kind, ok2 := r.u8()
		if !ok2 {
			return nil, errors.New("wteng: bad export kind")
		}
		id, ok3 := r.u32()
		if !ok3 {
			return nil, errors.New("wteng: bad export idx")
		}
		var e Export
		e.Name = name
		e.Kind = kind
		e.Idx = id
		ex = append(ex, e)
	}
	return ex, nil
}

func parseCode(payload []byte, typeidxs []int) ([]FuncDef, error) {
	r := rdr{b: payload, pos: 0}
	n, ok := r.u32()
	if !ok {
		return nil, errors.New("wteng: bad code count")
	}
	if n != len(typeidxs) {
		return nil, errors.New("wteng: func/code count mismatch")
	}
	var fns []FuncDef
	for i := 0; i < n; i++ {
		sz, ok1 := r.u32()
		if !ok1 {
			return nil, errors.New("wteng: bad code size")
		}
		body, ok2 := r.bytes(sz)
		if !ok2 {
			return nil, errors.New("wteng: truncated code")
		}
		br := rdr{b: body, pos: 0}
		ng, ok3 := br.u32()
		if !ok3 {
			return nil, errors.New("wteng: bad local groups")
		}
		nlocals := 0
		for g := 0; g < ng; g++ {
			cnt, ok4 := br.u32()
			_, ok5 := br.u8()
			if !ok4 || !ok5 {
				return nil, errors.New("wteng: bad locals")
			}
			nlocals = nlocals + cnt
		}
		var fd FuncDef
		fd.Type = typeidxs[i]
		fd.Locals = nlocals
		fd.Code = body[br.pos:]
		fns = append(fns, fd)
	}
	return fns, nil
}

func skipLimits(r *rdr) bool {
	flag, ok := r.u8()
	if !ok {
		return false
	}
	_, ok2 := r.u32()
	if !ok2 {
		return false
	}
	if (flag & 1) != 0 {
		_, ok3 := r.u32()
		return ok3
	}
	return true
}

func parseImports(payload []byte) ([]Import, error) {
	r := rdr{b: payload, pos: 0}
	n, ok := r.u32()
	if !ok {
		return nil, errors.New("wteng: bad import count")
	}
	var out []Import
	for i := 0; i < n; i++ {
		mod, ok1 := r.name()
		name, ok2 := r.name()
		kind, ok3 := r.u8()
		if !ok1 || !ok2 || !ok3 {
			return nil, errors.New("wteng: bad import")
		}
		var imp Import
		imp.Module = mod
		imp.Name = name
		imp.Kind = kind
		if kind == 0 {
			t, ok4 := r.u32()
			if !ok4 {
				return nil, errors.New("wteng: bad import typeidx")
			}
			imp.Type = t
		} else if kind == 1 {
			_, ok4 := r.u8()
			if !ok4 || !skipLimits(&r) {
				return nil, errors.New("wteng: bad table import")
			}
		} else if kind == 2 {
			if !skipLimits(&r) {
				return nil, errors.New("wteng: bad memory import")
			}
		} else if kind == 3 {
			_, ok4 := r.u8()
			_, ok5 := r.u8()
			if !ok4 || !ok5 {
				return nil, errors.New("wteng: bad global import")
			}
		} else {
			return nil, errors.New("wteng: unknown import kind")
		}
		out = append(out, imp)
	}
	return out, nil
}

func parseMemory(payload []byte) (int, error) {
	r := rdr{b: payload, pos: 0}
	n, ok := r.u32()
	if !ok || n < 1 {
		return 0, nil
	}
	flag, ok1 := r.u8()
	min, ok2 := r.u32()
	if !ok1 || !ok2 {
		return 0, errors.New("wteng: bad memory")
	}
	if (flag & 1) != 0 {
		_, _ = r.u32()
	}
	return min, nil
}

func parseData(payload []byte) ([]DataSeg, error) {
	r := rdr{b: payload, pos: 0}
	n, ok := r.u32()
	if !ok {
		return nil, errors.New("wteng: bad data count")
	}
	var segs []DataSeg
	for i := 0; i < n; i++ {
		flags, ok1 := r.u8()
		if !ok1 {
			return nil, errors.New("wteng: bad data flags")
		}
		off := 0
		if flags == 0 || flags == 2 {
			if flags == 2 {
				_, okm := r.u32()
				if !okm {
					return nil, errors.New("wteng: bad data memidx")
				}
			}
			op, ok2 := r.u8()
			if !ok2 || op != 0x41 {
				return nil, errors.New("wteng: data offset")
			}
			v, ok3 := r.s32()
			end, ok4 := r.u8()
			if !ok3 || !ok4 || end != 0x0b {
				return nil, errors.New("wteng: data offset end")
			}
			off = int(v)
		}
		sz, ok5 := r.u32()
		if !ok5 {
			return nil, errors.New("wteng: data size")
		}
		bs, ok6 := r.bytes(sz)
		if !ok6 {
			return nil, errors.New("wteng: truncated data")
		}
		var d DataSeg
		d.Off = off
		d.Data = bs
		segs = append(segs, d)
	}
	return segs, nil
}

func NewModule(e *Engine, wasm []byte) (*Module, error) {
	if e == nil {
		return nil, errors.New("wteng: nil engine")
	}
	if len(wasm) < 8 {
		return nil, errors.New("wteng: too short")
	}
	if wasm[0] != 0 || wasm[1] != 97 || wasm[2] != 115 || wasm[3] != 109 {
		return nil, errors.New("wteng: bad magic")
	}
	ver := int(wasm[4]) | (int(wasm[5]) << 8) | (int(wasm[6]) << 16) | (int(wasm[7]) << 24)
	if ver != 1 {
		return nil, errors.New("wteng: not a core wasm module")
	}
	r := rdr{b: wasm, pos: 8}
	var types []FuncType
	var typeidxs []int
	var funcs []FuncDef
	var exports []Export
	var imports []Import
	var sections []Section
	var data []DataSeg
	memPages := 0
	hasStart := false
	start := 0
	for r.remain() > 0 {
		id, ok := r.u8()
		if !ok {
			return nil, errors.New("wteng: bad section id")
		}
		sz, ok2 := r.u32()
		if !ok2 {
			return nil, errors.New("wteng: bad section size")
		}
		off := r.pos
		payload, ok3 := r.bytes(sz)
		if !ok3 {
			return nil, errors.New("wteng: truncated section")
		}
		var sec Section
		sec.ID = id
		sec.Name = SectionName(id)
		sec.Size = sz
		sec.Off = off
		sections = append(sections, sec)
		if id == 1 {
			ts, err := parseTypes(payload)
			if err != nil {
				return nil, err
			}
			types = ts
		} else if id == 2 {
			imps, err := parseImports(payload)
			if err != nil {
				return nil, err
			}
			imports = imps
		} else if id == 3 {
			idx, err := parseFuncSec(payload)
			if err != nil {
				return nil, err
			}
			typeidxs = idx
		} else if id == 5 {
			mp, err := parseMemory(payload)
			if err != nil {
				return nil, err
			}
			memPages = mp
		} else if id == 7 {
			ex, err := parseExports(payload)
			if err != nil {
				return nil, err
			}
			exports = ex
		} else if id == 8 {
			sr := rdr{b: payload, pos: 0}
			si, ok4 := sr.u32()
			if !ok4 {
				return nil, errors.New("wteng: bad start")
			}
			hasStart = true
			start = si
		} else if id == 10 {
			fns, err := parseCode(payload, typeidxs)
			if err != nil {
				return nil, err
			}
			funcs = fns
		} else if id == 11 {
			ds, err := parseData(payload)
			if err != nil {
				return nil, err
			}
			data = ds
		}
	}
	m := &Module{}
	m.Types = types
	m.Imports = imports
	m.Funcs = funcs
	m.Exports = exports
	m.Sections = sections
	m.Data = data
	m.MemPages = memPages
	m.Size = len(wasm)
	m.Version = ver
	m.HasStart = hasStart
	m.Start = start
	return m, nil
}

func (m *Module) NImpFunc() int {
	if m == nil {
		return 0
	}
	n := 0
	for i := 0; i < len(m.Imports); i++ {
		if m.Imports[i].Kind == 0 {
			n = n + 1
		}
	}
	return n
}

func (m *Module) ImpFunc(idx int) (Import, bool) {
	n := 0
	for i := 0; i < len(m.Imports); i++ {
		if m.Imports[i].Kind != 0 {
			continue
		}
		if n == idx {
			return m.Imports[i], true
		}
		n = n + 1
	}
	var z Import
	return z, false
}

func NewInstance(s *Store, m *Module) (*Instance, error) {
	if s == nil || m == nil {
		return nil, errors.New("wteng: nil store/module")
	}
	pages := m.MemPages
	if pages <= 0 && len(m.Data) > 0 {
		pages = 1
	}
	in := &Instance{}
	in.Mod = m
	if pages > 0 {
		in.Mem = make([]byte, pages*65536)
	}
	for i := 0; i < len(m.Data); i++ {
		d := m.Data[i]
		if d.Off < 0 || d.Off+len(d.Data) > len(in.Mem) {
			return nil, errors.New("wteng: data out of bounds")
		}
		for j := 0; j < len(d.Data); j++ {
			in.Mem[d.Off+j] = d.Data[j]
		}
	}
	return in, nil
}

func (m *Module) HasExport(name string) bool {
	if m == nil {
		return false
	}
	for i := 0; i < len(m.Exports); i++ {
		if m.Exports[i].Name == name && m.Exports[i].Kind == 0 {
			return true
		}
	}
	return false
}

func (in *Instance) loadI32(addr int) (int64, error) {
	if addr < 0 || addr+4 > len(in.Mem) {
		return 0, errors.New("wteng: load oob")
	}
	u := int(in.Mem[addr]) | (int(in.Mem[addr+1]) << 8) | (int(in.Mem[addr+2]) << 16) | (int(in.Mem[addr+3]) << 24)
	return wrap32(int64(u)), nil
}

func (in *Instance) storeI32(addr int, v int64) error {
	if addr < 0 || addr+4 > len(in.Mem) {
		return errors.New("wteng: store oob")
	}
	u := int(wrap32(v))
	in.Mem[addr] = byte(u & 255)
	in.Mem[addr+1] = byte((u >> 8) & 255)
	in.Mem[addr+2] = byte((u >> 16) & 255)
	in.Mem[addr+3] = byte((u >> 24) & 255)
	return nil
}

func (in *Instance) hostCall(idx int, args []int64) ([]int64, error) {
	imp, ok := in.Mod.ImpFunc(idx)
	if !ok {
		return nil, errors.New("wteng: bad import idx")
	}
	if imp.Name == "fd_write" && len(args) >= 4 {
		iov := int(args[1])
		cnt := int(args[2])
		nw := int(args[3])
		wrote := 0
		for i := 0; i < cnt; i++ {
			ptrv, err := in.loadI32(iov + i*8)
			if err != nil {
				return nil, err
			}
			lenv, err2 := in.loadI32(iov + i*8 + 4)
			if err2 != nil {
				return nil, err2
			}
			p := int(ptrv)
			n := int(lenv)
			if p < 0 || p+n > len(in.Mem) {
				return []int64{8}, nil
			}
			if n > 0 {
				fmt.Print(string(in.Mem[p : p+n]))
			}
			wrote = wrote + n
		}
		_ = in.storeI32(nw, int64(wrote))
		return []int64{0}, nil
	}
	if imp.Name == "log" && len(args) >= 2 {
		p := int(args[0])
		n := int(args[1])
		if p >= 0 && p+n <= len(in.Mem) && n > 0 {
			fmt.Print(string(in.Mem[p : p+n]))
		}
		return nil, nil
	}
	if imp.Name == "proc_exit" || imp.Name == "abort" {
		in.Exited = true
		if len(args) > 0 {
			in.ExitCode = int(args[0])
		}
		return nil, nil
	}
	return nil, errors.New("wteng: unknown import " + imp.Module + "." + imp.Name)
}

func (in *Instance) invoke(idx int, args []int64) ([]int64, error) {
	nimp := in.Mod.NImpFunc()
	if idx < nimp {
		return in.hostCall(idx, args)
	}
	return in.exec(idx-nimp, args)
}

func (in *Instance) exec(idx int, args []int64) ([]int64, error) {
	m := in.Mod
	if idx < 0 || idx >= len(m.Funcs) {
		return nil, errors.New("wteng: bad func idx")
	}
	fn := m.Funcs[idx]
	if fn.Type < 0 || fn.Type >= len(m.Types) {
		return nil, errors.New("wteng: bad func type")
	}
	ft := m.Types[fn.Type]
	if len(args) != ft.Params {
		return nil, errors.New("wteng: arity")
	}
	nloc := ft.Params + fn.Locals
	locals := make([]int64, nloc)
	for i := 0; i < len(args); i++ {
		locals[i] = wrap32(args[i])
	}
	var stack []int64
	r := rdr{b: fn.Code, pos: 0}
	for r.remain() > 0 {
		op, ok := r.u8()
		if !ok {
			return nil, errors.New("wteng: truncated op")
		}
		if op == 0x0b || op == 0x0f {
			break
		}
		if op == 0x1a {
			if len(stack) < 1 {
				return nil, errors.New("wteng: drop underflow")
			}
			stack = stack[0 : len(stack)-1]
			continue
		}
		if op == 0x20 {
			i, ok1 := r.u32()
			if !ok1 || i < 0 || i >= len(locals) {
				return nil, errors.New("wteng: bad local.get")
			}
			stack = append(stack, locals[i])
			continue
		}
		if op == 0x21 {
			i, ok1 := r.u32()
			if !ok1 || i < 0 || i >= len(locals) || len(stack) < 1 {
				return nil, errors.New("wteng: bad local.set")
			}
			locals[i] = stack[len(stack)-1]
			stack = stack[0 : len(stack)-1]
			continue
		}
		if op == 0x41 {
			v, ok1 := r.s32()
			if !ok1 {
				return nil, errors.New("wteng: bad i32.const")
			}
			stack = append(stack, wrap32(v))
			continue
		}
		if op == 0x28 || op == 0x36 {
			_, okA := r.u32()
			off, okB := r.u32()
			if !okA || !okB {
				return nil, errors.New("wteng: bad memarg")
			}
			if op == 0x28 {
				if len(stack) < 1 {
					return nil, errors.New("wteng: load underflow")
				}
				addr := int(stack[len(stack)-1]) + off
				stack = stack[0 : len(stack)-1]
				v, err := in.loadI32(addr)
				if err != nil {
					return nil, err
				}
				stack = append(stack, v)
			} else {
				if len(stack) < 2 {
					return nil, errors.New("wteng: store underflow")
				}
				val := stack[len(stack)-1]
				addr := int(stack[len(stack)-2]) + off
				stack = stack[0 : len(stack)-2]
				err := in.storeI32(addr, val)
				if err != nil {
					return nil, err
				}
			}
			continue
		}
		if op == 0x6a || op == 0x6b || op == 0x6c {
			if len(stack) < 2 {
				return nil, errors.New("wteng: binop underflow")
			}
			b := stack[len(stack)-1]
			a := stack[len(stack)-2]
			stack = stack[0 : len(stack)-2]
			out := int64(0)
			if op == 0x6a {
				out = wrap32(a + b)
			} else if op == 0x6b {
				out = wrap32(a - b)
			} else {
				out = wrap32(a * b)
			}
			stack = append(stack, out)
			continue
		}
		if op == 0x10 {
			callee, ok1 := r.u32()
			if !ok1 {
				return nil, errors.New("wteng: bad call")
			}
			var ct FuncType
			nimp := m.NImpFunc()
			if callee < nimp {
				imp, ok2 := m.ImpFunc(callee)
				if !ok2 || imp.Type < 0 || imp.Type >= len(m.Types) {
					return nil, errors.New("wteng: bad import call")
				}
				ct = m.Types[imp.Type]
			} else {
				li := callee - nimp
				if li < 0 || li >= len(m.Funcs) {
					return nil, errors.New("wteng: bad call")
				}
				ct = m.Types[m.Funcs[li].Type]
			}
			if len(stack) < ct.Params {
				return nil, errors.New("wteng: call underflow")
			}
			cargs := make([]int64, ct.Params)
			base := len(stack) - ct.Params
			for i := 0; i < ct.Params; i++ {
				cargs[i] = stack[base+i]
			}
			stack = stack[0:base]
			got, err := in.invoke(callee, cargs)
			if err != nil {
				return nil, err
			}
			if in.Exited {
				return got, nil
			}
			for i := 0; i < len(got); i++ {
				stack = append(stack, got[i])
			}
			continue
		}
		return nil, errors.New("wteng: unsupported opcode")
	}
	if len(stack) < ft.Results {
		return nil, errors.New("wteng: missing result")
	}
	out := make([]int64, ft.Results)
	base := len(stack) - ft.Results
	for i := 0; i < ft.Results; i++ {
		out[i] = stack[base+i]
	}
	return out, nil
}

func (in *Instance) Call(name string, args []int64) ([]int64, error) {
	if in == nil || in.Mod == nil {
		return nil, errors.New("wteng: nil instance")
	}
	for i := 0; i < len(in.Mod.Exports); i++ {
		e := in.Mod.Exports[i]
		if e.Name == name && e.Kind == 0 {
			return in.invoke(e.Idx, args)
		}
	}
	return nil, errors.New("wteng: unknown export")
}

func (in *Instance) Run() error {
	if in == nil {
		return errors.New("wteng: nil instance")
	}
	if in.Mod.HasExport("_start") {
		var none []int64
		_, err := in.Call("_start", none)
		if err != nil {
			return err
		}
		if in.Exited && in.ExitCode != 0 {
			os.Exit(in.ExitCode)
		}
		return nil
	}
	if in.Mod.HasStart {
		_, err := in.invoke(in.Mod.Start, nil)
		return err
	}
	return errors.New("wteng: no _start")
}
