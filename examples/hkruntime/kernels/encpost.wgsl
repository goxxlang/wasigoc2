// An encoder linear's output, per token: y = clamp(y, cl[2], cl[3]) (CLIP), + bias (BIAS); then
//   OUT 0: d = y
//   OUT 1: d += F * y                             (a residual add)
//   OUT 2: d += F * rmsnorm(y) * gain             (a post-norm residual, NORM 1 w, 2 (1 + w))
//   OUT 3: d = y * TARGET / rms(y)                (rms matched: the bridge onto the residual axis)
//   OUT 4: d = rmsnorm(y) * gain                  (a norm into another buffer)
// One workgroup per token.

override N: u32;
override EPS: f32 = 1e-6;
override CLIP: bool = false;
override BIAS: bool = false;
override OUT: u32 = 0u;
override NORM: u32 = 1u;
override F: f32 = 1.0;
override TARGET: f32 = 1.0;

@group(0) @binding(0) var<storage, read> y: array<f32>;          // T x N
@group(0) @binding(1) var<storage, read> cl: array<f32>;         // [in lo, in hi, out lo, out hi]
@group(0) @binding(2) var<storage, read> bias: array<f32>;       // N
@group(0) @binding(3) var<storage, read> nw: array<f32>;         // N
@group(0) @binding(4) var<storage, read_write> d: array<f32>;    // T x N

var<workgroup> red: array<f32, 256>;

fn val(t: u32, i: u32) -> f32 {
  var x = y[t * N + i];
  if (CLIP) {
    x = clamp(x, cl[2], cl[3]);
  }
  if (BIAS) {
    x += bias[i];
  }
  return x;
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

@compute @workgroup_size(256)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nwg: vec3u,
        @builtin(local_invocation_index) tid: u32) {
  let t = wg.y * nwg.x + wg.x;
  var sc = 1.0;
  if (OUT >= 2u) {
    var ss = 0.0;
    for (var i = tid; i < N; i += 256u) {
      let x = val(t, i);
      ss += x * x;
    }
    let ms = sum256(ss, tid) / f32(N);
    if (OUT == 2u || OUT == 4u) {
      sc = inverseSqrt(ms + EPS);
    } else {
      sc = TARGET * inverseSqrt(max(ms, 1e-30));
    }
  }
  for (var i = tid; i < N; i += 256u) {
    let x = val(t, i) * sc;
    let at = t * N + i;
    if (OUT == 0u || OUT == 3u) {
      d[at] = x;
    } else if (OUT == 1u) {
      d[at] = d[at] + F * x;
    } else {
      var g = nw[i];
      if (NORM == 2u) {
        g = 1.0 + g;
      }
      if (OUT == 4u) {
        d[at] = x * g;
      } else {
        d[at] = d[at] + F * x * g;
      }
    }
  }
}
