// The audio encoder's local attention (decision E9). Frame t attends to frames t - PAST .. t.
// The query is scaled per dim by HD^-1/2 / softplus(0) * softplus(per_dim_scale); a key at
// distance d scores q.k + q.rel[PAST - d], capped as CAP * tanh(score / CAP), then softmax.
// Workgroup (head, frame), one thread per head dim.

override HD: u32;
override PAST: u32 = 12u;
override CAP: f32 = 50.0;

@group(0) @binding(0) var<storage, read> q: array<f32>;          // T x H*HD
@group(0) @binding(1) var<storage, read> k: array<f32>;
@group(0) @binding(2) var<storage, read> v: array<f32>;
@group(0) @binding(3) var<storage, read> rel: array<f32>;        // (PAST + 1) x H*HD
@group(0) @binding(4) var<storage, read> pds: array<f32>;        // HD, per_dim_scale
@group(0) @binding(5) var<storage, read_write> o: array<f32>;    // T x H*HD

var<workgroup> red: array<f32, 256>;
var<workgroup> sc: array<f32, 64>;

fn sum(x: f32, tid: u32) -> f32 {
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
  let h = wg.x;
  let H = nwg.x;
  let t = wg.z * nwg.y + wg.y;
  let stride = H * HD;
  let on = tid < HD;
  var qd = 0.0;
  if (on) {
    let sp = log(1.0 + exp(pds[tid]));
    qd = q[t * stride + h * HD + tid] * inverseSqrt(f32(HD)) / log(2.0) * sp;
  }
  let first = i32(t) - i32(PAST);
  var mx = -3.0e38;
  for (var j = 0u; j <= PAST; j++) {
    let kt = first + i32(j);
    var s = 0.0;
    if (kt >= 0) {
      var part = 0.0;
      if (on) {
        let d = t - u32(kt);
        part = qd * (k[u32(kt) * stride + h * HD + tid] + rel[(PAST - d) * stride + h * HD + tid]);
      }
      s = sum(part, tid);
      s = CAP * tanh(s / CAP);
      mx = max(mx, s);
    }
    if (tid == 0u) {
      sc[j] = s;
    }
  }
  workgroupBarrier();
  var total = 0.0;
  var acc = 0.0;
  for (var j = 0u; j <= PAST; j++) {
    let kt = first + i32(j);
    if (kt < 0) {
      continue;
    }
    let e = exp(sc[j] - mx);
    total += e;
    if (on) {
      acc += e * v[u32(kt) * stride + h * HD + tid];
    }
  }
  if (on) {
    o[t * stride + h * HD + tid] = acc / total;
  }
}
