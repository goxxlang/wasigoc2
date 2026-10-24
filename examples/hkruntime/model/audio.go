package model

// Gemma 4's audio encoder, assimilated by hodgemix (assimtowers.go), run like the vision
// encoder (encoder.go) over every frame at once. The page sends log-mel frames (tower.wasm
// mel, decision E7). The order between the tensors follows decisions E8-E10 (ENCODERS.md):
//   E8  two 3 x 3 stride-2 convolutions, each with an RMS norm over its channels and ReLU;
//       frames x frequencies x channels flatten into input_proj_linear (one token per 40 ms)
//   E9  per block: x += 1/2 ffw1(x); x += attn(x); x += lconv(x); x += 1/2 ffw2(x); x = norm_out(x)
//       ffw   = post_norm(l2(silu(l1(pre_norm(x)))))
//       attn  = norm_post_attn(post(local attention(norm_pre_attn(x)))), frames t-12..t, keys plus
//               relative_k_proj of sinusoidal distances, softplus(per_dim_scale) on q, cap 50
//       lconv = linear_end(silu(conv_norm(causal depthwise conv(glu(linear_start(pre_norm(x)))))))
//   E10 output_proj (+ bias), unit RMS norm, embed_audio, the bridge onto the residual axis

import (
	"errors"
	"math"

	"safetensors"
	"webgpu"
)

const (
	audPrefix = "model.audio_tower."
	audEmbedW = "model.embed_audio.embedding_projection"
	audPast   = 12   // attention_context_left - 1
	audResid  = 0.5  // residual_weight of the feed-forwards
	audKW     = 5    // conv_kernel_size
	// AudMaxFrames is 30 s of 10 ms log-mel frames.
	AudMaxFrames = 3000
)

type confFFW struct {
	pre, post region
	l1, l2    encLin
}

type confLayer struct {
	ff1, ff2                    confFFW
	preAttn, postAttn, normOut region
	q, k, v, post, relK        encLin
	pds                         region
	lpre, cnorm, dw             region
	lstart, lend                encLin
}

type audioEnc struct {
	d, hd, heads, bins, c0, c1, outW int
	conv0w, conv0n, conv1w, conv1n   region
	inProj                           encLin
	layers                           []confLayer
	outProj                          encLin
	outBias                          region
	embed                            encLin

	sized                                                                   bool
	mel, s0, s1, x, alt, q, k, v, att, g, a2, c, y, xq, xs, rp, rel, o1, e, fin region
}

// HasAudio reports that the checkpoint carries the assimilated audio encoder and the bridge.
func (m *Model) HasAudio() bool {
	return m.aenc != nil && m.bridge != nil
}

func (m *Model) bf16Region(st *safetensors.File, name string) (region, *safetensors.Tensor, error) {
	t := st.Get(name)
	if t == nil || t.DType != "BF16" {
		return region{}, nil, errors.New("model: no BF16 " + name)
	}
	r, err := m.weights.alloc(4 * int(t.Elements()))
	return r, t, err
}

func (m *Model) ffwOf(st *safetensors.File, p string) (confFFW, error) {
	f := confFFW{}
	var err error
	if f.pre, _, err = m.normOf(st, p+"pre_layer_norm.weight"); err != nil {
		return f, err
	}
	if f.post, _, err = m.normOf(st, p+"post_layer_norm.weight"); err != nil {
		return f, err
	}
	if f.l1, err = m.encLinOf(st, p+"ffw_layer_1.linear", p+"ffw_layer_1"); err != nil {
		return f, err
	}
	f.l2, err = m.encLinOf(st, p+"ffw_layer_2.linear", p+"ffw_layer_2")
	return f, err
}

func (m *Model) allocAudio(st *safetensors.File) error {
	if st.Get(audPrefix+"subsample_conv_projection.input_proj_linear.weight") == nil {
		return nil
	}
	a := &audioEnc{}
	var err error
	var t *safetensors.Tensor
	if a.conv0w, t, err = m.bf16Region(st, audPrefix+"subsample_conv_projection.layer0.conv.weight"); err != nil {
		return err
	}
	if len(t.Shape) != 4 || t.Shape[1] != 1 || t.Shape[2] != 3 || t.Shape[3] != 3 {
		return errors.New("model: the first audio convolution is not [c, 1, 3, 3]")
	}
	a.c0 = t.Shape[0]
	if a.conv1w, t, err = m.bf16Region(st, audPrefix+"subsample_conv_projection.layer1.conv.weight"); err != nil {
		return err
	}
	if len(t.Shape) != 4 || t.Shape[1] != a.c0 || t.Shape[2] != 3 || t.Shape[3] != 3 {
		return errors.New("model: the second audio convolution is not [c, " + itoa(a.c0) + ", 3, 3]")
	}
	a.c1 = t.Shape[0]
	if a.c0 > 256 || a.c1 > 256 {
		return errors.New("model: an audio convolution has more than 256 channels")
	}
	if a.conv0n, _, err = m.normOf(st, audPrefix+"subsample_conv_projection.layer0.norm.weight"); err != nil {
		return err
	}
	if a.conv1n, _, err = m.normOf(st, audPrefix+"subsample_conv_projection.layer1.norm.weight"); err != nil {
		return err
	}
	if a.inProj, err = m.encLinOf(st, audPrefix+"subsample_conv_projection.input_proj_linear", ""); err != nil {
		return err
	}
	a.d = a.inProj.l.n
	// Two halvings of the frequency axis: input_proj takes c1 x bins/4.
	a.bins = 4 * a.inProj.l.k / a.c1
	for i := 0; ; i++ {
		p := audPrefix + "layers." + itoa(i) + "."
		if st.Get(p+"norm_out.weight") == nil {
			break
		}
		L := confLayer{}
		if L.ff1, err = m.ffwOf(st, p+"feed_forward1."); err != nil {
			return err
		}
		if L.ff2, err = m.ffwOf(st, p+"feed_forward2."); err != nil {
			return err
		}
		if L.preAttn, _, err = m.normOf(st, p+"norm_pre_attn.weight"); err != nil {
			return err
		}
		if L.postAttn, _, err = m.normOf(st, p+"norm_post_attn.weight"); err != nil {
			return err
		}
		if L.normOut, _, err = m.normOf(st, p+"norm_out.weight"); err != nil {
			return err
		}
		hd := 0
		if L.pds, hd, err = m.normOf(st, p+"self_attn.per_dim_scale"); err != nil {
			return err
		}
		a.hd = hd
		names := []string{"q_proj", "k_proj", "v_proj", "post"}
		var ls []encLin
		for _, n := range names {
			e, err := m.encLinOf(st, p+"self_attn."+n+".linear", p+"self_attn."+n)
			if err != nil {
				return err
			}
			ls = append(ls, e)
		}
		L.q, L.k, L.v, L.post = ls[0], ls[1], ls[2], ls[3]
		if L.relK, err = m.encLinOf(st, p+"self_attn.relative_k_proj", ""); err != nil {
			return err
		}
		if L.lpre, _, err = m.normOf(st, p+"lconv1d.pre_layer_norm.weight"); err != nil {
			return err
		}
		if L.cnorm, _, err = m.normOf(st, p+"lconv1d.conv_norm.weight"); err != nil {
			return err
		}
		if L.dw, t, err = m.bf16Region(st, p+"lconv1d.depthwise_conv1d.weight"); err != nil {
			return err
		}
		if len(t.Shape) != 3 || t.Shape[0] != a.d || t.Shape[1] != 1 || t.Shape[2] != audKW {
			return errors.New("model: the depthwise convolution is not [" + itoa(a.d) + ", 1, " + itoa(audKW) + "]")
		}
		if L.lstart, err = m.encLinOf(st, p+"lconv1d.linear_start.linear", p+"lconv1d.linear_start"); err != nil {
			return err
		}
		if L.lend, err = m.encLinOf(st, p+"lconv1d.linear_end.linear", p+"lconv1d.linear_end"); err != nil {
			return err
		}
		a.layers = append(a.layers, L)
	}
	if len(a.layers) == 0 || a.hd == 0 || a.d%a.hd != 0 || a.hd > 256 {
		return errors.New("model: the audio encoder has no usable layers")
	}
	a.heads = a.d / a.hd
	if a.outProj, err = m.encLinOf(st, audPrefix+"output_proj", ""); err != nil {
		return err
	}
	if a.outBias, _, err = m.normOf(st, audPrefix+"output_proj.bias"); err != nil {
		return err
	}
	a.outW = a.outProj.l.n
	if a.embed, err = m.encLinOf(st, audEmbedW, ""); err != nil {
		return err
	}
	if a.embed.l.n != m.bridge.l.k {
		return errors.New("model: embed_audio ends at " + itoa(a.embed.l.n) + ", the bridge takes " + itoa(m.bridge.l.k))
	}
	m.aenc = a
	return nil
}

func ffwDests(dests map[string]*dest, p string, f confFFW) {
	dests[p+"pre_layer_norm.weight"] = &dest{r: f.pre, wide: true}
	dests[p+"post_layer_norm.weight"] = &dest{r: f.post, wide: true}
	encDests(dests, f.l1)
	encDests(dests, f.l2)
}

func (m *Model) audioDests(dests map[string]*dest) {
	a := m.aenc
	if a == nil {
		return
	}
	sp := audPrefix + "subsample_conv_projection."
	dests[sp+"layer0.conv.weight"] = &dest{r: a.conv0w, wide: true}
	dests[sp+"layer0.norm.weight"] = &dest{r: a.conv0n, wide: true}
	dests[sp+"layer1.conv.weight"] = &dest{r: a.conv1w, wide: true}
	dests[sp+"layer1.norm.weight"] = &dest{r: a.conv1n, wide: true}
	encDests(dests, a.inProj)
	layers := a.layers
	for i, L := range layers {
		p := audPrefix + "layers." + itoa(i) + "."
		ffwDests(dests, p+"feed_forward1.", L.ff1)
		ffwDests(dests, p+"feed_forward2.", L.ff2)
		dests[p+"norm_pre_attn.weight"] = &dest{r: L.preAttn, wide: true}
		dests[p+"norm_post_attn.weight"] = &dest{r: L.postAttn, wide: true}
		dests[p+"norm_out.weight"] = &dest{r: L.normOut, wide: true}
		dests[p+"self_attn.per_dim_scale"] = &dest{r: L.pds, wide: true}
		dests[p+"lconv1d.pre_layer_norm.weight"] = &dest{r: L.lpre, wide: true}
		dests[p+"lconv1d.conv_norm.weight"] = &dest{r: L.cnorm, wide: true}
		dests[p+"lconv1d.depthwise_conv1d.weight"] = &dest{r: L.dw, wide: true}
		lins := []encLin{L.q, L.k, L.v, L.post, L.relK, L.lstart, L.lend}
		for _, e := range lins {
			encDests(dests, e)
		}
	}
	encDests(dests, a.outProj)
	dests[audPrefix+"output_proj.bias"] = &dest{r: a.outBias, wide: true}
	encDests(dests, a.embed)
}

func (a *audioEnc) size(m *Model) error {
	if a.sized {
		return nil
	}
	T0 := AudMaxFrames
	T1 := (T0-1)/2 + 1
	T := (T1-1)/2 + 1
	F1 := (a.bins + 1) / 2
	ffn := a.layers[0].ff1.l1.l.n
	wide := ffn
	if 2*a.d > wide {
		wide = 2 * a.d
	}
	if m.C.Dim > wide {
		wide = m.C.Dim
	}
	sizes := []int{4 * T0 * a.bins, 4 * T1 * F1 * a.c0, 4 * T * a.inProj.l.k, 4 * T * a.d, 4 * T * a.d, 4 * T * a.d, 4 * T * a.d,
		4 * T * a.d, 4 * T * a.d, 4 * T * ffn, 4 * T * 2 * a.d, 4 * T * a.d, 4 * T * wide, T * wide, 4 * T,
		4 * (audPast + 1) * a.d, 4 * (audPast + 1) * a.d, 4 * T * a.outW, 4 * T * a.embed.l.n, 4 * T * m.C.Dim}
	rs := make([]region, len(sizes))
	var err error
	for i, n := range sizes {
		if rs[i], err = m.scratch(n); err != nil {
			return err
		}
	}
	a.mel, a.s0, a.s1, a.x, a.alt, a.q, a.k = rs[0], rs[1], rs[2], rs[3], rs[4], rs[5], rs[6]
	a.v, a.att, a.g, a.a2, a.c, a.y, a.xq, a.xs = rs[7], rs[8], rs[9], rs[10], rs[11], rs[12], rs[13], rs[14]
	a.rp, a.rel, a.o1, a.e, a.fin = rs[15], rs[16], rs[17], rs[18], rs[19]
	a.sized = true
	return nil
}

// audLinear runs one clipped encoder linear on T rows of x: prep (norm), matmul, post (OUT, f).
func (m *Model) audLinear(prog *webgpu.Program, a *audioEnc, T int, e *encLin, x region, norm int, nw region, out int, f float64, postNorm, d region) error {
	if err := m.encPrep(prog, T, e.l.k, 0, norm, x, m.zero, nw, e, a.xq, a.xs); err != nil {
		return err
	}
	if err := m.encMatmul(prog, T, e.l, a.xq, a.xs, a.y); err != nil {
		return err
	}
	return m.encPostF(prog, T, e.l.n, out, f, a.y, e, false, m.zero, postNorm, d, 1)
}

func (m *Model) audFFW(prog *webgpu.Program, a *audioEnc, T int, f *confFFW) error {
	if err := m.audLinear(prog, a, T, &f.l1, a.x, 1, f.pre, 0, 1, m.zero, a.g); err != nil {
		return err
	}
	if err := m.encPrepX(prog, T, f.l2.l.k, 3, 0, 1, 0, a.g, m.zero, m.zero, &f.l2, a.xq, a.xs); err != nil {
		return err
	}
	if err := m.encMatmul(prog, T, f.l2.l, a.xq, a.xs, a.y); err != nil {
		return err
	}
	return m.encPostF(prog, T, a.d, 2, audResid, a.y, &f.l2, false, m.zero, f.post, a.x, 1)
}

// Audio runs the encoder on frames x bins log-mel values and returns the reserved ids of its
// tokens, one per 40 ms.
func (m *Model) Audio(mel []float32, frames, bins int) ([]int, error) {
	a := m.aenc
	if a == nil || m.bridge == nil {
		return nil, errors.New("this model has no audio encoder")
	}
	if bins != a.bins || frames < 4 || frames > AudMaxFrames || len(mel) != frames*bins {
		return nil, errors.New("model: audio is 4 to " + itoa(AudMaxFrames) + " frames of " + itoa(a.bins) + " log-mel bins")
	}
	if err := a.size(m); err != nil {
		return nil, err
	}
	T1 := (frames-1)/2 + 1
	T := (T1-1)/2 + 1
	F1 := (a.bins + 1) / 2
	F2 := (F1 + 1) / 2
	if F2*a.c1 != a.inProj.l.k {
		return nil, errors.New("model: the audio convolutions do not fit input_proj")
	}
	in := make([]byte, 4*len(mel))
	for i, v := range mel {
		b := math.Float32bits(v)
		in[4*i] = byte(b)
		in[4*i+1] = byte(b >> 8)
		in[4*i+2] = byte(b >> 16)
		in[4*i+3] = byte(b >> 24)
	}
	if err := m.Dev.Write(a.mel.buf, a.mel.off, in); err != nil {
		return nil, err
	}
	prog := &webgpu.Program{}
	// E8: subsampling.
	c0, err := m.pipe("encconv2d", "CIN=1", "COUT="+itoa(a.c0), "FIN="+itoa(a.bins), "TIN="+itoa(frames), "EPS=0.000001")
	if err != nil {
		return nil, err
	}
	g0, err := m.group(c0, a.mel, a.conv0w, a.conv0n, a.s0)
	if err != nil {
		return nil, err
	}
	m.dispatch(prog, c0, g0, T1*F1)
	c1, err := m.pipe("encconv2d", "CIN="+itoa(a.c0), "COUT="+itoa(a.c1), "FIN="+itoa(F1), "TIN="+itoa(T1), "EPS=0.000001")
	if err != nil {
		return nil, err
	}
	g1, err := m.group(c1, a.s0, a.conv1w, a.conv1n, a.s1)
	if err != nil {
		return nil, err
	}
	m.dispatch(prog, c1, g1, T*F2)
	if err := m.audLinear(prog, a, T, &a.inProj, a.s1, 0, m.zero, 0, 1, m.zero, a.x); err != nil {
		return nil, err
	}
	rp, err := m.pipe("encrelpos", "C="+itoa(a.d), "PAST="+itoa(audPast))
	if err != nil {
		return nil, err
	}
	rg, err := m.group(rp, a.rp)
	if err != nil {
		return nil, err
	}
	prog.Dispatch(rp, []webgpu.Group{rg}, audPast+1, 1, 1)
	if err := m.flush(prog); err != nil {
		return nil, err
	}
	ra, err := m.pipe("encrelattn", "HD="+itoa(a.hd), "PAST="+itoa(audPast), "CAP=50")
	if err != nil {
		return nil, err
	}
	dw, err := m.pipe("encdwconv", "C="+itoa(a.d), "KW="+itoa(audKW))
	if err != nil {
		return nil, err
	}
	layers := a.layers
	for li := range layers {
		L := &layers[li]
		// E9: x += 1/2 ffw1(x).
		if err := m.audFFW(prog, a, T, &L.ff1); err != nil {
			return nil, err
		}
		// E9: attention.
		if err := m.audLinear(prog, a, T, &L.q, a.x, 1, L.preAttn, 0, 1, m.zero, a.q); err != nil {
			return nil, err
		}
		if err := m.audLinear(prog, a, T, &L.k, a.x, 1, L.preAttn, 0, 1, m.zero, a.k); err != nil {
			return nil, err
		}
		if err := m.audLinear(prog, a, T, &L.v, a.x, 1, L.preAttn, 0, 1, m.zero, a.v); err != nil {
			return nil, err
		}
		if err := m.audLinear(prog, a, audPast+1, &L.relK, a.rp, 0, m.zero, 0, 1, m.zero, a.rel); err != nil {
			return nil, err
		}
		ag, err := m.group(ra, a.q, a.k, a.v, a.rel, L.pds, a.att)
		if err != nil {
			return nil, err
		}
		prog.Dispatch(ra, []webgpu.Group{ag}, a.heads, T, 1)
		if err := m.audLinear(prog, a, T, &L.post, a.att, 0, m.zero, 2, 1, L.postAttn, a.x); err != nil {
			return nil, err
		}
		if err := m.flush(prog); err != nil {
			return nil, err
		}
		// E9: light convolution.
		if err := m.audLinear(prog, a, T, &L.lstart, a.x, 1, L.lpre, 0, 1, m.zero, a.a2); err != nil {
			return nil, err
		}
		dg, err := m.group(dw, a.a2, L.dw, a.c)
		if err != nil {
			return nil, err
		}
		m.dispatch(prog, dw, dg, T)
		if err := m.encPrepX(prog, T, a.d, 0, 1, 1, 1, a.c, m.zero, L.cnorm, &L.lend, a.xq, a.xs); err != nil {
			return nil, err
		}
		if err := m.encMatmul(prog, T, L.lend.l, a.xq, a.xs, a.y); err != nil {
			return nil, err
		}
		if err := m.encPostF(prog, T, a.d, 1, 1, a.y, &L.lend, false, m.zero, m.zero, a.x, 1); err != nil {
			return nil, err
		}
		// E9: x += 1/2 ffw2(x), then norm_out.
		if err := m.audFFW(prog, a, T, &L.ff2); err != nil {
			return nil, err
		}
		if err := m.encPostF(prog, T, a.d, 4, 1, a.x, nil, false, m.zero, L.normOut, a.alt, 1); err != nil {
			return nil, err
		}
		a.x, a.alt = a.alt, a.x
		if err := m.flush(prog); err != nil {
			return nil, err
		}
	}
	// E10: output_proj, embed_audio, the bridge.
	if err := m.encPrep(prog, T, a.d, 0, 0, a.x, m.zero, m.zero, nil, a.xq, a.xs); err != nil {
		return nil, err
	}
	if err := m.encMatmul(prog, T, a.outProj.l, a.xq, a.xs, a.y); err != nil {
		return nil, err
	}
	if err := m.encPost(prog, T, a.outW, 0, a.y, nil, true, a.outBias, m.zero, a.o1, 1); err != nil {
		return nil, err
	}
	if err := m.audLinear(prog, a, T, &a.embed, a.o1, 3, m.zero, 0, 1, m.zero, a.e); err != nil {
		return nil, err
	}
	br := m.bridge
	if err := m.encPrep(prog, T, br.l.k, 2, 0, a.e, br.meanF, m.zero, nil, a.xq, a.xs); err != nil {
		return nil, err
	}
	if err := m.encMatmul(prog, T, br.l, a.xq, a.xs, a.y); err != nil {
		return nil, err
	}
	if err := m.encPost(prog, T, m.C.Dim, 3, a.y, nil, true, br.meanRow, m.zero, a.fin, br.rms); err != nil {
		return nil, err
	}
	if err := m.flush(prog); err != nil {
		return nil, err
	}
	keep := map[int]slot{}
	old := m.slots // wasigoc: a range over a field does not resolve
	for id, sl := range old {
		if sl.kind != "audio" {
			keep[id] = sl
		}
	}
	base := m.C.Vocab + mediaRange
	ids := make([]int, T)
	for i := 0; i < T; i++ {
		ids[i] = base + i
		keep[ids[i]] = slot{kind: "audio", gpu: true, buf: a.fin.buf, off: a.fin.off + 4*m.C.Dim*i}
	}
	m.slots = keep
	return ids, nil
}
