enable f16;

fn select_10e73b() {
  var res : f16 = select(1.0h, 1.0h, true);
  prevent_dce = res;
}

@group(2) @binding(0) var<storage, read_write> prevent_dce : f16;

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  select_10e73b();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  select_10e73b();
}

@compute @workgroup_size(1)
fn compute_main() {
  select_10e73b();
}
