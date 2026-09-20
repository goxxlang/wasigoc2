package loader

import (
	"bytes"
	"context"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"github.com/tetratelabs/wazero"
	"github.com/tetratelabs/wazero/api"
	"github.com/tetratelabs/wazero/imports/wasi_snapshot_preview1"
	"github.com/tetratelabs/wazero/sys"

	"wasmloader/wasmbin"
)

// Loader compiles and instantiates compiled .wasm modules. Later modules
// may import exports from modules loaded earlier (same runtime).
type Loader struct {
	mu      sync.Mutex
	ctx     context.Context
	rt      wazero.Runtime
	stdout  io.Writer
	stderr  io.Writer
	stdin   io.Reader
	dirs    []dirMount
	env     [][2]string
	modules map[string]*Module

	stringImports []stringImport
}

type dirMount struct {
	host, guest string
}

type Option func(*Loader)

func WithStdout(w io.Writer) Option { return func(l *Loader) { l.stdout = w } }
func WithStderr(w io.Writer) Option { return func(l *Loader) { l.stderr = w } }
func WithStdin(r io.Reader) Option  { return func(l *Loader) { l.stdin = r } }
func WithEnv(key, val string) Option {
	return func(l *Loader) { l.env = append(l.env, [2]string{key, val}) }
}
func WithDir(host, guest string) Option {
	return func(l *Loader) { l.dirs = append(l.dirs, dirMount{host: host, guest: guest}) }
}

// WithStringImport registers an "env" host import using the packed
// string-argument convention: each of the arity string arguments is passed
// as a (ptr,len) pair read directly from the calling guest's own linear
// memory, and the single result is a packed (ptr<<32|len) JSON envelope
// ({"ok":bool,"result":string,"error":string}) written into a buffer the
// calling guest allocates via its own exported "alloc" (see the GuiKit wasm
// IDL, wasm/IDL.md in gddisney/guikit, for the guest side of this
// convention — any guest following it can use any host import registered
// this way, not just GuiKit's).
func WithStringImport(name string, arity int, fn func(args []string) (string, error)) Option {
	return func(l *Loader) {
		l.stringImports = append(l.stringImports, stringImport{name: name, arity: arity, fn: fn})
	}
}

func New(opts ...Option) (*Loader, error) {
	ctx := context.Background()
	l := &Loader{
		ctx:     ctx,
		rt:      wazero.NewRuntime(ctx),
		stdout:  io.Discard,
		stderr:  io.Discard,
		stdin:   bytes.NewReader(nil),
		modules: make(map[string]*Module),
	}
	for _, opt := range opts {
		opt(l)
	}
	if err := instantiateWASI(ctx, l.rt); err != nil {
		_ = l.rt.Close(ctx)
		return nil, err
	}
	if err := instantiateEnv(ctx, l.rt, l.stdout, l.stringImports); err != nil {
		_ = l.rt.Close(ctx)
		return nil, err
	}
	return l, nil
}

func instantiateWASI(ctx context.Context, rt wazero.Runtime) error {
	if _, err := wasi_snapshot_preview1.Instantiate(ctx, rt); err != nil {
		return fmt.Errorf("wasi_snapshot_preview1: %w", err)
	}
	b := rt.NewHostModuleBuilder("wasi_unstable")
	wasi_snapshot_preview1.NewFunctionExporter().ExportFunctions(b)
	if _, err := b.Instantiate(ctx); err != nil {
		return fmt.Errorf("wasi_unstable: %w", err)
	}
	return nil
}

func (l *Loader) Close() error {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.modules = nil
	return l.rt.Close(l.ctx)
}

// Module is an instantiated wasm guest.
type Module struct {
	Name     string
	Path     string
	Image    *wasmbin.Image
	compiled wazero.CompiledModule
	inst     api.Module
	loader   *Loader
}

type LoadConfig struct {
	// Args become WASI argv. argv[0] defaults to Name.
	Args []string
	// Start is the post-instantiate function list. Empty skips _start
	// (use this for reactors / calling a named export).
	Start []string
	// SkipStart is a convenience for Start = {}.
	SkipStart bool
}

func (l *Loader) LoadFile(name, path string, cfg LoadConfig) (*Module, error) {
	raw, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	if name == "" {
		name = strings.TrimSuffix(filepath.Base(path), filepath.Ext(path))
	}
	mod, err := l.Load(name, raw, cfg)
	if err != nil {
		return nil, err
	}
	mod.Path = path
	return mod, nil
}

func (l *Loader) Load(name string, raw []byte, cfg LoadConfig) (*Module, error) {
	img, err := wasmbin.Parse(raw)
	if err != nil {
		return nil, err
	}
	if name == "" {
		name = img.ModuleName
	}
	if name == "" {
		name = "main"
	}

	l.mu.Lock()
	defer l.mu.Unlock()
	if _, exists := l.modules[name]; exists {
		return nil, fmt.Errorf("module %q already loaded", name)
	}

	compiled, err := l.rt.CompileModule(l.ctx, wasmbin.GuestCore(raw))
	if err != nil {
		return nil, fmt.Errorf("compile %s: %w", name, err)
	}

	mc := wazero.NewModuleConfig().
		WithName(name).
		WithStdout(l.stdout).
		WithStderr(l.stderr).
		WithStdin(l.stdin).
		WithSysNanotime().
		WithSysWalltime().
		WithSysNanosleep().
		WithRandSource(zeroRand{})

	args := cfg.Args
	if len(args) == 0 {
		args = []string{name}
	}
	mc = mc.WithArgs(args...)
	for _, kv := range l.env {
		mc = mc.WithEnv(kv[0], kv[1])
	}
	if len(l.dirs) > 0 {
		fs := wazero.NewFSConfig()
		for _, d := range l.dirs {
			guest := d.guest
			if guest == "" {
				guest = "/"
			}
			fs = fs.WithDirMount(d.host, guest)
		}
		mc = mc.WithFSConfig(fs)
	}
	if cfg.SkipStart {
		mc = mc.WithStartFunctions()
	} else if cfg.Start != nil {
		mc = mc.WithStartFunctions(cfg.Start...)
	}

	inst, err := l.rt.InstantiateModule(l.ctx, compiled, mc)
	if err != nil {
		_ = compiled.Close(l.ctx)
		if exit, ok := err.(*sys.ExitError); ok && exit.ExitCode() == 0 {
			mod := &Module{Name: name, Image: img, compiled: compiled, inst: inst, loader: l}
			l.modules[name] = mod
			return mod, nil
		}
		return nil, fmt.Errorf("instantiate %s: %w", name, err)
	}

	mod := &Module{Name: name, Image: img, compiled: compiled, inst: inst, loader: l}
	l.modules[name] = mod
	return mod, nil
}

func (l *Loader) Get(name string) *Module {
	l.mu.Lock()
	defer l.mu.Unlock()
	return l.modules[name]
}

func (l *Loader) List() []*Module {
	l.mu.Lock()
	defer l.mu.Unlock()
	out := make([]*Module, 0, len(l.modules))
	for _, m := range l.modules {
		out = append(out, m)
	}
	return out
}

func (m *Module) Call(fn string, args ...uint64) ([]uint64, error) {
	if m.inst == nil {
		return nil, fmt.Errorf("module %s has no instance (exited during start)", m.Name)
	}
	f := m.inst.ExportedFunction(fn)
	if f == nil {
		return nil, fmt.Errorf("%s has no export %q", m.Name, fn)
	}
	results, err := f.Call(m.loader.ctx, args...)
	if err != nil {
		if exit, ok := err.(*sys.ExitError); ok && exit.ExitCode() == 0 {
			return results, nil
		}
		return nil, err
	}
	return results, nil
}

// Memory returns the module's exported linear memory, or nil if it has none.
func (m *Module) Memory() api.Memory {
	if m.inst == nil {
		return nil
	}
	return m.inst.Memory()
}

// ReadMemory copies length bytes at ptr out of the module's linear memory.
// The returned slice is a copy, safe to retain past further guest calls.
func (m *Module) ReadMemory(ptr, length uint32) ([]byte, bool) {
	mem := m.Memory()
	if mem == nil {
		return nil, false
	}
	buf, ok := mem.Read(ptr, length)
	if !ok {
		return nil, false
	}
	out := make([]byte, len(buf))
	copy(out, buf)
	return out, true
}

// WriteMemory copies data into the module's linear memory starting at ptr.
func (m *Module) WriteMemory(ptr uint32, data []byte) bool {
	if len(data) == 0 {
		return true
	}
	mem := m.Memory()
	if mem == nil {
		return false
	}
	return mem.Write(ptr, data)
}

// CallIfExported invokes fn with no arguments when the module exports it,
// otherwise it is a no-op. This is used for the WASI reactor "_initialize"
// export, which SkipStart/WithStartFunctions() leaves uncalled but which
// reactor guests (e.g. Go's GOOS=wasip1 with //go:wasmexport) require before
// any other export is safe to call.
func (m *Module) CallIfExported(fn string) (bool, error) {
	if m.inst == nil || m.inst.ExportedFunction(fn) == nil {
		return false, nil
	}
	_, err := m.Call(fn)
	return true, err
}

func (m *Module) ExportedFunctions() []string {
	if m.inst == nil {
		return nil
	}
	fns := m.inst.ExportedFunctionDefinitions()
	out := make([]string, 0, len(fns))
	for name := range fns {
		out = append(out, name)
	}
	return out
}

// ParseArgs converts text args to wasm stack values for the named export.
func (m *Module) ParseArgs(fn string, texts []string) ([]uint64, error) {
	if m.inst == nil {
		return nil, fmt.Errorf("module %s has no instance", m.Name)
	}
	f := m.inst.ExportedFunction(fn)
	if f == nil {
		return nil, fmt.Errorf("%s has no export %q", m.Name, fn)
	}
	params := f.Definition().ParamTypes()
	if len(texts) != len(params) {
		return nil, fmt.Errorf("%s expects %d arg(s), got %d", fn, len(params), len(texts))
	}
	out := make([]uint64, len(texts))
	for i, t := range texts {
		v, err := parseValue(t, params[i])
		if err != nil {
			return nil, fmt.Errorf("arg %d: %w", i, err)
		}
		out[i] = v
	}
	return out, nil
}

func FormatResults(fn api.Function, results []uint64) []string {
	if fn == nil {
		out := make([]string, len(results))
		for i, r := range results {
			out[i] = fmt.Sprintf("%d", r)
		}
		return out
	}
	rts := fn.Definition().ResultTypes()
	out := make([]string, len(results))
	for i, r := range results {
		if i < len(rts) {
			out[i] = formatValue(r, rts[i])
		} else {
			out[i] = fmt.Sprintf("%d", r)
		}
	}
	return out
}

func (m *Module) FormatCall(fn string, results []uint64) []string {
	if m.inst == nil {
		return FormatResults(nil, results)
	}
	return FormatResults(m.inst.ExportedFunction(fn), results)
}

type zeroRand struct{}

func (zeroRand) Read(p []byte) (int, error) {
	for i := range p {
		p[i] = 0
	}
	return len(p), nil
}
