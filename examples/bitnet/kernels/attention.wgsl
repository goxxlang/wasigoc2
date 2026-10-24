// Causal attention for one new token: softmax(q k^T / sqrt(DENOM)) v over the
// cached positions 0..pos, one workgroup per query head. Grouped-query: head
// h reads key/value head h / (H / KVH).

override H: u32;
override KVH: u32;
override HD: u32;
override CTX: u32;
override DENOM: f32 = 0.0; // query_pre_attn_scalar when the config sets one; 0 means head_dim.

struct Step {
  pos: u32,
  row: u32,
  p0: u32,
  p1: u32,
}

@group(0) @binding(0) var<uniform> st: Step;
@group(0) @binding(1) var<storage, read> q: array<f32>;          // H * HD
@group(0) @binding(2) var<storage, read> kc: array<f32>;         // CTX * KVH * HD
@group(0) @binding(3) var<storage, read> vc: array<f32>;
@group(0) @binding(4) var<storage, read_write> att: array<f32>;  // H * CTX scratch
@group(0) @binding(5) var<storage, read_write> o: array<f32>;    // H * HD

var<workgroup> red: array<f32, 256>;

fn reduce(x: f32, tid: u32, is_max: bool) -> f32 {
  red[tid] = x;
  workgroupBarrier();
  for (var s = 128u; s > 0u; s >>= 1u) {
    if (tid < s) {
      if (is_max) {
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
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(local_invocation_index) tid: u32) {
  let h = wg.x;
  let kvh = h / (H / KVH);
  let n = st.pos + 1u;
  let qb = h * HD;
  let ab = h * CTX;
  let denom = select(f32(HD), DENOM, DENOM > 0.0);
  let scale = inverseSqrt(denom);

  var m = -3.0e38;
  for (var t = tid; t < n; t += 256u) {
    let kb = (t * KVH + kvh) * HD;
    var s = 0.0;
    for (var d = 0u; d < HD; d++) {
      s += q[qb + d] * kc[kb + d];
    }
    s *= scale;
    att[ab + t] = s;
    m = max(m, s);
  }
  let mx = reduce(m, tid, true);

  var sum = 0.0;
  for (var t = tid; t < n; t += 256u) {
    let e = exp(att[ab + t] - mx);
    att[ab + t] = e;
    sum += e;
  }
  let total = reduce(sum, tid, false);
  storageBarrier();

  for (var d = tid; d < HD; d += 256u) {
    var acc = 0.0;
    for (var t = 0u; t < n; t++) {
      acc += att[ab + t] * vc[(t * KVH + kvh) * HD + d];
    }
    o[qb + d] = acc / total;
  }
}
