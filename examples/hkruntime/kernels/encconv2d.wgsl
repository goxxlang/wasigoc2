// The audio encoder's subsampling convolution (decision E8): a 3 x 3 kernel, stride 2 and
// zero padding 1 on time and frequency, then an RMS norm over the output channels of each
// (time, frequency) cell times its weight, then ReLU. Layout is [time][frequency][channel],
// so a row of the last block flattens as frequency * channels + channel. Workgroup per
// output cell, one thread per output channel.

override CIN: u32;
override COUT: u32;
override FIN: u32;            // input frequencies
override TIN: u32;            // input frames
override EPS: f32 = 1e-6;

@group(0) @binding(0) var<storage, read> x: array<f32>;          // TIN x FIN x CIN
@group(0) @binding(1) var<storage, read> w: array<f32>;          // COUT x CIN x 3 x 3
@group(0) @binding(2) var<storage, read> nw: array<f32>;         // COUT
@group(0) @binding(3) var<storage, read_write> o: array<f32>;    // TOUT x FOUT x COUT

var<workgroup> red: array<f32, 256>;

@compute @workgroup_size(256)
fn main(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nwg: vec3u,
        @builtin(local_invocation_index) tid: u32) {
  let fout = (FIN + 1u) / 2u;
  let cell = wg.y * nwg.x + wg.x;
  let t = cell / fout;
  let f = cell % fout;
  var acc = 0.0;
  if (tid < COUT) {
    for (var dt = 0u; dt < 3u; dt++) {
      let ti = i32(2u * t + dt) - 1;
      if (ti < 0 || ti >= i32(TIN)) {
        continue;
      }
      for (var df = 0u; df < 3u; df++) {
        let fi = i32(2u * f + df) - 1;
        if (fi < 0 || fi >= i32(FIN)) {
          continue;
        }
        let xb = (u32(ti) * FIN + u32(fi)) * CIN;
        for (var c = 0u; c < CIN; c++) {
          acc += x[xb + c] * w[((tid * CIN + c) * 3u + dt) * 3u + df];
        }
      }
    }
  }
  red[tid] = acc * acc;
  workgroupBarrier();
  for (var s = 128u; s > 0u; s >>= 1u) {
    if (tid < s) {
      red[tid] += red[tid + s];
    }
    workgroupBarrier();
  }
  let inv = inverseSqrt(red[0] / f32(COUT) + EPS);
  if (tid < COUT) {
    o[cell * COUT + tid] = max(acc * inv * nw[tid], 0.0);
  }
}
