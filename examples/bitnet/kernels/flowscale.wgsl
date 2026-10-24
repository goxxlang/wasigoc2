// flow.wgsl's integer currents into a BitLinear limb's real ones: flow[i] (+)= acc[i] * act_scale *
// weight_scale, and acc is cleared for the next capture. ACCUM adds a further limb onto limb 0's.

override N: u32;              // output cells x input cells
override ACCUM: bool = false;

@group(0) @binding(0) var<storage, read_write> acc: array<atomic<i32>>;
@group(0) @binding(1) var<storage, read> xs: array<f32>;     // [0]: the activations' dequant scale
@group(0) @binding(2) var<storage, read> ws: array<f32>;     // [0]: the limb's weight scale
@group(0) @binding(3) var<storage, read_write> flow: array<f32>;

@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) gid: vec3u, @builtin(num_workgroups) nwg: vec3u) {
  let i = gid.y * nwg.x * 256u + gid.x;
  if (i >= N) {
    return;
  }
  let v = f32(atomicExchange(&acc[i], 0)) * (xs[0] * ws[0]);
  if (ACCUM) {
    flow[i] = flow[i] + v;
  } else {
    flow[i] = v;
  }
}
