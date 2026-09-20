// Package safespace is a clean-room of WASMSafeSpace's public cage API
// for a wasigocvm guest (wasigocvm.bat + toolchain\sysroot).
//
// Public shape (WASMSafeSpace README / src/sandbox, not V8 source):
//   - Sandbox: Initialize / TearDown / base / end / size / Contains /
//     Allocate / current / set_current
//   - SandboxedPointer: encode = offset from base, decode = base +
//     (encoded % size) so every 32-bit value still Contains()
//   - TrustedPointerTable: tag-checked handle to an outside-cage object
//   - CodePointerTable: CFI — indirect calls only land on registered
//     entrypoints (a corrupted in-bounds handle may pick a different
//     still-legitimate entry)
//   - ExternalPointerTable: same tag-checked handle shape for raw
//     external ids
//
// WASMSafeSpace's own wasm32 path does not call OS mmap: the module's
// linear memory already is the process boundary, so the cage is one
// heap allocation. This package does the same mapping — a []byte backing
// store — which is also what wasi-libc's libwasi-emulated-mman is
// (MAP_ANON → heap). Addresses are a disjoint virtual range per cage so
// two sandboxes never overlap, matching Contains() on a native mmap
// reservation.
//
// This is not a derivative of V8 or WASMSafeSpace source.
package safespace

const DefaultSize = 67108864
const NullHandle = 0
const TagTrusted = 1
const TagCode = 2
const TagExternal = 3

var nextBase = 268435456
var current *Sandbox

type Sandbox struct {
	Base int
	End  int
	Size int
	ok   bool
	bump int
	mem  []byte
}

type CodeEnt struct {
	Object     int
	Entrypoint int
	Used       bool
}

type CodePointerTable struct {
	ents []CodeEnt
}

type taggedEnt struct {
	Ptr  int
	Tag  int
	Used bool
}

type TrustedPointerTable struct {
	ents []taggedEnt
}

type ExternalPointerTable struct {
	ents []taggedEnt
}

func New(size int) *Sandbox {
	if size <= 0 {
		size = DefaultSize
	}
	s := &Sandbox{}
	s.Base = nextBase
	s.Size = size
	s.End = s.Base + size
	s.mem = make([]byte, size)
	s.bump = 0
	s.ok = true
	nextBase = nextBase + size
	return s
}

func SetCurrent(s *Sandbox) {
	current = s
}

func Current() *Sandbox {
	return current
}

func (s *Sandbox) TearDown() {
	if s == nil {
		return
	}
	s.ok = false
	s.mem = nil
	s.bump = 0
	s.Size = 0
	s.End = s.Base
	if current == s {
		current = nil
	}
}

func (s *Sandbox) Contains(addr int) bool {
	if s == nil || !s.ok {
		return false
	}
	return addr >= s.Base && addr < s.End
}

func Inside(addr int) bool {
	if current == nil {
		return false
	}
	return current.Contains(addr)
}

func Outside(addr int) bool {
	return !Inside(addr)
}

func (s *Sandbox) Allocate(n int, align int) int {
	if s == nil || !s.ok || n <= 0 {
		return 0
	}
	if align <= 0 {
		align = 8
	}
	off := s.bump
	mis := (s.Base + off) % align
	if mis != 0 {
		off = off + (align - mis)
	}
	if off+n > s.Size {
		return 0
	}
	s.bump = off + n
	return s.Base + off
}

// GrowCage extends the reservation. Compressed offsets (Encode =
// ptr-Base) stay valid — same shape as cppgc caged-heap growth.
func (s *Sandbox) GrowCage(extra int) bool {
	if s == nil || !s.ok || extra <= 0 {
		return extra == 0
	}
	ns := s.Size + extra
	nmem := make([]byte, ns)
	for i := 0; i < s.Size; i++ {
		nmem[i] = s.mem[i]
	}
	s.mem = nmem
	s.Size = ns
	s.End = s.Base + ns
	return true
}

// GrowTail extends the last Allocate if addr+oldn is still the bump tip.
func (s *Sandbox) GrowTail(addr int, oldn int, newn int) bool {
	if s == nil || !s.ok || newn < oldn {
		return false
	}
	if addr+oldn != s.Base+s.bump {
		return false
	}
	extra := newn - oldn
	if extra <= 0 {
		return true
	}
	if s.bump+extra > s.Size {
		if !s.GrowCage(extra) {
			return false
		}
	}
	s.bump = s.bump + extra
	return true
}

func (s *Sandbox) Read(addr int, n int) ([]byte, bool) {
	if s == nil || !s.ok || n < 0 {
		return nil, false
	}
	off := addr - s.Base
	if off < 0 || off+n > s.Size {
		return nil, false
	}
	out := make([]byte, n)
	for i := 0; i < n; i++ {
		out[i] = s.mem[off+i]
	}
	return out, true
}

func (s *Sandbox) LoadU8(addr int) (byte, bool) {
	if s == nil || !s.ok {
		return 0, false
	}
	off := addr - s.Base
	if off < 0 || off >= s.Size {
		return 0, false
	}
	return s.mem[off], true
}

func (s *Sandbox) StoreU8(addr int, v byte) bool {
	if s == nil || !s.ok {
		return false
	}
	off := addr - s.Base
	if off < 0 || off >= s.Size {
		return false
	}
	s.mem[off] = v
	return true
}

func (s *Sandbox) LoadU32(addr int) (uint32, bool) {
	if s == nil || !s.ok {
		return 0, false
	}
	off := addr - s.Base
	if off < 0 || off+4 > s.Size {
		return 0, false
	}
	u := uint32(s.mem[off]) | (uint32(s.mem[off+1]) << 8) | (uint32(s.mem[off+2]) << 16) | (uint32(s.mem[off+3]) << 24)
	return u, true
}

func (s *Sandbox) StoreU32(addr int, v uint32) bool {
	if s == nil || !s.ok {
		return false
	}
	off := addr - s.Base
	if off < 0 || off+4 > s.Size {
		return false
	}
	s.mem[off] = byte(v)
	s.mem[off+1] = byte(v >> 8)
	s.mem[off+2] = byte(v >> 16)
	s.mem[off+3] = byte(v >> 24)
	return true
}

func (s *Sandbox) LoadU64(addr int) (uint64, bool) {
	if s == nil || !s.ok {
		return 0, false
	}
	off := addr - s.Base
	if off < 0 || off+8 > s.Size {
		return 0, false
	}
	lo := uint32(s.mem[off]) | (uint32(s.mem[off+1]) << 8) | (uint32(s.mem[off+2]) << 16) | (uint32(s.mem[off+3]) << 24)
	hi := uint32(s.mem[off+4]) | (uint32(s.mem[off+5]) << 8) | (uint32(s.mem[off+6]) << 16) | (uint32(s.mem[off+7]) << 24)
	return uint64(lo) | (uint64(hi) << 32), true
}

func (s *Sandbox) StoreU64(addr int, v uint64) bool {
	if s == nil || !s.ok {
		return false
	}
	off := addr - s.Base
	if off < 0 || off+8 > s.Size {
		return false
	}
	s.mem[off] = byte(v)
	s.mem[off+1] = byte(v >> 8)
	s.mem[off+2] = byte(v >> 16)
	s.mem[off+3] = byte(v >> 24)
	s.mem[off+4] = byte(v >> 32)
	s.mem[off+5] = byte(v >> 40)
	s.mem[off+6] = byte(v >> 48)
	s.mem[off+7] = byte(v >> 56)
	return true
}

func (s *Sandbox) Write(addr int, data []byte) bool {
	if s == nil || !s.ok {
		return false
	}
	off := addr - s.Base
	if off < 0 || off+len(data) > s.Size {
		return false
	}
	for i := 0; i < len(data); i++ {
		s.mem[off+i] = data[i]
	}
	return true
}

func (s *Sandbox) StoreI32(addr int, v int) bool {
	if s == nil || !s.ok {
		return false
	}
	off := addr - s.Base
	if off < 0 || off+4 > s.Size {
		return false
	}
	u := v
	s.mem[off] = byte(u & 255)
	s.mem[off+1] = byte((u >> 8) & 255)
	s.mem[off+2] = byte((u >> 16) & 255)
	s.mem[off+3] = byte((u >> 24) & 255)
	return true
}

func (s *Sandbox) LoadI32(addr int) (int, bool) {
	if s == nil || !s.ok {
		return 0, false
	}
	off := addr - s.Base
	if off < 0 || off+4 > s.Size {
		return 0, false
	}
	u := int(s.mem[off]) | (int(s.mem[off+1]) << 8) | (int(s.mem[off+2]) << 16) | (int(s.mem[off+3]) << 24)
	if u >= 2147483648 {
		u = u - 4294967296
	}
	return u, true
}

func Encode(s *Sandbox, ptr int) (uint32, bool) {
	if s == nil || !s.Contains(ptr) {
		return 0, false
	}
	return uint32(ptr - s.Base), true
}

func Decode(s *Sandbox, encoded uint32) int {
	if s == nil || !s.ok || s.Size <= 0 {
		return 0
	}
	off := int(encoded) % s.Size
	if off < 0 {
		off = off + s.Size
	}
	return s.Base + off
}

func (t *CodePointerTable) Register(codeObject int, entrypoint int) uint32 {
	if t == nil {
		return NullHandle
	}
	var e CodeEnt
	e.Object = codeObject
	e.Entrypoint = entrypoint
	e.Used = true
	t.ents = append(t.ents, e)
	return uint32(len(t.ents))
}

func (t *CodePointerTable) GetEntrypoint(h uint32) int {
	if t == nil || h == 0 {
		return 0
	}
	i := int(h) - 1
	if i < 0 || i >= len(t.ents) || !t.ents[i].Used {
		return 0
	}
	return t.ents[i].Entrypoint
}

func (t *CodePointerTable) GetCodeObject(h uint32) int {
	if t == nil || h == 0 {
		return 0
	}
	i := int(h) - 1
	if i < 0 || i >= len(t.ents) || !t.ents[i].Used {
		return 0
	}
	return t.ents[i].Object
}

func (t *CodePointerTable) SetEntrypoint(h uint32, entrypoint int) bool {
	if t == nil || h == 0 {
		return false
	}
	i := int(h) - 1
	if i < 0 || i >= len(t.ents) || !t.ents[i].Used {
		return false
	}
	t.ents[i].Entrypoint = entrypoint
	return true
}

func (t *CodePointerTable) Contains(h uint32) bool {
	if t == nil || h == 0 {
		return false
	}
	i := int(h) - 1
	return i >= 0 && i < len(t.ents) && t.ents[i].Used
}

func (t *TrustedPointerTable) Put(ptr int, tag int) uint32 {
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

func (t *TrustedPointerTable) Get(h uint32, tag int) int {
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

func (t *ExternalPointerTable) Put(ptr int, tag int) uint32 {
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

func (t *ExternalPointerTable) Get(h uint32, tag int) int {
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
