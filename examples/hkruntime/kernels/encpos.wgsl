// A vision encoder's learned position: x[t] += table[0][col] + table[1][row] for patch t at
// (t / COLS, t % COLS). The table is [2, ROWS_MAX, D] f32. One workgroup per token.

override D: u32;
override COLS: u32;
override PLANE: u32;          // rows of one table plane

@group(0) @binding(0) var<storage, read> tab: array<f32>;        // 2 x PLANE x D
@group(0) @binding(1) var<storage, read_write> x: array<f32>;    // T x D

@compute @workgroup_size(256)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nwg: vec3u,
        @builtin(local_invocation_index) tid: u32) {
  let t = wg.y * nwg.x + wg.x;
  let col = t % COLS;
  let row = t / COLS;
  for (var i = tid; i < D; i += 256u) {
    x[t * D + i] = x[t * D + i] + tab[col * D + i] + tab[(PLANE + row) * D + i];
  }
}
