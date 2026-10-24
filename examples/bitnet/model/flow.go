package model

// Flow instrumentation: per token, every BitLinear's currents on the model's cells and the cell sums of
// the vectors between them (hodgemix eval/flow.go and cochain.go, on the device). The cells are
// hodgemix's (hodgemix RUN.yaml --volume MODEL volume.safetensors): cell.D, cell.F, cell.Q and cell.KV
// give every channel's cell on the residual, FFN and attention axes. The kernels are flow.wgsl (exact
// integer currents per row cell and input cell), flowscale.wgsl (to real currents) and cellsum.wgsl.

import (
	"errors"
	"safetensors"
	"strconv"
	"webgpu"
)

// Axes, roles and nodes, in the order of the instrumented buffer.
const (
	axD  = 0
	axF  = 1
	axQ  = 2
	axKV = 3

	roleQ    = 0
	roleK    = 1
	roleV    = 2
	roleO    = 3
	roleGate = 4
	roleUp   = 5
	roleDown = 6
	nRoles   = 7

	nodeD    = 0 // the residual entering the layer
	nodeDm   = 1 // after the attention write
	nodeDout = 2 // after the FFN write
	nodeA    = 3 // the attention output (o's input)
	nodeH    = 4 // the FFN hidden act(gate) * up (down's input)
	nodeQ    = 5
	nodeK    = 6
	nodeV    = 7
	nodeGate = 8
	nodeUp   = 9
	nNodes   = 10
)

var axisNames = []string{"D", "F", "Q", "KV"}
var roleNames = []string{"q", "k", "v", "o", "gate", "up", "down"}
var nodeNames = []string{"D", "Dm", "Dout", "A", "H", "q", "k", "v", "gate", "up"}

// roleRows and roleCols are each role's output and input axes; nodeAxis each node's.
var roleRows = []int{axQ, axKV, axKV, axD, axF, axF, axD}
var roleCols = []int{axD, axD, axD, axQ, axD, axD, axF}
var nodeAxis = []int{axD, axD, axD, axQ, axF, axQ, axKV, axKV, axF, axF}

type flowAxis struct {
	n, cells, per int    // channels, cells, channels a cell
	cell, order   region // channel -> cell (u32); the channels cell-major (u32)
}

type flowState struct {
	ax    []*flowAxis
	acc   region // one BitLinear limb's integer currents
	out   region // every layer's currents, then its node sums
	edge  []int  // byte offset of layer l, role r: edge[l*nRoles+r]
	node  []int  // of layer l, node n: node[l*nNodes+n]
	bytes int
	body  webgpu.Program
}

// u32bytes is v as little-endian words.
func u32bytes(v []uint32) []byte {
	b := make([]byte, 4*len(v))
	for i, x := range v {
		b[4*i] = byte(x)
		b[4*i+1] = byte(x >> 8)
		b[4*i+2] = byte(x >> 16)
		b[4*i+3] = byte(x >> 24)
	}
	return b
}

// cellsOf reads an I32 [n] tensor of a volume file.
func cellsOf(st *safetensors.File, name string, n int) ([]uint32, error) {
	t := st.Get(name)
	if t == nil {
		return nil, errors.New("model: the volume file has no " + name)
	}
	if t.DType != "I32" || int(t.Elements()) != n {
		return nil, errors.New("model: " + name + " is " + t.DType + " of " + itoa(int(t.Elements())) + ", want I32 of " + itoa(n) + ": not this model's cells")
	}
	buf := make([]byte, int(t.Size()))
	r, err := st.Stream(1<<20, func(x *safetensors.Tensor) bool { return x.Name == name })
	if err != nil {
		return nil, err
	}
	for r.Next() {
		copy(buf[int(r.Off):], r.Data)
	}
	if r.Err != nil {
		return nil, r.Err
	}
	out := make([]uint32, n)
	for i := range out {
		out[i] = uint32(buf[4*i]) | uint32(buf[4*i+1])<<8 | uint32(buf[4*i+2])<<16 | uint32(buf[4*i+3])<<24
	}
	return out, nil
}

// newFlowAxis puts an axis' cells on the device; every cell must hold the same number of channels.
func (m *Model) newFlowAxis(name string, cell []uint32) (*flowAxis, error) {
	nc := 0
	for _, c := range cell {
		if int(c)+1 > nc {
			nc = int(c) + 1
		}
	}
	count := make([]int, nc)
	for _, c := range cell {
		count[c]++
	}
	for _, k := range count {
		if k != count[0] || k == 0 {
			return nil, errors.New("model: " + name + ": every cell must hold the same number of channels")
		}
	}
	order := make([]uint32, 0, len(cell))
	for c := 0; c < nc; c++ {
		for j, x := range cell {
			if int(x) == c {
				order = append(order, uint32(j))
			}
		}
	}
	a := &flowAxis{n: len(cell), cells: nc, per: count[0]}
	var err error
	if a.cell, err = m.own(4 * len(cell)); err != nil {
		return nil, err
	}
	if a.order, err = m.own(4 * len(order)); err != nil {
		return nil, err
	}
	if err := m.Dev.Write(a.cell.buf, 0, u32bytes(cell)); err != nil {
		return nil, err
	}
	if err := m.Dev.Write(a.order.buf, 0, u32bytes(order)); err != nil {
		return nil, err
	}
	return a, nil
}

// Instrument reads the cells of volume (hodgemix's volume file for this model) and records the
// instrumented body StepFlow runs.
func (m *Model) Instrument(volume string) error {
	c := m.C
	hd := c.HeadDim
	st, err := safetensors.Open(volume)
	if err != nil {
		return err
	}
	lens := []int{c.Dim, c.FFN, c.Heads * hd, c.KVHeads * hd}
	fl := &flowState{}
	for a := 0; a < 4; a++ {
		cell, err := cellsOf(st, "cell."+axisNames[a], lens[a])
		if err != nil {
			return err
		}
		ax, err := m.newFlowAxis("cell."+axisNames[a], cell)
		if err != nil {
			return err
		}
		fl.ax = append(fl.ax, ax)
	}
	if fl.ax[axQ].cells%c.Heads != 0 || fl.ax[axKV].cells != fl.ax[axQ].cells/(c.Heads/c.KVHeads) {
		return errors.New("model: the attention cells do not keep heads whole")
	}
	align := int(m.Dev.Limit("minStorageBufferOffsetAlignment"))
	if align <= 0 {
		align = 256
	}
	at, most := 0, 0
	for l := 0; l < c.Layers; l++ {
		for r := 0; r < nRoles; r++ {
			n := fl.ax[roleRows[r]].cells * fl.ax[roleCols[r]].cells
			fl.edge = append(fl.edge, at)
			at += (4*n + align - 1) / align * align
			if n > most {
				most = n
			}
		}
		for k := 0; k < nNodes; k++ {
			fl.node = append(fl.node, at)
			at += (4*fl.ax[nodeAxis[k]].cells + align - 1) / align * align
		}
	}
	fl.bytes = at
	if fl.out, err = m.own(at); err != nil {
		return err
	}
	if fl.acc, err = m.own(4 * most); err != nil {
		return err
	}
	if err := m.layers2(&fl.body, fl); err != nil {
		return err
	}
	m.fl = fl
	return nil
}

// capLin captures a BitLinear's currents into layer l's role block: every limb's integer currents
// (flow.wgsl) scaled by the input's and the limb's scales (flowscale.wgsl), limbs summed.
func (m *Model) capLin(fl *flowState, b *webgpu.Program, l int, role int, lin *linear) error {
	if fl == nil {
		return nil
	}
	ra, ca := fl.ax[roleRows[role]], fl.ax[roleCols[role]]
	if lin.n != ra.n || lin.k != ca.n {
		return errors.New("model: " + lin.name + " is " + itoa(lin.k) + " -> " + itoa(lin.n) + ", the cells " + itoa(ca.n) + " -> " + itoa(ra.n))
	}
	n := ra.cells * ca.cells
	out := region{buf: fl.out.buf, off: fl.edge[l*nRoles+role], size: 4 * n}
	acc := region{buf: fl.acc.buf, off: 0, size: 4 * n}
	ws := []region{lin.w}
	ss := []region{lin.s}
	centers := []int{1}
	for _, lb := range lin.limbs {
		if lb.radix < 2 || lb.radix > 4 {
			return errors.New("model: " + lin.name + ": limb radix " + itoa(lb.radix) + " is not 2, 3 or 4")
		}
		ws = append(ws, lb.w)
		ss = append(ss, lb.s)
		centers = append(centers, (lb.radix-1)/2)
	}
	for i := range ws {
		p, err := m.pipe("flow", "K="+itoa(lin.k), "NP="+itoa(lin.n/4), "NC="+itoa(ca.cells), "CENTER="+itoa(centers[i]))
		if err != nil {
			return err
		}
		g, err := m.group(p, m.xq, ws[i], ca.cell, ra.cell, acc)
		if err != nil {
			return err
		}
		m.dispatch(b, p, g, (lin.n/4+7)/8)
		mode := "ACCUM=0"
		if i > 0 {
			mode = "ACCUM=1"
		}
		ps, err := m.pipe("flowscale", "N="+itoa(n), mode)
		if err != nil {
			return err
		}
		gs, err := m.group(ps, acc, m.xs, ss[i], out)
		if err != nil {
			return err
		}
		m.dispatch(b, ps, gs, (n+255)/256)
	}
	return nil
}

// capSum captures the cell sums of a (MODE 1: act(a) * b) on an axis as layer l's node.
func (m *Model) capSum(fl *flowState, b *webgpu.Program, l int, node int, a region, bv region, axis int, mode int) error {
	if fl == nil {
		return nil
	}
	ax := fl.ax[axis]
	p, err := m.pipe("cellsum", "NCELL="+itoa(ax.cells), "M="+itoa(ax.per), "MODE="+itoa(mode), "ACT="+itoa(m.C.Act))
	if err != nil {
		return err
	}
	out := region{buf: fl.out.buf, off: fl.node[l*nNodes+node], size: 4 * ax.cells}
	g, err := m.group(p, a, bv, ax.order, out)
	if err != nil {
		return err
	}
	m.dispatch(b, p, g, (ax.cells+63)/64)
	return nil
}

// Instrumented is whether Instrument has run.
func (m *Model) Instrumented() bool {
	return m.fl != nil
}

// FlowBytes is the size of one token's flow (StepFlow's second result).
func (m *Model) FlowBytes() int {
	if m.fl == nil {
		return 0
	}
	return m.fl.bytes
}

// StepFlow runs one token at pos through the instrumented body and returns the logits after it and
// its flow: every layer's role blocks (row cells x input cells, f32) then node blocks (cell sums),
// each at the offset FlowLayout gives.
func (m *Model) StepFlow(tok int, pos int) ([]byte, []byte, error) {
	fl := m.fl
	if fl == nil {
		return nil, nil, errors.New("model: not instrumented")
	}
	if pos+1 > m.Ctx {
		return nil, nil, errors.New("model: context full (" + itoa(m.Ctx) + " tokens)")
	}
	if tok < 0 || tok >= m.C.Vocab {
		return nil, nil, errors.New("model: token " + itoa(tok) + " is outside the vocabulary")
	}
	p := &m.prog
	p.Reset()
	if err := m.queueEmbed(p, tok, pos); err != nil {
		return nil, nil, err
	}
	p.Append(&fl.body)
	p.Read(fl.out.buf, 0, fl.bytes)
	p.Append(&m.head)
	p.Read(m.logits.buf, 0, 4*m.C.Vocab)
	reply, err := m.Dev.Run(p)
	if err != nil {
		return nil, nil, err
	}
	if len(reply) != fl.bytes+4*m.C.Vocab {
		return nil, nil, errors.New("model: the flow step returned " + itoa(len(reply)) + " bytes, want " + itoa(fl.bytes+4*m.C.Vocab))
	}
	return reply[fl.bytes:], reply[:fl.bytes], nil
}

// FlowLayout is the flow's layout as JSON: the axes (channels, cells), every role's row and input
// axes, every node's axis, and each block's byte offset (edge[l*7+r], node[l*10+n]).
func (m *Model) FlowLayout() string {
	fl := m.fl
	if fl == nil {
		return "null"
	}
	s := `{"bytes":` + itoa(fl.bytes) + `,"layers":` + itoa(m.C.Layers) + `,"axes":{`
	for a, ax := range fl.ax {
		if a > 0 {
			s += ","
		}
		s += `"` + axisNames[a] + `":{"n":` + itoa(ax.n) + `,"cells":` + itoa(ax.cells) + `}`
	}
	s += `},"roles":[`
	for r := 0; r < nRoles; r++ {
		if r > 0 {
			s += ","
		}
		s += `{"name":"` + roleNames[r] + `","rows":"` + axisNames[roleRows[r]] + `","cols":"` + axisNames[roleCols[r]] + `"}`
	}
	s += `],"nodes":[`
	for k := 0; k < nNodes; k++ {
		if k > 0 {
			s += ","
		}
		s += `{"name":"` + nodeNames[k] + `","axis":"` + axisNames[nodeAxis[k]] + `"}`
	}
	s += `],"edge":` + ints(fl.edge) + `,"node":` + ints(fl.node) + `}`
	return s
}

func ints(v []int) string {
	s := "["
	for i, x := range v {
		if i > 0 {
			s += ","
		}
		s += strconv.Itoa(x)
	}
	return s + "]"
}
