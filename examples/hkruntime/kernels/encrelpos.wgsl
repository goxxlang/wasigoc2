// The audio encoder's relative positions: row r is distance PAST - r (PAST, PAST-1, ..., 0) as a
// sinusoidal signal of C channels, sin then cos over C/2 timescales from 1 to 1e4 (decision E9).
// relative_k_proj maps the rows to per-head keys. Workgroup per row.

override C: u32;
override PAST: u32;

@group(0) @binding(0) var<storage, read_write> o: array<f32>;    // (PAST + 1) x C

@compute @workgroup_size(256)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(local_invocation_index) tid: u32) {
  let r = wg.x;
  let p = f32(PAST - r);
  let half = C / 2u;
  let inc = log(1.0e4) / f32(max(half - 1u, 1u));
  for (var k = tid; k < half; k += 256u) {
    let a = p * exp(-f32(k) * inc);
    o[r * C + k] = sin(a);
    o[r * C + half + k] = cos(a);
  }
}
