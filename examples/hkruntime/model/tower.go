package model

// The L-A vision and audio towers' embedding layers (hodgemix towers.go, tower/project.go).
// The page runs the towers (tower.wasm) and sends each token's features and RoPE phase;
// here they are projected into the residual stream and held as reserved ids past the
// vocabulary. queueEmbed writes a reserved id's vector in place of an embedding row,
// and its phase goes to rope.wgsl in the step uniform.

import (
	"errors"
	"math"
	"strings"

	"safetensors"
	"webgpu"
)

// VisSlots is the vision range, 14x14 patches. Audio ids start past it (hodgemix tower/names.go).
const VisSlots = 196

const (
	visEmbed = "bnzmix.embed_vision"
	audEmbed = "bnzmix.embed_audio"
)

// towerTab is one modality's projector [D, df], the feature mean it was centred on,
// the embedding row mean, and the embedding rms it matches.
type towerTab struct {
	d, df   int
	w       []float32
	meanF   []float32
	meanRow []float32
	rms     float64
}

// slot is a reserved id's residual-stream vector and RoPE phase: f32 bytes x, or (gpu) the
// vector already on the device at buf+off (encoder.go).
type slot struct {
	kind  string
	x     []byte
	phase float32
	gpu   bool
	buf   webgpu.Buffer
	off   int
}

// HasTowers reports that the checkpoint carries both embedding layers.
func (m *Model) HasTowers() bool {
	return m.vis != nil && m.aud != nil
}

func le32(b []byte) uint32 {
	return uint32(b[0]) | uint32(b[1])<<8 | uint32(b[2])<<16 | uint32(b[3])<<24
}

func f32At(b []byte, i int) float32 {
	return math.Float32frombits(le32(b[4*i:]))
}

func bf16At(b []byte, i int) float32 {
	return math.Float32frombits((uint32(b[2*i]) | uint32(b[2*i+1])<<8) << 16)
}

// isTowerTensor is a tensor of the embedding layers; upload keeps its bytes (towerRaw).
func isTowerTensor(name string) bool {
	return strings.HasPrefix(name, visEmbed+".") || strings.HasPrefix(name, audEmbed+".") || strings.HasPrefix(name, bridgeW+".")
}

// loadTowers builds the embedding layers from the bytes upload kept. A checkpoint
// without them has no towers.
func (m *Model) loadTowers(st *safetensors.File) error {
	raw := m.towerRaw
	m.towerRaw = nil
	m.bridgeRMS(raw)
	if st.Get(visEmbed+".weight") == nil && st.Get(audEmbed+".weight") == nil {
		return nil
	}
	vis, err := towerOf(st, raw, visEmbed)
	if err != nil {
		return err
	}
	aud, err := towerOf(st, raw, audEmbed)
	if err != nil {
		return err
	}
	if vis.d != m.C.Dim || aud.d != m.C.Dim {
		return errors.New("model: tower width " + itoa(vis.d) + "/" + itoa(aud.d) + ", hidden " + itoa(m.C.Dim))
	}
	m.vis, m.aud = vis, aud
	return nil
}

func towerOf(st *safetensors.File, raw map[string][]byte, prefix string) (*towerTab, error) {
	wt := st.Get(prefix + ".weight")
	if wt == nil || len(wt.Shape) != 2 {
		return nil, errors.New("model: no " + prefix + ".weight")
	}
	tab := &towerTab{}
	wb := raw[prefix+".weight"]
	if wt.DType == "U8" {
		r4, df := wt.Shape[0], wt.Shape[1]
		tab.d, tab.df = 4*r4, df
		sc := raw[prefix+".weight_scale"]
		if len(sc) < 2 {
			return nil, errors.New("model: no " + prefix + ".weight_scale")
		}
		scale := bf16At(sc, 0)
		w := make([]float32, tab.d*df)
		n := r4 * df
		for s := 0; s < 4; s++ {
			for i := 0; i < n; i++ {
				w[s*n+i] = float32(int((wb[i]>>uint(2*s))&3)-1) * scale
			}
		}
		for l := 1; ; l++ {
			ln := prefix + ".weight_limb" + itoa(l)
			d := raw[ln]
			if d == nil {
				break
			}
			ls := raw[ln+"_scale"]
			lr := raw[ln+"_radix"]
			if len(d) != n || len(ls) < 2 || len(lr) < 4 {
				return nil, errors.New("model: " + ln + " is incomplete")
			}
			lscale := bf16At(ls, 0)
			c := (int(le32(lr)) - 1) / 2
			for s := 0; s < 4; s++ {
				for i := 0; i < n; i++ {
					w[s*n+i] += lscale * float32(int((d[i]>>uint(2*s))&3)-c)
				}
			}
		}
		tab.w = w
	} else if wt.DType == "F32" {
		tab.d, tab.df = wt.Shape[0], wt.Shape[1]
		w := make([]float32, tab.d*tab.df)
		for i := 0; i < len(w); i++ {
			w[i] = f32At(wb, i)
		}
		tab.w = w
	} else {
		return nil, errors.New("model: " + prefix + ".weight is " + wt.DType)
	}
	mf := raw[prefix+".mean_feature"]
	my := raw[prefix+".mean_row"]
	rms := raw[prefix+".rms"]
	if len(mf) != 4*tab.df || len(my) != 4*tab.d || len(rms) < 4 {
		return nil, errors.New("model: " + prefix + " means do not match the projector")
	}
	tab.meanF = make([]float32, tab.df)
	for i := 0; i < tab.df; i++ {
		tab.meanF[i] = f32At(mf, i)
	}
	tab.meanRow = make([]float32, tab.d)
	for i := 0; i < tab.d; i++ {
		tab.meanRow[i] = f32At(my, i)
	}
	tab.rms = float64(f32At(rms, 0))
	return tab, nil
}

// project is x = P (f - meanF) + meanRow scaled to the embedding rms, as f32 bytes.
func (t *towerTab) project(f []float32) []byte {
	y := make([]float64, t.d)
	ss := 0.0
	for c := 0; c < t.d; c++ {
		s := float64(t.meanRow[c])
		row := c * t.df
		for j := 0; j < t.df; j++ {
			s += float64(t.w[row+j]) * (float64(f[j]) - float64(t.meanF[j]))
		}
		y[c] = s
		ss += s * s
	}
	sc := 1.0
	if r := math.Sqrt(ss / float64(t.d)); r > 0 && t.rms > 0 {
		sc = t.rms / r
	}
	out := make([]byte, 4*t.d)
	for c := 0; c < t.d; c++ {
		v := math.Float32bits(float32(y[c] * sc))
		out[4*c] = byte(v)
		out[4*c+1] = byte(v >> 8)
		out[4*c+2] = byte(v >> 16)
		out[4*c+3] = byte(v >> 24)
	}
	return out
}

// SetMedia projects n tokens of features (n x width, row-major) of kind "vision" or
// "audio" and returns their reserved ids. The kind's earlier ids are dropped: their
// positions are already in the cache.
func (m *Model) SetMedia(kind string, feat []float32, n int, phase []float32) ([]int, error) {
	var tab *towerTab
	base := m.C.Vocab
	if kind == "vision" {
		tab = m.vis
		if n > VisSlots {
			return nil, errors.New("model: an image is at most " + itoa(VisSlots) + " tokens, not " + itoa(n))
		}
	} else if kind == "audio" {
		tab = m.aud
		base += mediaRange
	} else {
		return nil, errors.New("model: no " + kind + " tower")
	}
	if tab == nil {
		return nil, errors.New("this model has no " + kind + " embedding layer")
	}
	if n < 1 || len(feat) != n*tab.df || len(phase) != n {
		return nil, errors.New("model: " + kind + " has " + itoa(len(feat)) + " features for " + itoa(n) + " tokens of " + itoa(tab.df))
	}
	keep := map[int]slot{}
	old := m.slots // wasigoc: a range over a field does not resolve
	for id, sl := range old {
		if sl.kind != kind {
			keep[id] = sl
		}
	}
	ids := make([]int, n)
	for i := 0; i < n; i++ {
		keep[base+i] = slot{kind: kind, x: tab.project(feat[i*tab.df : (i+1)*tab.df]), phase: phase[i]}
		ids[i] = base + i
	}
	m.slots = keep
	return ids, nil
}
