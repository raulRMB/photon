struct VertexInput {
	@location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) tangent: vec3f,
    @location(3) bitangent: vec3f,
	@location(4) color: vec4f,
    @location(5) uv: vec2f
}

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec4f,
    @location(1) normal: vec3f,
    @location(2) tangent: vec3f,
    @location(3) bitangent: vec3f,
    @location(4) uv: vec2f,
    @location(5) view_pos: vec3f,
    @location(6) frag_pos: vec3f,
    @location(7) padding: f32
}

struct MyUniforms {
    model : mat4x4<f32>,
    view : mat4x4<f32>,
    proj : mat4x4<f32>,
    view_pos: vec3f,
    pad : f32
}

@group(0) @binding(0) var<uniform> u_uniforms: MyUniforms;
@group(0) @binding(1) var texture_sampler: sampler;

@group(0) @binding(2) var base_color_texture: texture_2d<f32>;
@group(0) @binding(3) var normal_texture: texture_2d<f32>;
@group(0) @binding(4) var roughness_texture: texture_2d<f32>;
@group(0) @binding(5) var metallic_texture: texture_2d<f32>;

@group(0) @binding(6) var environment_texture: texture_2d<f32>;

const PI = 3.14159265359;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = u_uniforms.proj * u_uniforms.view * u_uniforms.model * vec4f(in.position, 1.0);

    let T = normalize((u_uniforms.model * vec4f(in.tangent, 0.0)).xyz);
    let B = normalize((u_uniforms.model * vec4f(in.bitangent, 0.0)).xyz);
    let N = normalize((u_uniforms.model * vec4f(in.normal, 0.0)).xyz);

    out.tangent = T;
    out.bitangent = B;
    out.normal = N;

    out.uv = in.uv;
    out.color = in.color;
    out.view_pos = u_uniforms.view_pos;
    out.frag_pos = (u_uniforms.model * vec4f(in.position, 1.0)).xyz;
    out.padding = u_uniforms.pad;
    return out;
}


fn fresnelSchlick(cos_theta: f32, F0: vec3f) -> vec3f
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

fn DistributionGGX(N: vec3f, H: vec3f, roughness: f32) -> f32
{
    let a      = roughness*roughness;
    let a2     = a*a;
    let NdotH  = max(dot(N, H), 0.0);
    let NdotH2 = NdotH*NdotH;

    let num   = a2;
    var denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

fn GeometrySchlickGGX(NdotV: f32, roughness: f32) -> f32
{
    let r: f32 = (roughness + 1.0);
    let k: f32 = (r*r) / 8.0;

    let num: f32 = NdotV;
    let denom: f32 = NdotV * (1.0 - k) + k;

    return num / denom;
}

fn GeometrySmith(N: vec3f, V: vec3f, L: vec3f, roughness: f32) -> f32
{
    let NdotV = max(dot(N, V), 0.0);
    let NdotL = max(dot(N, L), 0.0);
    let ggx2  = GeometrySchlickGGX(NdotV, roughness);
    let ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

struct Light {
    position: vec3f,
    color: vec3f
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    var lights = array<Light, 4>();

    let cos0 = cos(in.padding);
    let sin0 = sin(in.padding);
    let cos1 = cos(in.padding + PI / 2.0);
    let sin1 = sin(in.padding + PI / 2.0);
    let cos2 = cos(in.padding + PI);
    let sin2 = sin(in.padding + PI);
    let cos3 = cos(in.padding + PI * 3.0 / 2.0);
    let sin3 = sin(in.padding + PI * 3.0 / 2.0);

    lights[0].position = vec3f(cos0, 1, sin0);
    lights[0].color = vec3f(1.0, 0.0, 0.0);

    lights[1].position = vec3f(cos1, 1, sin1);
    lights[1].color = vec3f(0.0, 1.0, 0.0);

    lights[2].position = vec3f(cos2, 0, sin2);
    lights[2].color = vec3f(0.0, 0.0, 1.0);

    lights[3].position = vec3f(cos3, -1, sin3);
    lights[3].color = vec3f(1.0, 1.0, 0.0);

    let frag_pos = in.frag_pos;



    let metalic = textureSample(base_color_texture, texture_sampler, in.uv).r;
    let albedo = textureSample(base_color_texture, texture_sampler, in.uv).rgb;
    let roughness = textureSample(roughness_texture, texture_sampler, in.uv).r;
    let local_normal = textureSample(normal_texture, texture_sampler, in.uv).rgb;

    let TBN = mat3x3f(
        normalize(in.tangent),
        normalize(in.bitangent),
        normalize(in.normal)
    );

    let world_normal = normalize(TBN * (local_normal * 2.0 - 1.0));

    let N = normalize(in.normal);
    // let N = normalize(mix(in.normal, world_normal, 0.5));
    let V = normalize(in.view_pos - frag_pos);

    var F0 = vec3f(0.04);
    F0 = mix(F0, albedo, metalic);

    var Lo = vec3f(0.0);
    for(var i = 0; i < 4; i++)
    {
        let light_pos = lights[i].position;
        let light_color = lights[i].color;

        let L = normalize(light_pos - frag_pos);
        let H = normalize(V + L);

        let distance     = length(light_pos - frag_pos);
        let attenuation  = 1.0 / (distance * distance);
        let radiance     = light_color * attenuation;

        let NDF = DistributionGGX(N, H, roughness);
        let G = GeometrySmith(N, V, L, roughness);
        let F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        let kS = F;
        var kD = vec3f(1.0) - kS;
        kD *= 1.0 - metalic;

        let numerator = NDF * G * F;
        let denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        let specular = numerator / denominator;

        let NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    let ambient = vec3f(0.03) * albedo * 0.2;
    var color = ambient + Lo;

    color = color / (color + vec3f(1.0));
    color = pow(color, vec3f(1.0 / 2.2));

    let ibl_direction = -reflect(V, N);

    let theta = acos(ibl_direction.y / length(ibl_direction));
    let phi = atan2(ibl_direction.z, ibl_direction.x);

    let ibl_uv = vec2f(phi / (2.0 * PI) + 0.5,theta / PI);

    let ibl_sample = textureSampleLevel(environment_texture, texture_sampler, ibl_uv, 6.0).rgb;

    color = ibl_sample;

    return vec4f(color, 1.0);
}