// dot4: the dot product of two packed 4 x int8 vectors, on the hardware
// instruction (WGSL packed_4x8_integer_dot_product). Prepended to the
// kernels that use it when the device has the feature.
requires packed_4x8_integer_dot_product;

fn dot4(a: u32, b: u32) -> i32 {
  return dot4I8Packed(a, b);
}
