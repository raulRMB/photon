enable f16;

@group(0) @binding(0) var<uniform> u : array<mat2x3<f16>, 4>;

fn a(a : array<mat2x3<f16>, 4>) {}
fn b(m : mat2x3<f16>) {}
fn c(v : vec3<f16>) {}
fn d(f : f16) {}

@compute @workgroup_size(1)
fn f() {
    a(u);
    b(u[1]);
    c(u[1][0].zxy);
    d(u[1][0].zxy.x);
}
