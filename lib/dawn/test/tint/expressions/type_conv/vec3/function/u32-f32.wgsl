var<private> t : u32;
fn m() -> vec3<u32> {
    t = 1u;
    return vec3<u32>(t);
}
fn f() {
    var v : vec3<f32> = vec3<f32>(m());
}