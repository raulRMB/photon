//
// Created by Raul Romero on 2024-02-26.
//

#ifndef PHOTON_RENDERER_H
#define PHOTON_RENDERER_H

#include "PhotonCore.h"

#include <webgpu/webgpu_cpp.h>
#include <iostream>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif

namespace photon
{

const uint32_t kWidth = 512;
const uint32_t kHeight = 512;

class Renderer
{
private:
    wgpu::Instance wInstance;
    wgpu::Device wDevice;
    wgpu::Surface wSurface;
    wgpu::SwapChain wSwapChain;
    wgpu::RenderPipeline wRenderPipeline;
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
    void SetupRenderPipeline();

    void Render();
};

} // photon

#endif //PHOTON_RENDERER_H
