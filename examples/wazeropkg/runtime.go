package wazero

import (
	"errors"
	"fmt"
	"io"

	"../safespacepkg"
	"../v8bindpkg"
	"../wasmbinpkg"
)

const callStackCeiling = 2000

type printWriter struct{}

func (p printWriter) Write(b []byte) (int, error) {
	fmt.Print(string(b))
	return len(b), nil
}

type hostFunc struct {
	name    string
	params  []byte
	results []byte
}

type hostModule struct {
	name string
	fns  []hostFunc
}

type CompiledModule struct {
	img   *wasmbin.Image
	funcs []*compiledFn
	name  string
	h     uint32
}

type compiledFn struct {
	body     []Op
	nlocals  int
	nparams  int
	nresults int
	params   []byte
	results  []byte
	hostName string
	name     string
	peer     *Module
	peerIdx  int
	typeIdx  int
	typeKey  uint32
	h        uint32
	codeH    uint32
	table    []uint32
	catches  []Catch
	brt      []uint32
	brtU     []uint64
	simdc    []uint64
}

type Runtime struct {
	cage    *safespace.Sandbox
	cpt     safespace.CodePointerTable
	tpt     safespace.TrustedPointerTable
	ept     safespace.ExternalPointerTable
	chpt    v8bind.CppHeapPointerTable
	interns []internRec
	tyEngine   uint32
	tyCompiled uint32
	tyInstance uint32
	tyFunction uint32
	tyV128     uint32
	tyMemory   uint32
	tyModule   uint32
	tyTable    uint32
	tyGlobal   uint32
	alock   uint32
	waitAddr []int
	waitWake []uint32
	modules map[string]*Module
	hosts   []*hostModule
	stdout  io.Writer
	stderr  io.Writer
	stdin   io.Reader
	closed  bool
}

type ModuleConfig struct {
	name    string
	stdout  io.Writer
	stderr  io.Writer
	stdin   io.Reader
	args    []string
	env     []envPair
	start   []string
	noStart bool
	hasStart bool
}

type Module struct {
	rt       *Runtime
	name     string
	img      *wasmbin.Image
	funcs    []*compiledFn
	globals  []uint64
	globAddr int
	globN    int
	globGia  uint32
	tables   [][]uint32
	tableAddr []int
	tableLen  []int
	tableCap  []int
	tableGia  []uint32
	droppedData []byte
	droppedElem []byte
	memBase  int
	memSize  int
	memPages int
	memGia   uint32
	memLinked byte
	memMax    int
	tableMax  []int
	instGia  uint32
	stdout   io.Writer
	stderr   io.Writer
	stdin    io.Reader
	args     []string
	env      []envPair
	Exited   bool
	ExitCode uint32
	h        uint32
	depth    int
	impName  []string
	impNP    []int
	impNR    []int
	boundN   int
}

type envPair struct {
	key string
	val string
}

type Function struct {
	mod    *Module
	idx    int
	params []byte
	results []byte
}

type Memory struct {
	mod *Module
}

func NewRuntime() *Runtime {
	r := &Runtime{}
	r.cage = safespace.New(safespace.DefaultSize)
	safespace.SetCurrent(r.cage)
	r.modules = make(map[string]*Module)
	r.stdout = printWriter{}
	r.stderr = printWriter{}
	r.internPrimitives()
	r.chpt.Put(1, int(r.tyEngine))
	instantiateDefaultHosts(r)
	return r
}

func (r *Runtime) SetStdout(w io.Writer) {
	if w == nil {
		r.stdout = printWriter{}
		return
	}
	r.stdout = w
}

func (r *Runtime) SetStderr(w io.Writer) {
	if w == nil {
		r.stderr = printWriter{}
		return
	}
	r.stderr = w
}

func (r *Runtime) SetStdin(rd io.Reader) {
	r.stdin = rd
}

func (r *Runtime) Close() error {
	if r == nil {
		return nil
	}
	r.closed = true
	r.modules = nil
	if r.cage != nil {
		r.cage.TearDown()
		r.cage = nil
	}
	return nil
}

func (r *Runtime) GetModule(name string) *Module {
	if r == nil {
		return nil
	}
	return r.modules[name]
}

func NewModuleConfig() *ModuleConfig {
	c := &ModuleConfig{}
	return c
}

func (c *ModuleConfig) WithName(name string) *ModuleConfig {
	c.name = name
	return c
}

func (c *ModuleConfig) WithStdout(w io.Writer) *ModuleConfig {
	c.stdout = w
	return c
}

func (c *ModuleConfig) WithStderr(w io.Writer) *ModuleConfig {
	c.stderr = w
	return c
}

func (c *ModuleConfig) WithStdin(rd io.Reader) *ModuleConfig {
	c.stdin = rd
	return c
}

func (c *ModuleConfig) WithArgs(args ...string) *ModuleConfig {
	c.args = args
	return c
}

func (c *ModuleConfig) WithEnv(key string, val string) *ModuleConfig {
	var kv envPair
	kv.key = key
	kv.val = val
	c.env = append(c.env, kv)
	return c
}

func (c *ModuleConfig) WithStartFunctions(names ...string) *ModuleConfig {
	c.start = names
	c.hasStart = true
	if len(names) == 0 {
		c.noStart = true
	}
	return c
}

func (r *Runtime) NewHostModuleBuilder(name string) *HostModuleBuilder {
	b := &HostModuleBuilder{}
	b.rt = r
	b.name = name
	return b
}

type HostModuleBuilder struct {
	rt   *Runtime
	name string
	fns  []hostFunc
}

func (b *HostModuleBuilder) ExportFunction(name string, params []byte, results []byte) *HostModuleBuilder {
	var h hostFunc
	h.name = name
	h.params = params
	h.results = results
	b.fns = append(b.fns, h)
	return b
}

func (b *HostModuleBuilder) Instantiate() error {
	if b == nil || b.rt == nil {
		return errors.New("nil host builder")
	}
	hm := &hostModule{}
	hm.name = b.name
	hm.fns = b.fns
	b.rt.hosts = append(b.rt.hosts, hm)
	return nil
}

func (r *Runtime) CompileModule(bin []byte) (*CompiledModule, error) {
	if r == nil || r.closed {
		return nil, errors.New("runtime closed")
	}
	core := wasmbin.GuestCore(bin)
	img, err := wasmbin.Parse(core)
	if err != nil {
		return nil, err
	}
	nimp := img.NImpFunc()
	funcs := make([]*compiledFn, nimp+len(img.Bodies))
	for i := 0; i < len(funcs); i++ {
		funcs[i] = new(compiledFn)
	}
	jobs := make([]*Job, len(img.Bodies))
	for i := 0; i < len(img.Bodies); i++ {
		j := &Job{}
		j.kind = jobCompile
		j.img = img
		j.idx = i
		j.slot = funcs[nimp+i]
		jobs[i] = postJob(PrioBlocking, j)
	}
	for i := 0; i < len(jobs); i++ {
		jobJoin(jobs[i])
		if jobs[i].err != nil {
			return nil, jobs[i].err
		}
		if r.cage != nil && i < len(img.Bodies) {
			addr := r.cage.Allocate(len(img.Bodies[i].Code)+1, 8)
			if addr != 0 {
				r.cage.Write(addr, img.Bodies[i].Code)
				h := r.cpt.Register(addr, nimp+i+1)
				if jobs[i].slot != nil {
					jobs[i].slot.codeH = h
				}
			}
		}
	}
	for i := 0; i < len(funcs); i++ {
		if funcs[i] == nil {
			continue
		}
		funcs[i].typeKey = r.internFunc(funcs[i].params, funcs[i].results)
		if i < len(img.FuncNames) {
			funcs[i].name = img.FuncNames[i]
		}
		funcs[i].h = r.chpt.Put(i+1, int(funcs[i].typeKey))
		r.tpt.Put(int(funcs[i].h), int(funcs[i].typeKey))
	}
	cm := &CompiledModule{}
	cm.img = img
	cm.funcs = funcs
	cm.name = img.ModuleName
	cm.h = r.chpt.Put(len(funcs)+1, int(r.tyCompiled))
	r.tpt.Put(int(cm.h), int(r.tyCompiled))
	return cm, nil
}

func compileOne(img *wasmbin.Image, i int) (compiledFn, error) {
	var z compiledFn
	if img == nil || i < 0 || i >= len(img.Bodies) {
		return z, errors.New("compileOne")
	}
	if i >= len(img.FuncTypes) {
		return z, errors.New("bad type idx")
	}
	ti := img.FuncTypes[i]
	if int(ti) >= len(img.Types) {
		return z, errors.New("bad type idx")
	}
	ft := img.Types[ti]
	ops, catches, brt, brtU, simdc, err := compileBody(img, img.Bodies[i].Code, len(ft.Params), len(ft.Results))
	if err != nil {
		return z, err
	}
	z.body = ops
	z.catches = catches
	z.brt = brt
	z.brtU = brtU
	z.simdc = simdc
	z.nparams = len(ft.Params)
	z.nresults = len(ft.Results)
	z.nlocals = len(ft.Params) + int(img.Bodies[i].Locals)
	z.params = valBytes(ft.Params)
	z.results = valBytes(ft.Results)
	z.typeIdx = int(ti)
	return z, nil
}

func valBytes(ts []wasmbin.ValType) []byte {
	out := make([]byte, len(ts))
	for i := 0; i < len(ts); i++ {
		out[i] = byte(ts[i])
	}
	return out
}

func (c *CompiledModule) ExportedFunctions() map[string]bool {
	out := make(map[string]bool)
	if c == nil || c.img == nil {
		return out
	}
	for i := 0; i < len(c.img.Exports); i++ {
		e := c.img.Exports[i]
		if e.Knd == wasmbin.KindFunc {
			out[e.Name] = true
		}
	}
	return out
}

func (c *CompiledModule) ImportedFunctions() []wasmbin.Import {
	if c == nil || c.img == nil {
		return nil
	}
	var out []wasmbin.Import
	for i := 0; i < len(c.img.Imports); i++ {
		if c.img.Imports[i].Knd == wasmbin.KindFunc {
			out = append(out, c.img.Imports[i])
		}
	}
	return out
}

func (c *CompiledModule) Close() error {
	return nil
}

func (r *Runtime) InstantiateModule(compiled *CompiledModule, cfg *ModuleConfig) (*Module, error) {
	if r == nil || r.closed {
		return nil, errors.New("runtime closed")
	}
	if compiled == nil || compiled.img == nil {
		return nil, errors.New("nil compiled module")
	}
	if cfg == nil {
		cfg = NewModuleConfig()
	}
	name := cfg.name
	if name == "" {
		name = compiled.name
	}
	if name == "" {
		name = "main"
	}
	if _, exists := r.modules[name]; exists {
		return nil, errors.New("module " + name + " already loaded")
	}
	img := compiled.img
	m := &Module{}
	m.rt = r
	m.name = name
	m.img = img
	m.funcs = compiled.funcs
	m.stdout = cfg.stdout
	if m.stdout == nil {
		m.stdout = r.stdout
	}
	m.stderr = cfg.stderr
	if m.stderr == nil {
		m.stderr = r.stderr
	}
	m.stdin = cfg.stdin
	if m.stdin == nil {
		m.stdin = r.stdin
	}
	m.args = cfg.args
	m.env = cfg.env
	if err := r.bindImports(m, img); err != nil {
		return nil, err
	}
	if m.boundN == 0 && img.NImpFunc() > 0 {
		return nil, errors.New("boundN=0 nimp=" + itoa(img.NImpFunc()) + " nimports=" + itoa(len(img.Imports)))
	}
	if !memoryImported(img) {
		if err := m.initMemory(img); err != nil {
			return nil, err
		}
	}
	m.initGlobals(img)
	m.tableMax = make([]int, len(img.Tables))
	for i := 0; i < len(img.Tables); i++ {
		min := int(img.Tables[i].Min)
		if img.Tables[i].HasMax {
			m.tableMax[i] = int(img.Tables[i].Max)
		} else {
			m.tableMax[i] = 1048576
		}
		m.allocTable(i, min)
	}
	if len(img.Memories) > 0 && img.Memories[0].HasMax {
		m.memMax = int(img.Memories[0].Max)
	} else {
		m.memMax = 65536
	}
	m.droppedData = make([]byte, len(img.Data))
	m.droppedElem = make([]byte, len(img.Elems))
	for i := 0; i < len(img.Elems); i++ {
		el := img.Elems[i]
		if el.Flags&1 != 0 {
			continue
		}
		ti := int(el.Table)
		m.growTableTo(ti, int(el.Off)+len(el.Funcs))
		for j := 0; j < len(el.Funcs); j++ {
			at := int(el.Off) + j
			if el.Funcs[j] == 0xffffffff {
				m.storeTab(ti, at, 0)
			} else {
				m.storeTab(ti, at, el.Funcs[j]+1)
			}
		}
	}
	r.bindPeerResources(m, img)
	if m.memBase == 0 && (memoryImported(img) || len(img.Memories) > 0 || len(img.Data) > 0) {
		if err := m.initMemory(img); err != nil {
			return nil, err
		}
	}
	ch := r.chpt.Put(1, int(r.tyInstance))
	m.h = r.tpt.Put(int(ch), int(r.tyInstance))
	m.instGia = ch
	r.ept.Put(int(m.h), safespace.TagExternal)
	r.modules[name] = m

	if cfg.noStart {
		return m, nil
	}
	if cfg.hasStart {
		for i := 0; i < len(cfg.start); i++ {
			_, err := m.Call(cfg.start[i])
			if err != nil {
				return m, err
			}
		}
		return m, nil
	}
	entry := startExport(img)
	if entry != "" {
		_, err := m.Call(entry)
		if err != nil {
			return m, err
		}
	} else if img.HasStart {
		_, err := m.invoke(int(img.Start), nil)
		if err != nil {
			return m, err
		}
	}
	return m, nil
}

func startExport(img *wasmbin.Image) string {
	if img == nil {
		return ""
	}
	if img.HasExport("_start") {
		return "_start"
	}
	if img.HasExport("run") {
		return "run"
	}
	if img.HasExport("main") {
		return "main"
	}
	for i := 0; i < len(img.Exports); i++ {
		e := img.Exports[i]
		if e.Knd != wasmbin.KindFunc {
			continue
		}
		n := e.Name
		if len(n) >= 4 && n[len(n)-4:] == "#run" {
			return n
		}
	}
	return ""
}

func (r *Runtime) bindImports(m *Module, img *wasmbin.Image) error {
	nimp := img.NImpFunc()
	m.impName = make([]string, nimp)
	m.impNP = make([]int, nimp)
	m.impNR = make([]int, nimp)
	n := 0
	for i := 0; i < len(img.Imports); i++ {
		imp := img.Imports[i]
		if imp.Knd != wasmbin.KindFunc {
			continue
		}
		idx := n
		n++
		fn := new(compiledFn)
		if idx >= 0 && idx < len(m.funcs) && m.funcs[idx] != nil {
			*fn = *m.funcs[idx]
		}
		bound := false
		for h := 0; h < len(r.hosts); h++ {
			hm := r.hosts[h]
			if hm.name != imp.Module {
				continue
			}
			for f := 0; f < len(hm.fns); f++ {
				if hm.fns[f].name != imp.Name {
					continue
				}
				fn.hostName = hm.name + "." + hm.fns[f].name
				fn.nparams = len(hm.fns[f].params)
				fn.nresults = len(hm.fns[f].results)
				fn.params = hm.fns[f].params
				fn.results = hm.fns[f].results
				fn.typeIdx = int(imp.TypeIdx)
				bound = true
				break
			}
		}
		if !bound {
			peer := r.GetModule(imp.Module)
			if peer != nil {
				for e := 0; e < len(peer.img.Exports); e++ {
					ex := peer.img.Exports[e]
					if ex.Name == imp.Name && ex.Knd == wasmbin.KindFunc {
						fn.peer = peer
						fn.peerIdx = int(ex.Index)
						if int(imp.TypeIdx) < len(img.Types) {
							ft := img.Types[imp.TypeIdx]
							fn.nparams = len(ft.Params)
							fn.nresults = len(ft.Results)
							fn.params = valBytes(ft.Params)
							fn.results = valBytes(ft.Results)
						}
						fn.typeIdx = int(imp.TypeIdx)
						bound = true
						break
					}
				}
			}
		}
		if !bound {
			fn.hostName = imp.Module + "." + imp.Name
			if int(imp.TypeIdx) < len(img.Types) {
				ft := img.Types[imp.TypeIdx]
				fn.nparams = len(ft.Params)
				fn.nresults = len(ft.Results)
				fn.params = valBytes(ft.Params)
				fn.results = valBytes(ft.Results)
			}
			fn.typeIdx = int(imp.TypeIdx)
		}
		fn.typeKey = r.internFunc(fn.params, fn.results)
		fn.h = r.chpt.Put(idx+1, int(fn.typeKey))
		r.tpt.Put(int(fn.h), int(fn.typeKey))
		m.funcs = replaceFuncs(m.funcs, idx, fn)
		if idx >= 0 && idx < len(m.impName) {
			ns := make([]string, len(m.impName))
			np := make([]int, len(m.impNP))
			nr := make([]int, len(m.impNR))
			for j := 0; j < len(ns); j++ {
				ns[j] = m.impName[j]
				np[j] = m.impNP[j]
				nr[j] = m.impNR[j]
			}
			ns[idx] = fn.hostName
			np[idx] = fn.nparams
			nr[idx] = fn.nresults
			m.impName = ns
			m.impNP = np
			m.impNR = nr
		}
	}
	m.boundN = n
	return nil
}

func memoryImported(img *wasmbin.Image) bool {
	if img == nil {
		return false
	}
	for i := 0; i < len(img.Imports); i++ {
		if img.Imports[i].Knd == wasmbin.KindMemory {
			return true
		}
	}
	return false
}

func (r *Runtime) bindPeerResources(m *Module, img *wasmbin.Image) {
	if r == nil || m == nil || img == nil {
		return
	}
	gi := 0
	ti := 0
	mi := 0
	for i := 0; i < len(img.Imports); i++ {
		imp := img.Imports[i]
		peer := r.GetModule(imp.Module)
		if peer == nil {
			if imp.Knd == wasmbin.KindGlobal {
				gi++
			} else if imp.Knd == wasmbin.KindTable {
				ti++
			} else if imp.Knd == wasmbin.KindMemory {
				mi++
			}
			continue
		}
		if imp.Knd == wasmbin.KindMemory {
			m.memBase = peer.memBase
			m.memSize = peer.memSize
			m.memPages = peer.memPages
			m.memGia = peer.memGia
			m.memLinked = 1
			mi++
			continue
		}
		if imp.Knd == wasmbin.KindTable {
			if ti < len(peer.tableLen) {
				n := peer.tableLen[ti]
				m.growTableTo(ti, n)
				for j := 0; j < n; j++ {
					m.storeTab(ti, j, peer.loadTab(ti, j))
				}
			}
			ti++
			continue
		}
		if imp.Knd == wasmbin.KindGlobal {
			if gi < peer.globN {
				if gi >= m.globN {
					m.globN = gi + 1
				}
				m.storeGlob(gi, peer.loadGlob(gi))
			}
			gi++
		}
	}
}

func replaceFuncs(old []*compiledFn, idx int, fn *compiledFn) []*compiledFn {
	n := len(old)
	if idx < 0 {
		return old
	}
	if idx >= n {
		ns := make([]*compiledFn, idx+1)
		for i := 0; i < n; i++ {
			ns[i] = old[i]
		}
		ns[idx] = fn
		return ns
	}
	ns := make([]*compiledFn, n)
	for i := 0; i < n; i++ {
		ns[i] = old[i]
	}
	ns[idx] = fn
	return ns
}

func (m *Module) initMemory(img *wasmbin.Image) error {
	if m.memLinked != 0 && m.memBase != 0 {
		return m.writeActiveData(img, m.memSize)
	}
	pages := 0
	if len(img.Memories) > 0 {
		pages = int(img.Memories[0].Min)
	}
	if pages <= 0 && len(img.Data) > 0 {
		pages = 1
	}
	if pages <= 0 {
		return nil
	}
	size := pages * 65536
	m.memPages = pages
	m.memSize = size
	if m.rt == nil || m.rt.cage == nil {
		return errors.New("no WASMSafeSpace cage")
	}
	addr := m.rt.cage.Allocate(size, 8)
	if addr == 0 {
		return errors.New("cage full")
	}
	m.memBase = addr
	gia, ok := safespace.Encode(m.rt.cage, addr)
	if !ok {
		return errors.New("gia encode")
	}
	m.memGia = gia
	m.rt.ept.Put(int(gia), safespace.TagExternal)
	m.rt.chpt.Put(m.memBase, int(m.rt.tyMemory))
	m.rt.tpt.Put(int(gia), int(m.rt.tyMemory))
	return m.writeActiveData(img, size)
}

func (m *Module) writeActiveData(img *wasmbin.Image, size int) error {
	if m == nil || m.rt == nil || m.rt.cage == nil || img == nil {
		return nil
	}
	addr := m.memBase
	for i := 0; i < len(img.Data); i++ {
		d := img.Data[i]
		if d.Flags == 1 {
			continue
		}
		if d.Off < 0 || d.Off+len(d.Data) > size {
			return errors.New("data out of bounds")
		}
		if !m.rt.cage.Write(addr+d.Off, d.Data) {
			return errors.New("data write")
		}
	}
	return nil
}

func (m *Module) growTableTo(idx int, n int) {
	if m == nil || idx < 0 {
		return
	}
	m.growTableCage(idx, n)
}

func (m *Module) Name() string {
	if m == nil {
		return ""
	}
	return m.name
}

func (m *Module) ExportedTable(name string) []uint32 {
	if m == nil || m.img == nil {
		return nil
	}
	for i := 0; i < len(m.img.Exports); i++ {
		e := m.img.Exports[i]
		if e.Name == name && e.Knd == wasmbin.KindTable {
			return m.getTable(int(e.Index))
		}
	}
	return m.getTable(0)
}

func (m *Module) ExportedGlobal(name string) uint64 {
	if m == nil || m.img == nil {
		return 0
	}
	for i := 0; i < len(m.img.Exports); i++ {
		e := m.img.Exports[i]
		if e.Name == name && e.Knd == wasmbin.KindGlobal {
			idx := int(e.Index)
			if idx >= 0 && idx < m.globN {
				return m.loadGlob(idx)
			}
		}
	}
	return 0
}

func (m *Module) ExportedMemory(name string) *Memory {
	if m == nil || m.img == nil {
		return nil
	}
	for i := 0; i < len(m.img.Exports); i++ {
		e := m.img.Exports[i]
		if e.Name == name && e.Knd == wasmbin.KindMemory {
			return m.GetMemory()
		}
	}
	if name == "memory" {
		return m.GetMemory()
	}
	return nil
}

func (m *Module) GetMemory() *Memory {
	if m == nil {
		return nil
	}
	if m.memBase == 0 {
		return nil
	}
	return &Memory{mod: m}
}

func (m *Module) ExportedFunction(name string) *Function {
	if m == nil || m.img == nil {
		return nil
	}
	for i := 0; i < len(m.img.Exports); i++ {
		e := m.img.Exports[i]
		if e.Name == name && e.Knd == wasmbin.KindFunc {
			ft, ok := m.img.FuncTypeOf(e.Index)
			f := &Function{mod: m, idx: int(e.Index)}
			if ok {
				f.params = valBytes(ft.Params)
				f.results = valBytes(ft.Results)
			}
			return f
		}
	}
	return nil
}

func (m *Module) Call(name string, args ...uint64) ([]uint64, error) {
	f := m.ExportedFunction(name)
	if f == nil {
		return nil, errors.New(m.name + " has no export " + name)
	}
	return f.Call(args...)
}

func (m *Module) CloseWithExitCode(code uint32) error {
	m.Exited = true
	m.ExitCode = code
	return exitErr(code)
}

func (m *Module) Close() error {
	return m.CloseWithExitCode(0)
}

func (f *Function) Call(args ...uint64) ([]uint64, error) {
	if f == nil || f.mod == nil {
		return nil, errors.New("nil function")
	}
	return f.mod.invoke(f.idx, args)
}

func (f *Function) ParamTypes() []byte {
	if f == nil {
		return nil
	}
	return f.params
}

func (f *Function) ResultTypes() []byte {
	if f == nil {
		return nil
	}
	return f.results
}

func (mem *Memory) Read(offset uint32, length uint32) ([]byte, bool) {
	if mem == nil || mem.mod == nil {
		return nil, false
	}
	return mem.mod.readBytes(int(offset), int(length))
}

func (mem *Memory) Write(offset uint32, data []byte) bool {
	if mem == nil || mem.mod == nil {
		return false
	}
	return mem.mod.writeBytes(int(offset), data)
}

func (mem *Memory) WriteUint32Le(offset uint32, v uint32) bool {
	var b [4]byte
	b[0] = byte(v)
	b[1] = byte(v >> 8)
	b[2] = byte(v >> 16)
	b[3] = byte(v >> 24)
	return mem.Write(offset, b[:])
}

func (mem *Memory) Size() uint32 {
	if mem == nil || mem.mod == nil {
		return 0
	}
	return uint32(mem.mod.memSize)
}
