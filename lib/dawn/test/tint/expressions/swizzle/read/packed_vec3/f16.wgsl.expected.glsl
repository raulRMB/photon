#version 310 es
#extension GL_AMD_gpu_shader_half_float : require

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
struct S {
  f16vec3 v;
  uint pad;
  uint pad_1;
};

layout(binding = 0, std140) uniform U_block_ubo {
  S inner;
} U;

void f() {
  f16vec3 v = U.inner.v;
  float16_t x = U.inner.v.x;
  float16_t y = U.inner.v.y;
  float16_t z = U.inner.v.z;
  f16vec2 xx = U.inner.v.xx;
  f16vec2 xy = U.inner.v.xy;
  f16vec2 xz = U.inner.v.xz;
  f16vec2 yx = U.inner.v.yx;
  f16vec2 yy = U.inner.v.yy;
  f16vec2 yz = U.inner.v.yz;
  f16vec2 zx = U.inner.v.zx;
  f16vec2 zy = U.inner.v.zy;
  f16vec2 zz = U.inner.v.zz;
  f16vec3 xxx = U.inner.v.xxx;
  f16vec3 xxy = U.inner.v.xxy;
  f16vec3 xxz = U.inner.v.xxz;
  f16vec3 xyx = U.inner.v.xyx;
  f16vec3 xyy = U.inner.v.xyy;
  f16vec3 xyz = U.inner.v.xyz;
  f16vec3 xzx = U.inner.v.xzx;
  f16vec3 xzy = U.inner.v.xzy;
  f16vec3 xzz = U.inner.v.xzz;
  f16vec3 yxx = U.inner.v.yxx;
  f16vec3 yxy = U.inner.v.yxy;
  f16vec3 yxz = U.inner.v.yxz;
  f16vec3 yyx = U.inner.v.yyx;
  f16vec3 yyy = U.inner.v.yyy;
  f16vec3 yyz = U.inner.v.yyz;
  f16vec3 yzx = U.inner.v.yzx;
  f16vec3 yzy = U.inner.v.yzy;
  f16vec3 yzz = U.inner.v.yzz;
  f16vec3 zxx = U.inner.v.zxx;
  f16vec3 zxy = U.inner.v.zxy;
  f16vec3 zxz = U.inner.v.zxz;
  f16vec3 zyx = U.inner.v.zyx;
  f16vec3 zyy = U.inner.v.zyy;
  f16vec3 zyz = U.inner.v.zyz;
  f16vec3 zzx = U.inner.v.zzx;
  f16vec3 zzy = U.inner.v.zzy;
  f16vec3 zzz = U.inner.v.zzz;
  f16vec4 xxxx = U.inner.v.xxxx;
  f16vec4 xxxy = U.inner.v.xxxy;
  f16vec4 xxxz = U.inner.v.xxxz;
  f16vec4 xxyx = U.inner.v.xxyx;
  f16vec4 xxyy = U.inner.v.xxyy;
  f16vec4 xxyz = U.inner.v.xxyz;
  f16vec4 xxzx = U.inner.v.xxzx;
  f16vec4 xxzy = U.inner.v.xxzy;
  f16vec4 xxzz = U.inner.v.xxzz;
  f16vec4 xyxx = U.inner.v.xyxx;
  f16vec4 xyxy = U.inner.v.xyxy;
  f16vec4 xyxz = U.inner.v.xyxz;
  f16vec4 xyyx = U.inner.v.xyyx;
  f16vec4 xyyy = U.inner.v.xyyy;
  f16vec4 xyyz = U.inner.v.xyyz;
  f16vec4 xyzx = U.inner.v.xyzx;
  f16vec4 xyzy = U.inner.v.xyzy;
  f16vec4 xyzz = U.inner.v.xyzz;
  f16vec4 xzxx = U.inner.v.xzxx;
  f16vec4 xzxy = U.inner.v.xzxy;
  f16vec4 xzxz = U.inner.v.xzxz;
  f16vec4 xzyx = U.inner.v.xzyx;
  f16vec4 xzyy = U.inner.v.xzyy;
  f16vec4 xzyz = U.inner.v.xzyz;
  f16vec4 xzzx = U.inner.v.xzzx;
  f16vec4 xzzy = U.inner.v.xzzy;
  f16vec4 xzzz = U.inner.v.xzzz;
  f16vec4 yxxx = U.inner.v.yxxx;
  f16vec4 yxxy = U.inner.v.yxxy;
  f16vec4 yxxz = U.inner.v.yxxz;
  f16vec4 yxyx = U.inner.v.yxyx;
  f16vec4 yxyy = U.inner.v.yxyy;
  f16vec4 yxyz = U.inner.v.yxyz;
  f16vec4 yxzx = U.inner.v.yxzx;
  f16vec4 yxzy = U.inner.v.yxzy;
  f16vec4 yxzz = U.inner.v.yxzz;
  f16vec4 yyxx = U.inner.v.yyxx;
  f16vec4 yyxy = U.inner.v.yyxy;
  f16vec4 yyxz = U.inner.v.yyxz;
  f16vec4 yyyx = U.inner.v.yyyx;
  f16vec4 yyyy = U.inner.v.yyyy;
  f16vec4 yyyz = U.inner.v.yyyz;
  f16vec4 yyzx = U.inner.v.yyzx;
  f16vec4 yyzy = U.inner.v.yyzy;
  f16vec4 yyzz = U.inner.v.yyzz;
  f16vec4 yzxx = U.inner.v.yzxx;
  f16vec4 yzxy = U.inner.v.yzxy;
  f16vec4 yzxz = U.inner.v.yzxz;
  f16vec4 yzyx = U.inner.v.yzyx;
  f16vec4 yzyy = U.inner.v.yzyy;
  f16vec4 yzyz = U.inner.v.yzyz;
  f16vec4 yzzx = U.inner.v.yzzx;
  f16vec4 yzzy = U.inner.v.yzzy;
  f16vec4 yzzz = U.inner.v.yzzz;
  f16vec4 zxxx = U.inner.v.zxxx;
  f16vec4 zxxy = U.inner.v.zxxy;
  f16vec4 zxxz = U.inner.v.zxxz;
  f16vec4 zxyx = U.inner.v.zxyx;
  f16vec4 zxyy = U.inner.v.zxyy;
  f16vec4 zxyz = U.inner.v.zxyz;
  f16vec4 zxzx = U.inner.v.zxzx;
  f16vec4 zxzy = U.inner.v.zxzy;
  f16vec4 zxzz = U.inner.v.zxzz;
  f16vec4 zyxx = U.inner.v.zyxx;
  f16vec4 zyxy = U.inner.v.zyxy;
  f16vec4 zyxz = U.inner.v.zyxz;
  f16vec4 zyyx = U.inner.v.zyyx;
  f16vec4 zyyy = U.inner.v.zyyy;
  f16vec4 zyyz = U.inner.v.zyyz;
  f16vec4 zyzx = U.inner.v.zyzx;
  f16vec4 zyzy = U.inner.v.zyzy;
  f16vec4 zyzz = U.inner.v.zyzz;
  f16vec4 zzxx = U.inner.v.zzxx;
  f16vec4 zzxy = U.inner.v.zzxy;
  f16vec4 zzxz = U.inner.v.zzxz;
  f16vec4 zzyx = U.inner.v.zzyx;
  f16vec4 zzyy = U.inner.v.zzyy;
  f16vec4 zzyz = U.inner.v.zzyz;
  f16vec4 zzzx = U.inner.v.zzzx;
  f16vec4 zzzy = U.inner.v.zzzy;
  f16vec4 zzzz = U.inner.v.zzzz;
}

