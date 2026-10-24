// Embedding lookup. Ternary codes are packed 2 bits per value, and the
// value is code - 1, the same packing HodgeLinear uses for a weight byte.
// Tokens are vertices of a path. The coboundary on an edge is v[i+1] - v[i].
// One Laplacian step smooths that path. The result is the mean vertex signal.

override N: u32;
const DIM: u32 = 32u;

@group(0) @binding(0) var<storage, read> tokens: array<u32>;
@group(0) @binding(1) var<storage, read> lut: array<u32>;
@group(0) @binding(2) var<storage, read_write> outp: array<f32>;

fn code_at(bucket: u32, d: u32) -> f32 {
  let idx = bucket * DIM + d;
  let w = lut[idx / 16u];
  let shift = (idx % 16u) * 2u;
  let code = (w >> shift) & 3u;
  return f32(code) - 1.0;
}

@compute @workgroup_size(32)
fn main(@builtin(global_invocation_id) gid: vec3u) {
  let d = gid.x;
  if (d >= DIM) {
    return;
  }
  var sum = 0.0;
  for (var i = 0u; i < N; i++) {
    let v = code_at(tokens[i], d);
    var lv = 0.0;
    if (N > 1u) {
      if (i == 0u) {
        lv = v - code_at(tokens[1u], d);
      } else if (i + 1u == N) {
        lv = v - code_at(tokens[i - 1u], d);
      } else {
        lv = 2.0 * v - code_at(tokens[i - 1u], d) - code_at(tokens[i + 1u], d);
      }
    }
    sum += v - 0.5 * lv;
  }
  outp[d] = sum / f32(max(N, 1u));
}
