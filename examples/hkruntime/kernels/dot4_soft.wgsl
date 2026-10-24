// dot4: the dot product of two packed 4 x int8 vectors, in plain integer
// arithmetic, for devices without packed_4x8_integer_dot_product.

fn dot4(a: u32, b: u32) -> i32 {
  var s = 0;
  for (var j = 0u; j < 4u; j++) {
    let sh = 24u - 8u * j;
    s += (bitcast<i32>(a << sh) >> 24u) * (bitcast<i32>(b << sh) >> 24u);
  }
  return s;
}
