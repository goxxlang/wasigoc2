// Package v8bind is a clean-room of WASMv8Bindings' CppHeapPointerTable
// public shape for a wasigocvm guest.
//
// Public shape (WASMv8Bindings include/v8-sandbox.h +
// src/sandbox/cppheap-pointer-table.h, not V8 source):
//   - CppHeapPointerHandle is a small integer, never a raw pointer
//   - Put stores {ptr, tag}; Get returns 0 unless the tag matches
//   - Table storage lives outside the cage; payloads named here may live
//     in WASMSafeSpace pages
//
// This is not a derivative of V8 or WASMv8Bindings source.
package v8bind

const NullHandle = 0
const TagFirst = 1
const TagEngine = 1
const TagCompiled = 2
const TagInstance = 3
const TagFunction = 4

type taggedEnt struct {
	Ptr  int
	Tag  int
	Used bool
}

type CppHeapPointerTable struct {
	ents []taggedEnt
}

func (t *CppHeapPointerTable) Put(ptr int, tag int) uint32 {
	if t == nil {
		return NullHandle
	}
	var e taggedEnt
	e.Ptr = ptr
	e.Tag = tag
	e.Used = true
	t.ents = append(t.ents, e)
	return uint32(len(t.ents))
}

func (t *CppHeapPointerTable) Get(h uint32, tag int) int {
	if t == nil || h == 0 {
		return 0
	}
	i := int(h) - 1
	if i < 0 || i >= len(t.ents) || !t.ents[i].Used {
		return 0
	}
	if t.ents[i].Tag != tag {
		return 0
	}
	return t.ents[i].Ptr
}

func (t *CppHeapPointerTable) Contains(h uint32) bool {
	if t == nil || h == 0 {
		return false
	}
	i := int(h) - 1
	return i >= 0 && i < len(t.ents) && t.ents[i].Used
}
