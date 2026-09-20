// Package wazero is a clean-room of tetratelabs/wazero's interpreter
// embedding API, written in the Go++ subset.
//
// Public shape (wazero Runtime / CompileModule / InstantiateModule /
// HostModuleBuilder / ModuleConfig, plus api.ValueType): interpreter only,
// no wazevo, no golang.org/x/sys, no GOOS files. Decode is
// examples/wasmbinpkg (ported ~/WASMLoader wasmbin). Guest linear memory
// and compiled code pages live in examples/safespacepkg (ported
// ~/WASMSafeSpace). Compiled functions / instances are named through
// examples/v8bindpkg (ported ~/WASMv8Bindings CppHeapPointerTable) and
// safespace EPT/TPT/CPT.
//
// Memory is the WASMSafeSpace cage (one linear Memory). Object types are
// interned (type_key). CHPT names Oilpan payloads in the cage (tag =
// interned type id). TPT names trusted compiled/instance objects. EPT
// names memory GIA and outside resources. Compile and interpret run as
// Oilpan/cppgc jobs (~/WASMv8Bindings PostJob). WASM GC (0xfb) is not
// the heap — cppgc is. Traps are errors, not panics. Core wasm only.
package wazero

import "errors"

const ValueTypeI32 byte = 0x7f
const ValueTypeI64 byte = 0x7e
const ValueTypeF32 byte = 0x7d
const ValueTypeF64 byte = 0x7c
const ValueTypeFuncRef byte = 0x70
const ValueTypeExternRef byte = 0x6f

const ExternTypeFunc byte = 0
const ExternTypeTable byte = 1
const ExternTypeMemory byte = 2
const ExternTypeGlobal byte = 3

func EncodeI32(v int64) uint64 {
	return uint64(uint32(v))
}

func EncodeU32(v uint32) uint64 {
	return uint64(v)
}

func DecodeI32(v uint64) int64 {
	n := int64(v & 4294967295)
	if n >= 2147483648 {
		n = n - 4294967296
	}
	return n
}

func DecodeU32(v uint64) uint32 {
	return uint32(v)
}

func EncodeI64(v int64) uint64 {
	return uint64(v)
}

func EncodeF32(f float32) uint64 {
	return uint64(f32bits(f))
}

func DecodeF32(v uint64) float32 {
	return f32frombits(uint32(v))
}

func EncodeF64(f float64) uint64 {
	return f64bits(f)
}

func DecodeF64(v uint64) float64 {
	return f64frombits(v)
}

func wrap32(v uint64) uint64 {
	return v & 4294967295
}

func asI32(v uint64) int64 {
	return DecodeI32(v)
}

func asI64(v uint64) int64 {
	return int64(v)
}

func boolU32(c bool) uint64 {
	if c {
		return 1
	}
	return 0
}

type ExitError struct {
	Code uint32
}

func (e *ExitError) Error() string {
	return "guest exited with code " + itoa(int(e.Code))
}

func (e *ExitError) ExitCode() uint32 {
	if e == nil {
		return 0
	}
	return e.Code
}

func exitErr(code uint32) error {
	if code == 0 {
		return nil
	}
	return errors.New("guest exited with code " + itoa(int(code)))
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := false
	if n < 0 {
		neg = true
		n = -n
	}
	var d [20]byte
	i := 20
	for n > 0 {
		i--
		d[i] = byte('0' + (n % 10))
		n = n / 10
	}
	if neg {
		i--
		d[i] = '-'
	}
	return string(d[i:])
}
