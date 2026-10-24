// Flow capture: one BitLinear limb's currents on the model's cells (hodgemix eval/flow.go on the
// device). The weight is read exactly as bitlinear.wgsl reads it (packed rows, 4 inputs a word, the
// 2-bit codes of rows r, r + NP, r + 2NP, r + 3NP in bits 0-1 .. 6-7 of each byte), and every product
// a_j (code - CENTER) is added, exact in i32, into its (output row cell, input cell) sum: colCell[j] is
// input channel j's cell, rowCell[i] output row i's. A workgroup sums its 32 rows in workgroup memory,
// then adds each nonzero (row cell, input cell) partial into acc. flowscale.wgsl turns acc into
// currents (x act_scale x weight_scale) and clears it.

override K: u32;            // inputs
override NP: u32;           // packed rows = outputs / 4
override NC: u32;           // input cells
override CENTER: i32 = 1;   // the digit that means 0

const ROWS = 8u;            // packed rows per workgroup
const LANES = 32u;          // threads per packed row

@group(0) @binding(0) var<storage, read> xq: array<u32>;             // K/4 words of int8
@group(0) @binding(1) var<storage, read> w: array<u32>;              // NP x K/4 words
@group(0) @binding(2) var<storage, read> colCell: array<u32>;        // K
@group(0) @binding(3) var<storage, read> rowCell: array<u32>;        // 4 * NP
@group(0) @binding(4) var<storage, read_write> acc: array<atomic<i32>>;  // output cells x NC

var<workgroup> sh: array<atomic<i32>, 4u * ROWS * NC>;

@compute @workgroup_size(256)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nwg: vec3u,
        @builtin(local_invocation_index) tid: u32) {
  for (var i = tid; i < 4u * ROWS * NC; i += 256u) {
    atomicStore(&sh[i], 0);
  }
  workgroupBarrier();

  let kw = K / 4u;
  let row = tid / LANES;
  let lane = tid % LANES;
  let base = (wg.y * nwg.x + wg.x) * ROWS;
  let r = base + row;
  if (r < NP) {
    for (var k = lane; k < kw; k += LANES) {
      let ww = w[r * kw + k];
      let a = xq[k];
      for (var t = 0u; t < 4u; t++) {
        let av = (bitcast<i32>(a << (24u - 8u * t))) >> 24u;   // int8 input 4k + t
        if (av == 0) {
          continue;
        }
        let c = colCell[4u * k + t];
        let codes = ww >> (8u * t);
        for (var i = 0u; i < 4u; i++) {
          let d = i32((codes >> (2u * i)) & 3u) - CENTER;
          if (d != 0) {
            atomicAdd(&sh[(row * 4u + i) * NC + c], av * d);
          }
        }
      }
    }
  }
  workgroupBarrier();

  for (var idx = tid; idx < 4u * ROWS * NC; idx += 256u) {
    let lr = idx / NC;
    let pr = base + lr / 4u;           // packed row
    if (pr < NP) {
      let v = atomicLoad(&sh[idx]);
      if (v != 0) {
        let out = (lr % 4u) * NP + pr;   // output row
        atomicAdd(&acc[rowCell[out] * NC + idx % NC], v);
      }
    }
  }
}
