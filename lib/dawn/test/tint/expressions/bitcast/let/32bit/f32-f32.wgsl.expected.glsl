#version 310 es

void f() {
  float a = 2.003662109375f;
  float b = a;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f();
  return;
}
