fn f_1() {
  var v = 0i;
  var offset_1 = 0u;
  var count = 0u;
  let x_14 = extractBits(v, offset_1, count);
  return;
}

@compute @workgroup_size(1i, 1i, 1i)
fn f() {
  f_1();
}
