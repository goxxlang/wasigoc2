// wasitime — wasigocvm runtime CLI.
//
// Command surface from ~/WASMLoader (inspect / run / call / link / example).
// Engine is the ported ~/WASMLoader on the Go++ wazero interpreter
// (examples/wazeropkg) + WASMSafeSpace + WASMv8Bindings-shaped CHPT.
// If a file still has an outer wrapper, only the nested core module is loaded.
package main

import (
	"../wasmbinpkg"
	"../wasmloaderpkg"
	"fmt"
	"os"
	"strings"
)

func usage() {
	fmt.Println("wasitime — wasigocvm runtime")
	fmt.Println("")
	fmt.Println("Usage:")
	fmt.Println("  wasitime inspect <file.wasm>")
	fmt.Println("  wasitime run     <file.wasm> [--] [argv...]")
	fmt.Println("  wasitime call    <file.wasm> <export> [args...]")
	fmt.Println("  wasitime link    name=file.wasm ... [--call] module.export [args...]")
	fmt.Println("  wasitime example <add|double|hello|plugin> [out.wasm]")
	fmt.Println("  wasitime <file.wasm>          same as run")
}

func die(err error) {
	fmt.Println(err.Error())
	os.Exit(1)
}

func newLoader() *loader.Loader {
	l, err := loader.New()
	if err != nil {
		die(err)
	}
	return l
}

func cmdInspect(path string) {
	raw, err := os.ReadFile(strings.ReplaceAll(path, "\\", "/"))
	if err != nil {
		raw2, err2 := os.ReadFile(path)
		if err2 != nil {
			die(err2)
		}
		raw = raw2
	}
	text, err3 := wasmbin.InspectText(path, raw)
	if err3 != nil {
		die(err3)
	}
	fmt.Print(text)
}

func cmdRun(path string, argv []string) {
	l := newLoader()
	cfg := loader.LoadConfig{}
	cfg.Args = prependArgv0(path, argv)
	_, err := l.LoadFile("", path, cfg)
	if err != nil {
		die(err)
	}
}

func cmdCall(path string, export string, texts []string) {
	l := newLoader()
	cfg := loader.LoadConfig{}
	cfg.SkipStart = true
	mod, err := l.LoadFile("", path, cfg)
	if err != nil {
		die(err)
	}
	callPrint(mod, export, texts)
}

func cmdLink(args []string) {
	specs, call, callArgs := peelLinkArgs(args, "")
	if len(specs) == 0 {
		usage()
		os.Exit(2)
	}
	l := newLoader()
	var last *loader.Module
	for i := 0; i < len(specs); i++ {
		name, path, ok := strings.Cut(specs[i], "=")
		if !ok {
			path = specs[i]
			name = ""
		}
		cfg := loader.LoadConfig{}
		cfg.SkipStart = true
		mod, err := l.LoadFile(name, path, cfg)
		if err != nil {
			die(err)
		}
		fmt.Println("loaded " + mod.Name + " (" + path + ")")
		last = mod
	}
	if call == "" {
		return
	}
	modName, fn, ok := strings.Cut(call, ".")
	if !ok {
		if last == nil {
			die(fmt.Errorf("nothing to call"))
		}
		callPrint(last, call, callArgs)
		return
	}
	mod := l.Get(modName)
	if mod == nil {
		die(fmt.Errorf("module %s is not loaded", modName))
	}
	callPrint(mod, fn, callArgs)
}

func callPrint(mod *loader.Module, export string, texts []string) {
	args, err := mod.ParseArgs(export, texts)
	if err != nil {
		die(err)
	}
	results, err2 := mod.Call(export, args...)
	if err2 != nil {
		die(err2)
	}
	if len(results) == 0 {
		return
	}
	fmt.Println(strings.Join(mod.FormatCall(export, results), " "))
}

func peelLinkArgs(rest []string, call string) ([]string, string, []string) {
	callOut := call
	var specs []string
	for i := 0; i < len(rest); i++ {
		a := rest[i]
		if a == "--call" || a == "-call" {
			if i+1 >= len(rest) {
				return specs, "", nil
			}
			return specs, rest[i+1], rest[i+2:]
		}
		if strings.HasPrefix(a, "--call=") {
			return specs, strings.TrimPrefix(a, "--call="), rest[i+1:]
		}
		if isModuleSpec(a) {
			specs = append(specs, a)
		} else {
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
	out := []string{path}
	for i := 0; i < len(argv); i++ {
		out = append(out, argv[i])
	}
	return out
}

func cmdExample(args []string) {
	if len(args) < 1 || len(args) > 2 {
		die(fmt.Errorf("usage: wasitime example <add|double|hello|plugin> [out.wasm]"))
	}
	raw := wasmbin.Example(args[0])
	if raw == nil {
		die(fmt.Errorf("unknown example %s (want add, double, hello, plugin)", args[0]))
	}
	if len(args) == 2 {
		err := os.WriteFile(args[1], raw, 0644)
		if err != nil {
			die(err)
		}
		return
	}
	fmt.Print(string(raw))
}

func main() {
	args := os.Args
	i := 1
	if len(args) > 1 && args[1] == "--" {
		i = 2
	}
	if len(args) <= i {
		usage()
		os.Exit(2)
	}
	cmd := args[i]
	rest := args[i+1:]
	if strings.HasSuffix(strings.ToLower(cmd), ".wasm") {
		cmdRun(cmd, rest)
		return
	}
	if cmd == "help" || cmd == "-h" || cmd == "--help" {
		usage()
		return
	}
	if cmd == "inspect" {
		if len(rest) != 1 {
			usage()
			os.Exit(2)
		}
		cmdInspect(rest[0])
		return
	}
	if cmd == "run" {
		if len(rest) < 1 {
			usage()
			os.Exit(2)
		}
		cmdRun(rest[0], rest[1:])
		return
	}
	if cmd == "call" {
		if len(rest) < 2 {
			usage()
			os.Exit(2)
		}
		cmdCall(rest[0], rest[1], rest[2:])
		return
	}
	if cmd == "link" {
		cmdLink(rest)
		return
	}
	if cmd == "example" {
		cmdExample(rest)
		return
	}
	fmt.Println("unknown command " + cmd)
	usage()
	os.Exit(2)
}
