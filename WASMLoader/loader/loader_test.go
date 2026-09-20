package loader

import (
	"bytes"
	"strings"
	"testing"

	"wasmloader/wasmbin"
)

func TestLoadAndCallAdd(t *testing.T) {
	l, err := New()
	if err != nil {
		t.Fatal(err)
	}
	defer l.Close()

	mod, err := l.Load("math", wasmbin.AddModule(), LoadConfig{SkipStart: true})
	if err != nil {
		t.Fatal(err)
	}
	got, err := mod.Call("add", 2, 3)
	if err != nil {
		t.Fatal(err)
	}
	if len(got) != 1 || got[0] != 5 {
		t.Fatalf("add(2,3)=%v", got)
	}
}

func TestLinkCompiledModules(t *testing.T) {
	l, err := New()
	if err != nil {
		t.Fatal(err)
	}
	defer l.Close()

	if _, err := l.Load("math", wasmbin.AddModule(), LoadConfig{SkipStart: true}); err != nil {
		t.Fatal(err)
	}
	app, err := l.Load("app", wasmbin.DoubleModule(), LoadConfig{SkipStart: true})
	if err != nil {
		t.Fatal(err)
	}
	got, err := app.Call("double", 21)
	if err != nil {
		t.Fatal(err)
	}
	if len(got) != 1 || got[0] != 42 {
		t.Fatalf("double(21)=%v", got)
	}
}

func TestWASIHello(t *testing.T) {
	var out bytes.Buffer
	l, err := New(WithStdout(&out))
	if err != nil {
		t.Fatal(err)
	}
	defer l.Close()

	if _, err := l.Load("hello", wasmbin.HelloModule(), LoadConfig{}); err != nil {
		t.Fatal(err)
	}
	if got := out.String(); got != "hello, wasm\n" {
		t.Fatalf("stdout %q", got)
	}
}

func TestHostLogPlugin(t *testing.T) {
	var out bytes.Buffer
	l, err := New(WithStdout(&out))
	if err != nil {
		t.Fatal(err)
	}
	defer l.Close()

	plug, err := l.Load("plugin", wasmbin.PluginModule(), LoadConfig{SkipStart: true})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := plug.Call("run"); err != nil {
		t.Fatal(err)
	}
	if got := out.String(); got != "plugin loaded\n" {
		t.Fatalf("stdout %q", got)
	}
}

func TestParseArgs(t *testing.T) {
	l, err := New()
	if err != nil {
		t.Fatal(err)
	}
	defer l.Close()
	mod, err := l.Load("math", wasmbin.AddModule(), LoadConfig{SkipStart: true})
	if err != nil {
		t.Fatal(err)
	}
	args, err := mod.ParseArgs("add", []string{"40", "0x2"})
	if err != nil {
		t.Fatal(err)
	}
	got, err := mod.Call("add", args...)
	if err != nil {
		t.Fatal(err)
	}
	if got[0] != 42 {
		t.Fatalf("got %v", got)
	}
}

func TestMissingImportFails(t *testing.T) {
	l, err := New()
	if err != nil {
		t.Fatal(err)
	}
	defer l.Close()
	_, err = l.Load("app", wasmbin.DoubleModule(), LoadConfig{SkipStart: true})
	if err == nil || !strings.Contains(err.Error(), "math") && !strings.Contains(err.Error(), "import") {
		// wazero error mentions the missing module/function
		if err == nil {
			t.Fatal("expected missing-import error")
		}
	}
}

func TestDuplicateName(t *testing.T) {
	l, err := New()
	if err != nil {
		t.Fatal(err)
	}
	defer l.Close()
	if _, err := l.Load("math", wasmbin.AddModule(), LoadConfig{SkipStart: true}); err != nil {
		t.Fatal(err)
	}
	if _, err := l.Load("math", wasmbin.AddModule(), LoadConfig{SkipStart: true}); err == nil {
		t.Fatal("expected duplicate name error")
	}
}
