int func(inout int pointer) {
  return pointer;
}

[numthreads(1, 1, 1)]
void main() {
  int F = 0;
  const int r = func(F);
  return;
}
