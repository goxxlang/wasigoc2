package loader

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"../wazeropkg"
	"../wasmbinpkg"
)

type dirMount struct {
	host  string
	guest string
}

type envKV struct {
	key string
	val string
}

type Loader struct {
	rt     *wazero.Runtime
	stdout io.Writer
	stderr io.Writer
	stdin  io.Reader
	dirs   []dirMount
	env    []envKV
	modules map[string]*Module
}

func New() (*Loader, error) {
	l := &Loader{}
	l.rt = wazero.NewRuntime()
	l.stdout = nil
	l.stderr = nil
	l.stdin = bytes.NewReader(nil)
	l.modules = make(map[string]*Module)
	return l, nil
}

func (l *Loader) SetStdout(w io.Writer) {
	l.stdout = w
	if l.rt != nil {
		l.rt.SetStdout(w)
	}
}

func (l *Loader) SetStderr(w io.Writer) {
	l.stderr = w
	if l.rt != nil {
		l.rt.SetStderr(w)
	}
}

func (l *Loader) SetStdin(r io.Reader) {
	l.stdin = r
	if l.rt != nil {
		l.rt.SetStdin(r)
	}
}

func (l *Loader) SetEnv(key string, val string) {
	var e envKV
	e.key = key
	e.val = val
	l.env = append(l.env, e)
}

func (l *Loader) SetDir(host string, guest string) {
	var d dirMount
	d.host = host
	d.guest = guest
	l.dirs = append(l.dirs, d)
}

func (l *Loader) Close() error {
	l.modules = nil
	if l.rt != nil {
		return l.rt.Close()
	}
	return nil
}

type Module struct {
	Name     string
	Path     string
	Image    *wasmbin.Image
	inst     *wazero.Module
	loader   *Loader
	Exited   bool
	ExitCode int
}

type LoadConfig struct {
	Args      []string
	Start     []string
	SkipStart bool
}

func guestPath(p string) string {
	return strings.ReplaceAll(p, "\\", "/")
}

func (l *Loader) LoadFile(name string, path string, cfg LoadConfig) (*Module, error) {
	p := guestPath(path)
	raw, err := os.ReadFile(p)
	if err != nil {
		raw2, err2 := os.ReadFile(path)
		if err2 != nil {
			return nil, err2
		}
		raw = raw2
	}
	if name == "" {
		name = strings.TrimSuffix(filepath.Base(guestPath(path)), filepath.Ext(guestPath(path)))
	}
	mod, err3 := l.Load(name, raw, cfg)
	if err3 != nil {
		return nil, err3
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
	if _, exists := l.modules[name]; exists {
		return nil, errors.New("module " + name + " already loaded")
	}
	compiled, err2 := l.rt.CompileModule(wasmbin.GuestCore(raw))
	if err2 != nil {
		return nil, fmt.Errorf("compile %s: %s", name, err2.Error())
	}
	mc := wazero.NewModuleConfig().WithName(name)
	if l.stdout != nil {
		mc = mc.WithStdout(l.stdout)
	}
	if l.stderr != nil {
		mc = mc.WithStderr(l.stderr)
	}
	if l.stdin != nil {
		mc = mc.WithStdin(l.stdin)
	}
	args := cfg.Args
	if len(args) == 0 {
		args = []string{name}
	}
	mc = mc.WithArgs(args...)
	for i := 0; i < len(l.env); i++ {
		mc = mc.WithEnv(l.env[i].key, l.env[i].val)
	}
	if cfg.SkipStart {
		mc = mc.WithStartFunctions()
	} else if cfg.Start != nil {
		mc = mc.WithStartFunctions(cfg.Start...)
	}
	inst, err4 := l.rt.InstantiateModule(compiled, mc)
	if err4 != nil {
		return nil, fmt.Errorf("instantiate %s: %s", name, err4.Error())
	}
	mod := &Module{Name: name, Image: img, inst: inst, loader: l}
	if inst != nil {
		mod.Exited = inst.Exited
		mod.ExitCode = int(inst.ExitCode)
	}
	l.modules[name] = mod
	return mod, nil
}

func (l *Loader) Get(name string) *Module {
	return l.modules[name]
}

func (m *Module) Call(fn string, args ...uint64) ([]uint64, error) {
	if m == nil || m.inst == nil {
		return nil, errors.New("module has no instance")
	}
	got, err := m.inst.Call(fn, args...)
	if err != nil {
		return nil, err
	}
	return got, nil
}

func (m *Module) CallIfExported(fn string) (bool, error) {
	if m == nil || m.Image == nil || !m.Image.HasExport(fn) {
		return false, nil
	}
	_, err := m.Call(fn)
	return true, err
}

func (m *Module) ParseArgs(fn string, texts []string) ([]uint64, error) {
	if m == nil || m.Image == nil {
		return nil, errors.New("module has no instance")
	}
	var ft wasmbin.FuncType
	found := false
	for i := 0; i < len(m.Image.Exports); i++ {
		e := m.Image.Exports[i]
		if e.Name == fn && e.Knd == wasmbin.KindFunc {
			t, ok := m.Image.FuncTypeOf(e.Index)
			if ok {
				ft = t
				found = true
			}
		}
	}
	if !found {
		return nil, errors.New(m.Name + " has no export " + fn)
	}
	if len(texts) != len(ft.Params) {
		return nil, fmt.Errorf("%s expects %d arg(s), got %d", fn, len(ft.Params), len(texts))
	}
	out := make([]uint64, len(texts))
	for i := 0; i < len(texts); i++ {
		v, err := parseValue(texts[i], ft.Params[i])
		if err != nil {
			return nil, fmt.Errorf("arg %d: %s", i, err.Error())
		}
		out[i] = v
	}
	return out, nil
}

func (m *Module) FormatCall(fn string, results []uint64) []string {
	if m == nil || m.Image == nil {
		out := make([]string, len(results))
		for i := 0; i < len(results); i++ {
			out[i] = strconv.FormatUint(results[i], 10)
		}
		return out
	}
	var ft wasmbin.FuncType
	for i := 0; i < len(m.Image.Exports); i++ {
		e := m.Image.Exports[i]
		if e.Name == fn && e.Knd == wasmbin.KindFunc {
			t, ok := m.Image.FuncTypeOf(e.Index)
			if ok {
				ft = t
			}
		}
	}
	out := make([]string, len(results))
	for i := 0; i < len(results); i++ {
		vt := wasmbin.ValI32
		if i < len(ft.Results) {
			vt = ft.Results[i]
		}
		out[i] = formatValue(results[i], vt)
	}
	return out
}

func parseValue(s string, vt wasmbin.ValType) (uint64, error) {
	s = strings.TrimSpace(s)
	if vt == wasmbin.ValI32 {
		n, err := strconv.ParseInt(s, 0, 32)
		if err != nil {
			u, uerr := strconv.ParseUint(s, 0, 32)
			if uerr != nil {
				return 0, err
			}
			return u, nil
		}
		return uint64(uint32(int32(n))), nil
	}
	if vt == wasmbin.ValI64 {
		n, err := strconv.ParseInt(s, 0, 64)
		if err != nil {
			u, uerr := strconv.ParseUint(s, 0, 64)
			if uerr != nil {
				return 0, err
			}
			return u, nil
		}
		return uint64(n), nil
	}
	return 0, errors.New("unsupported value type")
}

func formatValue(v uint64, vt wasmbin.ValType) string {
	if vt == wasmbin.ValI32 {
		n := int64(v & 4294967295)
		if n >= 2147483648 {
			n = n - 4294967296
		}
		return strconv.FormatInt(n, 10)
	}
	if vt == wasmbin.ValI64 {
		return strconv.FormatInt(int64(v), 10)
	}
	return strconv.FormatUint(v, 10)
}
