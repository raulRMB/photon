fn min_82b28f() {
  var res : vec2<u32> = min(vec2<u32>(1u), vec2<u32>(1u));
  prevent_dce = res;
}

@group(2) @binding(0) var<storage, read_write> prevent_dce : vec2<u32>;

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  min_82b28f();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  min_82b28f();
}

@compute @workgroup_size(1)
fn compute_main() {
  min_82b28f();
}
