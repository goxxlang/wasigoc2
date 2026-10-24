// The token's embedding row (bf16) into the residual stream x (f32). The
// embedding may be split across buffers; this binding is the chunk holding
// the token, and st.row is the token's row within it.

override D: u32;
override SCALE: f32 = 1.0; // Gemma multiplies the token by sqrt(hidden). 1 for every other model.

struct Step {
  pos: u32,
  row: u32,
  p0: u32,
  p1: u32,
}

@group(0) @binding(0) var<uniform> st: Step;
@group(0) @binding(1) var<storage, read> emb: array<u32>;       // rows x D bf16, two per word
@group(0) @binding(2) var<storage, read_write> x: array<f32>;

@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) gid: vec3u) {
  let i = gid.x;
  if (i >= D) {
    return;
  }
  let e = st.row * D + i;
  let w = emb[e >> 1u];
  x[i] = bitcast<f32>(select(w << 16u, w & 0xffff0000u, (e & 1u) == 1u)) * SCALE;
}
