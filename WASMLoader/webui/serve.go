package webui

import (
	"bytes"
	"embed"
	"encoding/json"
	"fmt"
	"io"
	"io/fs"
	"net/http"
	"strings"

	"wasmloader/loader"
	"wasmloader/wasmbin"
)

//go:embed static/*
var staticFS embed.FS

type imageJSON struct {
	Error    string        `json:"error,omitempty"`
	Version  uint32        `json:"version"`
	Size     int           `json:"size"`
	Hex      string        `json:"hex"`
	Module   string        `json:"module,omitempty"`
	Sections []sectionJSON `json:"sections"`
	Imports  []symbolJSON  `json:"imports"`
	Exports  []symbolJSON  `json:"exports"`
}

type sectionJSON struct {
	ID   byte   `json:"id"`
	Name string `json:"name"`
	Size uint32 `json:"size"`
}

type symbolJSON struct {
	Module string `json:"module,omitempty"`
	Name   string `json:"name"`
	Kind   string `json:"kind"`
	Type   string `json:"type"`
}

func Handler() http.Handler {
	mux := http.NewServeMux()
	sub, err := fs.Sub(staticFS, "static")
	if err != nil {
		panic(err)
	}
	mux.Handle("/static/", http.StripPrefix("/static/", http.FileServer(http.FS(sub))))
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" {
			http.NotFound(w, r)
			return
		}
		http.ServeFileFS(w, r, sub, "index.html")
	})
	mux.HandleFunc("/api/inspect", handleInspectText)
	mux.HandleFunc("/api/inspect.json", handleInspectJSON)
	mux.HandleFunc("/api/example/", handleExample)
	mux.HandleFunc("/api/link", handleLink)
	return mux
}

func readWASM(r *http.Request) ([]byte, error) {
	if strings.HasPrefix(r.Header.Get("Content-Type"), "multipart/") {
		if err := r.ParseMultipartForm(32 << 20); err != nil {
			return nil, err
		}
		f, _, err := r.FormFile("wasm")
		if err != nil {
			return nil, err
		}
		defer f.Close()
		return io.ReadAll(io.LimitReader(f, 32<<20))
	}
	return io.ReadAll(io.LimitReader(r.Body, 32<<20))
}

func handleInspectText(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "POST a .wasm body", http.StatusMethodNotAllowed)
		return
	}
	raw, err := readWASM(r)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	label := r.Header.Get("X-Wasm-Name")
	if label == "" {
		label = "module.wasm"
	}
	var buf bytes.Buffer
	img, err := wasmbin.Parse(raw)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeInspect(&buf, label, raw, img)
	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	_, _ = w.Write(buf.Bytes())
}

func handleInspectJSON(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "POST a .wasm body", http.StatusMethodNotAllowed)
		return
	}
	raw, err := readWASM(r)
	if err != nil {
		writeJSON(w, http.StatusBadRequest, imageJSON{Error: err.Error()})
		return
	}
	img, err := wasmbin.Parse(raw)
	if err != nil {
		writeJSON(w, http.StatusBadRequest, imageJSON{Error: err.Error()})
		return
	}
	writeJSON(w, http.StatusOK, toJSON(raw, img))
}

func handleExample(w http.ResponseWriter, r *http.Request) {
	name := strings.TrimPrefix(r.URL.Path, "/api/example/")
	raw := wasmbin.Example(name)
	if raw == nil {
		http.Error(w, "unknown example", http.StatusNotFound)
		return
	}
	w.Header().Set("Content-Type", "application/wasm")
	w.Header().Set("Content-Disposition", `attachment; filename="`+name+`.wasm"`)
	_, _ = w.Write(raw)
}

func handleLink(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "POST multipart name+wasm fields", http.StatusMethodNotAllowed)
		return
	}
	if err := r.ParseMultipartForm(32 << 20); err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	names := r.MultipartForm.Value["name"]
	files := r.MultipartForm.File["wasm"]
	if len(files) == 0 {
		http.Error(w, "no wasm files", http.StatusBadRequest)
		return
	}

	var stdout, stderr bytes.Buffer
	l, err := loader.New(loader.WithStdout(&stdout), loader.WithStderr(&stderr))
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	defer l.Close()

	var last *loader.Module
	for i, fh := range files {
		name := ""
		if i < len(names) {
			name = names[i]
		}
		f, err := fh.Open()
		if err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		raw, err := io.ReadAll(io.LimitReader(f, 32<<20))
		f.Close()
		if err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		if name == "" {
			name = strings.TrimSuffix(fh.Filename, ".wasm")
		}
		mod, err := l.Load(name, raw, loader.LoadConfig{SkipStart: true})
		if err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		last = mod
	}

	call := r.FormValue("call")
	if call == "" {
		w.Header().Set("Content-Type", "text/plain; charset=utf-8")
		fmt.Fprintf(w, "loaded %d module(s)\n%s%s", len(files), stdout.String(), stderr.String())
		return
	}

	modName, fn, ok := strings.Cut(call, ".")
	var mod *loader.Module
	if ok {
		mod = l.Get(modName)
	} else {
		fn = call
		mod = last
	}
	if mod == nil {
		http.Error(w, "module not loaded: "+modName, http.StatusBadRequest)
		return
	}

	var texts []string
	if s := strings.TrimSpace(r.FormValue("args")); s != "" {
		texts = strings.Fields(s)
	}

	// WASI command: calling _start after SkipStart
	if fn == "_start" && len(texts) == 0 {
		if _, err := mod.Call("_start"); err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		w.Header().Set("Content-Type", "text/plain; charset=utf-8")
		io.WriteString(w, strings.TrimSuffix(stdout.String()+stderr.String(), ""))
		if stdout.Len() == 0 && stderr.Len() == 0 {
			io.WriteString(w, "(ok, no stdout)")
		}
		return
	}

	args, err := mod.ParseArgs(fn, texts)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	results, err := mod.Call(fn, args...)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	var b strings.Builder
	if stdout.Len() > 0 {
		b.WriteString(stdout.String())
	}
	if stderr.Len() > 0 {
		b.WriteString(stderr.String())
	}
	if len(results) > 0 {
		fmt.Fprintf(&b, "%s.%s => %s\n", mod.Name, fn, strings.Join(mod.FormatCall(fn, results), " "))
	} else if b.Len() == 0 {
		fmt.Fprintf(&b, "%s.%s => (no results)\n", mod.Name, fn)
	}
	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	io.WriteString(w, b.String())
}

func writeInspect(w io.Writer, label string, raw []byte, img *wasmbin.Image) {
	fmt.Fprintf(w, "%s\n", label)
	fmt.Fprintf(w, "magic    %s\n", wasmbin.HexPrefix(raw, 8))
	fmt.Fprintf(w, "version  %d\n", img.Version)
	fmt.Fprintf(w, "size     %d bytes\n", img.Size)
	if img.ModuleName != "" {
		fmt.Fprintf(w, "name     %s\n", img.ModuleName)
	}
	fmt.Fprintf(w, "sections\n")
	for _, s := range img.Sections {
		fmt.Fprintf(w, "  %-10s %5d bytes\n", s.Name, s.Size)
	}
	if len(img.Imports) > 0 {
		fmt.Fprintf(w, "imports\n")
		for _, imp := range img.Imports {
			fmt.Fprintf(w, "  %s.%s  %s  %s\n", imp.Module, imp.Name, imp.Kind, img.ImportType(imp))
		}
	}
	if len(img.Exports) > 0 {
		fmt.Fprintf(w, "exports\n")
		for _, ex := range img.Exports {
			fmt.Fprintf(w, "  %s  %s  %s\n", ex.Name, ex.Kind, img.ExportType(ex))
		}
	}
}

func toJSON(raw []byte, img *wasmbin.Image) imageJSON {
	out := imageJSON{
		Version: img.Version,
		Size:    img.Size,
		Hex:     wasmbin.HexPrefix(raw, 8),
		Module:  img.ModuleName,
	}
	for _, s := range img.Sections {
		out.Sections = append(out.Sections, sectionJSON{ID: s.ID, Name: s.Name, Size: s.Size})
	}
	for _, imp := range img.Imports {
		out.Imports = append(out.Imports, symbolJSON{
			Module: imp.Module, Name: imp.Name, Kind: imp.Kind.String(), Type: img.ImportType(imp),
		})
	}
	for _, ex := range img.Exports {
		out.Exports = append(out.Exports, symbolJSON{
			Name: ex.Name, Kind: ex.Kind.String(), Type: img.ExportType(ex),
		})
	}
	return out
}

func writeJSON(w http.ResponseWriter, code int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	_ = json.NewEncoder(w).Encode(v)
}
