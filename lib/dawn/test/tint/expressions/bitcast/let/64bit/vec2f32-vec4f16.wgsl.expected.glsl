#version 310 es
#extension GL_AMD_gpu_shader_half_float : require

f16vec4 tint_bitcast_to_f16(vec2 src) {
  uvec2 r = floatBitsToUint(src);
  f16vec2 v_xy = unpackFloat2x16(r.x);
  f16vec2 v_zw = unpackFloat2x16(r.y);
  return f16vec4(v_xy.x, v_xy.y, v_zw.x, v_zw.y);
}

void f() {
  vec2 a = vec2(2.003662109375f, -513.03125f);
  f16vec4 b = tint_bitcast_to_f16(a);
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f();
  return;
}
