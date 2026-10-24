// HodgeLinear, ternary weights and int8 activations: out = (W x_q) * act_scale * weight_scale, with W
// ternary and x_q int8 (norm.wgsl made it).
//
// W is read as hodgemix packs it (uint8
// [N/4, K]): byte k of packed row r holds the 2-bit codes (weight + 1) of
// output rows r, r + N/4, r + 2N/4, r + 3N/4 at input k, in bits 0-1, 2-3,
// 4-5, 6-7. A u32 of W is 4 consecutive inputs for those 4 rows, so
// (w >> 2i) & 0x03030303 is 4 codes in {0, 1, 2} as unsigned bytes, and
//   sum a * (code - 1) = dot4(a, codes) - dot4(a, 0x01010101).
// Needs dot4 (dot4_native.wgsl or dot4_soft.wgsl) prepended.
//
// The same kernel runs a weight's further digit limbs (hodgemix limbs.go): the 2-bit codes are
// digits of radix r in {2, 3, 4} meaning (code - CENTER), CENTER = (r - 1) / 2, with their own scale,
// dispatched with ACCUM so the limbs sum. The ternary limb 0 is CENTER = 1.

override K: u32;            // inputs
override NP: u32;           // packed rows = outputs / 4
override ACCUM: bool = false;  // out += result (a residual add) instead of out = result
override CENTER: i32 = 1;   // the digit that means 0

const ROWS = 8u;            // packed rows per workgroup
const LANES = 32u;          // threads per packed row

@group(0) @binding(0) var<storage, read> xq: array<u32>;        // K/4 words of int8
@group(0) @binding(1) var<storage, read> xs: array<f32>;        // [0]: the activations' dequant scale
@group(0) @binding(2) var<storage, read> w: array<u32>;         // NP x K/4 words
@group(0) @binding(3) var<storage, read> ws: array<f32>;        // [0]: the weight scale
@group(0) @binding(4) var<storage, read_write> o: array<f32>;   // 4 * NP outputs

var<workgroup> sx: array<u32, K / 4u>;
var<workgroup> red: array<vec4<i32>, 256>;

@compute @workgroup_size(256)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nwg: vec3u,
        @builtin(local_invocation_index) tid: u32) {
  let kw = K / 4u;
  for (var k = tid; k < kw; k += 256u) {
    sx[k] = xq[k];
  }
  workgroupBarrier();

  let row = tid / LANES;
  let lane = tid % LANES;
  let r = (wg.y * nwg.x + wg.x) * ROWS + row;
  var acc = vec4<i32>(0);
  if (r < NP) {
    let base = r * kw;
    for (var k = lane; k < kw; k += LANES) {
      let ww = w[base + k];
      let a = sx[k];
      let ones = dot4(a, 0x01010101u);
      acc += vec4<i32>(dot4(a, ww & 0x03030303u),
                       dot4(a, (ww >> 2u) & 0x03030303u),
                       dot4(a, (ww >> 4u) & 0x03030303u),
                       dot4(a, (ww >> 6u) & 0x03030303u)) - vec4<i32>(ones * CENTER);
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
  if (lane == 0u && r < NP) {
    let v = vec4<f32>(red[tid]) * (xs[0] * ws[0]);
    for (var i = 0u; i < 4u; i++) {
      let idx = i * NP + r;
      if (ACCUM) {
        o[idx] = o[idx] + v[i];
      } else {
        o[idx] = v[i];
      }
    }
  }
}
