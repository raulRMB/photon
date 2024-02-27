//
// Created by Raul Romero on 2024-02-26.
//

#include "Renderer.h"
#include <GLFW/glfw3.h>

namespace photon
{

void Renderer::GetDevice(void (*callback)(wgpu::Device)) {
    wInstance.RequestAdapter(
            nullptr,
            // TODO(https://bugs.chromium.org/p/dawn/issues/detail?id=1892): Use
            // wgpu::RequestAdapterStatus, wgpu::Adapter, and wgpu::Device.
            [](WGPURequestAdapterStatus status, WGPUAdapter cAdapter,
               const char* message, void* userdata) {
                if (status != WGPURequestAdapterStatus_Success) {
                    exit(0);
                }
                wgpu::Adapter adapter = wgpu::Adapter::Acquire(cAdapter);
                adapter.RequestDevice(
                        nullptr,
                        [](WGPURequestDeviceStatus status, WGPUDevice cDevice,
                           const char* message, void* userdata) {
                            wgpu::Device device = wgpu::Device::Acquire(cDevice);
                            device.SetUncapturedErrorCallback(
                                    [](WGPUErrorType type, const char* message, void* userdata) {
                                        std::cout << "Error: " << type << " - message: " << message;
                                    },
                                    nullptr);
                            reinterpret_cast<void (*)(wgpu::Device)>(userdata)(device);
                        },
                        userdata);
            },
            reinterpret_cast<void*>(callback));
}

Renderer &Renderer::Instance()
{
    static Renderer instance;
    return instance;
}

void Renderer::Start()
{
#if defined(__EMSCRIPTEN__)
    wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
    canvasDesc.selector = "#canvas";

    wgpu::SurfaceDescriptor surfaceDesc{.nextInChain = &canvasDesc};
    wSurface = wInstance.CreateSurface(&surfaceDesc);
#else
    if (!glfwInit()) {
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window =
            glfwCreateWindow(kWidth, kHeight, "WebGPU window", nullptr, nullptr);

    wSurface = wgpu::glfw::CreateSurfaceForWindow(wInstance, window);
#endif

    InitGraphics();

#if defined(__EMSCRIPTEN__)
    auto RenderLoopCallBack = [](void* arg)
    {
        static_cast<Renderer*>(arg)->Render();
    };
    emscripten_set_main_loop_arg(RenderLoopCallBack, this, -1, true);
#else
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        Render();
        wSwapChain.Present();
        wInstance.ProcessEvents();
    }
#endif
}

void Renderer::InitGraphics()
{
    SetupSwapChain();
    SetupRenderPipeline();
}

void Renderer::SetupSwapChain()
{
    wgpu::SwapChainDescriptor scDesc{
            .usage = wgpu::TextureUsage::RenderAttachment,
            .format = wgpu::TextureFormat::BGRA8Unorm,
            .width = kWidth,
            .height = kHeight,
            .presentMode = wgpu::PresentMode::Fifo};
    wSwapChain = wDevice.CreateSwapChain(wSurface, &scDesc);
}

const char shaderCode[] = R"(
@vertex fn vertexMain(@builtin(vertex_index) i : u32) ->
  @builtin(position) vec4f {
    const pos = array(vec2f(0, 1), vec2f(-1, -1), vec2f(1, -1));
    return vec4f(pos[i], 0, 1);
}
@fragment fn fragmentMain() -> @location(0) vec4f {
    return vec4f(1, 0, 0, 1);
}
)";

void Renderer::SetupRenderPipeline()
{
    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.code = shaderCode;

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
            .nextInChain = &wgslDesc};
    wgpu::ShaderModule shaderModule =
            wDevice.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState{
            .format = wgpu::TextureFormat::BGRA8Unorm};

    wgpu::FragmentState fragmentState{.module = shaderModule,
            .targetCount = 1,
            .targets = &colorTargetState};

    wgpu::RenderPipelineDescriptor descriptor{
            .vertex = {.module = shaderModule},
            .fragment = &fragmentState};
    wRenderPipeline = wDevice.CreateRenderPipeline(&descriptor);
}

void Renderer::Render()
{
    wgpu::RenderPassColorAttachment attachment{
            .view = wSwapChain.GetCurrentTextureView(),
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store};

    wgpu::RenderPassDescriptor renderpass{.colorAttachmentCount = 1,
            .colorAttachments = &attachment};

    wgpu::CommandEncoder encoder = wDevice.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
    pass.SetPipeline(wRenderPipeline);
    pass.Draw(3);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    wDevice.GetQueue().Submit(1, &commands);
}

bool Renderer::Go()
{
    Renderer& instance = Instance();
    instance.wInstance = wgpu::CreateInstance();
    instance.GetDevice([](wgpu::Device dev)
    {
        Renderer& instance = Instance();
        instance.wDevice = dev;
        instance.Start();
    });

    return EXIT_SUCCESS;
}

} // photon