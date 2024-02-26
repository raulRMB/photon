int3 tint_div(int3 lhs, int3 rhs) {
  return (lhs / (((rhs == (0).xxx) | ((lhs == (-2147483648).xxx) & (rhs == (-1).xxx))) ? (1).xxx : rhs));
}

[numthreads(1, 1, 1)]
void f() {
  const int3 a = int3(1, 2, 3);
  const int3 b = int3(4, 5, 6);
  const int3 r = tint_div(a, b);
  return;
}
