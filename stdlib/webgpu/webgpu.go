// Package webgpu is compute on the host's WebGPU, from a wasigocvm guest.
//
// It is the guest side of ~/go++/WASMWebGPU: gocvm.Call("webgpu", ...)
// reaches the catalog in this module (WASMWebGPU/src/catalog.cc), whose
// webgpu.h runs on the JS host's navigator.gpu over one suspending import
// (webgpu.batch; WASMJsLoader answers it). wasigocvm.bat links WASMWebGPU
// into any program that imports this package.
//
// Objects are small ids. Set-up (buffers, shaders, pipelines, bind groups)
// is one call each; the work itself is a Program — writes, dispatches,
// copies and reads — run with one call, so a step of a model costs one
// trip to the host.
package webgpu

import (
	"errors"
	"gocvm"
	"strconv"
	"strings"
)

type Buffer uint32
type Shader uint32
type Pipeline uint32
type Group uint32

// Device is the opened adapter + device. Info holds the catalog's
// key=value lines; a key that repeats (feature, wgsl) keeps every value.
type Device struct {
	Info     map[string]string
	Features []string
	WGSL     []string
}

func call(api string, rest string) (string, error) {
	reply, err := gocvm.Call("webgpu", api+"\x1f"+rest)
	if err != nil {
		return "", err
	}
	if strings.HasPrefix(reply, "error:") {
		return "", errors.New(reply)
	}
	return reply, nil
}

func callID(api string, rest string) (uint32, error) {
	s, err := call(api, rest)
	if err != nil {
		return 0, err
	}
	v, perr := strconv.ParseUint(strings.TrimSpace(s), 10, 32)
	if perr != nil {
		return 0, errors.New("webgpu: " + api + ": reply `" + s + "` is not an id")
	}
	return uint32(v), nil
}

// Open opens the adapter and a device with every limit the adapter has.
func Open() (*Device, error) {
	s, err := call("Open", "")
	if err != nil {
		return nil, err
	}
	d := &Device{Info: map[string]string{}}
	lines := strings.Split(s, "\n")
	for _, line := range lines {
		eq := strings.Index(line, "=")
		if eq < 0 {
			continue
		}
		k := line[:eq]
		v := line[eq+1:]
		if k == "feature" {
			d.Features = append(d.Features, v)
		} else if k == "wgsl" {
			d.WGSL = append(d.WGSL, v)
		} else {
			d.Info[k] = v
		}
	}
	return d, nil
}

// Has reports a device feature ("shader-f16") or a WGSL language feature
// ("packed_4x8_integer_dot_product").
func (d *Device) Has(name string) bool {
	for _, f := range d.Features {
		if f == name {
			return true
		}
	}
	for _, f := range d.WGSL {
		if f == name {
			return true
		}
	}
	return false
}

// Limit is a device limit by its WebGPU name (maxBufferSize, ...), or 0.
func (d *Device) Limit(name string) uint64 {
	v, err := strconv.ParseUint(d.Info[name], 10, 64)
	if err != nil {
		return 0
	}
	return v
}

// NewBuffer is a storage + uniform + copy buffer of at least size bytes.
func (d *Device) NewBuffer(size int) (Buffer, error) {
	id, err := callID("Buffer", strconv.Itoa(size))
	return Buffer(id), err
}

// Free drops a buffer, shader, pipeline or group by its id.
func (d *Device) Free(id uint32) error {
	_, err := call("Free", strconv.FormatUint(uint64(id), 10))
	return err
}

// Write puts data at byte offset off (a multiple of 4) of b.
func (d *Device) Write(b Buffer, off int, data []byte) error {
	_, err := call("Write", strconv.FormatUint(uint64(b), 10)+" "+strconv.Itoa(off)+"\n"+string(data))
	return err
}

// Read is n bytes of b at off (both multiples of 4), once queued work is done.
func (d *Device) Read(b Buffer, off int, n int) ([]byte, error) {
	s, err := call("Read", strconv.FormatUint(uint64(b), 10)+" "+strconv.Itoa(off)+" "+strconv.Itoa(n))
	if err != nil {
		return nil, err
	}
	return []byte(s), nil
}

// NewShader compiles WGSL; a compile error names its line.
func (d *Device) NewShader(wgsl string) (Shader, error) {
	id, err := callID("Shader", wgsl)
	return Shader(id), err
}

// NewPipeline is a compute pipeline on entry with layout "auto". consts
// are WGSL override values, each "name=value".
func (d *Device) NewPipeline(s Shader, entry string, consts ...string) (Pipeline, error) {
	rest := strconv.FormatUint(uint64(s), 10) + " " + entry
	for _, c := range consts {
		rest += " " + c
	}
	id, err := callID("Pipeline", rest)
	return Pipeline(id), err
}

// Binding is one buffer of a bind group. Size 0 binds to the buffer's end.
type Binding struct {
	Binding int
	Buffer  Buffer
	Offset  int
	Size    int
}

// NewGroup is bind group `index` of pipeline p.
func (d *Device) NewGroup(p Pipeline, index int, bindings ...Binding) (Group, error) {
	rest := strconv.FormatUint(uint64(p), 10) + " " + strconv.Itoa(index)
	for _, b := range bindings {
		rest += " " + strconv.Itoa(b.Binding) + "=" + strconv.FormatUint(uint64(b.Buffer), 10) +
			":" + strconv.Itoa(b.Offset) + ":" + strconv.Itoa(b.Size)
	}
	id, err := callID("Group", rest)
	return Group(id), err
}

// Program is GPU work for one Run: ops run in order, and the reply is the
// bytes of every Read, in order. Encoding: WASMWebGPU/src/catalog.cc.
type Program struct {
	b []byte
}

func (p *Program) word(v uint32) {
	p.b = append(p.b, byte(v), byte(v>>8), byte(v>>16), byte(v>>24))
}

// Reset empties the program for reuse.
func (p *Program) Reset() {
	p.b = p.b[:0]
}

// Append adds q's ops after p's (a fixed sequence built once, reused).
func (p *Program) Append(q *Program) {
	p.b = append(p.b, q.b...)
}

// Len is the program's size in bytes.
func (p *Program) Len() int {
	return len(p.b)
}

// Dispatch runs pipeline pl on x*y*z workgroups with groups bound at
// indices 0, 1, ...
func (p *Program) Dispatch(pl Pipeline, groups []Group, x int, y int, z int) {
	p.word(1)
	p.word(uint32(pl))
	p.word(uint32(len(groups)))
	for i, g := range groups {
		p.word(uint32(i))
		p.word(uint32(g))
	}
	p.word(uint32(x))
	p.word(uint32(y))
	p.word(uint32(z))
}

// Write puts data at byte offset off of b before the ops after it run.
func (p *Program) Write(b Buffer, off int, data []byte) {
	p.word(2)
	p.word(uint32(b))
	p.word(uint32(off))
	p.word(uint32(len(data)))
	p.b = append(p.b, data...)
	for len(p.b)%4 != 0 {
		p.b = append(p.b, 0)
	}
}

// WriteU32 writes 32-bit words (u32, i32 or f32 bits) at off of b.
func (p *Program) WriteU32(b Buffer, off int, words ...uint32) {
	p.word(2)
	p.word(uint32(b))
	p.word(uint32(off))
	p.word(uint32(4 * len(words)))
	for _, w := range words {
		p.word(w)
	}
}

// Copy copies n bytes from src at srcOff to dst at dstOff.
func (p *Program) Copy(src Buffer, srcOff int, dst Buffer, dstOff int, n int) {
	p.word(3)
	p.word(uint32(src))
	p.word(uint32(srcOff))
	p.word(uint32(dst))
	p.word(uint32(dstOff))
	p.word(uint32(n))
}

// Read appends n bytes of b at off to the Run's reply.
func (p *Program) Read(b Buffer, off int, n int) {
	p.word(4)
	p.word(uint32(b))
	p.word(uint32(off))
	p.word(uint32(n))
}

// Run runs the program; its reply is the bytes of its Reads.
func (d *Device) Run(p *Program) ([]byte, error) {
	s, err := call("Run", string(p.b))
	if err != nil {
		return nil, err
	}
	return []byte(s), nil
}
