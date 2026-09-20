package wasmbin

import (
	"encoding/binary"
	"fmt"
)

const (
	Magic   = "\x00asm"
	Version = uint32(1)
	// ComponentVersion is the WebAssembly component-model preamble
	// (wasip2 / wasm-component-ld). Core modules stay at Version==1.
	ComponentVersion = uint32(0x1000d)

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
)

type Kind byte

const (
	KindFunc   Kind = 0
	KindTable  Kind = 1
	KindMemory Kind = 2
	KindGlobal Kind = 3
)

func (k Kind) String() string {
	switch k {
	case KindFunc:
		return "func"
	case KindTable:
		return "table"
	case KindMemory:
		return "memory"
	case KindGlobal:
		return "global"
	default:
		return fmt.Sprintf("kind(%d)", k)
	}
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
	switch t {
	case ValI32:
		return "i32"
	case ValI64:
		return "i64"
	case ValF32:
		return "f32"
	case ValF64:
		return "f64"
	case ValV128:
		return "v128"
	case ValFuncRef:
		return "funcref"
	case ValExternRef:
		return "externref"
	default:
		return fmt.Sprintf("val(0x%02x)", byte(t))
	}
}

type FuncType struct {
	Params  []ValType
	Results []ValType
}

func (t FuncType) String() string {
	return formatFuncType(t)
}

type Limits struct {
	Min uint32
	Max *uint32
}

type Import struct {
	Module  string
	Name    string
	Kind    Kind
	TypeIdx uint32
	Limits  Limits
	ValType ValType
	Mutable bool
}

type Export struct {
	Name  string
	Kind  Kind
	Index uint32
}

type Section struct {
	ID   byte
	Name string
	Size uint32
	Off  int
}

type Image struct {
	Version    uint32
	Size       int
	Sections   []Section
	Types      []FuncType
	Imports    []Import
	FuncTypes  []uint32
	Memories   []Limits
	Exports    []Export
	Start      *uint32
	Custom     []Custom
	ModuleName string
}

type Custom struct {
	Name string
	Size int
}

func SectionName(id byte) string {
	switch id {
	case SecCustom:
		return "custom"
	case SecType:
		return "type"
	case SecImport:
		return "import"
	case SecFunction:
		return "function"
	case SecTable:
		return "table"
	case SecMemory:
		return "memory"
	case SecGlobal:
		return "global"
	case SecExport:
		return "export"
	case SecStart:
		return "start"
	case SecElement:
		return "element"
	case SecCode:
		return "code"
	case SecData:
		return "data"
	case SecDataCnt:
		return "datacount"
	default:
		return fmt.Sprintf("section(%d)", id)
	}
}

func IsWASM(b []byte) bool {
	return len(b) >= 8 && string(b[:4]) == Magic && binary.LittleEndian.Uint32(b[4:8]) == Version
}

// IsComponent reports a component-model binary (wasigocvm / wasip2).
func IsComponent(b []byte) bool {
	return len(b) >= 8 && string(b[:4]) == Magic && binary.LittleEndian.Uint32(b[4:8]) == ComponentVersion
}

// IsWASMBinary is a core module or a component (anything with \0asm).
func IsWASMBinary(b []byte) bool {
	return IsWASM(b) || IsComponent(b)
}

// PeelMimicry unwraps a core-v1 shell whose custom section "unil"/"gocos"
// holds a wasigocvm component (WASMJsLoader wrapMimicryWasm).
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
		size, err := r.u32()
		if err != nil {
			return nil
		}
		payload, err := r.raw(int(size))
		if err != nil {
			return nil
		}
		if id != SecCustom {
			continue
		}
		pr := &reader{b: payload}
		name, err := pr.name()
		if err != nil {
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

// ExtractComponentCore returns the largest nested core module from a
// component-model binary (section id 1 = core:module). wasigocvm.bat
// wasm32-wasip2 output is this preamble (0x0d / layer 1).
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
		size, err := r.u32()
		if err != nil {
			break
		}
		payload, err := r.raw(int(size))
		if err != nil {
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

// GuestCore is the module the runtime instantiates: peel mimicry, then
// the nested core inside a wasip2 component. Core v1 bytes pass through.
func GuestCore(raw []byte) []byte {
	if peeled := PeelMimicry(raw); len(peeled) > 0 {
		raw = peeled
	}
	if core := ExtractComponentCore(raw); len(core) > 0 {
		return core
	}
	return raw
}

func Parse(src []byte) (*Image, error) {
	if len(src) < 8 {
		return nil, fmt.Errorf("truncated wasm header (%d bytes)", len(src))
	}
	if string(src[:4]) != Magic {
		return nil, fmt.Errorf("not a wasm module (magic % x, want 00 61 73 6d)", src[:min(4, len(src))])
	}
	outer := binary.LittleEndian.Uint32(src[4:8])
	core := GuestCore(src)
	if len(core) < 8 {
		return nil, fmt.Errorf("truncated wasm header (%d bytes)", len(core))
	}
	ver := binary.LittleEndian.Uint32(core[4:8])
	if ver != Version {
		if outer == ComponentVersion {
			return nil, fmt.Errorf("component has no nested core module")
		}
		return nil, fmt.Errorf("unsupported wasm version %d", outer)
	}

	img := &Image{Version: outer, Size: len(src)}
	r := &reader{b: core, off: 8}

	for r.remaining() > 0 {
		id, err := r.u8()
		if err != nil {
			return nil, fmt.Errorf("section id: %w", err)
		}
		size, err := r.u32()
		if err != nil {
			return nil, fmt.Errorf("section %s size: %w", SectionName(id), err)
		}
		payloadOff := r.off
		payload, err := r.raw(int(size))
		if err != nil {
			return nil, fmt.Errorf("section %s payload: %w", SectionName(id), err)
		}
		img.Sections = append(img.Sections, Section{
			ID:   id,
			Name: SectionName(id),
			Size: size,
			Off:  payloadOff,
		})
		pr := &reader{b: payload}
		if err := decodeSection(img, id, pr); err != nil {
			return nil, fmt.Errorf("section %s: %w", SectionName(id), err)
		}
	}
	return img, nil
}

func decodeSection(img *Image, id byte, r *reader) error {
	switch id {
	case SecCustom:
		name, err := r.name()
		if err != nil {
			return err
		}
		img.Custom = append(img.Custom, Custom{Name: name, Size: r.remaining()})
		if name == "name" {
			img.ModuleName, _ = parseModuleName(r.b[r.off:])
		}
	case SecType:
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			form, err := r.u8()
			if err != nil {
				return err
			}
			if form != 0x60 {
				return fmt.Errorf("unknown functype 0x%02x", form)
			}
			ft, err := readFuncType(r)
			if err != nil {
				return err
			}
			img.Types = append(img.Types, ft)
		}
	case SecImport:
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			imp, err := readImport(r)
			if err != nil {
				return err
			}
			img.Imports = append(img.Imports, imp)
		}
	case SecFunction:
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			idx, err := r.u32()
			if err != nil {
				return err
			}
			img.FuncTypes = append(img.FuncTypes, idx)
		}
	case SecMemory:
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			lim, err := readLimits(r)
			if err != nil {
				return err
			}
			img.Memories = append(img.Memories, lim)
		}
	case SecExport:
		n, err := r.u32()
		if err != nil {
			return err
		}
		for i := uint32(0); i < n; i++ {
			name, err := r.name()
			if err != nil {
				return err
			}
			kind, err := r.u8()
			if err != nil {
				return err
			}
			idx, err := r.u32()
			if err != nil {
				return err
			}
			img.Exports = append(img.Exports, Export{Name: name, Kind: Kind(kind), Index: idx})
		}
	case SecStart:
		idx, err := r.u32()
		if err != nil {
			return err
		}
		img.Start = &idx
	}
	return nil
}

func readFuncType(r *reader) (FuncType, error) {
	var ft FuncType
	np, err := r.u32()
	if err != nil {
		return ft, err
	}
	for i := uint32(0); i < np; i++ {
		t, err := r.u8()
		if err != nil {
			return ft, err
		}
		ft.Params = append(ft.Params, ValType(t))
	}
	nr, err := r.u32()
	if err != nil {
		return ft, err
	}
	for i := uint32(0); i < nr; i++ {
		t, err := r.u8()
		if err != nil {
			return ft, err
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
	min, err := r.u32()
	if err != nil {
		return Limits{}, err
	}
	lim := Limits{Min: min}
	if flag&1 != 0 {
		max, err := r.u32()
		if err != nil {
			return Limits{}, err
		}
		lim.Max = &max
	}
	return lim, nil
}

func readImport(r *reader) (Import, error) {
	mod, err := r.name()
	if err != nil {
		return Import{}, err
	}
	name, err := r.name()
	if err != nil {
		return Import{}, err
	}
	kind, err := r.u8()
	if err != nil {
		return Import{}, err
	}
	imp := Import{Module: mod, Name: name, Kind: Kind(kind)}
	switch Kind(kind) {
	case KindFunc:
		imp.TypeIdx, err = r.u32()
	case KindTable:
		if _, err = r.u8(); err != nil {
			return Import{}, err
		}
		imp.Limits, err = readLimits(r)
	case KindMemory:
		imp.Limits, err = readLimits(r)
	case KindGlobal:
		vt, e := r.u8()
		if e != nil {
			return Import{}, e
		}
		mut, e := r.u8()
		if e != nil {
			return Import{}, e
		}
		imp.ValType = ValType(vt)
		imp.Mutable = mut == 1
	default:
		return Import{}, fmt.Errorf("unknown import kind %d", kind)
	}
	return imp, err
}

func parseModuleName(payload []byte) (string, error) {
	r := &reader{b: payload}
	for r.remaining() > 0 {
		id, err := r.u8()
		if err != nil {
			return "", err
		}
		size, err := r.u32()
		if err != nil {
			return "", err
		}
		body, err := r.raw(int(size))
		if err != nil {
			return "", err
		}
		if id == 0 {
			nr := &reader{b: body}
			return nr.name()
		}
	}
	return "", nil
}

// FuncTypeOf returns the type of the function at index (imports first).
func (img *Image) FuncTypeOf(funcIdx uint32) (FuncType, bool) {
	var n uint32
	for _, imp := range img.Imports {
		if imp.Kind != KindFunc {
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
	if ex.Kind != KindFunc {
		return ex.Kind.String()
	}
	ft, ok := img.FuncTypeOf(ex.Index)
	if !ok {
		return "func"
	}
	return ft.String()
}

func (img *Image) ImportType(imp Import) string {
	if imp.Kind != KindFunc {
		if imp.Kind == KindMemory {
			return formatLimits(imp.Limits)
		}
		return imp.Kind.String()
	}
	if int(imp.TypeIdx) >= len(img.Types) {
		return "func"
	}
	return img.Types[imp.TypeIdx].String()
}
