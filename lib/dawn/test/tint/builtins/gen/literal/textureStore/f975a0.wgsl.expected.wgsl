@group(1) @binding(0) var arg_0 : texture_storage_2d<rgba8snorm, read_write>;

fn textureStore_f975a0() {
  textureStore(arg_0, vec2<i32>(1i), vec4<f32>(1.0f));
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureStore_f975a0();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureStore_f975a0();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureStore_f975a0();
}
