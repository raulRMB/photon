//
// Created by Raul Romero on 2024-02-26.
//

#include "Renderer.h"
#include "Reader.h"
#include "ResourceLoader.h"
#include "Logger.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

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
                LogError("Failed to acquire adapter");
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
    canvasDesc.selector = "canvas";

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
//    emscripten_set_resize_callback(nullptr, this, true, [](int, int, void* arg)
//    {
//        static_cast<Renderer*>(arg)->wSwapChain.Configure(kWidth, kHeight);
//    });
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
    SetupCamera();
    SetupSwapChain();
    SetupMeshVertexBuffers();
    SetupDepthStencil();
    LoadTextures("brick", ETextureImportType::jpg);
    SetupMeshUniformBuffer();
    SetupSampler();

    SetupMeshBindGroupLayout();
    SetupMeshBindGroup();

    SetupSkyboxBindGroupLayout();
    SetupSkyboxBindGroup();

    SetupMeshPipeline();

    SetupSkyboxPipeline();

    Mesh = ResourceLoader::LoadMesh("su.glb", wDevice, EModelImportType::glb);
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

void Renderer::SetupMeshPipeline()
{
    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.code = Reader::ReadTextFile("shaders/pbr_mat.wgsl");

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
            .nextInChain = &wgslDesc};
    shaderModuleDescriptor.label = "PBR Material Shader Module";
    wgpu::ShaderModule shaderModule =
            wDevice.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState{
            .format = wgpu::TextureFormat::BGRA8Unorm};

    wgpu::FragmentState fragmentState{.module = shaderModule,
            .targetCount = 1,
            .targets = &colorTargetState};

    wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor{
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &wBindGroupLayout};

    wgpu::PipelineLayout pipelineLayout = wDevice.CreatePipelineLayout(&pipelineLayoutDescriptor);

    wgpu::RenderPipelineDescriptor descriptor{
            .label = "Mesh",
            .layout = pipelineLayout,
            .vertex = {.module = shaderModule},
            .depthStencil = &wDepthStencilState,
            .fragment = &fragmentState,
    };

    descriptor.vertex.bufferCount = static_cast<uint32_t>(wVertexBufferLayouts.size());
    descriptor.vertex.buffers = wVertexBufferLayouts.data();

    wRenderPipeline = wDevice.CreateRenderPipeline(&descriptor);
}

void Renderer::Render()
{
    wgpu::RenderPassColorAttachment attachment{
            .view = wSwapChain.GetCurrentTextureView(),
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store};

    wgpu::RenderPassDepthStencilAttachment depthStencilAttachment{};
    depthStencilAttachment.view = wDepthTextureView;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
    depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
    depthStencilAttachment.depthReadOnly = false;
    depthStencilAttachment.stencilClearValue = 0;
    depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
    depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
    depthStencilAttachment.stencilReadOnly = true;

    wgpu::RenderPassDescriptor renderpass{.colorAttachmentCount = 1,
            .colorAttachments = &attachment,
            .depthStencilAttachment = &depthStencilAttachment};

    wgpu::CommandEncoder encoder = wDevice.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);

    DrawSkybox(pass);
    DrawMesh(pass);

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

void Renderer::SetupMeshBindGroupLayout()
{
    wBindGroupLayoutEntries.resize(7, {});

    wBindGroupLayoutEntries[0].binding = 0;
    wBindGroupLayoutEntries[0].buffer.hasDynamicOffset = false;
    wBindGroupLayoutEntries[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    wBindGroupLayoutEntries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    wBindGroupLayoutEntries[0].buffer.minBindingSize = sizeof(ShaderUniforms);

    wBindGroupLayoutEntries[1].binding = 1;
    wBindGroupLayoutEntries[1].visibility = wgpu::ShaderStage::Fragment;
    wBindGroupLayoutEntries[1].sampler.type = wgpu::SamplerBindingType::Filtering;

    wBindGroupLayoutEntries[2].binding = 2;
    wBindGroupLayoutEntries[2].visibility = wgpu::ShaderStage::Fragment;
    wBindGroupLayoutEntries[2].texture.sampleType = wgpu::TextureSampleType::Float;
    wBindGroupLayoutEntries[2].texture.viewDimension = wgpu::TextureViewDimension::e2D;
    wBindGroupLayoutEntries[2].texture.multisampled = false;

    wBindGroupLayoutEntries[3].binding = 3;
    wBindGroupLayoutEntries[3].visibility = wgpu::ShaderStage::Fragment;
    wBindGroupLayoutEntries[3].texture.sampleType = wgpu::TextureSampleType::Float;
    wBindGroupLayoutEntries[3].texture.viewDimension = wgpu::TextureViewDimension::e2D;
    wBindGroupLayoutEntries[3].texture.multisampled = false;

    wBindGroupLayoutEntries[4].binding = 4;
    wBindGroupLayoutEntries[4].visibility = wgpu::ShaderStage::Fragment;
    wBindGroupLayoutEntries[4].texture.sampleType = wgpu::TextureSampleType::Float;
    wBindGroupLayoutEntries[4].texture.viewDimension = wgpu::TextureViewDimension::e2D;
    wBindGroupLayoutEntries[4].texture.multisampled = false;

    wBindGroupLayoutEntries[5].binding = 5;
    wBindGroupLayoutEntries[5].visibility = wgpu::ShaderStage::Fragment;
    wBindGroupLayoutEntries[5].texture.sampleType = wgpu::TextureSampleType::Float;
    wBindGroupLayoutEntries[5].texture.viewDimension = wgpu::TextureViewDimension::e2D;
    wBindGroupLayoutEntries[5].texture.multisampled = false;

    wBindGroupLayoutEntries[6].binding = 6;
    wBindGroupLayoutEntries[6].visibility = wgpu::ShaderStage::Fragment;
    wBindGroupLayoutEntries[6].texture.sampleType = wgpu::TextureSampleType::Float;
    wBindGroupLayoutEntries[6].texture.viewDimension = wgpu::TextureViewDimension::Cube;
    wBindGroupLayoutEntries[6].texture.multisampled = false;

    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor{
        .entryCount = static_cast<uint32_t>(wBindGroupLayoutEntries.size()),
        .entries = wBindGroupLayoutEntries.data()
    };

    wBindGroupLayout = wDevice.CreateBindGroupLayout(&bindGroupLayoutDescriptor);
}

void Renderer::SetupMeshBindGroup()
{
    wBindGroupEntries.resize(7, {});

    wBindGroupEntries[0].binding = 0;
    wBindGroupEntries[0].buffer = wUniformBuffer;
    wBindGroupEntries[0].offset = 0;
    wBindGroupEntries[0].size = sizeof(ShaderUniforms);

    wBindGroupEntries[1].binding = 1;
    wBindGroupEntries[1].sampler = wSampler;

    wBindGroupEntries[2].binding = 2;
    wBindGroupEntries[2].textureView = wAlbedoTextureView;

    wBindGroupEntries[3].binding = 3;
    wBindGroupEntries[3].textureView = wNormalTextureView;

    wBindGroupEntries[4].binding = 4;
    wBindGroupEntries[4].textureView = wMetallicTextureView;

    wBindGroupEntries[5].binding = 5;
    wBindGroupEntries[5].textureView = wRoughnessTextureView;

    wBindGroupEntries[6].binding = 6;
    wBindGroupEntries[6].textureView = wSkyboxTextureView;

    wgpu::BindGroupDescriptor bindGroupDescriptor{
        .layout = wBindGroupLayout,
        .entryCount = static_cast<uint32_t>(wBindGroupEntries.size()),
        .entries = wBindGroupEntries.data()
    };

    wBindGroup = wDevice.CreateBindGroup(&bindGroupDescriptor);
}

void Renderer::SetupSampler()
{
    wgpu::SamplerDescriptor samplerDescriptor{
        .addressModeU = wgpu::AddressMode::ClampToEdge,
        .addressModeV = wgpu::AddressMode::ClampToEdge,
        .addressModeW = wgpu::AddressMode::ClampToEdge,
        .magFilter = wgpu::FilterMode::Linear,
        .minFilter = wgpu::FilterMode::Linear,
        .mipmapFilter = wgpu::MipmapFilterMode::Linear,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 0.0f,
        .compare = wgpu::CompareFunction::Undefined,
        .maxAnisotropy = 1,
    };

    wSampler = wDevice.CreateSampler(&samplerDescriptor);
}

void Renderer::SetupMeshUniformBuffer()
{
    wgpu::BufferDescriptor bufferDescriptor{
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = sizeof(ShaderUniforms),
        .mappedAtCreation = false
    };

    wUniformBuffer = wDevice.CreateBuffer(&bufferDescriptor);
    ShaderUniforms uniforms{};
    wDevice.GetQueue().WriteBuffer(wUniformBuffer, 0, &uniforms, sizeof(ShaderUniforms));

    bufferDescriptor.size = sizeof(SkyboxMapUniforms);
    wSkyboxUniformBuffer = wDevice.CreateBuffer(&bufferDescriptor);
    SkyboxMapUniforms cubeMapUniforms{};
    wDevice.GetQueue().WriteBuffer(wSkyboxUniformBuffer, 0, &cubeMapUniforms, sizeof(SkyboxMapUniforms));
}

void Renderer::LoadTextures(const std::string& name, const ETextureImportType type)
{
    std::string suffix = "";

    switch (type)
    {
        case ETextureImportType::png:
            suffix = ".png";
            break;
        case ETextureImportType::jpg:
            suffix = ".jpg";
            break;
        default:
            break;
    }

    if(wAlbedoTexture = ResourceLoader::LoadTexture((name + "_c" + suffix).c_str(), wDevice, type, &wAlbedoTextureView); !wAlbedoTexture)
    {
        LogError("Could not load albedo texture!");
    }

    if(wNormalTexture = ResourceLoader::LoadTexture((name + "_n" + suffix).c_str(), wDevice, type, &wNormalTextureView); !wNormalTexture)
    {
        LogError("Could not load normal texture!");
    }

    if(wRoughnessTexture = ResourceLoader::LoadTexture((name + "_r" + suffix).c_str(), wDevice, type, &wRoughnessTextureView); !wRoughnessTexture)
    {
        LogError("Could not load roughness texture!");
    }

    if(wMetallicTexture = ResourceLoader::LoadTexture((name + "_m" + suffix).c_str(), wDevice, type, &wMetallicTextureView); !wMetallicTexture)
    {
        LogError("Could not load metallic texture!");
    }

    if(wSkyboxTexture = ResourceLoader::LoadCubeMap("golden_bay", wDevice, ETextureImportType::png, &wSkyboxTextureView); !wSkyboxTexture)
    {
        LogError("Could not load cubemap texture!");
    }
}

void Renderer::SetupDepthStencil()
{
    wDepthStencilState = wgpu::DepthStencilState{
        .format = wgpu::TextureFormat::Depth16Unorm,
        .depthWriteEnabled = true,
        .depthCompare = wgpu::CompareFunction::LessEqual,
        .stencilReadMask = 0,
        .stencilWriteMask = 0,
    };

    wgpu::TextureDescriptor depthTextureDescriptor{
        .usage = wgpu::TextureUsage::RenderAttachment,
        .dimension = wgpu::TextureDimension::e2D,
        .size = {kWidth, kHeight, 1},
        .format = wgpu::TextureFormat::Depth16Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormatCount = 1,
        .viewFormats = &wDepthStencilState.format,
    };

    wgpu::TextureViewDescriptor depthTextureViewDescriptor{
        .format = wDepthStencilState.format,
        .dimension = wgpu::TextureViewDimension::e2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = wgpu::TextureAspect::DepthOnly,
    };

    wDepthTexture = wDevice.CreateTexture(&depthTextureDescriptor);
    wDepthTextureView = wDepthTexture.CreateView(&depthTextureViewDescriptor);
}

void Renderer::SetupMeshVertexBuffers()
{
    wVertexAttributes.resize(6, {});

    wVertexAttributes[0].shaderLocation = 0;
    wVertexAttributes[0].format = wgpu::VertexFormat::Float32x3;
    wVertexAttributes[0].offset = 0;

    // Normal attribute
    wVertexAttributes[1].shaderLocation = 1;
    wVertexAttributes[1].format = wgpu::VertexFormat::Float32x3;
    wVertexAttributes[1].offset = 0;

    // Tangent attribute
    wVertexAttributes[2].shaderLocation = 2;
    wVertexAttributes[2].format = wgpu::VertexFormat::Float32x3;
    wVertexAttributes[2].offset = 0;

    // Bitangent attribute
    wVertexAttributes[3].shaderLocation = 3;
    wVertexAttributes[3].format = wgpu::VertexFormat::Float32x3;
    wVertexAttributes[3].offset = 0;

    // Color attribute
    wVertexAttributes[4].shaderLocation = 4;
    wVertexAttributes[4].format = wgpu::VertexFormat::Float32x4;
    wVertexAttributes[4].offset = 0;

    // UV attribute
    wVertexAttributes[5].shaderLocation = 5;
    wVertexAttributes[5].format = wgpu::VertexFormat::Float32x2;
    wVertexAttributes[5].offset = 0;

    wVertexBufferLayouts.resize(6, {});

    // Position attribute
    wVertexBufferLayouts[0].attributeCount = 1;
    wVertexBufferLayouts[0].arrayStride = 3 * sizeof(float);
    wVertexBufferLayouts[0].attributes = &wVertexAttributes[0];
    wVertexBufferLayouts[0].stepMode = wgpu::VertexStepMode::Vertex;

    // Normal attribute
    wVertexBufferLayouts[1].attributeCount = 1;
    wVertexBufferLayouts[1].arrayStride = 3 * sizeof(float);
    wVertexBufferLayouts[1].attributes = &wVertexAttributes[1];
    wVertexBufferLayouts[1].stepMode = wgpu::VertexStepMode::Vertex;

    // Tangent attribute
    wVertexBufferLayouts[2].attributeCount = 1;
    wVertexBufferLayouts[2].arrayStride = 3 * sizeof(float);
    wVertexBufferLayouts[2].attributes = &wVertexAttributes[2];
    wVertexBufferLayouts[2].stepMode = wgpu::VertexStepMode::Vertex;

    // Bitangent attribute
    wVertexBufferLayouts[3].attributeCount = 1;
    wVertexBufferLayouts[3].arrayStride = 3 * sizeof(float);
    wVertexBufferLayouts[3].attributes = &wVertexAttributes[3];
    wVertexBufferLayouts[3].stepMode = wgpu::VertexStepMode::Vertex;

    // Color attribute
    wVertexBufferLayouts[4].attributeCount = 1;
    wVertexBufferLayouts[4].arrayStride = 4 * sizeof(float);
    wVertexBufferLayouts[4].attributes = &wVertexAttributes[4];
    wVertexBufferLayouts[4].stepMode = wgpu::VertexStepMode::Vertex;

    // UV attribute
    wVertexBufferLayouts[5].attributeCount = 1;
    wVertexBufferLayouts[5].arrayStride = 2 * sizeof(float);
    wVertexBufferLayouts[5].attributes = &wVertexAttributes[5];
    wVertexBufferLayouts[5].stepMode = wgpu::VertexStepMode::Vertex;

    // CubeMap Vertex Attribute
    wSkyboxVertexAttribute.shaderLocation = 0;
    wSkyboxVertexAttribute.format = wgpu::VertexFormat::Float32x3;
    wSkyboxVertexAttribute.offset = 0;

    wSkyboxVertexBufferLayout.attributeCount = 1;
    wSkyboxVertexBufferLayout.arrayStride = 3 * sizeof(float);
    wSkyboxVertexBufferLayout.attributes = &wSkyboxVertexAttribute;
    wSkyboxVertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
}

void Renderer::DrawMesh(wgpu::RenderPassEncoder& renderPass)
{
    renderPass.SetPipeline(wRenderPipeline);
    renderPass.SetBindGroup(0, wBindGroup, 0, nullptr);

    v3 pos = v3(0.f);
    v3 rot = v3(0.f);
    v3 scale = v3(1.f);

    auto& positionBuffer = Mesh.positionBuffer;
    auto& normalBuffer = Mesh.normalBuffer;
    auto& tangentBuffer = Mesh.tangentBuffer;
    auto& bitangentBuffer = Mesh.bitangentBuffer;
    auto& uvBuffer = Mesh.uvBuffer;
    auto& colorBuffer = Mesh.colorBuffer;
    auto& indexBuffer = Mesh.indexBuffer;

    auto& pointData = Mesh.pointData;
    auto& normalData = Mesh.normalData;
    auto& tangentData = Mesh.tangentData;
    auto& bitangentData = Mesh.bitangentData;
    auto& colorData = Mesh.colorData;
    auto& uvData = Mesh.uvData;

    auto& indexData = Mesh.indexData;
    auto& indexCount = Mesh.indexCount;

    ShaderUniforms uniforms{};
    m4 model = m4(1.0f);
    model = glm::translate(model, pos);
    model = glm::rotate(model, glm::radians((f32)glfwGetTime() * 30.f), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians((f32)glfwGetTime() * 80.f), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians((f32)glfwGetTime() * 40.f), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale * 0.5f);
    uniforms.m_Model = model;
    uniforms.m_CameraPosition = Camera.Position;

    uniforms.m_Projection = glm::perspective(glm::radians(45.0f), (f32)kWidth / (f32)kHeight, 0.1f, 100.0f);
    uniforms.m_View = glm::lookAt(Camera.Position, v3(0.0f), Camera.Up);
    uniforms.m_padding = (f32)glfwGetTime();

    wDevice.GetQueue().WriteBuffer(wUniformBuffer, 0, &uniforms, sizeof(ShaderUniforms));

    renderPass.SetVertexBuffer(0, positionBuffer, 0, pointData.size() * sizeof(f32));
    renderPass.SetVertexBuffer(1, normalBuffer, 0, normalData.size() * sizeof(f32));
    renderPass.SetVertexBuffer(2, tangentBuffer, 0, tangentData.size() * sizeof(f32));
    renderPass.SetVertexBuffer(3, bitangentBuffer, 0, bitangentData.size() * sizeof(f32));
    renderPass.SetVertexBuffer(4, colorBuffer, 0, colorData.size() * sizeof(f32));
    renderPass.SetVertexBuffer(5, uvBuffer, 0, uvData.size() * sizeof(f32));
    renderPass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint16, 0, indexData.size() * sizeof(u16));
    renderPass.DrawIndexed(indexCount, 1, 0, 0, 0);
}

void Renderer::SetupCamera()
{
    Camera = CCamera();
    Camera.Position = v3(0.0f, 0.0f, 3.0f);
    Camera.Front = v3(0.0f, 0.0f, -1.0f);
    Camera.Up = v3(0.0f, 1.0f, 0.0f);
    Camera.Right = v3(1.0f, 0.0f, 0.0f);
}

void Renderer::SetupSkyboxPipeline()
{
    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.code = Reader::ReadTextFile("shaders/cubemap.wgsl");

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{
        .nextInChain = &wgslDesc
    };
    shaderModuleDescriptor.label = "CubeMap Shader Module";
    wgpu::ShaderModule shaderModule =
            wDevice.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState{
            .format = wgpu::TextureFormat::BGRA8Unorm};

    wgpu::FragmentState fragmentState{.module = shaderModule,
            .targetCount = 1,
            .targets = &colorTargetState};

    wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor{
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &wSkyboxBindGroupLayout};

    wgpu::PipelineLayout pipelineLayout = wDevice.CreatePipelineLayout(&pipelineLayoutDescriptor);

    wgpu::RenderPipelineDescriptor descriptor{
            .label = "CubeMap",
            .layout = pipelineLayout,
            .vertex = {.module = shaderModule},
            .depthStencil = &wDepthStencilState,
            .fragment = &fragmentState,
    };

//    descriptor.vertex.bufferCount = 1;
//    descriptor.vertex.buffers = &wCubeMapVertexBufferLayout;

    wCubeMapPipeline = wDevice.CreateRenderPipeline(&descriptor);
}

void Renderer::SetupSkyboxBindGroupLayout()
{
    wSkyboxBindGroupEntryLayouts[0].binding = 0;
    wSkyboxBindGroupEntryLayouts[0].buffer.hasDynamicOffset = false;
    wSkyboxBindGroupEntryLayouts[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    wSkyboxBindGroupEntryLayouts[0].buffer.type = wgpu::BufferBindingType::Uniform;
    wSkyboxBindGroupEntryLayouts[0].buffer.minBindingSize = sizeof(SkyboxMapUniforms);

    wSkyboxBindGroupEntryLayouts[1].binding = 1;
    wSkyboxBindGroupEntryLayouts[1].visibility = wgpu::ShaderStage::Fragment;
    wSkyboxBindGroupEntryLayouts[1].sampler.type = wgpu::SamplerBindingType::Filtering;

    wSkyboxBindGroupEntryLayouts[2].binding = 2;
    wSkyboxBindGroupEntryLayouts[2].visibility = wgpu::ShaderStage::Fragment;
    wSkyboxBindGroupEntryLayouts[2].texture.sampleType = wgpu::TextureSampleType::Float;
    wSkyboxBindGroupEntryLayouts[2].texture.viewDimension = wgpu::TextureViewDimension::Cube;
    wSkyboxBindGroupEntryLayouts[2].texture.multisampled = false;

    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor{
        .entryCount = wSkyboxBindGroupEntryLayouts.size(),
        .entries = wSkyboxBindGroupEntryLayouts.data()
    };

    wSkyboxBindGroupLayout = wDevice.CreateBindGroupLayout(&bindGroupLayoutDescriptor);
}

void Renderer::SetupSkyboxBindGroup()
{
    wSkyboxBindGroupEntries[0].binding = 0;
    wSkyboxBindGroupEntries[0].buffer = wSkyboxUniformBuffer;
    wSkyboxBindGroupEntries[0].offset = 0;
    wSkyboxBindGroupEntries[0].size = sizeof(SkyboxMapUniforms);

    wSkyboxBindGroupEntries[1].binding = 1;
    wSkyboxBindGroupEntries[1].sampler = wSampler;

    wSkyboxBindGroupEntries[2].binding = 2;
    wSkyboxBindGroupEntries[2].textureView = wSkyboxTextureView;


    wgpu::BindGroupDescriptor bindGroupDescriptor{
        .layout = wSkyboxBindGroupLayout,
        .entryCount = wSkyboxBindGroupEntries.size(),
        .entries = wSkyboxBindGroupEntries.data()
    };

    wSkyboxBindGroup = wDevice.CreateBindGroup(&bindGroupDescriptor);
}

void Renderer::DrawSkybox(wgpu::RenderPassEncoder &renderPass)
{
    renderPass.SetBindGroup(0, wSkyboxBindGroup, 0, nullptr);
    renderPass.SetPipeline(wCubeMapPipeline);

    Camera.Position = v3(cos(glfwGetTime() * .2f) * 4.f, 0.0f, -sin(glfwGetTime() * .2f) * 4.f);

    SkyboxMapUniforms uniforms{};
    m4 projection = glm::perspective(glm::radians(45.0f), (f32)kWidth / (f32)kHeight, 0.1f, 100.0f);
    m4 view = glm::lookAt(Camera.Position, v3(0.0f), Camera.Up);
    uniforms.m_MVPi = glm::inverse(projection * glm::mat4(glm::mat3(view)));

    wDevice.GetQueue().WriteBuffer(wSkyboxUniformBuffer, 0, &uniforms, sizeof(SkyboxMapUniforms));
    renderPass.Draw(3);
}

} // photon