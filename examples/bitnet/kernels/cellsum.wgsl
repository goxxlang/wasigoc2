// A vector's cell sums on one axis (hodgemix eval/flow.go's nodes): out[c] = sum of the M channels of
// cell c, listed cell-major in order. MODE 1 sums the FFN hidden act(a) * b instead (the gate's
// activation as norm.wgsl ACT applies it). One thread a cell, a fixed order: deterministic.

override NCELL: u32;
override M: u32;             // channels per cell
override MODE: u32 = 0u;
override ACT: u32 = 0u;      // MODE 1: 0 relu^2, 1 silu, 2 gelu tanh

@group(0) @binding(0) var<storage, read> a: array<f32>;
@group(0) @binding(1) var<storage, read> b: array<f32>;      // MODE 1 only
@group(0) @binding(2) var<storage, read> order: array<u32>;  // NCELL * M channels, cell-major
@group(0) @binding(3) var<storage, read_write> out: array<f32>;

fn v(j: u32) -> f32 {
  if (MODE == 1u) {
    let x = a[j];
    if (ACT == 1u) {
      return x / (1.0 + exp(-x)) * b[j];
    }
    if (ACT == 2u) {
      return 0.5 * x * (1.0 + tanh(0.7978845608028654 * (x + 0.044715 * x * x * x))) * b[j];
    }
    let g = max(x, 0.0);
    return g * g * b[j];
  }
  return a[j];
}

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid: vec3u) {
  let c = gid.x;
  if (c >= NCELL) {
    return;
  }
  var s = 0.0;
  for (var t = 0u; t < M; t++) {
    s += v(order[c * M + t]);
  }
  out[c] = s;
}
