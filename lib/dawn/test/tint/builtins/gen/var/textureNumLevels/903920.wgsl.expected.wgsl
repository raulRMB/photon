@group(1) @binding(0) var arg_0 : texture_cube_array<i32>;

fn textureNumLevels_903920() {
  var res : u32 = textureNumLevels(arg_0);
  prevent_dce = res;
}

@group(2) @binding(0) var<storage, read_write> prevent_dce : u32;

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureNumLevels_903920();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureNumLevels_903920();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureNumLevels_903920();
}
