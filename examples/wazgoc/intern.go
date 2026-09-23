package wazgoc

import "../wasmbinpkg"

// Interned object types. CHPT tags are these ids; Get fails unless
// the interned type matches. Payloads live in the WASMSafeSpace cage.
// TPT names trusted compiled/instance objects. EPT names memory GIA.
// This is type_key interning, not WASM GC.

const (
	tyEngine   = 1
	tyCompiled = 2
	tyInstance = 3
	tyFunction = 4
	tyV128     = 5
	tyFunc     = 6
	tyMemory   = 7
	tyModule   = 8
	tyTable    = 9
	tyI32      = 10
	tyI64      = 11
	tyF32      = 12
	tyF64      = 13
	tyFuncRef  = 14
	tyExtern   = 15
	tyGlobal   = 16
)

type internRec struct {
	kind    int
	params  []byte
	results []byte
}

func (r *Runtime) intern(kind int, params []byte, results []byte) uint32 {
	if r == nil {
		return 0
	}
	for i := 0; i < len(r.interns); i++ {
		it := r.interns[i]
		if it.kind != kind {
			continue
		}
		if !bytesEq(it.params, params) || !bytesEq(it.results, results) {
			continue
		}
		return uint32(i + 1)
	}
	var rec internRec
	rec.kind = kind
	rec.params = copyBytes(params)
	rec.results = copyBytes(results)
	r.interns = append(r.interns, rec)
	return uint32(len(r.interns))
}

func (r *Runtime) internFunc(params []byte, results []byte) uint32 {
	return r.intern(tyFunc, params, results)
}

func (r *Runtime) internTypeIdx(img *wasmbin.Image, typeIdx int) uint32 {
	if r == nil || img == nil || typeIdx < 0 || typeIdx >= len(img.Types) {
		return 0
	}
	ft := img.Types[typeIdx]
	return r.internFunc(valBytes(ft.Params), valBytes(ft.Results))
}

func bytesEq(a []byte, b []byte) bool {
	if len(a) != len(b) {
		return false
	}
	for i := 0; i < len(a); i++ {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

func copyBytes(s []byte) []byte {
	if len(s) == 0 {
		return nil
	}
	out := make([]byte, len(s))
	for i := 0; i < len(s); i++ {
		out[i] = s[i]
	}
	return out
}

func (r *Runtime) internPrimitives() {
	r.tyEngine = r.intern(tyEngine, nil, nil)
	r.tyCompiled = r.intern(tyCompiled, nil, nil)
	r.tyInstance = r.intern(tyInstance, nil, nil)
	r.tyFunction = r.intern(tyFunction, nil, nil)
	r.tyV128 = r.intern(tyV128, nil, nil)
	r.tyMemory = r.intern(tyMemory, nil, nil)
	r.tyModule = r.intern(tyModule, nil, nil)
	r.tyTable = r.intern(tyTable, nil, nil)
	r.tyGlobal = r.intern(tyGlobal, nil, nil)
}

func (r *Runtime) newV128(lo uint64, hi uint64) uint64 {
	if r == nil || r.cage == nil {
		return 0
	}
	addr := r.cage.Allocate(16, 16)
	if addr == 0 {
		return 0
	}
	r.cage.StoreU64(addr, lo)
	r.cage.StoreU64(addr+8, hi)
	return uint64(r.chpt.Put(addr, int(r.tyV128)))
}

func (r *Runtime) v128Bits(h uint64) (uint64, uint64, bool) {
	if r == nil {
		return 0, 0, false
	}
	addr := r.chpt.Get(uint32(h), int(r.tyV128))
	if addr == 0 || r.cage == nil {
		return 0, 0, false
	}
	lo, ok1 := r.cage.LoadU64(addr)
	hi, ok2 := r.cage.LoadU64(addr + 8)
	if !ok1 || !ok2 {
		return 0, 0, false
	}
	return lo, hi, true
}
