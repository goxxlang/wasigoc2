// RMSNorm, then (for a HodgeLinear's input) per-token absmax int8 quantization.
//
//   MODE 0: v = a                      -> rmsnorm -> int8 (xq, xs)
//   MODE 1: v = act(a) * b             -> rmsnorm -> int8   (the gated FFN:
//           act(gate) * up, then ffn_sub_norm, then down_proj's quant;
//           ACT 0 relu^2 (Hodge), 1 silu (Llama, Qwen), 2 gelu tanh)
//   MODE 2: v = a                      -> rmsnorm -> f32 y  (the final norm)
//
// rmsnorm(v) = v / sqrt(mean(v^2) + EPS) * gain. gain is the weight, or (1+weight)
// when GEMMA is 1 (Gemma, VaultGemma). Quantization is
// per-token absmax: s = 127 / max|y|, q = clamp(round(y s), -128, 127), and the
// dequant scale 1 / s goes to xs[0]. One workgroup per vector.

override K: u32;
override EPS: f32 = 1e-5;
override MODE: u32 = 0u;
override ACT: u32 = 0u;
override GEMMA: u32 = 0u; // 0: y * w (Llama, Qwen, Hodge). 1: y * (1+w) (Gemma, VaultGemma).

@group(0) @binding(0) var<storage, read> a: array<f32>;
@group(0) @binding(1) var<storage, read> b: array<f32>;          // MODE 1 only
@group(0) @binding(2) var<storage, read> nw: array<f32>;         // the norm's weight
@group(0) @binding(3) var<storage, read_write> xq: array<u32>;   // K/4 words of int8
@group(0) @binding(4) var<storage, read_write> xs: array<f32>;
@group(0) @binding(5) var<storage, read_write> y: array<f32>;    // MODE 2 only

var<workgroup> red: array<f32, 256>;

fn v(i: u32) -> f32 {
  if (MODE == 1u) {
    let x = a[i];
    if (ACT == 1u) {
      return x / (1.0 + exp(-x)) * b[i];
    }
    if (ACT == 2u) {
      return 0.5 * x * (1.0 + tanh(0.7978845608028654 * (x + 0.044715 * x * x * x))) * b[i];
    }
    let g = max(x, 0.0);
    return g * g * b[i];
  }
  return a[i];
}

fn sum256(x: f32, tid: u32) -> f32 {
  red[tid] = x;
  workgroupBarrier();
  for (var s = 128u; s > 0u; s >>= 1u) {
    if (tid < s) {
      red[tid] += red[tid + s];
    }
    workgroupBarrier();
  }
  let r = red[0];
  workgroupBarrier();
  return r;
}

// gain is the per-channel multiplier. An all-ones weight is a sub-norm the layout
// inserted: that path is the identity (multiply by 1), including when GEMMA is on,
// where a stored 1 would otherwise become 2.
fn gain(i: u32, unit: bool) -> f32 {
  if (unit && MODE != 2u) {
    return 1.0;
  }
  if (GEMMA == 1u) {
    return 1.0 + nw[i];
  }
  return nw[i];
}

fn max256(x: f32, tid: u32) -> f32 {
  red[tid] = x;
  workgroupBarrier();
  for (var s = 128u; s > 0u; s >>= 1u) {
    if (tid < s) {
      red[tid] = max(red[tid], red[tid + s]);
    }
    workgroupBarrier();
  }
  let r = red[0];
  workgroupBarrier();
  return r;
}

@compute @workgroup_size(256)
fn main(@builtin(local_invocation_index) tid: u32) {
  var ss = 0.0;
  var ones = 0.0;
  for (var i = tid; i < K; i += 256u) {
    let x = v(i);
    ss += x * x;
    if (nw[i] == 1.0) {
      ones += 1.0;
    }
  }
  let invRms = inverseSqrt(sum256(ss, tid) / f32(K) + EPS);
  // A gain of all ones is a sub-norm the layout inserted. Skip the rms and keep the
  // per-channel multiply (ones) so the following quant sees the sublayer output.
  let unit = sum256(ones, tid) == f32(K);
  let inv = select(invRms, 1.0, unit && MODE != 2u);

  if (MODE == 2u) {
    for (var i = tid; i < K; i += 256u) {
      y[i] = v(i) * inv * gain(i, unit);
    }
    return;
  }

  var m = 0.0;
  for (var i = tid; i < K; i += 256u) {
    m = max(m, abs(v(i) * inv * gain(i, unit)));
  }
  let s = 127.0 / max(max256(m, tid), 1e-5);
  for (var g = tid; g < K / 4u; g += 256u) {
    var word = 0u;
    for (var j = 0u; j < 4u; j++) {
      let i = g * 4u + j;
      let q = i32(clamp(round(v(i) * inv * gain(i, unit) * s), -128.0, 127.0));
      word |= (bitcast<u32>(q) & 0xffu) << (8u * j);
    }
    xq[g] = word;
  }
  if (tid == 0u) {
    xs[0] = 1.0 / s;
  }
}
