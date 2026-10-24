// hodgelinear.wgsl over many tokens: token t is workgroup z, so one dispatch runs a weight
// (or one of its limbs, ACCUM) on every token of an encoder. o[t] = (W xq[t]) * xs[t] * ws.
// Packing, digits and CENTER are hodgelinear.wgsl's. Needs dot4 prepended.

override K: u32;               // inputs
override NP: u32;              // packed rows = outputs / 4
override ACCUM: bool = false;  // o += result (a limb) instead of o = result
override CENTER: i32 = 1;      // the digit that means 0

const ROWS = 8u;               // packed rows per workgroup
const LANES = 32u;             // threads per packed row

@group(0) @binding(0) var<storage, read> xq: array<u32>;        // T x K/4 words of int8
@group(0) @binding(1) var<storage, read> xs: array<f32>;        // T dequant scales
@group(0) @binding(2) var<storage, read> w: array<u32>;         // NP x K/4 words
@group(0) @binding(3) var<storage, read> ws: array<f32>;        // [0]: the weight scale
@group(0) @binding(4) var<storage, read_write> o: array<f32>;   // T x 4 NP

var<workgroup> sx: array<u32, K / 4u>;
var<workgroup> red: array<vec4<i32>, 256>;

@compute @workgroup_size(256)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nwg: vec3u,
        @builtin(local_invocation_index) tid: u32) {
  let kw = K / 4u;
  let t = wg.z;
  for (var k = tid; k < kw; k += 256u) {
    sx[k] = xq[t * kw + k];
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
    let v = vec4<f32>(red[tid]) * (xs[t] * ws[0]);
    let ob = t * 4u * NP;
    for (var i = 0u; i < 4u; i++) {
      let idx = ob + i * NP + r;
      if (ACCUM) {
        o[idx] = o[idx] + v[i];
      } else {
        o[idx] = v[i];
      }
    }
  }
}
