@group(1) @binding(0) var arg_0 : texture_storage_1d<rgba16uint, read_write>;

fn textureStore_9ba5c1() {
  textureStore(arg_0, 1u, vec4<u32>(1u));
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureStore_9ba5c1();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureStore_9ba5c1();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureStore_9ba5c1();
}
