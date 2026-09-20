package wazero

import (
	"errors"
	"time"
)

func (m *Module) execReturnCall(in Op, stack *[]uint64) ([]uint64, error) {
	st := *stack
	if in.Kind == opReturnCallI {
		if len(st) < 1 {
			return nil, errors.New("return_call_indirect underflow")
		}
		off := int(st[len(st)-1])
		st = st[0 : len(st)-1]
		tab := m.getTable(int(in.B))
		if off < 0 || off >= len(tab) || tab[off] == 0 {
			return nil, errors.New("return_call_indirect")
		}
		callee := int(tab[off]) - 1
		if !m.funcTypeOK(callee, int(in.A)) {
			return nil, errors.New("return_call_indirect type")
		}
		in.A = uint32(callee)
	}
	cal := int(in.A)
	np := 0
	if cal >= 0 && cal < len(m.impName) && m.impName[cal] != "" {
		np = m.impNP[cal]
	} else if cal >= 0 && cal < len(m.funcs) && m.funcs[cal] != nil {
		np = m.funcs[cal].nparams
	}
	if len(st) < np {
		return nil, errors.New("return_call underflow")
	}
	base := len(st) - np
	cargs := make([]uint64, np)
	for i := 0; i < np; i++ {
		cargs[i] = st[base+i]
	}
	*stack = st[0:base]
	return m.invoke(cal, cargs)
}

func (m *Module) doAtomic(in Op, stack []uint64) ([]uint64, error) {
	sub := in.A
	off := int(in.U)
	if sub == 3 {
		return stack, nil
	}
	if sub == 0 {
		if len(stack) < 2 {
			return nil, errors.New("atomic.notify underflow")
		}
		cnt := uint32(stack[len(stack)-1])
		addr := int(stack[len(stack)-2]) + off
		stack = stack[0 : len(stack)-2]
		n := m.atomicNotify(addr, cnt)
		stack = append(stack, uint64(n))
		return stack, nil
	}
	if sub == 1 || sub == 2 {
		if len(stack) < 3 {
			return nil, errors.New("atomic.wait underflow")
		}
		timeout := int64(stack[len(stack)-1])
		expected := stack[len(stack)-2]
		addr := int(stack[len(stack)-3]) + off
		stack = stack[0 : len(stack)-3]
		wide := sub == 2
		code := m.atomicWait(addr, expected, timeout, wide)
		stack = append(stack, uint64(code))
		return stack, nil
	}
	if sub >= 16 && sub <= 22 {
		if len(stack) < 1 {
			return nil, errors.New("atomic.load underflow")
		}
		addr := int(stack[len(stack)-1]) + off
		stack = stack[0 : len(stack)-1]
		v, err := m.atomicLoad(sub, addr)
		if err != nil {
			return nil, err
		}
		stack = append(stack, v)
		return stack, nil
	}
	if sub >= 23 && sub <= 29 {
		if len(stack) < 2 {
			return nil, errors.New("atomic.store underflow")
		}
		val := stack[len(stack)-1]
		addr := int(stack[len(stack)-2]) + off
		stack = stack[0 : len(stack)-2]
		return stack, m.atomicStore(sub, addr, val)
	}
	if sub >= 48 && sub <= 78 {
		if len(stack) < 3 {
			return nil, errors.New("atomic.cmpxchg underflow")
		}
		rep := stack[len(stack)-1]
		exp := stack[len(stack)-2]
		addr := int(stack[len(stack)-3]) + off
		stack = stack[0 : len(stack)-3]
		old, err := m.atomicCmpxchg(sub, addr, exp, rep)
		if err != nil {
			return nil, err
		}
		stack = append(stack, old)
		return stack, nil
	}
	if len(stack) < 2 {
		return nil, errors.New("atomic.rmw underflow")
	}
	val := stack[len(stack)-1]
	addr := int(stack[len(stack)-2]) + off
	stack = stack[0 : len(stack)-2]
	old, err := m.atomicRmw(sub, addr, val)
	if err != nil {
		return nil, err
	}
	stack = append(stack, old)
	return stack, nil
}

func (m *Module) lockAtom() {
	if m == nil || m.rt == nil {
		return
	}
	for m.rt.alock != 0 {
	}
	m.rt.alock = 1
}

func (m *Module) unlockAtom() {
	if m == nil || m.rt == nil {
		return
	}
	m.rt.alock = 0
}

func (m *Module) atomicWait(addr int, expected uint64, timeout int64, wide bool) uint32 {
	m.lockAtom()
	got, err := m.atomicLoadAt(addr, wide)
	if err != nil {
		m.unlockAtom()
		return 1
	}
	mask := uint64(4294967295)
	if wide {
		mask = ^uint64(0)
	}
	if (got & mask) != (expected & mask) {
		m.unlockAtom()
		return 1
	}
	slot := len(m.rt.waitAddr)
	m.rt.waitAddr = append(m.rt.waitAddr, addr)
	m.rt.waitWake = append(m.rt.waitWake, 0)
	m.unlockAtom()
	start := time.Now().UnixNano()
	for {
		m.lockAtom()
		w := uint32(0)
		if slot < len(m.rt.waitWake) {
			w = m.rt.waitWake[slot]
		}
		m.unlockAtom()
		if w != 0 {
			return 0
		}
		if timeout >= 0 {
			now := time.Now().UnixNano()
			if timeout == 0 || now-start >= timeout {
				return 2
			}
		}
	}
}

func (m *Module) atomicNotify(addr int, count uint32) uint32 {
	if m == nil || m.rt == nil {
		return 0
	}
	m.lockAtom()
	n := uint32(0)
	for i := 0; i < len(m.rt.waitAddr) && n < count; i++ {
		if m.rt.waitAddr[i] == addr && m.rt.waitWake[i] == 0 {
			m.rt.waitWake[i] = 1
			n = n + 1
		}
	}
	m.unlockAtom()
	return n
}

func (m *Module) atomicLoadAt(addr int, wide bool) (uint64, error) {
	n := 4
	if wide {
		n = 8
	}
	b, ok := m.readBytes(addr, n)
	if !ok {
		return 0, errors.New("atomic.wait oob")
	}
	v := uint64(0)
	for i := 0; i < n; i++ {
		v = v | (uint64(b[i]) << uint(i*8))
	}
	return v, nil
}

func (m *Module) atomicLoad(sub uint32, addr int) (uint64, error) {
	m.lockAtom()
	defer m.unlockAtom()

	if sub == 16 || sub == 18 || sub == 19 {
		n := 4
		if sub == 18 {
			n = 1
		} else if sub == 19 {
			n = 2
		}
		b, ok := m.readBytes(addr, n)
		if !ok {
			return 0, errors.New("atomic.load oob")
		}
		v := uint64(0)
		for i := 0; i < n; i++ {
			v = v | (uint64(b[i]) << uint(i*8))
		}
		return wrap32(v), nil
	}
	n := 8
	if sub == 20 {
		n = 1
	} else if sub == 21 {
		n = 2
	} else if sub == 22 {
		n = 4
	}
	b, ok := m.readBytes(addr, n)
	if !ok {
		return 0, errors.New("atomic.load oob")
	}
	v := uint64(0)
	for i := 0; i < n; i++ {
		v = v | (uint64(b[i]) << uint(i*8))
	}
	return v, nil
}

func (m *Module) atomicStore(sub uint32, addr int, val uint64) error {
	m.lockAtom()
	defer m.unlockAtom()

	n := 4
	if sub == 24 {
		n = 8
	} else if sub == 25 || sub == 27 {
		n = 1
	} else if sub == 26 || sub == 28 {
		n = 2
	} else if sub == 29 {
		n = 4
	}
	b := make([]byte, n)
	for i := 0; i < n; i++ {
		b[i] = byte(val >> uint(i*8))
	}
	if !m.writeBytes(addr, b) {
		return errors.New("atomic.store oob")
	}
	return nil
}

func (m *Module) atomicRmw(sub uint32, addr int, val uint64) (uint64, error) {
	m.lockAtom()
	defer m.unlockAtom()

	n := atomicWidth(sub)
	b, ok := m.readBytes(addr, n)
	if !ok {
		return 0, errors.New("atomic.rmw oob")
	}
	old := uint64(0)
	for i := 0; i < n; i++ {
		old = old | (uint64(b[i]) << uint(i*8))
	}
	nv := old
	kind := uint32(0)
	if sub >= 30 {
		kind = (sub - 30) / 7
	}
	if kind == 0 {
		nv = old + val
	} else if kind == 1 {
		nv = old - val
	} else if kind == 2 {
		nv = old & val
	} else if kind == 3 {
		nv = old | val
	} else if kind == 4 {
		nv = old ^ val
	} else {
		nv = val
	}
	out := make([]byte, n)
	for i := 0; i < n; i++ {
		out[i] = byte(nv >> uint(i*8))
	}
	if !m.writeBytes(addr, out) {
		return 0, errors.New("atomic.rmw oob")
	}
	if n <= 4 {
		return wrap32(old), nil
	}
	return old, nil
}

func atomicWidth(sub uint32) int {
	if sub < 30 {
		return 4
	}
	k := (sub - 30) % 7
	if k == 1 {
		return 8
	}
	if k == 2 || k == 4 {
		return 1
	}
	if k == 3 || k == 5 {
		return 2
	}
	return 4
}

func (m *Module) atomicCmpxchg(sub uint32, addr int, exp uint64, rep uint64) (uint64, error) {
	m.lockAtom()
	defer m.unlockAtom()

	n := 4
	if sub == 49 {
		n = 8
	}
	b, ok := m.readBytes(addr, n)
	if !ok {
		return 0, errors.New("atomic.cmpxchg oob")
	}
	old := uint64(0)
	for i := 0; i < n; i++ {
		old = old | (uint64(b[i]) << uint(i*8))
	}
	mask := uint64(4294967295)
	if n == 8 {
		mask = ^uint64(0)
	} else if n == 1 {
		mask = 255
	} else if n == 2 {
		mask = 65535
	}
	if (old & mask) == (exp & mask) {
		out := make([]byte, n)
		for i := 0; i < n; i++ {
			out[i] = byte(rep >> uint(i*8))
		}
		if !m.writeBytes(addr, out) {
			return 0, errors.New("atomic.cmpxchg oob")
		}
	}
	if n <= 4 {
		return wrap32(old), nil
	}
	return old, nil
}
