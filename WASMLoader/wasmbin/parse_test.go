package wasmbin

import (
	"testing"
)

func TestAddRoundTrip(t *testing.T) {
	raw := AddModule()
	if !IsWASM(raw) {
		t.Fatalf("not wasm: %s", HexPrefix(raw, 8))
	}
	img, err := Parse(raw)
	if err != nil {
		t.Fatal(err)
	}
	if len(img.Exports) != 1 || img.Exports[0].Name != "add" {
		t.Fatalf("exports: %+v", img.Exports)
	}
	if got := img.ExportType(img.Exports[0]); got != "(i32, i32) -> (i32)" {
		t.Fatalf("type %q", got)
	}
}

func TestDoubleImportsMath(t *testing.T) {
	img, err := Parse(DoubleModule())
	if err != nil {
		t.Fatal(err)
	}
	if len(img.Imports) != 1 {
		t.Fatalf("imports: %+v", img.Imports)
	}
	if img.Imports[0].Module != "math" || img.Imports[0].Name != "add" {
		t.Fatalf("import %+v", img.Imports[0])
	}
	if got := img.ImportType(img.Imports[0]); got != "(i32, i32) -> (i32)" {
		t.Fatalf("import type %q", got)
	}
}

func wrapComponent(core []byte) []byte {
	out := []byte{0x00, 0x61, 0x73, 0x6d, 0x0d, 0x00, 0x01, 0x00}
	out = append(out, 1)
	out = appendU32(out, uint32(len(core)))
	return append(out, core...)
}

func TestParseComponent(t *testing.T) {
	core := AddModule()
	raw := wrapComponent(core)
	if !IsComponent(raw) {
		t.Fatalf("not component: %s", HexPrefix(raw, 8))
	}
	if IsWASM(raw) {
		t.Fatal("component classified as core")
	}
	img, err := Parse(raw)
	if err != nil {
		t.Fatal(err)
	}
	if img.Version != ComponentVersion {
		t.Fatalf("version %d want %d", img.Version, ComponentVersion)
	}
	if len(img.Exports) != 1 || img.Exports[0].Name != "add" {
		t.Fatalf("exports: %+v", img.Exports)
	}
	got := GuestCore(raw)
	if !IsWASM(got) {
		t.Fatalf("GuestCore not core: %s", HexPrefix(got, 8))
	}
}

func TestRejectsGarbage(t *testing.T) {
	if _, err := Parse([]byte("not wasm")); err == nil {
		t.Fatal("expected error")
	}
	if IsWASM([]byte{0, 1, 2, 3}) {
		t.Fatal("false positive")
	}
}

func TestHelloHasWASIImport(t *testing.T) {
	img, err := Parse(HelloModule())
	if err != nil {
		t.Fatal(err)
	}
	if len(img.Imports) != 1 || img.Imports[0].Name != "fd_write" {
		t.Fatalf("imports %+v", img.Imports)
	}
	found := false
	for _, ex := range img.Exports {
		if ex.Name == "_start" {
			found = true
		}
	}
	if !found {
		t.Fatalf("missing _start: %+v", img.Exports)
	}
}
