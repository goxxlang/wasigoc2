// webgpu: a WGSL kernel on the host's WebGPU through gocvm.Call("webgpu").
//
//	compile.bat examples\webgpupkg\main.go -o webgpupkg.wasm
//	node WASMJsLoader/serve.mjs, then WASMJsLoader/webgpu.html?wasm=...
package main

import (
	"fmt"
	"math"
	"os"
	"webgpu"
)

const saxpy = `
override A: f32 = 1.0;
@group(0) @binding(0) var<storage, read> x: array<f32>;
@group(0) @binding(1) var<storage, read_write> y: array<f32>;
@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id: vec3u) {
  if (id.x < arrayLength(&y)) { y[id.x] = A * x[id.x] + y[id.x]; }
}
`

func f32le(vs []float32) []byte {
	out := make([]byte, 0, 4*len(vs))
	for _, v := range vs {
		b := math.Float32bits(v)
		out = append(out, byte(b), byte(b>>8), byte(b>>16), byte(b>>24))
	}
	return out
}

func fail(what string, err error) {
	fmt.Println(what+":", err)
	os.Exit(1)
}

func main() {
	d, err := webgpu.Open()
	if err != nil {
		fail("open", err)
	}
	fmt.Println("adapter:", d.Info["vendor"], d.Info["architecture"], d.Info["description"])
	fmt.Println("maxBufferSize:", d.Limit("maxBufferSize"))
	fmt.Println("dot4:", d.Has("packed_4x8_integer_dot_product"), "f16:", d.Has("shader-f16"))

	const n = 1000
	xs := make([]float32, n)
	ys := make([]float32, n)
	for i := 0; i < n; i++ {
		xs[i] = float32(i)
		ys[i] = 1
	}
	x, err := d.NewBuffer(4 * n)
	if err != nil {
		fail("buffer", err)
	}
	y, err := d.NewBuffer(4 * n)
	if err != nil {
		fail("buffer", err)
	}
	sh, err := d.NewShader(saxpy)
	if err != nil {
		fail("shader", err)
	}
	pl, err := d.NewPipeline(sh, "main", "A=3")
	if err != nil {
		fail("pipeline", err)
	}
	g, err := d.NewGroup(pl, 0, webgpu.Binding{Binding: 0, Buffer: x}, webgpu.Binding{Binding: 1, Buffer: y})
	if err != nil {
		fail("group", err)
	}
	var p webgpu.Program
	p.Write(x, 0, f32le(xs))
	p.Write(y, 0, f32le(ys))
	p.Dispatch(pl, []webgpu.Group{g}, (n+63)/64, 1, 1)
	p.Read(y, 0, 4*n)
	out, err := d.Run(&p)
	if err != nil {
		fail("run", err)
	}
	bad := 0
	for i := 0; i < n; i++ {
		b := uint32(out[4*i]) | uint32(out[4*i+1])<<8 | uint32(out[4*i+2])<<16 | uint32(out[4*i+3])<<24
		if math.Float32frombits(b) != float32(3*i+1) {
			bad++
		}
	}
	fmt.Println("saxpy y = 3x + y:", n-bad, "/", n, "correct")
	if bad != 0 {
		os.Exit(1)
	}
}
