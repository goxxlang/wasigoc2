// An encoder's queries and keys in place, per token and head: RMS norm with the head norm's
// weight (q_norm, k_norm), then 2-D rotary position. The first half of the head turns with the
// patch column, the second with its row; within a half, dim j pairs with j + HD/4 (rotate-half)
// at frequency THETA^(-2j / (HD/2)). Token t is patch (t / COLS, t % COLS).
// Workgroup (head, token).

override HD: u32;
override COLS: u32;
override THETA: f32 = 100.0;
override EPS: f32 = 1e-6;

@group(0) @binding(0) var<storage, read_write> q: array<f32>;   // T x H*HD
@group(0) @binding(1) var<storage, read_write> k: array<f32>;   // T x H*HD
@group(0) @binding(2) var<storage, read> qn: array<f32>;        // HD
@group(0) @binding(3) var<storage, read> kn: array<f32>;        // HD

var<workgroup> hv: array<f32, 512>;
var<workgroup> red: array<f32, 64>;

fn turn(which: u32, base: u32, pos: vec2<f32>, tid: u32) {
  // RMS norm of this head.
  var ss = 0.0;
  for (var i = tid; i < HD; i += 64u) {
    var x = 0.0;
    if (which == 0u) {
      x = q[base + i];
    } else {
      x = k[base + i];
    }
    hv[i] = x;
    ss += x * x;
  }
  red[tid] = ss;
  workgroupBarrier();
  for (var s = 32u; s > 0u; s >>= 1u) {
    if (tid < s) {
      red[tid] += red[tid + s];
    }
    workgroupBarrier();
  }
  let inv = inverseSqrt(red[0] / f32(HD) + EPS);
  workgroupBarrier();
  for (var i = tid; i < HD; i += 64u) {
    var g = 0.0;
    if (which == 0u) {
      g = qn[i];
    } else {
      g = kn[i];
    }
    hv[i] = hv[i] * inv * g;
  }
  workgroupBarrier();
  let half = HD / 2u;
  let quarter = HD / 4u;
  for (var i = tid; i < half; i += 64u) {
    let side = i / quarter;            // 0 column, 1 row
    let j = i % quarter;
    let at = side * half + j;
    let ang = pos[side] * pow(THETA, -2.0 * f32(j) / f32(half));
    let c = cos(ang);
    let s = sin(ang);
    let x1 = hv[at];
    let x2 = hv[at + quarter];
    let r1 = x1 * c - x2 * s;
    let r2 = x2 * c + x1 * s;
    if (which == 0u) {
      q[base + at] = r1;
      q[base + at + quarter] = r2;
    } else {
      k[base + at] = r1;
      k[base + at + quarter] = r2;
    }
  }
  workgroupBarrier();
}

@compute @workgroup_size(64)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nwg: vec3u,
        @builtin(local_invocation_index) tid: u32) {
  let h = wg.x;
  let t = wg.z * nwg.y + wg.y;
  let base = (t * nwg.x + h) * HD;
  let pos = vec2<f32>(f32(t % COLS), f32(t / COLS));
  turn(0u, base, pos, tid);
  turn(1u, base, pos, tid);
}
