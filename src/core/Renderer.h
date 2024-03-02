//
// Created by Raul Romero on 2024-02-26.
//

#ifndef PHOTON_RENDERER_H
#define PHOTON_RENDERER_H

#include "PhotonCore.h"
#include "CMesh.h"
#include "CCamera.h"
#include "ResourceLoader.h"

#include <webgpu/webgpu_cpp.h>
#include <iostream>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif

#include <vector>
#include <array>

namespace photon
{

struct ShaderUniforms
{
    m4 m_Model;
    m4 m_View;
    m4 m_Projection;
    v3 m_CameraPosition;
    f32 m_padding;
};

struct SkyboxMapUniforms
{
    m4 m_MVPi;
};

class Renderer
{
#ifdef __EMSCRIPTEN__
    u32 kWidth = 1920;
    u32 kHeight = 900;
#else
    u32 kWidth = 1920;
    u32 kHeight = 1080;
#endif

private:
    wgpu::Instance wInstance;
    wgpu::Device wDevice;
    wgpu::Surface wSurface;
    wgpu::SwapChain wSwapChain;
    wgpu::RenderPipeline wRenderPipeline;

    wgpu::RenderPipeline wCubeMapPipeline;

    std::vector<wgpu::VertexBufferLayout> wVertexBufferLayouts;
    std::vector<wgpu::VertexAttribute> wVertexAttributes;

    std::vector<wgpu::BindGroupLayoutEntry> wBindGroupLayoutEntries;
    wgpu::BindGroupLayout wBindGroupLayout;
    wgpu::BindGroup wBindGroup;
    std::vector<wgpu::BindGroupEntry> wBindGroupEntries;

    wgpu::Buffer wUniformBuffer;
    wgpu::Buffer wSkyboxUniformBuffer;

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

    wgpu::Texture wSkyboxTexture;
    wgpu::TextureView wSkyboxTextureView;

    wgpu::BindGroupLayout wSkyboxBindGroupLayout;
    wgpu::BindGroup wSkyboxBindGroup;
    std::array<wgpu::BindGroupEntry, 3> wSkyboxBindGroupEntries;
    std::array<wgpu::BindGroupLayoutEntry, 3> wSkyboxBindGroupEntryLayouts;

    wgpu::VertexAttribute wSkyboxVertexAttribute;
    wgpu::VertexBufferLayout wSkyboxVertexBufferLayout;

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

    void SetupMeshVertexBuffers();

    void SetupDepthStencil();
    void LoadTextures(const std::string& name, ETextureImportType type);
    void SetupMeshUniformBuffer();
    void SetupSampler();
    void SetupMeshBindGroupLayout();
    void SetupMeshBindGroup();

    void SetupSkyboxBindGroupLayout();
    void SetupSkyboxBindGroup();

    void SetupMeshPipeline();
    void SetupSkyboxPipeline();

    void DrawMesh(wgpu::RenderPassEncoder& renderPass);
    void DrawSkybox(wgpu::RenderPassEncoder& renderPass);
    void Render();

    void SetupCamera();
};

} // photon

#endif //PHOTON_RENDERER_H
