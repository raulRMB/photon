@group(1) @binding(0) var arg_0 : texture_storage_2d_array<rgba8sint, write>;

fn textureStore_a1352c() {
  var arg_1 = vec2<u32>(1u);
  var arg_2 = 1u;
  var arg_3 = vec4<i32>(1i);
  textureStore(arg_0, arg_1, arg_2, arg_3);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureStore_a1352c();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureStore_a1352c();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureStore_a1352c();
}
