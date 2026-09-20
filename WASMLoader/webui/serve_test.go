package webui

import (
	"bytes"
	"io"
	"mime/multipart"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"wasmloader/wasmbin"
)

func TestInspectAndLink(t *testing.T) {
	srv := httptest.NewServer(Handler())
	t.Cleanup(srv.Close)

	res, err := http.Post(srv.URL+"/api/inspect.json", "application/wasm", bytes.NewReader(wasmbin.AddModule()))
	if err != nil {
		t.Fatal(err)
	}
	body, _ := io.ReadAll(res.Body)
	res.Body.Close()
	if res.StatusCode != 200 {
		t.Fatalf("inspect %d %s", res.StatusCode, body)
	}
	if !strings.Contains(string(body), `"name":"add"`) {
		t.Fatalf("body %s", body)
	}

	var buf bytes.Buffer
	w := multipart.NewWriter(&buf)
	_ = w.WriteField("name", "math")
	fw, err := w.CreateFormFile("wasm", "add.wasm")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := fw.Write(wasmbin.AddModule()); err != nil {
		t.Fatal(err)
	}
	_ = w.WriteField("name", "app")
	fw, err = w.CreateFormFile("wasm", "double.wasm")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := fw.Write(wasmbin.DoubleModule()); err != nil {
		t.Fatal(err)
	}
	_ = w.WriteField("call", "app.double")
	_ = w.WriteField("args", "21")
	if err := w.Close(); err != nil {
		t.Fatal(err)
	}

	res, err = http.Post(srv.URL+"/api/link", w.FormDataContentType(), &buf)
	if err != nil {
		t.Fatal(err)
	}
	out, _ := io.ReadAll(res.Body)
	res.Body.Close()
	if res.StatusCode != 200 {
		t.Fatalf("link %d %s", res.StatusCode, out)
	}
	if !strings.Contains(string(out), "42") {
		t.Fatalf("want 42, got %s", out)
	}
}

func TestIndexServed(t *testing.T) {
	srv := httptest.NewServer(Handler())
	t.Cleanup(srv.Close)
	res, err := http.Get(srv.URL + "/")
	if err != nil {
		t.Fatal(err)
	}
	defer res.Body.Close()
	b, _ := io.ReadAll(res.Body)
	if res.StatusCode != 200 || !bytes.Contains(b, []byte("WASM Loader")) {
		t.Fatalf("index %d %s", res.StatusCode, b[:min(200, len(b))])
	}
}
