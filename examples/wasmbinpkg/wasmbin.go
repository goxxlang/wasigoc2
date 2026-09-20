package wasmbin

import (
	"encoding/binary"
	"fmt"
)

const (
	SecCustom   = 0
	SecType     = 1
	SecImport   = 2
	SecFunction = 3
	SecTable    = 4
	SecMemory   = 5
	SecGlobal   = 6
	SecExport   = 7
	SecStart    = 8
	SecElement  = 9
	SecCode     = 10
	SecData     = 11
	SecDataCnt  = 12
	SecTag      = 13
	CoreVersion       = uint32(1)
	ComponentVersion  = uint32(0x1000d)
)

type Kind byte

const (
	KindFunc   Kind = 0
	KindTable  Kind = 1
	KindMemory Kind = 2
	KindGlobal Kind = 3
	KindTag    Kind = 4
)

func (k Kind) String() string {
	if k == KindFunc {
		return "func"
	}
	if k == KindTable {
		return "table"
	}
	if k == KindMemory {
		return "memory"
	}
	if k == KindGlobal {
		return "global"
	}
	if k == KindTag {
		return "tag"
	}
	return "kind(" + fmt.Sprintf("%d", int(k)) + ")"
}

type ValType byte

const (
	ValI32       ValType = 0x7f
	ValI64       ValType = 0x7e
	ValF32       ValType = 0x7d
	ValF64       ValType = 0x7c
	ValV128      ValType = 0x7b
	ValFuncRef   ValType = 0x70
	ValExternRef ValType = 0x6f
)

func (t ValType) String() string {
	if t == ValI32 {
		return "i32"
	}
	if t == ValI64 {
		return "i64"
	}
	if t == ValF32 {
		return "f32"
	}
	if t == ValF64 {
		return "f64"
	}
	if t == ValV128 {
		return "v128"
	}
	if t == ValFuncRef {
		return "funcref"
	}
	if t == ValExternRef {
		return "externref"
	}
	return "val"
}

type FuncType struct {
	Params  []ValType
	Results []ValType
}

func (t FuncType) String() string {
	return formatFuncType(t)
}

type Limits struct {
	Min    uint32
	Max    uint32
	HasMax bool
	Shared bool
	Mem64  bool
}

type Import struct {
	Module  string
	Name    string
	Knd     Kind
	TypeIdx uint32
	Lim     Limits
	VType   ValType
	Mutable bool
}

type Export struct {
	Name  string
	Knd   Kind
	Index uint32
}

type Section struct {
	ID   byte
	Name string
	Size uint32
	Off  int
}

type FuncBody struct {
	Locals uint32
	Code   []byte
}

type DataSeg struct {
	Off   int
	Data  []byte
	Flags byte
}

type Global struct {
	VType   ValType
	Mutable bool
	Init    uint64
}

type Elem struct {
	Table uint32
	Off   uint32
	Funcs []uint32
	Flags byte
}

type Image struct {
	Version    uint32
	Size       int
	Sections   []Section
	Types      []FuncType
	Imports    []Import
	FuncTypes  []uint32
	Memories   []Limits
	Globals    []Global
	Tables     []Limits
	Elems      []Elem
	Exports    []Export
	Start      uint32
	HasStart   bool
	Customs    []Custom
	ModuleName string
	FuncNames  []string
	Bodies     []FuncBody
	Data       []DataSeg
	Tags       []uint32
}

type Custom struct {
	Name string
	Size int
}

func SectionName(id byte) string {
	if id == SecCustom {
		return "custom"
	}
	if id == SecType {
		return "type"
	}
	if id == SecImport {
		return "import"
	}
	if id == SecFunction {
		return "function"
	}
	if id == SecTable {
		return "table"
	}
	if id == SecMemory {
		return "memory"
	}
	if id == SecGlobal {
		return "global"
	}
	if id == SecExport {
		return "export"
	}
	if id == SecStart {
		return "start"
	}
	if id == SecElement {
		return "element"
	}
	if id == SecCode {
		return "code"
	}
	if id == SecData {
		return "data"
	}
	if id == SecTag {
		return "tag"
	}
	if id == SecDataCnt {
		return "datacount"
	}
	return "section(" + fmt.Sprintf("%d", int(id)) + ")"
}

func magicOK(b []byte) bool {
	return len(b) >= 4 && b[0] == 0 && b[1] == 97 && b[2] == 115 && b[3] == 109
}

func IsWASM(b []byte) bool {
	if !magicOK(b) || len(b) < 8 {
		return false
	}
	return binary.LittleEndian.Uint32(b[4:8]) == CoreVersion
}

func IsComponent(b []byte) bool {
	if !magicOK(b) || len(b) < 8 {
		return false
	}
	return binary.LittleEndian.Uint32(b[4:8]) == ComponentVersion
}

func IsWASMBinary(b []byte) bool {
	return IsWASM(b) || IsComponent(b)
}

func PeelMimicry(raw []byte) []byte {
	if !IsWASM(raw) {
		return nil
	}
	r := &reader{b: raw, off: 8}
	for r.remaining() > 0 {
		id, err := r.u8()
		if err != nil {
			return nil
		}
		size, err2 := r.u32()
		if err2 != nil {
			return nil
		}
		payload, err3 := r.raw(int(size))
		if err3 != nil {
			return nil
		}
		if id != SecCustom {
			continue
		}
		pr := &reader{b: payload}
		name, err4 := pr.name()
		if err4 != nil {
			continue
		}
		if (name == "unil" || name == "gocos") && pr.remaining() > 0 {
			return payload[pr.off:]
		}
	}
	return nil
}

func coreModuleIn(payload []byte) []byte {
	if IsWASM(payload) {
		return payload
	}
	r := &reader{b: payload}
	n, err := r.u32()
	if err != nil || n == 0 {
		return nil
	}
	rest := payload[r.off:]
	if IsWASM(rest) {
		return rest
	}
	return nil
}

// ExtractComponentCore discards an outer wrapper and returns the nested
// core module. The wrapper is not instantiated.
func ExtractComponentCore(raw []byte) []byte {
	if !IsComponent(raw) {
		return nil
	}
	r := &reader{b: raw, off: 8}
	var best []byte
	for r.remaining() > 0 {
		id, err := r.u8()
		if err != nil {
			break
		}
		size, err2 := r.u32()
		if err2 != nil {
			break
		}
		payload, err3 := r.raw(int(size))
		if err3 != nil {
			break
		}
		if id != 1 {
			continue
		}
		mod := coreModuleIn(payload)
		if mod != nil && (best == nil || len(mod) > len(best)) {
			best = mod
		}
	}
	return best
}

// GuestCore is the bytes the interpreter instantiates: a core module.
// An outer wrapper, if present, is thrown away.
func GuestCore(raw []byte) []byte {
	peeled := PeelMimicry(raw)
	if len(peeled) > 0 {
		raw = peeled
	}
	core := ExtractComponentCore(raw)
	if len(core) > 0 {
		return core
	}
	return raw
}

func Parse(src []byte) (*Image, error) {
	if len(src) < 8 {
		return nil, fmt.Errorf("truncated wasm header (%d bytes)", len(src))
	}
	if !magicOK(src) {
		return nil, fmt.Errorf("not a wasm module (magic %s, want 00 61 73 6d)", HexPrefix(src, min(4, len(src))))
	}
	outer := binary.LittleEndian.Uint32(src[4:8])
	core := GuestCore(src)
	if len(core) < 8 {
		return nil, fmt.Errorf("truncated wasm header (%d bytes)", len(core))
	}
	ver := binary.LittleEndian.Uint32(core[4:8])
	if ver != CoreVersion {
		if outer == ComponentVersion {
			return nil, fmt.Errorf("component has no nested core module")
		}
		return nil, fmt.Errorf("unsupported wasm version %d", outer)
	}

	img := &Image{Version: ver, Size: len(core)}
	r := &reader{b: core, off: 8}

	for r.remaining() > 0 {
		id, err := r.u8()
		if err != nil {
			return nil, fmt.Errorf("section id: %s", err.Error())
		}
		size, err2 := r.u32()
		if err2 != nil {
			return nil, fmt.Errorf("section %s size: %s", SectionName(id), err2.Error())
		}
		payloadOff := r.off
		payload, err3 := r.raw(int(size))
		if err3 != nil {
			return nil, fmt.Errorf("section %s payload: %s", SectionName(id), err3.Error())
		}
		img.Sections = append(img.Sections, Section{
			ID:   id,
			Name: SectionName(id),
			Size: size,
			Off:  payloadOff,
		})
		pr := &reader{b: payload}
		if err4 := decodeSection(img, id, pr); err4 != nil {
			return nil, fmt.Errorf("section %s: %s", SectionName(id), err4.Error())
		}
	}
	return img, nil
}

func decodeSection(img *Image, id byte, r *reader) error {
	if id == SecCustom {
		name, err := r.name()
		if err != nil {
			return err
		}
		img.Customs = append(img.Customs, Custom{Name: name, Size: r.remaining()})
		if name == "name" {
			img.ModuleName, img.FuncNames = parseNames(r.b[r.off:])
		}
		return nil
	}
	if id == SecType {
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			form, err1 := r.u8()
			if err1 != nil {
				return err1
			}
			if form != 0x60 {
				return fmt.Errorf("unknown functype")
			}
			ft, err2 := readFuncType(r)
			if err2 != nil {
				return err2
			}
			img.Types = append(img.Types, ft)
		}
		return nil
	}
	if id == SecImport {
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			imp, err1 := readImport(r)
			if err1 != nil {
				return err1
			}
			img.Imports = append(img.Imports, imp)
			if imp.Knd == KindTable {
				img.Tables = append(img.Tables, imp.Lim)
			}
			if imp.Knd == KindMemory {
				img.Memories = append(img.Memories, imp.Lim)
			}
			if imp.Knd == KindGlobal {
				var g Global
				g.VType = imp.VType
				g.Mutable = imp.Mutable
				img.Globals = append(img.Globals, g)
			}
			if imp.Knd == KindTag {
				img.Tags = append(img.Tags, imp.TypeIdx)
			}
		}
		return nil
	}
	if id == SecFunction {
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			idx, err1 := r.u32()
			if err1 != nil {
				return err1
			}
			img.FuncTypes = append(img.FuncTypes, idx)
		}
		return nil
	}
	if id == SecMemory {
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			lim, err1 := readLimits(r)
			if err1 != nil {
				return err1
			}
			img.Memories = append(img.Memories, lim)
		}
		return nil
	}
	if id == SecExport {
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			name, err1 := r.name()
			if err1 != nil {
				return err1
			}
			kind, err2 := r.u8()
			if err2 != nil {
				return err2
			}
			idx, err3 := r.u32()
			if err3 != nil {
				return err3
			}
			img.Exports = append(img.Exports, Export{Name: name, Knd: Kind(kind), Index: idx})
		}
		return nil
	}
	if id == SecStart {
		idx, err := r.u32()
		if err != nil {
			return err
		}
		img.Start = idx
		img.HasStart = true
		return nil
	}
	if id == SecCode {
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			sz, err1 := r.u32()
			if err1 != nil {
				return err1
			}
			body, err2 := r.raw(int(sz))
			if err2 != nil {
				return err2
			}
			br := &reader{b: body}
			ng, err3 := br.u32()
			if err3 != nil {
				return err3
			}
			var nlocals uint32
			for g := uint32(0); g < ng; g++ {
				cnt, err4 := br.u32()
				if err4 != nil {
					return err4
				}
				_, err5 := br.u8()
				if err5 != nil {
					return err5
				}
				nlocals = nlocals + cnt
			}
			img.Bodies = append(img.Bodies, FuncBody{Locals: nlocals, Code: body[br.off:]})
		}
		return nil
	}
	if id == SecData {
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			flags, err1 := r.u8()
			if err1 != nil {
				return err1
			}
			off := 0
			if flags == 0 || flags == 2 {
				if flags == 2 {
					_, errm := r.u32()
					if errm != nil {
						return errm
					}
				}
				v, err2 := readInit(r)
				if err2 != nil {
					return fmt.Errorf("data offset")
				}
				off = int(uint32(v))
			}
			sz, err5 := r.u32()
			if err5 != nil {
				return err5
			}
			bs, err6 := r.raw(int(sz))
			if err6 != nil {
				return err6
			}
			img.Data = append(img.Data, DataSeg{Off: off, Data: bs, Flags: flags})
		}
		return nil
	}
	if id == SecTable {
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			_, err1 := r.u8()
			if err1 != nil {
				return err1
			}
			lim, err2 := readLimits(r)
			if err2 != nil {
				return err2
			}
			img.Tables = append(img.Tables, lim)
		}
		return nil
	}
	if id == SecGlobal {
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			g, err1 := readGlobal(r)
			if err1 != nil {
				return err1
			}
			img.Globals = append(img.Globals, g)
		}
		return nil
	}
	if id == SecElement {
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			el, err1 := readElem(r)
			if err1 != nil {
				return err1
			}
			img.Elems = append(img.Elems, el)
		}
		return nil
	}
	if id == SecTag {
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			_, err1 := r.u8()
			if err1 != nil {
				return err1
			}
			ti, err2 := r.u32()
			if err2 != nil {
				return err2
			}
			img.Tags = append(img.Tags, ti)
		}
		return nil
	}
	return nil
}

func readInit(r *reader) (uint64, error) {
	op, err := r.u8()
	if err != nil {
		return 0, err
	}
	var v uint64
	if op == 0x41 {
		n, err1 := r.i32()
		if err1 != nil {
			return 0, err1
		}
		v = uint64(uint32(n))
	} else if op == 0x42 {
		n, err1 := r.i64()
		if err1 != nil {
			return 0, err1
		}
		v = uint64(n)
	} else if op == 0x43 {
		b, err1 := r.raw(4)
		if err1 != nil {
			return 0, err1
		}
		v = uint64(b[0]) | uint64(b[1])<<8 | uint64(b[2])<<16 | uint64(b[3])<<24
	} else if op == 0x44 {
		b, err1 := r.raw(8)
		if err1 != nil {
			return 0, err1
		}
		v = uint64(b[0]) | uint64(b[1])<<8 | uint64(b[2])<<16 | uint64(b[3])<<24 |
			uint64(b[4])<<32 | uint64(b[5])<<40 | uint64(b[6])<<48 | uint64(b[7])<<56
	} else if op == 0x23 {
		_, err1 := r.u32()
		if err1 != nil {
			return 0, err1
		}
		v = 0
	} else if op == 0xd0 {
		_, err1 := r.u8()
		if err1 != nil {
			return 0, err1
		}
		v = 0
	} else if op == 0xd2 {
		idx, err1 := r.u32()
		if err1 != nil {
			return 0, err1
		}
		v = uint64(idx) + 1
	} else {
		return 0, fmt.Errorf("global init")
	}
	end, err2 := r.u8()
	if err2 != nil || end != 0x0b {
		return 0, fmt.Errorf("global init end")
	}
	return v, nil
}

func readGlobal(r *reader) (Global, error) {
	vt, err := r.u8()
	if err != nil {
		return Global{}, err
	}
	mut, err2 := r.u8()
	if err2 != nil {
		return Global{}, err2
	}
	init, err3 := readInit(r)
	if err3 != nil {
		return Global{}, err3
	}
	return Global{VType: ValType(vt), Mutable: mut == 1, Init: init}, nil
}

func readElem(r *reader) (Elem, error) {
	flags, err := r.u8()
	if err != nil {
		return Elem{}, err
	}
	var el Elem
	el.Flags = flags
	if flags == 2 || flags == 6 {
		t, err1 := r.u32()
		if err1 != nil {
			return Elem{}, err1
		}
		el.Table = t
	}
	if flags&1 == 0 {
		v, err2 := readInit(r)
		if err2 != nil {
			return Elem{}, fmt.Errorf("elem offset")
		}
		el.Off = uint32(v)
	}
	if flags == 1 || flags == 2 || flags == 3 {
		_, _ = r.u8()
	}
	n, err5 := r.u32()
	if err5 != nil {
		return Elem{}, err5
	}
	useExpr := flags&4 != 0 || flags == 1 || flags == 3 || flags == 5 || flags == 7
	for i := uint32(0); i < n; i++ {
		if useExpr {
			op, err6 := r.u8()
			if err6 != nil {
				return Elem{}, err6
			}
			if op == 0xd2 {
				idx, err7 := r.u32()
				end, err8 := r.u8()
				if err7 != nil || err8 != nil || end != 0x0b {
					return Elem{}, fmt.Errorf("elem func")
				}
				el.Funcs = append(el.Funcs, idx)
			} else if op == 0xd0 {
				_, _ = r.u8()
				end, err8 := r.u8()
				if err8 != nil || end != 0x0b {
					return Elem{}, fmt.Errorf("elem null")
				}
				el.Funcs = append(el.Funcs, 0xffffffff)
			} else {
				return Elem{}, fmt.Errorf("elem expr")
			}
		} else {
			idx, err6 := r.u32()
			if err6 != nil {
				return Elem{}, err6
			}
			el.Funcs = append(el.Funcs, idx)
		}
	}
	return el, nil
}

func readFuncType(r *reader) (FuncType, error) {
	var ft FuncType
	np, err := r.u32()
	if err != nil {
		return ft, err
	}
	for i := uint32(0); i < np; i++ {
		t, err1 := r.u8()
		if err1 != nil {
			return ft, err1
		}
		ft.Params = append(ft.Params, ValType(t))
	}
	nr, err2 := r.u32()
	if err2 != nil {
		return ft, err2
	}
	for i := uint32(0); i < nr; i++ {
		t, err3 := r.u8()
		if err3 != nil {
			return ft, err3
		}
		ft.Results = append(ft.Results, ValType(t))
	}
	return ft, nil
}

func readLimits(r *reader) (Limits, error) {
	flag, err := r.u8()
	if err != nil {
		return Limits{}, err
	}
	minv, err2 := r.u32()
	if err2 != nil {
		return Limits{}, err2
	}
	lim := Limits{Min: minv}
	if flag&1 != 0 {
		maxv, err3 := r.u32()
		if err3 != nil {
			return Limits{}, err3
		}
		lim.Max = maxv
		lim.HasMax = true
	}
	if flag&2 != 0 {
		lim.Shared = true
	}
	if flag&4 != 0 {
		lim.Mem64 = true
	}
	return lim, nil
}

func readImport(r *reader) (Import, error) {
	mod, err := r.name()
	if err != nil {
		return Import{}, err
	}
	name, err2 := r.name()
	if err2 != nil {
		return Import{}, err2
	}
	kind, err3 := r.u8()
	if err3 != nil {
		return Import{}, err3
	}
	imp := Import{Module: mod, Name: name, Knd: Kind(kind)}
	if Kind(kind) == KindFunc {
		imp.TypeIdx, err = r.u32()
		return imp, err
	}
	if Kind(kind) == KindTable {
		_, err4 := r.u8()
		if err4 != nil {
			return Import{}, err4
		}
		imp.Lim, err = readLimits(r)
		return imp, err
	}
	if Kind(kind) == KindMemory {
		imp.Lim, err = readLimits(r)
		return imp, err
	}
	if Kind(kind) == KindGlobal {
		vt, e := r.u8()
		if e != nil {
			return Import{}, e
		}
		mut, e2 := r.u8()
		if e2 != nil {
			return Import{}, e2
		}
		imp.VType = ValType(vt)
		imp.Mutable = mut == 1
		return imp, nil
	}
	if Kind(kind) == KindTag {
		imp.TypeIdx, err = r.u32()
		return imp, err
	}
	return Import{}, fmt.Errorf("unknown import kind %d", int(kind))
}

func parseNames(payload []byte) (string, []string) {
	r := &reader{b: payload}
	mod := ""
	var fns []string
	for r.remaining() > 0 {
		id, err := r.u8()
		if err != nil {
			break
		}
		size, err2 := r.u32()
		if err2 != nil {
			break
		}
		body, err3 := r.raw(int(size))
		if err3 != nil {
			break
		}
		if id == 0 {
			nr := &reader{b: body}
			mod, _ = nr.name()
		}
		if id == 1 {
			nr := &reader{b: body}
			n, e := nr.u32()
			if e != nil {
				continue
			}
			for i := uint32(0); i < n; i++ {
				idx, e1 := nr.u32()
				nm, e2 := nr.name()
				if e1 != nil || e2 != nil {
					break
				}
				for uint32(len(fns)) <= idx {
					fns = append(fns, "")
				}
				fns[idx] = nm
			}
		}
	}
	return mod, fns
}

func (img *Image) FuncTypeOf(funcIdx uint32) (FuncType, bool) {
	var n uint32
	for i := 0; i < len(img.Imports); i++ {
		imp := img.Imports[i]
		if imp.Knd != KindFunc {
			continue
		}
		if n == funcIdx {
			if int(imp.TypeIdx) >= len(img.Types) {
				return FuncType{}, false
			}
			return img.Types[imp.TypeIdx], true
		}
		n++
	}
	local := funcIdx - n
	if int(local) >= len(img.FuncTypes) {
		return FuncType{}, false
	}
	ti := img.FuncTypes[local]
	if int(ti) >= len(img.Types) {
		return FuncType{}, false
	}
	return img.Types[ti], true
}

func (img *Image) ExportType(ex Export) string {
	if ex.Knd != KindFunc {
		return ex.Knd.String()
	}
	ft, ok := img.FuncTypeOf(ex.Index)
	if !ok {
		return "func"
	}
	return ft.String()
}

func (img *Image) ImportType(imp Import) string {
	if imp.Knd != KindFunc {
		if imp.Knd == KindMemory {
			return formatLimits(imp.Lim)
		}
		return imp.Knd.String()
	}
	if int(imp.TypeIdx) >= len(img.Types) {
		return "func"
	}
	return img.Types[imp.TypeIdx].String()
}

func (img *Image) HasExport(name string) bool {
	if img == nil {
		return false
	}
	for i := 0; i < len(img.Exports); i++ {
		if img.Exports[i].Name == name && img.Exports[i].Knd == KindFunc {
			return true
		}
	}
	return false
}

func (img *Image) NImpFunc() int {
	n := 0
	for i := 0; i < len(img.Imports); i++ {
		if img.Imports[i].Knd == KindFunc {
			n++
		}
	}
	return n
}

func (img *Image) ImpFunc(idx int) (Import, bool) {
	n := 0
	for i := 0; i < len(img.Imports); i++ {
		if img.Imports[i].Knd != KindFunc {
			continue
		}
		if n == idx {
			return img.Imports[i], true
		}
		n++
	}
	var z Import
	return z, false
}
