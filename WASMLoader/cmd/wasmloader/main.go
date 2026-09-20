package main

import (
	"flag"
	"fmt"
	"io"
	"os"
	"strings"

	"wasmloader/loader"
	"wasmloader/wasmbin"
)

func main() {
	if len(os.Args) < 2 {
		usage(os.Stderr)
		os.Exit(2)
	}
	var err error
	switch os.Args[1] {
	case "inspect":
		err = cmdInspect(os.Args[2:])
	case "run":
		err = cmdRun(os.Args[2:])
	case "call":
		err = cmdCall(os.Args[2:])
	case "link":
		err = cmdLink(os.Args[2:])
	case "serve":
		err = cmdServe(os.Args[2:])
	case "example":
		err = cmdExample(os.Args[2:])
	case "help", "-h", "--help":
		usage(os.Stdout)
		return
	default:
		fmt.Fprintf(os.Stderr, "unknown command %q\n\n", os.Args[1])
		usage(os.Stderr)
		os.Exit(2)
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func usage(w io.Writer) {
	fmt.Fprint(w, `wasmloader — load and run compiled WebAssembly modules

Usage:
  wasmloader inspect <file.wasm>
  wasmloader run     [flags] <file.wasm> [--] [argv...]
  wasmloader call    [flags] <file.wasm> <export> [args...]
  wasmloader link    [flags] name=file.wasm ... --call <module.export> [args...]
  wasmloader serve   [--addr host:port]
  wasmloader example <add|double|hello|plugin> [out.wasm]

run executes a WASI command (_start). call instantiates without _start
and invokes a named export. link loads modules in order so later ones
can import earlier ones (e.g. math=add.wasm app=double.wasm).

Flags for run / call / link:
  --dir host[=guest]   mount a host directory (repeatable)
  --env KEY=VAL        set a WASI environment variable (repeatable)

Host imports provided automatically:
  wasi_snapshot_preview1 / wasi_unstable
  env.log(ptr:i32, len:i32)   write guest memory to stdout
  env.abort(code:i32)
`)
}

func cmdInspect(args []string) error {
	if len(args) != 1 {
		return fmt.Errorf("usage: wasmloader inspect <file.wasm>")
	}
	raw, err := os.ReadFile(args[0])
	if err != nil {
		return err
	}
	return printInspect(os.Stdout, args[0], raw)
}

func printInspect(w io.Writer, label string, raw []byte) error {
	img, err := wasmbin.Parse(raw)
	if err != nil {
		return err
	}
	fmt.Fprintf(w, "%s\n", label)
	fmt.Fprintf(w, "  magic    %s\n", wasmbin.HexPrefix(raw, 8))
	if img.Version == wasmbin.ComponentVersion {
		fmt.Fprintf(w, "  version  %d (component)\n", img.Version)
	} else {
		fmt.Fprintf(w, "  version  %d\n", img.Version)
	}
	fmt.Fprintf(w, "  size     %d bytes\n", img.Size)
	if img.ModuleName != "" {
		fmt.Fprintf(w, "  name     %s\n", img.ModuleName)
	}
	fmt.Fprintf(w, "  sections\n")
	for _, s := range img.Sections {
		fmt.Fprintf(w, "    %-10s %5d bytes @ %d\n", s.Name, s.Size, s.Off)
	}
	if len(img.Imports) > 0 {
		fmt.Fprintf(w, "  imports\n")
		for _, imp := range img.Imports {
			fmt.Fprintf(w, "    %s.%s  %s  %s\n", imp.Module, imp.Name, imp.Kind, img.ImportType(imp))
		}
	}
	if len(img.Exports) > 0 {
		fmt.Fprintf(w, "  exports\n")
		for _, ex := range img.Exports {
			fmt.Fprintf(w, "    %s  %s  %s\n", ex.Name, ex.Kind, img.ExportType(ex))
		}
	}
	if img.Start != nil {
		fmt.Fprintf(w, "  start    func %d\n", *img.Start)
	}
	for _, c := range img.Custom {
		fmt.Fprintf(w, "  custom   %q (%d bytes)\n", c.Name, c.Size)
	}
	return nil
}

type commonFlags struct {
	dirs []string
	envs []string
}

func parseCommon(fs *flag.FlagSet) *commonFlags {
	c := &commonFlags{}
	fs.Func("dir", "host[=guest] directory mount", func(s string) error {
		c.dirs = append(c.dirs, s)
		return nil
	})
	fs.Func("env", "KEY=VAL", func(s string) error {
		c.envs = append(c.envs, s)
		return nil
	})
	return c
}

func (c *commonFlags) apply() []loader.Option {
	var opts []loader.Option
	opts = append(opts, loader.WithStdout(os.Stdout), loader.WithStderr(os.Stderr), loader.WithStdin(os.Stdin))
	for _, d := range c.dirs {
		host, guest, ok := strings.Cut(d, "=")
		if !ok {
			host, guest = d, "/"
		}
		opts = append(opts, loader.WithDir(host, guest))
	}
	for _, e := range c.envs {
		k, v, ok := strings.Cut(e, "=")
		if !ok {
			k, v = e, ""
		}
		opts = append(opts, loader.WithEnv(k, v))
	}
	return opts
}

func cmdRun(args []string) error {
	fs := flag.NewFlagSet("run", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	cf := parseCommon(fs)
	if err := fs.Parse(args); err != nil {
		return err
	}
	rest := fs.Args()
	if len(rest) < 1 {
		return fmt.Errorf("usage: wasmloader run [flags] <file.wasm> [--] [argv...]")
	}
	path := rest[0]
	argv := rest[1:]

	l, err := loader.New(cf.apply()...)
	if err != nil {
		return err
	}
	defer l.Close()

	_, err = l.LoadFile("", path, loader.LoadConfig{Args: prependArgv0(path, argv)})
	return err
}

func cmdCall(args []string) error {
	fs := flag.NewFlagSet("call", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	cf := parseCommon(fs)
	if err := fs.Parse(args); err != nil {
		return err
	}
	rest := fs.Args()
	if len(rest) < 2 {
		return fmt.Errorf("usage: wasmloader call [flags] <file.wasm> <export> [args...]")
	}
	path, export, texts := rest[0], rest[1], rest[2:]

	l, err := loader.New(cf.apply()...)
	if err != nil {
		return err
	}
	defer l.Close()

	mod, err := l.LoadFile("", path, loader.LoadConfig{SkipStart: true})
	if err != nil {
		return err
	}
	return callPrint(mod, export, texts)
}

func cmdLink(args []string) error {
	fs := flag.NewFlagSet("link", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	cf := parseCommon(fs)
	callFlag := fs.String("call", "", "module.export to invoke after loading")
	if err := fs.Parse(args); err != nil {
		return err
	}
	specs, call, callArgs := peelLinkArgs(fs.Args(), *callFlag)
	if len(specs) == 0 {
		return fmt.Errorf("usage: wasmloader link name=file.wasm [name=file.wasm ...] [--call] module.export [args...]")
	}

	l, err := loader.New(cf.apply()...)
	if err != nil {
		return err
	}
	defer l.Close()

	var last *loader.Module
	for _, spec := range specs {
		name, path, ok := strings.Cut(spec, "=")
		if !ok {
			path = spec
			name = ""
		}
		mod, err := l.LoadFile(name, path, loader.LoadConfig{SkipStart: true})
		if err != nil {
			return err
		}
		fmt.Fprintf(os.Stderr, "loaded %s (%s)\n", mod.Name, path)
		last = mod
	}

	if call == "" {
		return nil
	}
	modName, fn, ok := strings.Cut(call, ".")
	if !ok {
		if last == nil {
			return fmt.Errorf("nothing to call")
		}
		return callPrint(last, call, callArgs)
	}
	mod := l.Get(modName)
	if mod == nil {
		return fmt.Errorf("module %q is not loaded", modName)
	}
	return callPrint(mod, fn, callArgs)
}

func callPrint(mod *loader.Module, export string, texts []string) error {
	args, err := mod.ParseArgs(export, texts)
	if err != nil {
		return err
	}
	results, err := mod.Call(export, args...)
	if err != nil {
		return err
	}
	if len(results) == 0 {
		return nil
	}
	fmt.Println(strings.Join(mod.FormatCall(export, results), " "))
	return nil
}

func peelLinkArgs(rest []string, call string) (specs []string, callOut string, callArgs []string) {
	callOut = call
	for i := 0; i < len(rest); i++ {
		a := rest[i]
		switch {
		case a == "--call" || a == "-call":
			if i+1 >= len(rest) {
				return specs, "", nil
			}
			return specs, rest[i+1], rest[i+2:]
		case strings.HasPrefix(a, "--call="):
			return specs, strings.TrimPrefix(a, "--call="), rest[i+1:]
		case isModuleSpec(a):
			specs = append(specs, a)
		default:
			if callOut == "" {
				return specs, a, rest[i+1:]
			}
			return specs, callOut, rest[i:]
		}
	}
	return specs, callOut, nil
}

func isModuleSpec(a string) bool {
	lower := strings.ToLower(a)
	return strings.HasSuffix(lower, ".wasm") || strings.Contains(a, "=")
}

func prependArgv0(path string, argv []string) []string {
	if len(argv) == 0 {
		return []string{path}
	}
	return append([]string{path}, argv...)
}

func cmdExample(args []string) error {
	if len(args) < 1 || len(args) > 2 {
		return fmt.Errorf("usage: wasmloader example <add|double|hello|plugin> [out.wasm]")
	}
	raw := wasmbin.Example(args[0])
	if raw == nil {
		return fmt.Errorf("unknown example %q (want add, double, hello, plugin)", args[0])
	}
	if len(args) == 2 {
		return os.WriteFile(args[1], raw, 0o644)
	}
	_, err := os.Stdout.Write(raw)
	return err
}
