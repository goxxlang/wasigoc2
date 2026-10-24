// An encoder linear's input, per token: v = a (MODE 0), act(a) * b (MODE 1, the gated MLP),
// a - b (MODE 2, b a per-channel mean) or act(a) (MODE 3); then an RMS norm (NORM 0 none,
// 1 times the weight, 2 times (1 + weight), 3 unit); then silu (POST 1); then the clipped
// linear's input clamp to [cl[0], cl[1]] (CLIP); then per-token absmax int8 for
// encmatmul.wgsl. One workgroup per token; token t is a row of a, b and the outputs.

override K: u32;
override EPS: f32 = 1e-6;
override MODE: u32 = 0u;
override ACT: u32 = 2u;       // MODE 1: 1 silu, 2 gelu tanh
override NORM: u32 = 0u;
override CLIP: bool = false;
override POST: u32 = 0u;

@group(0) @binding(0) var<storage, read> a: array<f32>;          // T x K
@group(0) @binding(1) var<storage, read> b: array<f32>;          // MODE 1: T x K; MODE 2: K
@group(0) @binding(2) var<storage, read> nw: array<f32>;         // the norm's weight, K
@group(0) @binding(3) var<storage, read> cl: array<f32>;         // [in lo, in hi, out lo, out hi]
@group(0) @binding(4) var<storage, read_write> xq: array<u32>;   // T x K/4 words of int8
@group(0) @binding(5) var<storage, read_write> xs: array<f32>;   // T dequant scales

var<workgroup> red: array<f32, 256>;

fn act(x: f32) -> f32 {
  if (ACT == 1u) {
    return x / (1.0 + exp(-x));
  }
  return 0.5 * x * (1.0 + tanh(0.7978845608028654 * (x + 0.044715 * x * x * x)));
}

fn v(t: u32, i: u32) -> f32 {
  let x = a[t * K + i];
  if (MODE == 1u) {
    return act(x) * b[t * K + i];
  }
  if (MODE == 3u) {
    return act(x);
  }
  if (MODE == 2u) {
    return x - b[i];
  }
  return x;
}

fn reduce(x: f32, tid: u32, isMax: bool) -> f32 {
  red[tid] = x;
  workgroupBarrier();
  for (var s = 128u; s > 0u; s >>= 1u) {
    if (tid < s) {
      if (isMax) {
        red[tid] = max(red[tid], red[tid + s]);
      } else {
        red[tid] += red[tid + s];
      }
    }
    workgroupBarrier();
  }
  let r = red[0];
  workgroupBarrier();
  return r;
}

fn gain(i: u32) -> f32 {
  if (NORM == 1u) {
    return nw[i];
  }
  if (NORM == 2u) {
    return 1.0 + nw[i];
  }
  return 1.0;
}

fn clip(x0: f32) -> f32 {
  var x = x0;
  if (POST == 1u) {
    x = x / (1.0 + exp(-x));
  }
  if (CLIP) {
    return clamp(x, cl[0], cl[1]);
  }
  return x;
}

@compute @workgroup_size(256)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nwg: vec3u,
        @builtin(local_invocation_index) tid: u32) {
  let t = wg.y * nwg.x + wg.x;
  var inv = 1.0;
  if (NORM != 0u) {
    var ss = 0.0;
    for (var i = tid; i < K; i += 256u) {
      let x = v(t, i);
      ss += x * x;
    }
    inv = inverseSqrt(reduce(ss, tid, false) / f32(K) + EPS);
  }
  var m = 0.0;
  for (var i = tid; i < K; i += 256u) {
    m = max(m, abs(clip(v(t, i) * inv * gain(i))));
  }
  let s = 127.0 / max(reduce(m, tid, true), 1e-5);
  for (var g = tid; g < K / 4u; g += 256u) {
    var word = 0u;
    for (var j = 0u; j < 4u; j++) {
      let i = g * 4u + j;
      let q = i32(clamp(round(clip(v(t, i) * inv * gain(i)) * s), -128.0, 127.0));
      word |= (bitcast<u32>(q) & 0xffu) << (8u * j);
    }
    xq[t * (K / 4u) + g] = word;
  }
  if (tid == 0u) {
    xs[t] = 1.0 / s;
  }
}
