package wasmbin

// Encoder writes a subset of the wasm binary format: types, imports,
// functions, memories, exports, code, and active data.

type Encoder struct {
	types    []FuncType
	imports  []encImport
	funcs    []uint32
	memories []Limits
	exports  []Export
	codes    [][]byte
	data     []encData
}

type encImport struct {
	Module, Name string
	Kind         Kind
	TypeIdx      uint32
	Limits       Limits
}

type encData struct {
	Offset uint32
	Bytes  []byte
}

func NewEncoder() *Encoder { return &Encoder{} }

func (e *Encoder) Type(params, results []ValType) uint32 {
	e.types = append(e.types, FuncType{Params: params, Results: results})
	return uint32(len(e.types) - 1)
}

func (e *Encoder) ImportFunc(module, name string, typeIdx uint32) uint32 {
	e.imports = append(e.imports, encImport{
		Module: module, Name: name, Kind: KindFunc, TypeIdx: typeIdx,
	})
	return uint32(len(e.importedFuncs()) - 1)
}

func (e *Encoder) Memory(minPages uint32) {
	e.memories = append(e.memories, Limits{Min: minPages})
}

func (e *Encoder) Func(typeIdx uint32, body []byte) uint32 {
	e.funcs = append(e.funcs, typeIdx)
	e.codes = append(e.codes, body)
	return uint32(len(e.importedFuncs()) + len(e.funcs) - 1)
}

func (e *Encoder) Export(name string, kind Kind, index uint32) {
	e.exports = append(e.exports, Export{Name: name, Kind: kind, Index: index})
}

func (e *Encoder) Data(offset uint32, payload []byte) {
	e.data = append(e.data, encData{Offset: offset, Bytes: payload})
}

func (e *Encoder) importedFuncs() []encImport {
	var out []encImport
	for _, imp := range e.imports {
		if imp.Kind == KindFunc {
			out = append(out, imp)
		}
	}
	return out
}

func (e *Encoder) Bytes() []byte {
	out := []byte{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00}
	if len(e.types) > 0 {
		out = appendSection(out, SecType, encodeVec(e.types, encodeType))
	}
	if len(e.imports) > 0 {
		out = appendSection(out, SecImport, encodeVec(e.imports, encodeImport))
	}
	if len(e.funcs) > 0 {
		out = appendSection(out, SecFunction, encodeVec(e.funcs, func(dst []byte, idx uint32) []byte {
			return appendU32(dst, idx)
		}))
	}
	if len(e.memories) > 0 {
		out = appendSection(out, SecMemory, encodeVec(e.memories, encodeLimitsAsMem))
	}
	if len(e.exports) > 0 {
		out = appendSection(out, SecExport, encodeVec(e.exports, encodeExport))
	}
	if len(e.codes) > 0 {
		out = appendSection(out, SecCode, encodeVec(e.codes, encodeCode))
	}
	if len(e.data) > 0 {
		out = appendSection(out, SecData, encodeVec(e.data, encodeData))
	}
	return out
}

func appendSection(dst []byte, id byte, payload []byte) []byte {
	dst = append(dst, id)
	dst = appendU32(dst, uint32(len(payload)))
	return append(dst, payload...)
}

func encodeVec[T any](items []T, enc func([]byte, T) []byte) []byte {
	dst := appendU32(nil, uint32(len(items)))
	for _, it := range items {
		dst = enc(dst, it)
	}
	return dst
}

func encodeName(dst []byte, s string) []byte {
	dst = appendU32(dst, uint32(len(s)))
	return append(dst, s...)
}

func encodeType(dst []byte, t FuncType) []byte {
	dst = append(dst, 0x60)
	dst = appendU32(dst, uint32(len(t.Params)))
	for _, p := range t.Params {
		dst = append(dst, byte(p))
	}
	dst = appendU32(dst, uint32(len(t.Results)))
	for _, r := range t.Results {
		dst = append(dst, byte(r))
	}
	return dst
}

func encodeImport(dst []byte, imp encImport) []byte {
	dst = encodeName(dst, imp.Module)
	dst = encodeName(dst, imp.Name)
	dst = append(dst, byte(imp.Kind))
	switch imp.Kind {
	case KindFunc:
		dst = appendU32(dst, imp.TypeIdx)
	case KindMemory:
		dst = encodeLimits(dst, imp.Limits)
	}
	return dst
}

func encodeLimits(dst []byte, l Limits) []byte {
	if l.Max != nil {
		dst = append(dst, 0x01)
		dst = appendU32(dst, l.Min)
		return appendU32(dst, *l.Max)
	}
	dst = append(dst, 0x00)
	return appendU32(dst, l.Min)
}

func encodeLimitsAsMem(dst []byte, l Limits) []byte {
	return encodeLimits(dst, l)
}

func encodeExport(dst []byte, ex Export) []byte {
	dst = encodeName(dst, ex.Name)
	dst = append(dst, byte(ex.Kind))
	return appendU32(dst, ex.Index)
}

func encodeCode(dst []byte, body []byte) []byte {
	dst = appendU32(dst, uint32(len(body)))
	return append(dst, body...)
}

func encodeData(dst []byte, d encData) []byte {
	dst = append(dst, 0x00) // active, memory 0
	dst = append(dst, 0x41) // i32.const
	dst = appendI32(dst, int32(d.Offset))
	dst = append(dst, 0x0b) // end
	dst = appendU32(dst, uint32(len(d.Bytes)))
	return append(dst, d.Bytes...)
}

// Instruction helpers for building function bodies.

func Body(ops ...[]byte) []byte {
	out := []byte{0x00} // 0 local groups
	for _, op := range ops {
		out = append(out, op...)
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
