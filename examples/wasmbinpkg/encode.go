package wasmbin

// Encoder writes a subset of the wasm binary format: types, imports,
// functions, memories, exports, code, and active data.

type Encoder struct {
	types     []*FuncType
	imports   []*encImport
	funcs     []uint32
	memMins   []uint32
	expNames  []string
	expKnds   []Kind
	expIdx    []uint32
	codes     [][]byte
	dataOff   []uint32
	dataBytes [][]byte
}

type encImport struct {
	Module  string
	Name    string
	Knd     Kind
	TypeIdx uint32
}

func NewEncoder() *Encoder {
	return &Encoder{}
}

func (e *Encoder) Type(params []ValType, results []ValType) uint32 {
	ft := new(FuncType)
	ft.Params = params
	ft.Results = results
	e.types = append(e.types, ft)
	return uint32(len(e.types) - 1)
}

func (e *Encoder) ImportFunc(module string, name string, typeIdx uint32) uint32 {
	imp := new(encImport)
	imp.Module = module
	imp.Name = name
	imp.Knd = KindFunc
	imp.TypeIdx = typeIdx
	e.imports = append(e.imports, imp)
	return uint32(e.nImportedFuncs() - 1)
}

func (e *Encoder) Memory(minPages uint32) {
	e.memMins = append(e.memMins, minPages)
}

func (e *Encoder) Func(typeIdx uint32, body []byte) uint32 {
	e.funcs = append(e.funcs, typeIdx)
	e.codes = append(e.codes, body)
	return uint32(e.nImportedFuncs() + len(e.funcs) - 1)
}

func (e *Encoder) AddExport(name string, kind Kind, index uint32) {
	e.expNames = append(e.expNames, name)
	e.expKnds = append(e.expKnds, kind)
	e.expIdx = append(e.expIdx, index)
}

func (e *Encoder) Data(offset uint32, payload []byte) {
	e.dataOff = append(e.dataOff, offset)
	e.dataBytes = append(e.dataBytes, payload)
}

func (e *Encoder) nImportedFuncs() int {
	n := 0
	for i := 0; i < len(e.imports); i++ {
		if e.imports[i].Knd == KindFunc {
			n++
		}
	}
	return n
}

func (e *Encoder) Bytes() []byte {
	out := []byte{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00}
	if len(e.types) > 0 {
		out = appendSection(out, SecType, encodeTypes(e.types))
	}
	if len(e.imports) > 0 {
		out = appendSection(out, SecImport, encodeImports(e.imports))
	}
	if len(e.funcs) > 0 {
		out = appendSection(out, SecFunction, encodeU32s(e.funcs))
	}
	if len(e.memMins) > 0 {
		out = appendSection(out, SecMemory, encodeMemMins(e.memMins))
	}
	if len(e.expNames) > 0 {
		out = appendSection(out, SecExport, encodeExportList(e.expNames, e.expKnds, e.expIdx))
	}
	if len(e.codes) > 0 {
		out = appendSection(out, SecCode, encodeCodes(e.codes))
	}
	if len(e.dataOff) > 0 {
		out = appendSection(out, SecData, encodeDataList(e.dataOff, e.dataBytes))
	}
	return out
}

func appendSection(dst []byte, id byte, payload []byte) []byte {
	dst = append(dst, id)
	dst = appendU32(dst, uint32(len(payload)))
	return append(dst, payload...)
}

func encodeName(dst []byte, s string) []byte {
	dst = appendU32(dst, uint32(len(s)))
	return append(dst, s...)
}

func encodeType(dst []byte, t *FuncType) []byte {
	dst = append(dst, 0x60)
	dst = appendU32(dst, uint32(len(t.Params)))
	for i := 0; i < len(t.Params); i++ {
		dst = append(dst, byte(t.Params[i]))
	}
	dst = appendU32(dst, uint32(len(t.Results)))
	for i := 0; i < len(t.Results); i++ {
		dst = append(dst, byte(t.Results[i]))
	}
	return dst
}

func encodeTypes(items []*FuncType) []byte {
	dst := appendU32(nil, uint32(len(items)))
	for i := 0; i < len(items); i++ {
		dst = encodeType(dst, items[i])
	}
	return dst
}

func encodeImport(dst []byte, imp *encImport) []byte {
	dst = encodeName(dst, imp.Module)
	dst = encodeName(dst, imp.Name)
	dst = append(dst, byte(imp.Knd))
	if imp.Knd == KindFunc {
		dst = appendU32(dst, imp.TypeIdx)
	}
	return dst
}

func encodeImports(items []*encImport) []byte {
	dst := appendU32(nil, uint32(len(items)))
	for i := 0; i < len(items); i++ {
		dst = encodeImport(dst, items[i])
	}
	return dst
}

func encodeU32s(items []uint32) []byte {
	dst := appendU32(nil, uint32(len(items)))
	for i := 0; i < len(items); i++ {
		dst = appendU32(dst, items[i])
	}
	return dst
}

func encodeMemMins(mins []uint32) []byte {
	dst := appendU32(nil, uint32(len(mins)))
	for i := 0; i < len(mins); i++ {
		dst = append(dst, 0x00)
		dst = appendU32(dst, mins[i])
	}
	return dst
}

func encodeExportList(names []string, knds []Kind, idxs []uint32) []byte {
	dst := appendU32(nil, uint32(len(names)))
	for i := 0; i < len(names); i++ {
		dst = encodeName(dst, names[i])
		dst = append(dst, byte(knds[i]))
		dst = appendU32(dst, idxs[i])
	}
	return dst
}

func encodeCode(dst []byte, body []byte) []byte {
	dst = appendU32(dst, uint32(len(body)))
	return append(dst, body...)
}

func encodeCodes(items [][]byte) []byte {
	dst := appendU32(nil, uint32(len(items)))
	for i := 0; i < len(items); i++ {
		dst = encodeCode(dst, items[i])
	}
	return dst
}

func encodeDataList(offs []uint32, payloads [][]byte) []byte {
	dst := appendU32(nil, uint32(len(offs)))
	for i := 0; i < len(offs); i++ {
		dst = append(dst, 0x00)
		dst = append(dst, 0x41)
		dst = appendI32(dst, int32(offs[i]))
		dst = append(dst, 0x0b)
		dst = appendU32(dst, uint32(len(payloads[i])))
		dst = append(dst, payloads[i]...)
	}
	return dst
}

func Body(ops ...[]byte) []byte {
	out := []byte{0x00}
	for i := 0; i < len(ops); i++ {
		out = append(out, ops[i]...)
	}
	return append(out, 0x0b)
}

func LocalGet(i uint32) []byte { return appendU32([]byte{0x20}, i) }
func I32Const(v int32) []byte  { return appendI32([]byte{0x41}, v) }
func I32Add() []byte           { return []byte{0x6a} }
func I32Mul() []byte           { return []byte{0x6c} }
func I32Store() []byte         { return []byte{0x36, 0x02, 0x00} }
func Call(fn uint32) []byte    { return appendU32([]byte{0x10}, fn) }
func Drop() []byte             { return []byte{0x1a} }
func End() []byte              { return []byte{0x0b} }
