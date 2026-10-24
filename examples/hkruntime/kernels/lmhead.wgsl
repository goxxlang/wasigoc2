// The output projection: logits = y E^T over the rows of one embedding
// chunk (tied weights: the embedding is the LM head), bf16 E, f32 y.

override D: u32;      // hidden size (even)
override ROWS: u32;   // rows in this chunk

const PER = 8u;       // rows per workgroup
const LANES = 32u;

@group(0) @binding(0) var<storage, read> y: array<f32>;
@group(0) @binding(1) var<storage, read> emb: array<u32>;          // ROWS x D bf16
@group(0) @binding(2) var<storage, read_write> logits: array<f32>; // this chunk's ROWS

var<workgroup> sy: array<f32, D>;
var<workgroup> red: array<f32, 256>;

@compute @workgroup_size(256)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nwg: vec3u,
        @builtin(local_invocation_index) tid: u32) {
  for (var i = tid; i < D; i += 256u) {
    sy[i] = y[i];
  }
  workgroupBarrier();
  let lane = tid % LANES;
  let r = (wg.y * nwg.x + wg.x) * PER + tid / LANES;
  var acc = 0.0;
  if (r < ROWS) {
    let base = r * (D / 2u);
    for (var k = lane; k < D / 2u; k += LANES) {
      let w = emb[base + k];
      acc += sy[2u * k] * bitcast<f32>(w << 16u) + sy[2u * k + 1u] * bitcast<f32>(w & 0xffff0000u);
    }
  }
  red[tid] = acc;
  workgroupBarrier();
  for (var s = LANES / 2u; s > 0u; s >>= 1u) {
    if (lane < s) {
      red[tid] += red[tid + s];
    }
    workgroupBarrier();
  }
  if (lane == 0u && r < ROWS) {
    logits[r] = red[tid];
  }
}
