@group(0) @binding(0) var<uniform> u : array<mat2x2<f32>, 4>;

@compute @workgroup_size(1)
fn f() {
    let t = transpose(u[2]);
    let l = length(u[0][1].yx);
    let a = abs(u[0][1].yx.x);
}
