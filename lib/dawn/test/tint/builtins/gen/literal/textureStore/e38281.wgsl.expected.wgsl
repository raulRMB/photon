@group(1) @binding(0) var arg_0 : texture_storage_2d_array<bgra8unorm, write>;

fn textureStore_e38281() {
  textureStore(arg_0, vec2<u32>(1u), 1u, vec4<f32>(1.0f));
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureStore_e38281();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureStore_e38281();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureStore_e38281();
}
