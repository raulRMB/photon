//
// Created by Raul Romero on 2024-02-26.
//

#ifndef PHOTON_RENDERER_H
#define PHOTON_RENDERER_H

#include "PhotonCore.h"
#include "CMesh.h"
#include "CCamera.h"

#include <webgpu/webgpu_cpp.h>
#include <iostream>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif

#include <vector>

namespace photon
{

const u32 kWidth = 1920;
const u32 kHeight = 900;

struct ShaderUniforms
{
    m4 m_Model;
    m4 m_View;
    m4 m_Projection;
    v3 m_CameraPosition;
    f32 m_padding;
};

class Renderer
{
private:
    wgpu::Instance wInstance;
    wgpu::Device wDevice;
    wgpu::Surface wSurface;
    wgpu::SwapChain wSwapChain;
    wgpu::RenderPipeline wRenderPipeline;

    std::vector<wgpu::VertexBufferLayout> wVertexBufferLayouts;
    std::vector<wgpu::VertexAttribute> wVertexAttributes;

    std::vector<wgpu::BindGroupLayoutEntry> wBindGroupLayoutEntries;
    wgpu::BindGroupLayout wBindGroupLayout;
    wgpu::BindGroup wBindGroup;
    std::vector<wgpu::BindGroupEntry> wBindGroupEntries;
    wgpu::Buffer wUniformBuffer;

    wgpu::DepthStencilState wDepthStencilState;
    wgpu::Texture wDepthTexture;
    wgpu::TextureView wDepthTextureView;

    wgpu::Sampler wSampler;

    wgpu::Texture wAlbedoTexture;
    wgpu::TextureView wAlbedoTextureView;

    wgpu::Texture wNormalTexture;
    wgpu::TextureView wNormalTextureView;

    wgpu::Texture wRoughnessTexture;
    wgpu::TextureView wRoughnessTextureView;

    wgpu::Texture wMetallicTexture;
    wgpu::TextureView wMetallicTextureView;

    wgpu::Texture wEnvironmentTexture;
    wgpu::TextureView wEnvironmentTextureView;

    CMesh Mesh;
    CCamera Camera;
public:
    static Renderer& Instance();
    static bool Go();
private:
    Renderer() = default;
    ~Renderer() = default;

    void Start();

    void GetDevice(void (*callback)(wgpu::Device));

    void InitGraphics();

    void SetupSwapChain();

    void SetupVertexBuffers();
    void SetupDepthStencil();
    void LoadTextures();
    void SetupUniformBuffer();
    void SetupSampler();
    void SetupBindGroupLayout();
    void SetupBindGroup();

    void SetupRenderPipeline();


    void DrawMesh(wgpu::RenderPassEncoder& renderPass);
    void Render();

    void SetupCamera();
};

} // photon

#endif //PHOTON_RENDERER_H
