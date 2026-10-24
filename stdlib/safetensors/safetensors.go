// Package safetensors reads the safetensors format: an 8-byte little-endian
// header length, a JSON header naming each tensor's dtype, shape and byte
// range, then the tensor bytes.
//
// os.File here reads forward only, so a file is streamed: Stream hands each
// tensor's bytes to a callback in file order, a chunk at a time, and a model
// never has to fit in memory at once.
package safetensors

import (
	"encoding/json"
	"errors"
	"os"
	"sort"
	"strconv"
)

// Tensor is one header entry. Begin and End are byte offsets in the file.
type Tensor struct {
	Name  string
	DType string
	Shape []int
	Begin int64
	End   int64
}

// Size is the tensor's byte length.
func (t *Tensor) Size() int64 {
	return t.End - t.Begin
}

// Elements is the product of the tensor's shape.
func (t *Tensor) Elements() int64 {
	n := int64(1)
	for _, d := range t.Shape {
		n *= int64(d)
	}
	return n
}

// File is a parsed header. Tensors is in file order.
type File struct {
	Path     string
	Tensors  []*Tensor
	Metadata map[string]string
	byName   map[string]*Tensor
}

// Get is the tensor called name, or nil.
func (f *File) Get(name string) *Tensor {
	return f.byName[name]
}

func u64le(b []byte) uint64 {
	var v uint64
	for i := 7; i >= 0; i-- {
		v = v<<8 | uint64(b[i])
	}
	return v
}

func toInt64(v any) (int64, bool) {
	f, ok := v.(float64)
	if !ok {
		return 0, false
	}
	return int64(f), true
}

// Open reads and parses the header of the safetensors file at path.
func Open(path string) (*File, error) {
	fp, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	lenb := make([]byte, 8)
	got := 0
	for got < 8 {
		n, _ := fp.Read(lenb[got:])
		if n <= 0 {
			fp.Close()
			return nil, errors.New("safetensors: " + path + ": too short")
		}
		got += n
	}
	n := u64le(lenb)
	if n == 0 || n > 100<<20 {
		fp.Close()
		return nil, errors.New("safetensors: " + path + ": header length " + strconv.FormatUint(n, 10) + " is not plausible")
	}
	head := make([]byte, int(n))
	got = 0
	for got < len(head) {
		k, _ := fp.Read(head[got:])
		if k <= 0 {
			fp.Close()
			return nil, errors.New("safetensors: " + path + ": header cut short")
		}
		got += k
	}
	fp.Close()
	return Parse(path, head)
}

// Parse builds a File from header bytes (the JSON after the length).
func Parse(path string, head []byte) (*File, error) {
	var v any
	if err := json.Unmarshal(head, &v); err != nil {
		return nil, errors.New("safetensors: " + path + ": header: " + err.Error())
	}
	m, ok := v.(map[string]any)
	if !ok {
		return nil, errors.New("safetensors: " + path + ": header is not an object")
	}
	base := int64(8 + len(head))
	f := &File{Path: path, Metadata: map[string]string{}, byName: map[string]*Tensor{}}
	for name, raw := range m {
		e, ok := raw.(map[string]any)
		if !ok {
			continue
		}
		if name == "__metadata__" {
			for k, mv := range e {
				if s, ok := mv.(string); ok {
					f.Metadata[k] = s
				}
			}
			continue
		}
		t := &Tensor{Name: name}
		t.DType, _ = e["dtype"].(string)
		shape, _ := e["shape"].([]any)
		for _, d := range shape {
			di, ok := toInt64(d)
			if !ok {
				return nil, errors.New("safetensors: " + name + ": bad shape")
			}
			t.Shape = append(t.Shape, int(di))
		}
		offs, _ := e["data_offsets"].([]any)
		if len(offs) != 2 {
			return nil, errors.New("safetensors: " + name + ": bad data_offsets")
		}
		b, ok1 := toInt64(offs[0])
		en, ok2 := toInt64(offs[1])
		if !ok1 || !ok2 || en < b {
			return nil, errors.New("safetensors: " + name + ": bad data_offsets")
		}
		t.Begin = base + b
		t.End = base + en
		f.Tensors = append(f.Tensors, t)
		f.byName[name] = t
	}
	ts := f.Tensors
	sort.Slice(ts, func(i int, j int) bool { return ts[i].Begin < ts[j].Begin })
	return f, nil
}

// Stream reads the file front to back, handing out the bytes of each
// wanted tensor a piece at a time:
//
//	r, err := f.Stream(8<<20, want)
//	for r.Next() { use(r.Cur, r.Off, r.Data) }
//	if r.Err != nil { ... }
//
// Off is the piece's offset within its tensor, Data at most chunk bytes and
// reused by the next call. Tensors want rejects are skipped.
func (f *File) Stream(chunk int, want func(t *Tensor) bool) (*Reader, error) {
	fp, err := os.Open(f.Path)
	if err != nil {
		return nil, err
	}
	r := &Reader{fp: fp, buf: make([]byte, chunk), f: f, want: want, ti: -1}
	return r, nil
}

// Reader is a Stream in progress.
type Reader struct {
	Cur  *Tensor // the tensor Data belongs to
	Off    int64
	Data   []byte
	Err    error

	fp   os.File // os.Open's file (a handle in this runtime)
	buf  []byte
	f    *File
	want func(t *Tensor) bool
	ti   int
	pos  int64
	done bool
}

// fill reads exactly len(b) bytes.
func (r *Reader) fill(b []byte) bool {
	got := 0
	for got < len(b) {
		n, _ := r.fp.Read(b[got:])
		if n <= 0 {
			r.Err = errors.New("safetensors: " + r.f.Path + ": unexpected end of file")
			return false
		}
		got += n
	}
	r.pos += int64(len(b))
	return true
}

// Next moves to the next piece; false at the end or on an error (Err).
func (r *Reader) Next() bool {
	if r.done {
		return false
	}
	for {
		if r.ti >= 0 && r.pos < r.Cur.End {
			n := int64(len(r.buf))
			if r.Cur.End-r.pos < n {
				n = r.Cur.End - r.pos
			}
			r.Off = r.pos - r.Cur.Begin
			if !r.fill(r.buf[:n]) {
				return r.stop()
			}
			r.Data = r.buf[:n]
			return true
		}
		r.ti++
		for r.ti < len(r.f.Tensors) && !r.want(r.f.Tensors[r.ti]) {
			r.ti++
		}
		if r.ti >= len(r.f.Tensors) {
			return r.stop()
		}
		r.Cur = r.f.Tensors[r.ti]
		if r.Cur.Begin < r.pos {
			r.Err = errors.New("safetensors: " + r.Cur.Name + " overlaps the tensor before it")
			return r.stop()
		}
		for r.pos < r.Cur.Begin {
			n := int64(len(r.buf))
			if r.Cur.Begin-r.pos < n {
				n = r.Cur.Begin - r.pos
			}
			if !r.fill(r.buf[:n]) {
				return r.stop()
			}
		}
	}
}

func (r *Reader) stop() bool {
	if !r.done {
		r.done = true
		r.fp.Close()
	}
	return false
}
