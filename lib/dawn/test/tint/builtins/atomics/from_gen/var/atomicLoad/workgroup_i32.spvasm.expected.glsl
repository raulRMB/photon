#version 310 es

uint local_invocation_index_1 = 0u;
shared int arg_0;
void atomicLoad_afcc03() {
  int res = 0;
  int x_11 = atomicOr(arg_0, 0);
  res = x_11;
  return;
}

void compute_main_inner(uint local_invocation_index_2) {
  atomicExchange(arg_0, 0);
  barrier();
  atomicLoad_afcc03();
  return;
}

void compute_main_1() {
  uint x_30 = local_invocation_index_1;
  compute_main_inner(x_30);
  return;
}

void compute_main(uint local_invocation_index_1_param) {
  {
    atomicExchange(arg_0, 0);
  }
  barrier();
  local_invocation_index_1 = local_invocation_index_1_param;
  compute_main_1();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main(gl_LocalInvocationIndex);
  return;
}
