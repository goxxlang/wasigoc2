// The audio encoder's light convolution (decision E9): GLU over linear_start's two halves,
// g = a[:C] * sigmoid(a[C:]), then a causal depthwise convolution over time,
// y[t, c] = sum_k w[c, k] g[t - (KW - 1) + k, c]. Workgroup per frame.

override C: u32;
override KW: u32 = 5u;

@group(0) @binding(0) var<storage, read> a: array<f32>;          // T x 2C
@group(0) @binding(1) var<storage, read> w: array<f32>;          // C x KW
@group(0) @binding(2) var<storage, read_write> y: array<f32>;    // T x C

fn glu(t: u32, c: u32) -> f32 {
  let g = a[t * 2u * C + C + c];
  return a[t * 2u * C + c] / (1.0 + exp(-g));
}

@compute @workgroup_size(256)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nwg: vec3u,
        @builtin(local_invocation_index) tid: u32) {
  let t = wg.y * nwg.x + wg.x;
  for (var c = tid; c < C; c += 256u) {
    var s = 0.0;
    for (var k = 0u; k < KW; k++) {
      let ti = i32(t) - i32(KW - 1u) + i32(k);
      if (ti >= 0) {
        s += w[c * KW + k] * glu(u32(ti), c);
      }
    }
    y[t * C + c] = s;
  }
}
