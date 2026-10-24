// A vision encoder's pooling: token p of the pooled grid is the mean of a POOL x POOL block of
// the COLS-wide patch grid. One workgroup per pooled token.

override D: u32;
override COLS: u32;
override POOL: u32 = 3u;

@group(0) @binding(0) var<storage, read> x: array<f32>;          // T x D
@group(0) @binding(1) var<storage, read_write> o: array<f32>;    // P x D

@compute @workgroup_size(256)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nwg: vec3u,
        @builtin(local_invocation_index) tid: u32) {
  let p = wg.y * nwg.x + wg.x;
  let pc = COLS / POOL;
  let r0 = (p / pc) * POOL;
  let c0 = (p % pc) * POOL;
  let inv = 1.0 / f32(POOL * POOL);
  for (var i = tid; i < D; i += 256u) {
    var s = 0.0;
    for (var dy = 0u; dy < POOL; dy++) {
      for (var dx = 0u; dx < POOL; dx++) {
        s += x[((r0 + dy) * COLS + c0 + dx) * D + i];
      }
    }
    o[p * D + i] = s * inv;
  }
}
