package main

import (
	"../wasmbinpkg"
	"../wasmloaderpkg"
	"bytes"
	"fmt"
)

func main() {
	l, err := loader.New()
	fmt.Println(err == nil)
	cfg := loader.LoadConfig{}
	cfg.SkipStart = true
	mod, err2 := l.Load("math", wasmbin.AddModule(), cfg)
	fmt.Println(err2 == nil)
	got, err3 := mod.Call("add", 2, 3)
	fmt.Println(err3 == nil)
	fmt.Println(len(got) == 1 && got[0] == 5)

	app, err4 := l.Load("app", wasmbin.DoubleModule(), cfg)
	fmt.Println(err4 == nil)
	got2, err5 := app.Call("double", 21)
	fmt.Println(err5 == nil)
	fmt.Println(len(got2) == 1 && got2[0] == 42)

	var out bytes.Buffer
	h, err6 := loader.New()
	fmt.Println(err6 == nil)
	h.SetStdout(&out)
	_, err7 := h.Load("hello", wasmbin.HelloModule(), loader.LoadConfig{})
	fmt.Println(err7 == nil)
	fmt.Println(out.String() == "hello, wasm\n")

	var plug bytes.Buffer
	p, err8 := loader.New()
	fmt.Println(err8 == nil)
	p.SetStdout(&plug)
	pm, err9 := p.Load("plugin", wasmbin.PluginModule(), cfg)
	fmt.Println(err9 == nil)
	_, err10 := pm.Call("run")
	fmt.Println(err10 == nil)
	fmt.Println(plug.String() == "plugin loaded\n")

	raw := wasmbin.AddModule()
	fmt.Println(wasmbin.IsWASM(raw))
	text, err11 := wasmbin.InspectText("add.wasm", raw)
	fmt.Println(err11 == nil)
	fmt.Println(stringsContains(text, "add"))
	l.Close()
}

func stringsContains(s string, sub string) bool {
	for i := 0; i+len(sub) <= len(s); i++ {
		if s[i:i+len(sub)] == sub {
			return true
		}
	}
	return false
}
