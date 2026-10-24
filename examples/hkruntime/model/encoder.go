package model

// Gemma 4's vision encoder, assimilated by hodgemix (assimtowers.go): ternary linears with
// limbs, run over every patch at once (enc*.wgsl). The pooled tokens go through
// embed_vision and the bridge (bnzmix.embed_bridge) onto the residual axis, and stand in for
// embedding rows at reserved ids (tower.go), as the L-A towers' tokens do.
//
// The weights fix the tensors and their shapes; the order between them is Gemma 4's vision
// encoder as recorded in decisions E1-E6 (ENCODERS.md):
//   E1 patches of 16 x 16 RGB scaled to [-1, 1], at most 2520 patches (280 tokens x 3 x 3)
//   E2 input_proj, then x += position_table[0][col] + position_table[1][row]
//   E3 per layer: h = x + post_attention_norm(attn(input_norm(x)));
//                 x = h + post_feedforward_norm(mlp(pre_feedforward_norm(h))); norms times w
//   E4 bidirectional attention, q/k head norms, 2-D RoPE (theta 100), scale 1
//   E5 mlp = down(gelu_tanh(gate) * up); every linear clamps its input and output
//   E6 3 x 3 mean pool, unit RMS norm, embed_vision (768 -> 1536), the bridge (1536 -> D)

import (
	"errors"
	"fmt"
	"math"

	"safetensors"
	"webgpu"
)

const (
	visPrefix = "model.vision_tower."
	visEmbedW = "model.embed_vision.embedding_projection"
	bridgeW   = "bnzmix.embed_bridge"
	visTheta  = 100.0 // vision_config rope_theta
	visEps    = 1e-6  // vision_config rms_norm_eps
	// VisPool is the pooling block side (pooling_kernel_size).
	VisPool = 3
	// VisMaxPatches is 280 tokens (default_output_length) of VisPool x VisPool patches.
	VisMaxPatches = 2520
	// mediaRange is the reserved ids an image may use; audio ids start past it.
	mediaRange = 512
)

// encLin is an encoder linear and its clip bounds [in lo, in hi, out lo, out hi].
type encLin struct {
	name    string
	l       *linear
	clip    region
	clipped bool
}

type vitLayer struct {
	inNorm, postAttn, preFF, postFF, qn, kn region
	q, k, v, oProj, gate, up, down           encLin
}

type vision struct {
	d, hd, heads, ffn, patch, plane int
	inProj                          encLin
	pos                             region
	layers                          []vitLayer
	embed                           encLin
	out                             int // embed_vision's width

	sized                                          bool
	in, x, q, k, v, att, g, u, y, xq, xs, pool, e, fin region
}

type bridgeT struct {
	l              *linear
	meanF, meanRow region
	rms            float64
}

// HasVision reports that the checkpoint carries the assimilated vision encoder and the bridge.
func (m *Model) HasVision() bool {
	return m.vit != nil && m.bridge != nil
}

func (m *Model) encLinOf(st *safetensors.File, name string, clipBase string) (encLin, error) {
	t := st.Get(name + ".weight")
	if t == nil || t.DType != "U8" || len(t.Shape) != 2 {
		return encLin{}, errors.New("model: " + name + ".weight is not packed ternary")
	}
	l, err := m.lin(st, name, t.Shape[1], 4*t.Shape[0])
	if err != nil {
		return encLin{}, err
	}
	e := encLin{name: clipBase, l: l}
	if clipBase != "" && st.Get(clipBase+".input_min") != nil {
		if e.clip, err = m.weights.alloc(16); err != nil {
			return encLin{}, err
		}
		e.clipped = true
	}
	return e, nil
}

func (m *Model) normOf(st *safetensors.File, name string) (region, int, error) {
	t := st.Get(name)
	if t == nil {
		return region{}, 0, errors.New("model: no " + name)
	}
	n := int(t.Elements())
	r, err := m.f32of(st, name, n)
	return r, n, err
}

// allocEncoders places the bridge and the encoders the checkpoint has (vision here,
// audio in audio.go). Without the bridge no encoder can reach the residual axis.
func (m *Model) allocEncoders(st *safetensors.File) error {
	bt := st.Get(bridgeW + ".weight")
	if bt == nil || bt.DType != "U8" || len(bt.Shape) != 2 {
		return nil
	}
	var err error
	br := &bridgeT{}
	if br.l, err = m.lin(st, bridgeW, bt.Shape[1], m.C.Dim); err != nil {
		return err
	}
	if br.meanF, err = m.weights.alloc(4 * br.l.k); err != nil {
		return err
	}
	if br.meanRow, err = m.weights.alloc(4 * m.C.Dim); err != nil {
		return err
	}
	if m.zero, err = m.weights.alloc(4 * 4096); err != nil {
		return err
	}
	m.bridge = br
	if err := m.allocVision(st); err != nil {
		return err
	}
	return m.allocAudio(st)
}

func (m *Model) allocVision(st *safetensors.File) error {
	if st.Get(visPrefix+"patch_embedder.input_proj.weight") == nil {
		return nil
	}
	vs := &vision{}
	var err error
	if vs.inProj, err = m.encLinOf(st, visPrefix+"patch_embedder.input_proj", ""); err != nil {
		return err
	}
	vs.d = vs.inProj.l.n
	vs.patch = int(math.Sqrt(float64(vs.inProj.l.k / 3)))
	if vs.patch*vs.patch*3 != vs.inProj.l.k {
		return errors.New("model: input_proj takes " + itoa(vs.inProj.l.k) + " values, not a square RGB patch")
	}
	pt := st.Get(visPrefix + "patch_embedder.position_embedding_table")
	if pt == nil || pt.DType != "BF16" || len(pt.Shape) != 3 || pt.Shape[0] != 2 || pt.Shape[2] != vs.d {
		return errors.New("model: the vision position table is not [2, n, " + itoa(vs.d) + "]")
	}
	vs.plane = pt.Shape[1]
	if vs.pos, err = m.weights.alloc(4 * int(pt.Elements())); err != nil {
		return err
	}
	for i := 0; ; i++ {
		p := visPrefix + "encoder.layers." + itoa(i) + "."
		if st.Get(p+"input_layernorm.weight") == nil {
			break
		}
		L := vitLayer{}
		if L.inNorm, _, err = m.normOf(st, p+"input_layernorm.weight"); err != nil {
			return err
		}
		if L.postAttn, _, err = m.normOf(st, p+"post_attention_layernorm.weight"); err != nil {
			return err
		}
		if L.preFF, _, err = m.normOf(st, p+"pre_feedforward_layernorm.weight"); err != nil {
			return err
		}
		if L.postFF, _, err = m.normOf(st, p+"post_feedforward_layernorm.weight"); err != nil {
			return err
		}
		hd := 0
		if L.qn, hd, err = m.normOf(st, p+"self_attn.q_norm.weight"); err != nil {
			return err
		}
		if L.kn, _, err = m.normOf(st, p+"self_attn.k_norm.weight"); err != nil {
			return err
		}
		vs.hd = hd
		names := []string{"self_attn.q_proj", "self_attn.k_proj", "self_attn.v_proj", "self_attn.o_proj", "mlp.gate_proj", "mlp.up_proj", "mlp.down_proj"}
		var ls []encLin
		for _, n := range names {
			e, err := m.encLinOf(st, p+n+".linear", p+n)
			if err != nil {
				return err
			}
			ls = append(ls, e)
		}
		L.q, L.k, L.v, L.oProj, L.gate, L.up, L.down = ls[0], ls[1], ls[2], ls[3], ls[4], ls[5], ls[6]
		vs.ffn = L.gate.l.n
		vs.layers = append(vs.layers, L)
	}
	if len(vs.layers) == 0 || vs.hd == 0 || vs.d%vs.hd != 0 || 256%vs.hd != 0 {
		return errors.New("model: the vision encoder has no usable layers")
	}
	vs.heads = vs.d / vs.hd
	if vs.embed, err = m.encLinOf(st, visEmbedW, ""); err != nil {
		return err
	}
	vs.out = vs.embed.l.n
	if vs.out != m.bridge.l.k {
		return errors.New("model: embed_vision ends at " + itoa(vs.out) + ", the bridge takes " + itoa(m.bridge.l.k))
	}
	m.vit = vs
	return nil
}

func linDests(dests map[string]*dest, l *linear) {
	dests[l.name+".weight"] = &dest{r: l.w}
	dests[l.name+".weight_scale"] = &dest{r: l.s, wide: true}
	limbs := l.limbs // wasigoc: an outer range variable's field in an inner range does not resolve
	for j, lb := range limbs {
		ln := l.name + ".weight_limb" + itoa(j+1)
		dests[ln] = &dest{r: lb.w}
		dests[ln+"_scale"] = &dest{r: lb.s, wide: true}
		dests[ln+"_radix"] = &dest{radix: &lb.radix}
	}
}

func encDests(dests map[string]*dest, e encLin) {
	linDests(dests, e.l)
	if !e.clipped {
		return
	}
	bounds := []string{".input_min", ".input_max", ".output_min", ".output_max"}
	for i, n := range bounds {
		dests[e.name+n] = &dest{r: region{buf: e.clip.buf, off: e.clip.off + 4*i, size: 4}, wide: true}
	}
}

// encoderDests adds the bridge's and the encoders' tensors to upload's destinations.
func (m *Model) encoderDests(dests map[string]*dest) {
	if m.bridge == nil {
		return
	}
	linDests(dests, m.bridge.l)
	dests[bridgeW+".mean_feature"] = &dest{r: m.bridge.meanF}
	dests[bridgeW+".mean_row"] = &dest{r: m.bridge.meanRow}
	m.audioDests(dests)
	vs := m.vit
	if vs == nil {
		return
	}
	encDests(dests, vs.inProj)
	dests[visPrefix+"patch_embedder.position_embedding_table"] = &dest{r: vs.pos, wide: true}
	layers := vs.layers
	for i, L := range layers {
		p := visPrefix + "encoder.layers." + itoa(i) + "."
		dests[p+"input_layernorm.weight"] = &dest{r: L.inNorm, wide: true}
		dests[p+"post_attention_layernorm.weight"] = &dest{r: L.postAttn, wide: true}
		dests[p+"pre_feedforward_layernorm.weight"] = &dest{r: L.preFF, wide: true}
		dests[p+"post_feedforward_layernorm.weight"] = &dest{r: L.postFF, wide: true}
		dests[p+"self_attn.q_norm.weight"] = &dest{r: L.qn, wide: true}
		dests[p+"self_attn.k_norm.weight"] = &dest{r: L.kn, wide: true}
		lins := []encLin{L.q, L.k, L.v, L.oProj, L.gate, L.up, L.down}
		for _, e := range lins {
			encDests(dests, e)
		}
	}
	encDests(dests, vs.embed)
}

// bridgeRMS takes the bridge's target rms from the bytes upload kept (towerRaw).
func (m *Model) bridgeRMS(raw map[string][]byte) {
	if m.bridge == nil {
		return
	}
	if b := raw[bridgeW+".rms"]; len(b) >= 4 {
		m.bridge.rms = float64(f32At(b, 0))
	}
	if m.bridge.rms <= 0 {
		m.bridge.rms = 1
	}
}

func (m *Model) scratch(n int) (region, error) {
	return m.own(n)
}

func (vs *vision) size(m *Model) error {
	if vs.sized {
		return nil
	}
	T, P := VisMaxPatches, VisMaxPatches/(VisPool*VisPool)
	wide := vs.ffn
	if vs.d > wide {
		wide = vs.d
	}
	if vs.out > wide {
		wide = vs.out
	}
	if m.C.Dim > wide {
		wide = m.C.Dim
	}
	var err error
	sizes := []int{4 * T * vs.inProj.l.k, 4 * T * vs.d, 4 * T * vs.d, 4 * T * vs.d, 4 * T * vs.d, 4 * T * vs.d,
		4 * T * vs.ffn, 4 * T * vs.ffn, 4 * T * wide, T * wide, 4 * T, 4 * P * vs.d, 4 * P * vs.out, 4 * P * m.C.Dim}
	rs := make([]region, len(sizes))
	for i, n := range sizes {
		if rs[i], err = m.scratch(n); err != nil {
			return err
		}
	}
	vs.in, vs.x, vs.q, vs.k, vs.v, vs.att = rs[0], rs[1], rs[2], rs[3], rs[4], rs[5]
	vs.g, vs.u, vs.y, vs.xq, vs.xs, vs.pool, vs.e, vs.fin = rs[6], rs[7], rs[8], rs[9], rs[10], rs[11], rs[12], rs[13]
	vs.sized = true
	return nil
}

// encPrep queues encprep.wgsl: in (and b) -> int8 rows in xq/xs for T tokens.
func (m *Model) encPrep(prog *webgpu.Program, T, k, mode, norm int, a, b, nw region, e *encLin, xq, xs region) error {
	return m.encPrepX(prog, T, k, mode, norm, 2, 0, a, b, nw, e, xq, xs)
}

// encPrepX is encPrep with the activation (ACT: 1 silu, 2 gelu tanh) and POST (1: silu after
// the norm).
func (m *Model) encPrepX(prog *webgpu.Program, T, k, mode, norm, act, post int, a, b, nw region, e *encLin, xq, xs region) error {
	clip := "CLIP=0"
	cl := m.zero
	if e != nil && e.clipped {
		clip, cl = "CLIP=1", e.clip
	}
	p, err := m.pipe("encprep", "K="+itoa(k), "MODE="+itoa(mode), "NORM="+itoa(norm), "ACT="+itoa(act), "POST="+itoa(post), "EPS=0.000001", clip)
	if err != nil {
		return err
	}
	g, err := m.group(p, a, b, nw, cl, xq, xs)
	if err != nil {
		return err
	}
	m.dispatch(prog, p, g, T)
	return nil
}

// encMatmul queues encmatmul.wgsl for the weight and its limbs: out = W xq for T tokens.
func (m *Model) encMatmul(prog *webgpu.Program, T int, l *linear, xq, xs, out region) error {
	np := l.n / 4
	p, err := m.pipe("encmatmul", "K="+itoa(l.k), "NP="+itoa(np), "ACCUM=0")
	if err != nil {
		return err
	}
	g, err := m.group(p, xq, xs, l.w, l.s, out)
	if err != nil {
		return err
	}
	groups := (np + 7) / 8
	prog.Dispatch(p, []webgpu.Group{g}, groups, 1, T)
	limbs := l.limbs
	for _, lb := range limbs {
		lp, err := m.pipe("encmatmul", "K="+itoa(l.k), "NP="+itoa(np), "ACCUM=1", "CENTER="+itoa((lb.radix-1)/2))
		if err != nil {
			return err
		}
		lg, err := m.group(lp, xq, xs, lb.w, lb.s, out)
		if err != nil {
			return err
		}
		prog.Dispatch(lp, []webgpu.Group{lg}, groups, 1, T)
	}
	return nil
}

// encPost queues encpost.wgsl: y's clamp and bias (withBias), then OUT into d.
func (m *Model) encPost(prog *webgpu.Program, T, n, out int, y region, e *encLin, withBias bool, bias, nw, d region, target float64) error {
	return m.encPostF(prog, T, n, out, 1, y, e, withBias, bias, nw, d, target)
}

// encPostF is encPost with the residual scale f (encpost.wgsl F).
func (m *Model) encPostF(prog *webgpu.Program, T, n, out int, f float64, y region, e *encLin, withBias bool, bias, nw, d region, target float64) error {
	clip := "CLIP=0"
	cl := m.zero
	if e != nil && e.clipped {
		clip, cl = "CLIP=1", e.clip
	}
	hasBias := "BIAS=0"
	if withBias {
		hasBias = "BIAS=1"
	} else {
		bias = m.zero
	}
	p, err := m.pipe("encpost", "N="+itoa(n), "OUT="+itoa(out), "NORM=1", "EPS=0.000001", clip, hasBias, "TARGET="+jf(target), "F="+jf(f))
	if err != nil {
		return err
	}
	g, err := m.group(p, y, cl, bias, nw, d)
	if err != nil {
		return err
	}
	m.dispatch(prog, p, g, T)
	return nil
}

func jf(v float64) string {
	return fmt.Sprint(v)
}

// linear runs one clipped encoder linear on T rows of a: prep, matmul, post into d with OUT.
func (m *Model) encLinear(prog *webgpu.Program, vs *vision, T int, e *encLin, a region, norm int, nw region, out int, postNorm region, d region) error {
	if err := m.encPrep(prog, T, e.l.k, 0, norm, a, m.zero, nw, e, vs.xq, vs.xs); err != nil {
		return err
	}
	if err := m.encMatmul(prog, T, e.l, vs.xq, vs.xs, vs.y); err != nil {
		return err
	}
	return m.encPost(prog, T, e.l.n, out, vs.y, e, false, m.zero, postNorm, d, 1)
}

// flush submits prog and empties it for the next block.
func (m *Model) flush(prog *webgpu.Program) error {
	_, err := m.Dev.Run(prog)
	prog.Reset()
	return err
}

// Vision runs the encoder on an image of rows x cols patches (rgb is interleaved bytes,
// rows*patch high and cols*patch wide) and returns the reserved ids of its pooled tokens.
func (m *Model) Vision(rgb []byte, rows, cols int) ([]int, error) {
	vs := m.vit
	if vs == nil || m.bridge == nil {
		return nil, errors.New("this model has no vision encoder")
	}
	T := rows * cols
	if rows < VisPool || cols < VisPool || rows%VisPool != 0 || cols%VisPool != 0 || T > VisMaxPatches || rows > vs.plane || cols > vs.plane {
		return nil, errors.New("model: an image is a grid of multiples of " + itoa(VisPool) + " patches, at most " + itoa(VisMaxPatches))
	}
	P := T / (VisPool * VisPool)
	ps := vs.patch
	W := cols * ps
	if len(rgb) != rows*ps*W*3 {
		return nil, errors.New("model: the image has " + itoa(len(rgb)) + " bytes, not " + itoa(rows*ps*W*3))
	}
	if err := vs.size(m); err != nil {
		return nil, err
	}
	// E1: patches in raster order, each (y, x, channel), scaled to [-1, 1].
	K := vs.inProj.l.k
	in := make([]byte, 4*T*K)
	for t := 0; t < T; t++ {
		pr, pc := t/cols, t%cols
		for py := 0; py < ps; py++ {
			for px := 0; px < ps; px++ {
				src := ((pr*ps+py)*W + pc*ps + px) * 3
				for c := 0; c < 3; c++ {
					v := math.Float32bits(float32(rgb[src+c])/127.5 - 1)
					at := 4 * (t*K + (py*ps+px)*3 + c)
					in[at] = byte(v)
					in[at+1] = byte(v >> 8)
					in[at+2] = byte(v >> 16)
					in[at+3] = byte(v >> 24)
				}
			}
		}
	}
	if err := m.Dev.Write(vs.in.buf, vs.in.off, in); err != nil {
		return nil, err
	}
	prog := &webgpu.Program{}
	d, hd, ffn := vs.d, vs.hd, vs.ffn
	// E2: patch embedding and position.
	if err := m.encPrep(prog, T, K, 0, 0, vs.in, m.zero, m.zero, nil, vs.xq, vs.xs); err != nil {
		return nil, err
	}
	if err := m.encMatmul(prog, T, vs.inProj.l, vs.xq, vs.xs, vs.x); err != nil {
		return nil, err
	}
	pp, err := m.pipe("encpos", "D="+itoa(d), "COLS="+itoa(cols), "PLANE="+itoa(vs.plane))
	if err != nil {
		return nil, err
	}
	pg, err := m.group(pp, vs.pos, vs.x)
	if err != nil {
		return nil, err
	}
	m.dispatch(prog, pp, pg, T)
	qk, err := m.pipe("encqk", "HD="+itoa(hd), "COLS="+itoa(cols), "THETA="+jf(visTheta), "EPS=0.000001")
	if err != nil {
		return nil, err
	}
	at, err := m.pipe("encattn", "T="+itoa(T), "HD="+itoa(hd), "SCALE=1")
	if err != nil {
		return nil, err
	}
	// Each block is its own submission: one command buffer for the whole encoder runs past
	// the operating system's GPU watchdog on a large image.
	if err := m.flush(prog); err != nil {
		return nil, err
	}
	layers := vs.layers
	for li := range layers {
		L := &layers[li]
		// E3, E4: attention.
		if err := m.encLinear(prog, vs, T, &L.q, vs.x, 1, L.inNorm, 0, m.zero, vs.q); err != nil {
			return nil, err
		}
		if err := m.encLinear(prog, vs, T, &L.k, vs.x, 1, L.inNorm, 0, m.zero, vs.k); err != nil {
			return nil, err
		}
		if err := m.encLinear(prog, vs, T, &L.v, vs.x, 1, L.inNorm, 0, m.zero, vs.v); err != nil {
			return nil, err
		}
		qg, err := m.group(qk, vs.q, vs.k, L.qn, L.kn)
		if err != nil {
			return nil, err
		}
		prog.Dispatch(qk, []webgpu.Group{qg}, vs.heads, T, 1)
		ag, err := m.group(at, vs.q, vs.k, vs.v, vs.att)
		if err != nil {
			return nil, err
		}
		prog.Dispatch(at, []webgpu.Group{ag}, vs.heads, T, 1)
		if err := m.encLinear(prog, vs, T, &L.oProj, vs.att, 0, m.zero, 2, L.postAttn, vs.x); err != nil {
			return nil, err
		}
		if err := m.flush(prog); err != nil {
			return nil, err
		}
		// E3, E5: the gated MLP.
		if err := m.encLinear(prog, vs, T, &L.gate, vs.x, 1, L.preFF, 0, m.zero, vs.g); err != nil {
			return nil, err
		}
		if err := m.encLinear(prog, vs, T, &L.up, vs.x, 1, L.preFF, 0, m.zero, vs.u); err != nil {
			return nil, err
		}
		if err := m.encPrep(prog, T, ffn, 1, 0, vs.g, vs.u, m.zero, &L.down, vs.xq, vs.xs); err != nil {
			return nil, err
		}
		if err := m.encMatmul(prog, T, L.down.l, vs.xq, vs.xs, vs.y); err != nil {
			return nil, err
		}
		if err := m.encPost(prog, T, d, 2, vs.y, &L.down, false, m.zero, L.postFF, vs.x, 1); err != nil {
			return nil, err
		}
		if err := m.flush(prog); err != nil {
			return nil, err
		}
	}
	// E6: pool, embed_vision, the bridge.
	po, err := m.pipe("encpool", "D="+itoa(d), "COLS="+itoa(cols), "POOL="+itoa(VisPool))
	if err != nil {
		return nil, err
	}
	pog, err := m.group(po, vs.x, vs.pool)
	if err != nil {
		return nil, err
	}
	m.dispatch(prog, po, pog, P)
	if err := m.encLinear(prog, vs, P, &vs.embed, vs.pool, 3, m.zero, 0, m.zero, vs.e); err != nil {
		return nil, err
	}
	br := m.bridge
	if err := m.encPrep(prog, P, vs.out, 2, 0, vs.e, br.meanF, m.zero, nil, vs.xq, vs.xs); err != nil {
		return nil, err
	}
	if err := m.encMatmul(prog, P, br.l, vs.xq, vs.xs, vs.y); err != nil {
		return nil, err
	}
	if err := m.encPost(prog, P, m.C.Dim, 3, vs.y, nil, true, br.meanRow, m.zero, vs.fin, br.rms); err != nil {
		return nil, err
	}
	if _, err := m.Dev.Run(prog); err != nil {
		return nil, err
	}
	keep := map[int]slot{}
	old := m.slots // wasigoc: a range over a field does not resolve
	for id, sl := range old {
		if sl.kind != "vision" {
			keep[id] = sl
		}
	}
	ids := make([]int, P)
	for i := 0; i < P; i++ {
		ids[i] = m.C.Vocab + i
		keep[ids[i]] = slot{kind: "vision", gpu: true, buf: vs.fin.buf, off: vs.fin.off + 4*m.C.Dim*i}
	}
	m.slots = keep
	return ids, nil
}
