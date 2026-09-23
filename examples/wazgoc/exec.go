package wazgoc

import (
	"errors"
	"math"
	"math/bits"
)

func (m *Module) invoke(idx int, args []uint64) ([]uint64, error) {
	if m == nil {
		return nil, errors.New("nil module")
	}
	if m.Exited {
		return nil, exitErr(m.ExitCode)
	}
	if idx < 0 {
		return nil, errors.New("bad func idx")
	}
	if idx < len(m.impName) && m.impName[idx] != "" {
		np := m.impNP[idx]
		nr := m.impNR[idx]
		need := np
		if need < nr {
			need = nr
		}
		st := make([]uint64, need)
		for i := 0; i < len(args) && i < np; i++ {
			st[i] = args[i]
		}
		hostDispatch(m, m.impName[idx], st)
		if m.Exited {
			if m.ExitCode == 0 {
				if nr == 0 {
					return nil, nil
				}
				return st[0:nr], nil
			}
			return st[0:nr], exitErr(m.ExitCode)
		}
		if nr == 0 {
			return nil, nil
		}
		return st[0:nr], nil
	}
	if idx >= len(m.funcs) || m.funcs[idx] == nil {
		return nil, errors.New("bad func idx")
	}
	fn := *m.funcs[idx]
	if fn.codeH != 0 && m.rt != nil {
		ep := m.rt.cpt.GetEntrypoint(fn.codeH)
		if ep != 0 && ep != idx+1 {
			return nil, errors.New("code pointer")
		}
	}
	if fn.hostName != "" {
		need := fn.nparams
		if need < fn.nresults {
			need = fn.nresults
		}
		st := make([]uint64, need)
		for i := 0; i < len(args) && i < fn.nparams; i++ {
			st[i] = args[i]
		}
		hostDispatch(m, fn.hostName, st)
		if m.Exited {
			if m.ExitCode == 0 {
				if fn.nresults == 0 {
					return nil, nil
				}
				return st[0:fn.nresults], nil
			}
			return st[0:fn.nresults], exitErr(m.ExitCode)
		}
		if fn.nresults == 0 {
			return nil, nil
		}
		return st[0:fn.nresults], nil
	}
	if fn.peer != nil {
		return fn.peer.invoke(fn.peerIdx, args)
	}
	j := &Job{}
	j.kind = jobExec
	j.mod = m
	j.idx = idx
	j.args = args
	postJob(PrioBlocking, j)
	jobJoin(j)
	return j.results, j.err
}

type rlabel struct {
	height int
	keep   int
	kind   int
	start  int
}

func unwindLabels(labels *[]rlabel, stack *[]uint64, depth int) error {
	fi := len(*labels) - 1 - depth
	if fi < 0 || fi >= len(*labels) {
		return errors.New("br depth")
	}
	lab := (*labels)[fi]
	applyBr(stack, packHB(lab.height, lab.keep))
	*labels = (*labels)[0 : fi+1]
	return nil
}

func (m *Module) execFn(fn compiledFn, args []uint64) ([]uint64, error) {
	if m.depth >= callStackCeiling {
		return nil, errors.New("call stack overflow")
	}
	if len(args) != fn.nparams {
		return nil, errors.New("arity")
	}
	m.depth = m.depth + 1
	locals := make([]uint64, fn.nlocals)
	for i := 0; i < len(args); i++ {
		locals[i] = args[i]
	}
	var stack []uint64
	var labels []rlabel
	labels = append(labels, rlabel{height: 0, keep: fn.nresults, kind: 0})
	body := fn.body
	pc := 0
	for pc < len(body) {
		if m.Exited {
			m.depth = m.depth - 1
			if m.ExitCode == 0 {
				return nil, nil
			}
			return nil, exitErr(m.ExitCode)
		}
		in := body[pc]
		if in.Kind == opUnreachable {
			m.depth = m.depth - 1
			return nil, errors.New("unreachable")
		}
		if in.Kind == opReturn {
			break
		}
		if in.Kind == opLabelPush {
			np := int(in.B)
			ex := int(in.U)
			h := len(stack) - np - ex
			if h < 0 {
				h = 0
			}
			labels = append(labels, rlabel{height: h, keep: int(in.A), kind: 1, start: pc + 1})
			pc = pc + 1
			continue
		}
		if in.Kind == opLabelPop {
			if len(labels) > 1 {
				labels = labels[0 : len(labels)-1]
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opBr {
			if err := unwindLabels(&labels, &stack, int(in.B)); err != nil {
				m.depth = m.depth - 1
				return nil, err
			}
			pc = int(in.A)
			continue
		}
		if in.Kind == opBrIfz {
			if len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("br_ifz underflow")
			}
			c := stack[len(stack)-1]
			stack = stack[0 : len(stack)-1]
			if c == 0 {
				pc = int(in.A)
				continue
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opBrIf {
			if len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("br_if underflow")
			}
			c := stack[len(stack)-1]
			stack = stack[0 : len(stack)-1]
			if c != 0 {
				if err := unwindLabels(&labels, &stack, int(in.B)); err != nil {
					m.depth = m.depth - 1
					return nil, err
				}
				pc = int(in.A)
				continue
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opReturnCall || in.Kind == opReturnCallI {
			got, err := m.execReturnCall(in, &stack)
			m.depth = m.depth - 1
			if err != nil {
				return nil, err
			}
			return got, nil
		}
		if in.Kind == opCall {
			cal := int(in.A)
			np := 0
			nr := 0
			if cal >= 0 && cal < len(m.impName) && m.impName[cal] != "" {
				np = m.impNP[cal]
				nr = m.impNR[cal]
			} else if cal >= 0 && cal < len(m.funcs) && m.funcs[cal] != nil {
				np = m.funcs[cal].nparams
				nr = m.funcs[cal].nresults
			} else {
				m.depth = m.depth - 1
				return nil, errors.New("call nil")
			}
			if len(stack) < np {
				m.depth = m.depth - 1
				return nil, errors.New("call underflow")
			}
			_ = nr
			base := len(stack) - np
			cargs := make([]uint64, np)
			for i := 0; i < np; i++ {
				cargs[i] = stack[base+i]
			}
			stack = stack[0:base]
			got, err := m.invoke(int(in.A), cargs)
			if err != nil {
				m.depth = m.depth - 1
				return nil, err
			}
			for i := 0; i < len(got); i++ {
				stack = append(stack, got[i])
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opCallIndirect {
			if len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("call_indirect underflow")
			}
			off := int(stack[len(stack)-1])
			stack = stack[0 : len(stack)-1]
			callee := 0
			if in.U == 1 {
				if off == 0 {
					m.depth = m.depth - 1
					return nil, errors.New("call_ref null")
				}
				callee = off - 1
			} else {
				tab := m.getTable(int(in.B))
				if off < 0 || off >= len(tab) {
					m.depth = m.depth - 1
					return nil, errors.New("call_indirect oob")
				}
				slot := tab[off]
				if slot == 0 {
					m.depth = m.depth - 1
					return nil, errors.New("call_indirect null")
				}
				callee = int(slot) - 1
			}
			if !m.funcTypeOK(callee, int(in.A)) {
				m.depth = m.depth - 1
				return nil, errors.New("call_indirect type")
			}
			ft := *m.funcs[callee]
			if len(stack) < ft.nparams {
				m.depth = m.depth - 1
				return nil, errors.New("call_indirect arity")
			}
			base := len(stack) - ft.nparams
			cargs := make([]uint64, ft.nparams)
			for i := 0; i < ft.nparams; i++ {
				cargs[i] = stack[base+i]
			}
			stack = stack[0:base]
			got, err := m.invoke(callee, cargs)
			if err != nil {
				m.depth = m.depth - 1
				return nil, err
			}
			for i := 0; i < len(got); i++ {
				stack = append(stack, got[i])
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opDrop {
			if len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("drop underflow")
			}
			stack = stack[0 : len(stack)-1]
			pc = pc + 1
			continue
		}
		if in.Kind == opSelect {
			if len(stack) < 3 {
				m.depth = m.depth - 1
				return nil, errors.New("select underflow")
			}
			c := stack[len(stack)-1]
			v2 := stack[len(stack)-2]
			v1 := stack[len(stack)-3]
			stack = stack[0 : len(stack)-3]
			if c == 0 {
				stack = append(stack, v2)
			} else {
				stack = append(stack, v1)
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opLocalGet {
			i := int(in.A)
			if i < 0 || i >= len(locals) {
				m.depth = m.depth - 1
				return nil, errors.New("local.get")
			}
			stack = append(stack, locals[i])
			pc = pc + 1
			continue
		}
		if in.Kind == opLocalSet {
			i := int(in.A)
			if i < 0 || i >= len(locals) || len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("local.set")
			}
			locals[i] = stack[len(stack)-1]
			stack = stack[0 : len(stack)-1]
			pc = pc + 1
			continue
		}
		if in.Kind == opLocalTee {
			i := int(in.A)
			if i < 0 || i >= len(locals) || len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("local.tee")
			}
			locals[i] = stack[len(stack)-1]
			pc = pc + 1
			continue
		}
		if in.Kind == opGlobalGet {
			i := int(in.A)
			if i < 0 || i >= m.globN {
				m.depth = m.depth - 1
				return nil, errors.New("global.get")
			}
			stack = append(stack, m.loadGlob(i))
			pc = pc + 1
			continue
		}
		if in.Kind == opGlobalSet {
			i := int(in.A)
			if i < 0 || i >= m.globN || len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("global.set")
			}
			m.storeGlob(i, stack[len(stack)-1])
			stack = stack[0 : len(stack)-1]
			pc = pc + 1
			continue
		}
		if in.Kind == opConst {
			stack = append(stack, in.U)
			pc = pc + 1
			continue
		}
		if in.Kind == opLoad {
			if len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("load underflow")
			}
			addr := int(stack[len(stack)-1]) + int(in.B)
			stack = stack[0 : len(stack)-1]
			v, err := m.doLoad(byte(in.A), addr)
			if err != nil {
				m.depth = m.depth - 1
				return nil, err
			}
			stack = append(stack, v)
			pc = pc + 1
			continue
		}
		if in.Kind == opStore {
			if len(stack) < 2 {
				m.depth = m.depth - 1
				return nil, errors.New("store underflow")
			}
			val := stack[len(stack)-1]
			addr := int(stack[len(stack)-2]) + int(in.B)
			stack = stack[0 : len(stack)-2]
			err := m.doStore(byte(in.A), addr, val)
			if err != nil {
				m.depth = m.depth - 1
				return nil, err
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opMemSize {
			stack = append(stack, uint64(m.memPages))
			pc = pc + 1
			continue
		}
		if in.Kind == opMemGrow {
			if len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("memory.grow underflow")
			}
			delta := int(stack[len(stack)-1])
			stack = stack[0 : len(stack)-1]
			old := m.memPages
			maxp := m.memMax
			if maxp <= 0 {
				maxp = 65536
			}
			got := m.growLinearMemory(delta)
			stack = append(stack, uint64(got))
			pc = pc + 1
			continue
		}
		if in.Kind == opMemCopy {
			if len(stack) < 3 {
				m.depth = m.depth - 1
				return nil, errors.New("memory.copy underflow")
			}
			n := int(stack[len(stack)-1])
			src := int(stack[len(stack)-2])
			dst := int(stack[len(stack)-3])
			stack = stack[0 : len(stack)-3]
			buf, ok := m.readBytes(src, n)
			if !ok {
				m.depth = m.depth - 1
				return nil, errors.New("memory.copy oob")
			}
			if !m.writeBytes(dst, buf) {
				m.depth = m.depth - 1
				return nil, errors.New("memory.copy oob")
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opMemFill {
			if len(stack) < 3 {
				m.depth = m.depth - 1
				return nil, errors.New("memory.fill underflow")
			}
			n := int(stack[len(stack)-1])
			val := byte(stack[len(stack)-2])
			dst := int(stack[len(stack)-3])
			stack = stack[0 : len(stack)-3]
			fill := make([]byte, n)
			for i := 0; i < n; i++ {
				fill[i] = val
			}
			if !m.writeBytes(dst, fill) {
				m.depth = m.depth - 1
				return nil, errors.New("memory.fill oob")
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opBrTable {
			if len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("br_table underflow")
			}
			v := int(stack[len(stack)-1])
			stack = stack[0 : len(stack)-1]
			n := int(in.B)
			if n <= 0 {
				m.depth = m.depth - 1
				return nil, errors.New("br_table")
			}
			if v < 0 || v >= n-1 {
				v = n - 1
			}
			slot := int(in.A) + v
			if slot < 0 || slot >= len(fn.brt) {
				m.depth = m.depth - 1
				return nil, errors.New("br_table slot")
			}
			u := uint64(0)
			if slot < len(fn.brtU) {
				u = fn.brtU[slot]
			}
			if err := unwindLabels(&labels, &stack, int(u)); err != nil {
				m.depth = m.depth - 1
				return nil, err
			}
			pc = int(fn.brt[slot])
			continue
		}
		if in.Kind == opThrow || in.Kind == opThrowRef {
			hit := false
			tag := in.A
			for i := len(fn.catches) - 1; i >= 0; i-- {
				ch := fn.catches[i]
				if pc < int(ch.Start) || (ch.End != 0 && pc >= int(ch.End)) {
					continue
				}
				all := ch.Kind == 2 || ch.Kind == 3
				match := ch.Kind == 0 || ch.Kind == 1
				if !all && match && ch.Tag != tag {
					continue
				}
				if !all && !match {
					continue
				}
				if err := unwindLabels(&labels, &stack, int(ch.U)); err != nil {
					m.depth = m.depth - 1
					return nil, err
				}
				pc = int(ch.Target)
				hit = true
				break
			}
			if !hit {
				m.depth = m.depth - 1
				return nil, errors.New("uncaught")
			}
			continue
		}
		if in.Kind == opMemInit {
			if len(stack) < 3 {
				m.depth = m.depth - 1
				return nil, errors.New("memory.init underflow")
			}
			n := int(stack[len(stack)-1])
			src := int(stack[len(stack)-2])
			dst := int(stack[len(stack)-3])
			stack = stack[0 : len(stack)-3]
			di := int(in.A)
			if m.img == nil || di < 0 || di >= len(m.img.Data) {
				m.depth = m.depth - 1
				return nil, errors.New("memory.init")
			}
			if di < len(m.droppedData) && m.droppedData[di] != 0 {
				m.depth = m.depth - 1
				return nil, errors.New("memory.init dropped")
			}
			seg := m.img.Data[di]
			if src < 0 || n < 0 || src+n > len(seg.Data) {
				m.depth = m.depth - 1
				return nil, errors.New("memory.init oob")
			}
			if n > 0 && !m.writeBytes(dst, seg.Data[src:src+n]) {
				m.depth = m.depth - 1
				return nil, errors.New("memory.init oob")
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opDataDrop {
			di := int(in.A)
			if di >= 0 && di < len(m.droppedData) {
				m.droppedData[di] = 1
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opTableInit {
			if len(stack) < 3 {
				m.depth = m.depth - 1
				return nil, errors.New("table.init underflow")
			}
			n := int(stack[len(stack)-1])
			src := int(stack[len(stack)-2])
			dst := int(stack[len(stack)-3])
			stack = stack[0 : len(stack)-3]
			ei := int(in.A)
			ti := int(in.B)
			if m.img == nil || ei < 0 || ei >= len(m.img.Elems) {
				m.depth = m.depth - 1
				return nil, errors.New("table.init")
			}
			if ei < len(m.droppedElem) && m.droppedElem[ei] != 0 {
				m.depth = m.depth - 1
				return nil, errors.New("table.init dropped")
			}
			el := m.img.Elems[ei]
			if src < 0 || n < 0 || src+n > len(el.Funcs) {
				m.depth = m.depth - 1
				return nil, errors.New("table.init oob")
			}
			m.growTableTo(ti, dst+n)
			tab := m.getTable(ti)
			for i := 0; i < n; i++ {
				tab[dst+i] = el.Funcs[src+i] + 1
			}
			m.putTable(ti, tab)
			pc = pc + 1
			continue
		}
		if in.Kind == opElemDrop {
			ei := int(in.A)
			if ei >= 0 && ei < len(m.droppedElem) {
				m.droppedElem[ei] = 1
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opTableCopy {
			if len(stack) < 3 {
				m.depth = m.depth - 1
				return nil, errors.New("table.copy underflow")
			}
			n := int(stack[len(stack)-1])
			src := int(stack[len(stack)-2])
			dst := int(stack[len(stack)-3])
			stack = stack[0 : len(stack)-3]
			dt := m.getTable(int(in.A))
			st := m.getTable(int(in.B))
			if n < 0 || src < 0 || dst < 0 || src+n > len(st) || dst+n > len(dt) {
				m.depth = m.depth - 1
				return nil, errors.New("table.copy oob")
			}
			tmp := make([]uint32, n)
			for i := 0; i < n; i++ {
				tmp[i] = st[src+i]
			}
			for i := 0; i < n; i++ {
				dt[dst+i] = tmp[i]
			}
			m.putTable(int(in.A), dt)
			pc = pc + 1
			continue
		}
		if in.Kind == opTableGrow {
			if len(stack) < 2 {
				m.depth = m.depth - 1
				return nil, errors.New("table.grow underflow")
			}
			delta := int(stack[len(stack)-1])
			val := uint32(stack[len(stack)-2])
			stack = stack[0 : len(stack)-2]
			ti := int(in.A)
			old, ok := m.growTableFill(ti, delta, val)
			if !ok {
				stack = append(stack, wrap32(uint64(^uint32(0))))
			} else {
				stack = append(stack, uint64(old))
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opTableSize {
			tab := m.getTable(int(in.A))
			stack = append(stack, uint64(len(tab)))
			pc = pc + 1
			continue
		}
		if in.Kind == opTableFill {
			if len(stack) < 3 {
				m.depth = m.depth - 1
				return nil, errors.New("table.fill underflow")
			}
			n := int(stack[len(stack)-1])
			val := uint32(stack[len(stack)-2])
			i0 := int(stack[len(stack)-3])
			stack = stack[0 : len(stack)-3]
			tab := m.getTable(int(in.A))
			if i0 < 0 || n < 0 || i0+n > len(tab) {
				m.depth = m.depth - 1
				return nil, errors.New("table.fill oob")
			}
			for i := 0; i < n; i++ {
				tab[i0+i] = val
			}
			m.putTable(int(in.A), tab)
			pc = pc + 1
			continue
		}
		if in.Kind == opTableGet {
			if len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("table.get underflow")
			}
			off := int(stack[len(stack)-1])
			stack = stack[0 : len(stack)-1]
			tab := m.getTable(int(in.A))
			if off < 0 || off >= len(tab) {
				m.depth = m.depth - 1
				return nil, errors.New("table.get oob")
			}
			stack = append(stack, uint64(tab[off]))
			pc = pc + 1
			continue
		}
		if in.Kind == opTableSet {
			if len(stack) < 2 {
				m.depth = m.depth - 1
				return nil, errors.New("table.set underflow")
			}
			val := uint32(stack[len(stack)-1])
			off := int(stack[len(stack)-2])
			stack = stack[0 : len(stack)-2]
			tab := m.getTable(int(in.A))
			if off < 0 || off >= len(tab) {
				m.depth = m.depth - 1
				return nil, errors.New("table.set oob")
			}
			tab[off] = val
			m.putTable(int(in.A), tab)
			pc = pc + 1
			continue
		}
		if in.Kind == opRefNull {
			stack = append(stack, 0)
			pc = pc + 1
			continue
		}
		if in.Kind == opRefIsNull {
			if len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("ref.is_null underflow")
			}
			v := stack[len(stack)-1]
			stack = stack[0 : len(stack)-1]
			stack = append(stack, boolU32(v == 0))
			pc = pc + 1
			continue
		}
		if in.Kind == opRefFunc {
			stack = append(stack, uint64(in.A)+1)
			pc = pc + 1
			continue
		}
		if in.Kind == opRefEq {
			if len(stack) < 2 {
				m.depth = m.depth - 1
				return nil, errors.New("ref.eq underflow")
			}
			b := stack[len(stack)-1]
			a := stack[len(stack)-2]
			stack = stack[0 : len(stack)-2]
			stack = append(stack, boolU32(a == b))
			pc = pc + 1
			continue
		}
		if in.Kind == opRefAsNN {
			if len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("ref.as_non_null underflow")
			}
			if stack[len(stack)-1] == 0 {
				m.depth = m.depth - 1
				return nil, errors.New("null ref")
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opBrOnNull || in.Kind == opBrOnNonNull {
			if len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("br_on_null underflow")
			}
			v := stack[len(stack)-1]
			take := v == 0
			if in.Kind == opBrOnNonNull {
				take = v != 0
			}
			if take {
				if in.Kind == opBrOnNull {
					stack = stack[0 : len(stack)-1]
				}
				if err := unwindLabels(&labels, &stack, int(in.B)); err != nil {
					m.depth = m.depth - 1
					return nil, err
				}
				pc = int(in.A)
				continue
			}
			if in.Kind == opBrOnNonNull {
				stack = stack[0 : len(stack)-1]
			}
			pc = pc + 1
			continue
		}
		if in.Kind == opUnop {
			if len(stack) < 1 {
				m.depth = m.depth - 1
				return nil, errors.New("unop underflow")
			}
			a := stack[len(stack)-1]
			stack = stack[0 : len(stack)-1]
			var v uint64
			var err error
			if in.A >= 0x100 {
				v, err = truncSat(in.A-0x100, a)
			} else {
				v, err = doUnop(byte(in.A), a)
			}
			if err != nil {
				m.depth = m.depth - 1
				return nil, err
			}
			stack = append(stack, v)
			pc = pc + 1
			continue
		}
		if in.Kind == opBinop {
			if len(stack) < 2 {
				m.depth = m.depth - 1
				return nil, errors.New("binop underflow")
			}
			b := stack[len(stack)-1]
			a := stack[len(stack)-2]
			stack = stack[0 : len(stack)-2]
			v, err := doBinop(byte(in.A), a, b)
			if err != nil {
				m.depth = m.depth - 1
				return nil, err
			}
			stack = append(stack, v)
			pc = pc + 1
			continue
		}
		if in.Kind == opAtomic {
			st, err := m.doAtomic(in, stack)
			if err != nil {
				m.depth = m.depth - 1
				return nil, err
			}
			stack = st
			pc = pc + 1
			continue
		}
		if in.Kind == opSimd {
			st, err := m.doSimd(&fn, in, stack)
			if err != nil {
				m.depth = m.depth - 1
				return nil, err
			}
			stack = st
			pc = pc + 1
			continue
		}
		m.depth = m.depth - 1
		return nil, errors.New("bad op")
	}
	if len(stack) < fn.nresults {
		m.depth = m.depth - 1
		return nil, errors.New("missing result")
	}
	out := make([]uint64, fn.nresults)
	base := len(stack) - fn.nresults
	for i := 0; i < fn.nresults; i++ {
		out[i] = stack[base+i]
	}
	m.depth = m.depth - 1
	return out, nil
}

func applyBr(stack *[]uint64, u uint64) {
	keep := unpackK(u)
	base := unpackH(u)
	st := *stack
	if keep < 0 {
		keep = 0
	}
	if base < 0 {
		base = 0
	}
	if keep == 0 {
		if base <= len(st) {
			*stack = st[0:base]
		}
		return
	}
	if keep > len(st) {
		keep = len(st)
	}
	top := make([]uint64, keep)
	src := len(st) - keep
	for i := 0; i < keep; i++ {
		top[i] = st[src+i]
	}
	if base > len(st) {
		base = len(st)
	}
	st = st[0:base]
	for i := 0; i < keep; i++ {
		st = append(st, top[i])
	}
	*stack = st
}

func (m *Module) getTable(idx int) []uint32 {
	if m == nil || idx < 0 || idx >= len(m.tableLen) {
		return nil
	}
	n := m.tableLen[idx]
	out := make([]uint32, n)
	for i := 0; i < n; i++ {
		out[i] = m.loadTab(idx, i)
	}
	return out
}

func (m *Module) putTable(idx int, t []uint32) {
	if m == nil || idx < 0 {
		return
	}
	m.growTableCage(idx, len(t))
	for i := 0; i < len(t); i++ {
		m.storeTab(idx, i, t[i])
	}
}

func (m *Module) funcTypeOK(callee int, typeIdx int) bool {
	if m == nil || callee < 0 || callee >= len(m.funcs) || m.funcs[callee] == nil {
		return false
	}
	fn := m.funcs[callee]
	if m.rt != nil && fn.typeKey != 0 {
		want := m.rt.internTypeIdx(m.img, typeIdx)
		if want != 0 && fn.typeKey == want {
			return true
		}
	}
	if fn.typeIdx == typeIdx {
		return true
	}
	if m.img == nil || typeIdx < 0 || typeIdx >= len(m.img.Types) {
		return false
	}
	ft := m.img.Types[typeIdx]
	if len(fn.params) != len(ft.Params) || len(fn.results) != len(ft.Results) {
		return false
	}
	for i := 0; i < len(fn.params); i++ {
		if fn.params[i] != byte(ft.Params[i]) {
			return false
		}
	}
	for i := 0; i < len(fn.results); i++ {
		if fn.results[i] != byte(ft.Results[i]) {
			return false
		}
	}
	return true
}

func (m *Module) readBytes(addr int, n int) ([]byte, bool) {
	if n < 0 || addr < 0 || m.rt == nil || m.rt.cage == nil {
		return nil, false
	}
	return m.rt.cage.Read(m.memBase+addr, n)
}

func (m *Module) writeBytes(addr int, data []byte) bool {
	if addr < 0 || m.rt == nil || m.rt.cage == nil {
		return false
	}
	return m.rt.cage.Write(m.memBase+addr, data)
}

func (m *Module) doLoad(op byte, addr int) (uint64, error) {
	if op == 0x28 || op == 0x2a {
		if m.rt == nil || m.rt.cage == nil {
			return 0, errors.New("load oob")
		}
		v, ok := m.rt.cage.LoadU32(m.memBase + addr)
		if !ok {
			return 0, errors.New("load oob")
		}
		return uint64(v), nil
	}
	if op == 0x29 || op == 0x2b {
		if m.rt == nil || m.rt.cage == nil {
			return 0, errors.New("load oob")
		}
		v, ok := m.rt.cage.LoadU64(m.memBase + addr)
		if !ok {
			return 0, errors.New("load oob")
		}
		return v, nil
	}
	if op == 0x2c || op == 0x2d || op == 0x30 || op == 0x31 {
		b, ok := m.readBytes(addr, 1)
		if !ok {
			return 0, errors.New("load oob")
		}
		v := uint64(b[0])
		if (op == 0x2c || op == 0x30) && v >= 128 {
			if op == 0x2c {
				v = v | 18446744073709551360
				return wrap32(v), nil
			}
			return v | 18446744073709551488, nil
		}
		return v, nil
	}
	if op == 0x2e || op == 0x2f || op == 0x32 || op == 0x33 {
		b, ok := m.readBytes(addr, 2)
		if !ok {
			return 0, errors.New("load oob")
		}
		v := uint64(b[0]) | uint64(b[1])<<8
		if (op == 0x2e || op == 0x32) && v >= 32768 {
			if op == 0x2e {
				return wrap32(v | 18446744073709518848), nil
			}
			return v | 18446744073709518848, nil
		}
		return v, nil
	}
	if op == 0x34 || op == 0x35 {
		b, ok := m.readBytes(addr, 4)
		if !ok {
			return 0, errors.New("load oob")
		}
		v := uint64(b[0]) | uint64(b[1])<<8 | uint64(b[2])<<16 | uint64(b[3])<<24
		if op == 0x34 && v >= 2147483648 {
			return v | 18446744069414584320, nil
		}
		return v, nil
	}
	return 0, errors.New("load")
}

func (m *Module) doStore(op byte, addr int, val uint64) error {
	if op == 0x36 || op == 0x38 {
		var b [4]byte
		b[0] = byte(val)
		b[1] = byte(val >> 8)
		b[2] = byte(val >> 16)
		b[3] = byte(val >> 24)
		if !m.writeBytes(addr, b[:]) {
			return errors.New("store oob")
		}
		return nil
	}
	if op == 0x37 || op == 0x39 {
		var b [8]byte
		b[0] = byte(val)
		b[1] = byte(val >> 8)
		b[2] = byte(val >> 16)
		b[3] = byte(val >> 24)
		b[4] = byte(val >> 32)
		b[5] = byte(val >> 40)
		b[6] = byte(val >> 48)
		b[7] = byte(val >> 56)
		if !m.writeBytes(addr, b[:]) {
			return errors.New("store oob")
		}
		return nil
	}
	if op == 0x3a || op == 0x3c {
		var b [1]byte
		b[0] = byte(val)
		if !m.writeBytes(addr, b[:]) {
			return errors.New("store oob")
		}
		return nil
	}
	if op == 0x3b || op == 0x3d {
		var b [2]byte
		b[0] = byte(val)
		b[1] = byte(val >> 8)
		if !m.writeBytes(addr, b[:]) {
			return errors.New("store oob")
		}
		return nil
	}
	if op == 0x3e {
		var b [4]byte
		b[0] = byte(val)
		b[1] = byte(val >> 8)
		b[2] = byte(val >> 16)
		b[3] = byte(val >> 24)
		if !m.writeBytes(addr, b[:]) {
			return errors.New("store oob")
		}
		return nil
	}
	return errors.New("store")
}

func doUnop(op byte, a uint64) (uint64, error) {
	if op == 0x45 {
		return boolU32(wrap32(a) == 0), nil
	}
	if op == 0x50 {
		return boolU32(a == 0), nil
	}
	if op == 0x67 {
		return uint64(bits.LeadingZeros32(uint32(a))), nil
	}
	if op == 0x68 {
		return uint64(bits.TrailingZeros32(uint32(a))), nil
	}
	if op == 0x69 {
		return uint64(bits.OnesCount32(uint32(a))), nil
	}
	if op == 0x79 {
		return uint64(bits.LeadingZeros64(a)), nil
	}
	if op == 0x7a {
		return uint64(bits.TrailingZeros64(a)), nil
	}
	if op == 0x7b {
		return uint64(bits.OnesCount64(a)), nil
	}
	if op == 0xa7 {
		return wrap32(a), nil
	}
	if op == 0xac {
		return uint64(asI32(a)), nil
	}
	if op == 0xad {
		return wrap32(a), nil
	}
	if op == 0xc0 {
		v := a & 255
		if v >= 128 {
			return wrap32(v | 18446744073709551360), nil
		}
		return v, nil
	}
	if op == 0xc1 {
		v := a & 65535
		if v >= 32768 {
			return wrap32(v | 18446744073709518848), nil
		}
		return v, nil
	}
	if op == 0xc2 {
		v := a & 255
		if v >= 128 {
			return v | 18446744073709551488, nil
		}
		return v, nil
	}
	if op == 0xc3 {
		v := a & 65535
		if v >= 32768 {
			return v | 18446744073709518848, nil
		}
		return v, nil
	}
	if op == 0xc4 {
		v := a & 4294967295
		if v >= 2147483648 {
			return v | 18446744069414584320, nil
		}
		return v, nil
	}
	if op == 0x8b {
		return EncodeF32(float32(math.Abs(float64(DecodeF32(a))))), nil
	}
	if op == 0x8c {
		return EncodeF32(-DecodeF32(a)), nil
	}
	if op == 0x8d {
		return EncodeF32(float32(math.Sqrt(float64(DecodeF32(a))))), nil
	}
	if op == 0x8e {
		return EncodeF32(float32(fceil(float64(DecodeF32(a))))), nil
	}
	if op == 0x8f {
		return EncodeF32(float32(math.Floor(float64(DecodeF32(a))))), nil
	}
	if op == 0x90 {
		return EncodeF32(float32(math.Trunc(float64(DecodeF32(a))))), nil
	}
	if op == 0x91 {
		return EncodeF32(float32(fnearest(float64(DecodeF32(a))))), nil
	}
	if op == 0x99 {
		return EncodeF64(math.Abs(DecodeF64(a))), nil
	}
	if op == 0x9a {
		return EncodeF64(-DecodeF64(a)), nil
	}
	if op == 0x9b {
		return EncodeF64(math.Sqrt(DecodeF64(a))), nil
	}
	if op == 0x9c {
		return EncodeF64(fceil(DecodeF64(a))), nil
	}
	if op == 0x9d {
		return EncodeF64(math.Floor(DecodeF64(a))), nil
	}
	if op == 0x9e {
		return EncodeF64(math.Trunc(DecodeF64(a))), nil
	}
	if op == 0x9f {
		return EncodeF64(fnearest(DecodeF64(a))), nil
	}
	if op == 0xa8 {
		return truncI32(float64(DecodeF32(a)), true)
	}
	if op == 0xa9 {
		return truncI32(float64(DecodeF32(a)), false)
	}
	if op == 0xaa {
		return truncI32(DecodeF64(a), true)
	}
	if op == 0xab {
		return truncI32(DecodeF64(a), false)
	}
	if op == 0xae {
		return truncI64(float64(DecodeF32(a)), true)
	}
	if op == 0xaf {
		return truncI64(float64(DecodeF32(a)), false)
	}
	if op == 0xb0 {
		return truncI64(DecodeF64(a), true)
	}
	if op == 0xb1 {
		return truncI64(DecodeF64(a), false)
	}
	if op == 0xb2 {
		return EncodeF32(float32(asI32(a))), nil
	}
	if op == 0xb3 {
		return EncodeF32(float32(uint32(a))), nil
	}
	if op == 0xb4 {
		return EncodeF32(float32(int64(a))), nil
	}
	if op == 0xb5 {
		return EncodeF32(float32(a)), nil
	}
	if op == 0xb6 {
		return EncodeF32(float32(DecodeF64(a))), nil
	}
	if op == 0xb7 {
		return EncodeF64(float64(DecodeF32(a))), nil
	}
	if op == 0xb8 {
		return EncodeF64(float64(asI32(a))), nil
	}
	if op == 0xb9 {
		return EncodeF64(float64(uint32(a))), nil
	}
	if op == 0xba {
		return EncodeF64(float64(int64(a))), nil
	}
	if op == 0xbb {
		return EncodeF64(float64(a)), nil
	}
	if op == 0xbc || op == 0xbd || op == 0xbe || op == 0xbf {
		return a, nil
	}
	return a, nil
}

func doBinop(op byte, a uint64, b uint64) (uint64, error) {
	if op == 0x6a {
		return wrap32(a + b), nil
	}
	if op == 0x6b {
		return wrap32(a - b), nil
	}
	if op == 0x6c {
		return wrap32(a * b), nil
	}
	if op == 0x6d {
		if wrap32(b) == 0 {
			return 0, errors.New("i32.div_s by zero")
		}
		ia := asI32(a)
		ib := asI32(b)
		if ia == -2147483648 && ib == -1 {
			return 0, errors.New("i32.div_s overflow")
		}
		return EncodeI32(ia / ib), nil
	}
	if op == 0x6e {
		if wrap32(b) == 0 {
			return 0, errors.New("i32.div_u by zero")
		}
		return wrap32(wrap32(a) / wrap32(b)), nil
	}
	if op == 0x6f {
		if wrap32(b) == 0 {
			return 0, errors.New("i32.rem_s by zero")
		}
		return EncodeI32(asI32(a) % asI32(b)), nil
	}
	if op == 0x70 {
		if wrap32(b) == 0 {
			return 0, errors.New("i32.rem_u by zero")
		}
		return wrap32(wrap32(a) % wrap32(b)), nil
	}
	if op == 0x71 {
		return wrap32(a & b), nil
	}
	if op == 0x72 {
		return wrap32(a | b), nil
	}
	if op == 0x73 {
		return wrap32(a ^ b), nil
	}
	if op == 0x74 {
		return wrap32(wrap32(a) << (b & 31)), nil
	}
	if op == 0x75 {
		return EncodeI32(asI32(a) >> (b & 31)), nil
	}
	if op == 0x76 {
		return wrap32(wrap32(a) >> (b & 31)), nil
	}
	if op == 0x77 {
		return uint64(bits.RotateLeft32(uint32(a), int(b&31))), nil
	}
	if op == 0x78 {
		return uint64(bits.RotateLeft32(uint32(a), -int(b&31))), nil
	}
	if op == 0x7c {
		return a + b, nil
	}
	if op == 0x7d {
		return a - b, nil
	}
	if op == 0x7e {
		return a * b, nil
	}
	if op == 0x7f {
		if b == 0 {
			return 0, errors.New("i64.div_s by zero")
		}
		return uint64(int64(a) / int64(b)), nil
	}
	if op == 0x80 {
		if b == 0 {
			return 0, errors.New("i64.div_u by zero")
		}
		return a / b, nil
	}
	if op == 0x81 {
		if b == 0 {
			return 0, errors.New("i64.rem_s by zero")
		}
		return uint64(int64(a) % int64(b)), nil
	}
	if op == 0x82 {
		if b == 0 {
			return 0, errors.New("i64.rem_u by zero")
		}
		return a % b, nil
	}
	if op == 0x83 {
		return a & b, nil
	}
	if op == 0x84 {
		return a | b, nil
	}
	if op == 0x85 {
		return a ^ b, nil
	}
	if op == 0x86 {
		return a << (b & 63), nil
	}
	if op == 0x87 {
		return uint64(int64(a) >> (b & 63)), nil
	}
	if op == 0x88 {
		return a >> (b & 63), nil
	}
	if op == 0x89 {
		return bits.RotateLeft64(a, int(b&63)), nil
	}
	if op == 0x8a {
		return bits.RotateLeft64(a, -int(b&63)), nil
	}
	if op == 0x46 {
		return boolU32(wrap32(a) == wrap32(b)), nil
	}
	if op == 0x47 {
		return boolU32(wrap32(a) != wrap32(b)), nil
	}
	if op == 0x48 {
		return boolU32(asI32(a) < asI32(b)), nil
	}
	if op == 0x49 {
		return boolU32(wrap32(a) < wrap32(b)), nil
	}
	if op == 0x4a {
		return boolU32(asI32(a) > asI32(b)), nil
	}
	if op == 0x4b {
		return boolU32(wrap32(a) > wrap32(b)), nil
	}
	if op == 0x4c {
		return boolU32(asI32(a) <= asI32(b)), nil
	}
	if op == 0x4d {
		return boolU32(wrap32(a) <= wrap32(b)), nil
	}
	if op == 0x4e {
		return boolU32(asI32(a) >= asI32(b)), nil
	}
	if op == 0x4f {
		return boolU32(wrap32(a) >= wrap32(b)), nil
	}
	if op == 0x51 {
		return boolU32(a == b), nil
	}
	if op == 0x52 {
		return boolU32(a != b), nil
	}
	if op == 0x53 {
		return boolU32(int64(a) < int64(b)), nil
	}
	if op == 0x54 {
		return boolU32(a < b), nil
	}
	if op == 0x55 {
		return boolU32(int64(a) > int64(b)), nil
	}
	if op == 0x56 {
		return boolU32(a > b), nil
	}
	if op == 0x57 {
		return boolU32(int64(a) <= int64(b)), nil
	}
	if op == 0x58 {
		return boolU32(a <= b), nil
	}
	if op == 0x59 {
		return boolU32(int64(a) >= int64(b)), nil
	}
	if op == 0x5a {
		return boolU32(a >= b), nil
	}
	if op == 0x92 {
		return EncodeF32(DecodeF32(a) + DecodeF32(b)), nil
	}
	if op == 0x93 {
		return EncodeF32(DecodeF32(a) - DecodeF32(b)), nil
	}
	if op == 0x94 {
		return EncodeF32(DecodeF32(a) * DecodeF32(b)), nil
	}
	if op == 0x95 {
		return EncodeF32(DecodeF32(a) / DecodeF32(b)), nil
	}
	if op == 0x96 {
		return EncodeF32(float32(math.Min(float64(DecodeF32(a)), float64(DecodeF32(b))))), nil
	}
	if op == 0x97 {
		return EncodeF32(float32(math.Max(float64(DecodeF32(a)), float64(DecodeF32(b))))), nil
	}
	if op == 0x98 {
		return wrap32((wrap32(a) & 2147483647) | (wrap32(b) & 2147483648)), nil
	}
	if op == 0xa0 {
		return EncodeF64(DecodeF64(a) + DecodeF64(b)), nil
	}
	if op == 0xa1 {
		return EncodeF64(DecodeF64(a) - DecodeF64(b)), nil
	}
	if op == 0xa2 {
		return EncodeF64(DecodeF64(a) * DecodeF64(b)), nil
	}
	if op == 0xa3 {
		return EncodeF64(DecodeF64(a) / DecodeF64(b)), nil
	}
	if op == 0xa4 {
		return EncodeF64(math.Min(DecodeF64(a), DecodeF64(b))), nil
	}
	if op == 0xa5 {
		return EncodeF64(math.Max(DecodeF64(a), DecodeF64(b))), nil
	}
	if op == 0xa6 {
		sign := uint64(1) << 63
		return (a & (sign - 1)) | (b & sign), nil
	}
	if op == 0x5b {
		return boolU32(DecodeF32(a) == DecodeF32(b)), nil
	}
	if op == 0x5c {
		return boolU32(DecodeF32(a) != DecodeF32(b)), nil
	}
	if op == 0x5d {
		return boolU32(DecodeF32(a) < DecodeF32(b)), nil
	}
	if op == 0x5e {
		return boolU32(DecodeF32(a) > DecodeF32(b)), nil
	}
	if op == 0x5f {
		return boolU32(DecodeF32(a) <= DecodeF32(b)), nil
	}
	if op == 0x60 {
		return boolU32(DecodeF32(a) >= DecodeF32(b)), nil
	}
	if op == 0x61 {
		return boolU32(DecodeF64(a) == DecodeF64(b)), nil
	}
	if op == 0x62 {
		return boolU32(DecodeF64(a) != DecodeF64(b)), nil
	}
	if op == 0x63 {
		return boolU32(DecodeF64(a) < DecodeF64(b)), nil
	}
	if op == 0x64 {
		return boolU32(DecodeF64(a) > DecodeF64(b)), nil
	}
	if op == 0x65 {
		return boolU32(DecodeF64(a) <= DecodeF64(b)), nil
	}
	if op == 0x66 {
		return boolU32(DecodeF64(a) >= DecodeF64(b)), nil
	}
	return 0, errors.New("binop")
}

func truncSat(kind uint32, a uint64) (uint64, error) {
	var x float64
	if kind == 0 || kind == 1 || kind == 4 || kind == 5 {
		x = float64(DecodeF32(a))
	} else {
		x = DecodeF64(a)
	}
	if math.IsNaN(x) {
		return 0, nil
	}
	if kind == 0 {
		if x >= 2147483647 {
			return EncodeI32(2147483647), nil
		}
		if x <= -2147483648 {
			return EncodeI32(-2147483648), nil
		}
		return EncodeI32(int64(x)), nil
	}
	if kind == 1 {
		if x >= 4294967295 {
			return wrap32(4294967295), nil
		}
		if x <= 0 {
			return 0, nil
		}
		return wrap32(uint64(x)), nil
	}
	if kind == 2 {
		if x >= 2147483647 {
			return EncodeI32(2147483647), nil
		}
		if x <= -2147483648 {
			return EncodeI32(-2147483648), nil
		}
		return EncodeI32(int64(x)), nil
	}
	if kind == 3 {
		if x >= 4294967295 {
			return wrap32(4294967295), nil
		}
		if x <= 0 {
			return 0, nil
		}
		return wrap32(uint64(x)), nil
	}
	if kind == 4 {
		if math.IsInf(x, 1) {
			return uint64(9223372036854775807), nil
		}
		if x <= float64(-9223372036854775808) {
			return uint64(int64(-9223372036854775808)), nil
		}
		return uint64(int64(x)), nil
	}
	if kind == 5 || kind == 7 {
		if x <= 0 {
			return 0, nil
		}
		return uint64(x), nil
	}
	if kind == 6 {
		if math.IsInf(x, 1) {
			return uint64(9223372036854775807), nil
		}
		if x <= float64(-9223372036854775808) {
			return uint64(int64(-9223372036854775808)), nil
		}
		return uint64(int64(x)), nil
	}
	return 0, nil
}

func fceil(x float64) float64 {
	i := int64(x)
	f := float64(i)
	if f < x {
		f = f + 1
	}
	return f
}

func fnearest(x float64) float64 {
	t := math.Trunc(x)
	d := x - t
	if d < 0 {
		d = -d
	}
	if d < 0.5 {
		return t
	}
	if d > 0.5 {
		if x > 0 {
			return t + 1
		}
		return t - 1
	}
	i := int64(t)
	if i-i/2*2 == 0 {
		return t
	}
	if x > 0 {
		return t + 1
	}
	return t - 1
}

func truncI32(x float64, signed bool) (uint64, error) {
	if math.IsNaN(x) || math.IsInf(x, 0) {
		return 0, errors.New("trunc")
	}
	if signed {
		if x >= 2147483648 || x < -2147483648 {
			return 0, errors.New("trunc")
		}
		return EncodeI32(int64(x)), nil
	}
	if x >= 4294967296 || x <= -1 {
		return 0, errors.New("trunc")
	}
	return wrap32(uint64(x)), nil
}

func truncI64(x float64, signed bool) (uint64, error) {
	if math.IsNaN(x) || math.IsInf(x, 0) {
		return 0, errors.New("trunc")
	}
	if signed {
		if x >= 9223372036854775808.0 || x < -9223372036854775808.0 {
			return 0, errors.New("trunc")
		}
		return uint64(int64(x)), nil
	}
	if x >= 18446744073709551616.0 || x <= -1 {
		return 0, errors.New("trunc")
	}
	return uint64(x), nil
}
