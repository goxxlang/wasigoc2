// Bidirectional attention for an encoder: every token attends to all T tokens.
// o[t, h] = sum_j softmax_j(SCALE * q[t, h] . k[j, h]) v[j, h]. Workgroup (head, query);
// the T scores sit in workgroup memory, so T is an override (at most about 3500).

override T: u32;
override HD: u32;
override SCALE: f32 = 1.0;

@group(0) @binding(0) var<storage, read> q: array<f32>;          // T x H*HD
@group(0) @binding(1) var<storage, read> k: array<f32>;
@group(0) @binding(2) var<storage, read> v: array<f32>;
@group(0) @binding(3) var<storage, read_write> o: array<f32>;    // T x H*HD

var<workgroup> sc: array<f32, T>;
var<workgroup> sq: array<f32, 256>;
var<workgroup> red: array<f32, 256>;

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

@compute @workgroup_size(256)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nwg: vec3u,
        @builtin(local_invocation_index) tid: u32) {
  let h = wg.x;
  let H = nwg.x;
  let t = wg.z * nwg.y + wg.y;
  let stride = H * HD;
  let qb = t * stride + h * HD;
  for (var i = tid; i < HD; i += 256u) {
    sq[i] = q[qb + i];
  }
  workgroupBarrier();
  var m = -3.0e38;
  for (var j = tid; j < T; j += 256u) {
    let kb = j * stride + h * HD;
    var s = 0.0;
    for (var i = 0u; i < HD; i++) {
      s += sq[i] * k[kb + i];
    }
    s *= SCALE;
    sc[j] = s;
    m = max(m, s);
  }
  let mx = reduce(m, tid, true);
  var sum = 0.0;
  for (var j = tid; j < T; j += 256u) {
    let e = exp(sc[j] - mx);
    sc[j] = e;
    sum += e;
  }
  let inv = 1.0 / reduce(sum, tid, false);
  // Four groups of lanes split the keys; each lane holds one head dim.
  let parts = 256u / HD;
  let d = tid % HD;
  let part = tid / HD;
  var acc = 0.0;
  if (part < parts) {
    for (var j = part; j < T; j += parts) {
      acc += sc[j] * v[j * stride + h * HD + d];
    }
  }
  red[tid] = acc;
  workgroupBarrier();
  if (part == 0u) {
    var total = 0.0;
    for (var p = 0u; p < parts; p++) {
      total += red[p * HD + d];
    }
    o[qb + d] = total * inv;
  }
}
