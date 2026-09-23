package wazgoc

import (
	"../safespacepkg"
	"../wasmbinpkg"
)

// Linear memory and tables live in the WASMSafeSpace cage. Guest
// handles are compressed pointers (Encode = cage-relative uint32, the
// same algorithm as WASMv8Bindings CompressedPointer). Grow tries to
// extend the allocation in place so existing compressed GIAs stay
// valid; if something else sits at the bump tip, we relocate, copy,
// and re-name the object on CHPT / TPT / EPT.

func (m *Module) initGlobals(img *wasmbin.Image) {
	if m == nil || img == nil {
		return
	}
	n := len(img.Globals)
	m.globN = n
	m.globals = make([]uint64, n)
	if n == 0 || m.rt == nil {
		return
	}
	addr, gia, ok := m.rt.growCageObj(0, 0, n*8, int(m.rt.tyGlobal))
	if !ok {
		for i := 0; i < n; i++ {
			m.globals[i] = img.Globals[i].Init
		}
		return
	}
	m.globAddr = addr
	m.globGia = gia
	for i := 0; i < n; i++ {
		m.storeGlob(i, img.Globals[i].Init)
	}
}

func (m *Module) loadGlob(i int) uint64 {
	if m == nil || i < 0 || i >= m.globN {
		return 0
	}
	if m.globAddr != 0 && m.rt != nil && m.rt.cage != nil {
		v, ok := m.rt.cage.LoadU64(m.globAddr + i*8)
		if ok {
			return v
		}
	}
	if i < len(m.globals) {
		return m.globals[i]
	}
	return 0
}

func (m *Module) storeGlob(i int, v uint64) {
	if m == nil || i < 0 || i >= m.globN {
		return
	}
	if i < len(m.globals) {
		m.globals[i] = v
	}
	if m.globAddr != 0 && m.rt != nil && m.rt.cage != nil {
		m.rt.cage.StoreU64(m.globAddr+i*8, v)
	}
}

func (m *Module) growLinearMemory(delta int) uint32 {
	fail := uint32(4294967295)
	if m == nil || m.rt == nil || m.rt.cage == nil || delta < 0 {
		return fail
	}
	old := m.memPages
	maxp := m.memMax
	if maxp <= 0 {
		maxp = 65536
	}
	if old+delta > maxp {
		return fail
	}
	if delta == 0 {
		return uint32(old)
	}
	nsize := (old + delta) * 65536
	addr, gia, ok := m.rt.growCageObj(m.memBase, m.memSize, nsize, int(m.rt.tyMemory))
	if !ok {
		return fail
	}
	m.memBase = addr
	m.memSize = nsize
	m.memPages = old + delta
	m.memGia = gia
	return uint32(old)
}

func (r *Runtime) growCageObj(addr int, oldn int, newn int, tag int) (int, uint32, bool) {
	if r == nil || r.cage == nil || newn < oldn {
		return 0, 0, false
	}
	if addr != 0 && r.cage.GrowTail(addr, oldn, newn) {
		gia, ok := safespace.Encode(r.cage, addr)
		if !ok {
			return 0, 0, false
		}
		return addr, gia, true
	}
	naddr := r.cage.Allocate(newn, 8)
	if naddr == 0 {
		extra := newn
		if extra < 65536 {
			extra = 65536
		}
		if !r.cage.GrowCage(extra) {
			return 0, 0, false
		}
		naddr = r.cage.Allocate(newn, 8)
	}
	if naddr == 0 {
		return 0, 0, false
	}
	if addr != 0 && oldn > 0 {
		buf, ok := r.cage.Read(addr, oldn)
		if ok {
			r.cage.Write(naddr, buf)
		}
	}
	gia, ok := safespace.Encode(r.cage, naddr)
	if !ok {
		return 0, 0, false
	}
	r.chpt.Put(naddr, tag)
	r.tpt.Put(int(gia), tag)
	r.ept.Put(int(gia), safespace.TagExternal)
	return naddr, gia, true
}

func (m *Module) allocTable(idx int, n int) bool {
	if m == nil || m.rt == nil || idx < 0 {
		return false
	}
	for len(m.tableAddr) <= idx {
		m.tableAddr = append(m.tableAddr, 0)
		m.tableLen = append(m.tableLen, 0)
		m.tableCap = append(m.tableCap, 0)
		m.tableGia = append(m.tableGia, 0)
	}
	if n < 0 {
		n = 0
	}
	bytes := n * 4
	if bytes == 0 {
		bytes = 4
	}
	addr, gia, ok := m.rt.growCageObj(0, 0, bytes, int(m.rt.tyTable))
	if !ok {
		return false
	}
	m.tableAddr[idx] = addr
	m.tableCap[idx] = bytes / 4
	m.tableLen[idx] = n
	m.tableGia[idx] = gia
	for i := 0; i < n; i++ {
		m.rt.cage.StoreU32(addr+i*4, 0)
	}
	return true
}

func (m *Module) loadTab(idx int, off int) uint32 {
	if m == nil || m.rt == nil || m.rt.cage == nil {
		return 0
	}
	if idx < 0 || idx >= len(m.tableAddr) || off < 0 || off >= m.tableLen[idx] {
		return 0
	}
	v, ok := m.rt.cage.LoadU32(m.tableAddr[idx] + off*4)
	if !ok {
		return 0
	}
	return v
}

func (m *Module) storeTab(idx int, off int, v uint32) bool {
	if m == nil || m.rt == nil || m.rt.cage == nil {
		return false
	}
	if idx < 0 || idx >= len(m.tableAddr) || off < 0 || off >= m.tableLen[idx] {
		return false
	}
	return m.rt.cage.StoreU32(m.tableAddr[idx]+off*4, v)
}

func (m *Module) growTableCage(idx int, n int) bool {
	if m == nil || idx < 0 {
		return false
	}
	if idx >= len(m.tableAddr) || m.tableAddr[idx] == 0 {
		return m.allocTable(idx, n)
	}
	oldn := m.tableLen[idx]
	cap := m.tableCap[idx]
	if n <= cap {
		m.tableLen[idx] = n
		return true
	}
	addr, gia, ok := m.rt.growCageObj(m.tableAddr[idx], cap*4, n*4, int(m.rt.tyTable))
	if !ok {
		return false
	}
	m.tableAddr[idx] = addr
	m.tableGia[idx] = gia
	m.tableCap[idx] = n
	for i := oldn; i < n; i++ {
		m.rt.cage.StoreU32(addr+i*4, 0)
	}
	m.tableLen[idx] = n
	return true
}

func (m *Module) growTableFill(idx int, delta int, fill uint32) (int, bool) {
	if m == nil || delta < 0 {
		return -1, false
	}
	if idx >= len(m.tableLen) {
		if !m.allocTable(idx, 0) {
			return -1, false
		}
	}
	old := 0
	if idx < len(m.tableLen) {
		old = m.tableLen[idx]
	}
	tmax := 1048576
	if idx < len(m.tableMax) && m.tableMax[idx] > 0 {
		tmax = m.tableMax[idx]
	}
	if old+delta > tmax {
		return -1, false
	}
	if !m.growTableCage(idx, old+delta) {
		return -1, false
	}
	for i := 0; i < delta; i++ {
		m.storeTab(idx, old+i, fill)
	}
	return old, true
}
