// Copyright © Amer Koleci and Contributors.
// Distributed under the MIT license. See the LICENSE file in the project root for more information.

#include "GLFW/glfw3.h"
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#if defined(__linux__)
#   define GLFW_EXPOSE_NATIVE_X11
#elif defined(_WIN32)
#   define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include "GLFW/glfw3native.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <array>
#include <assert.h>
#include <vector>
#include <cstring>

#include <vgpu.h>

VGPUInstance instance = nullptr;
VGPUDevice device = nullptr;
VGPUSwapChain swapChain = nullptr;
VGPUTexture depthStencilTexture = nullptr;
VGPUBuffer vertexBuffer = nullptr;
VGPUBuffer indexBuffer = nullptr;
VGPUPipelineLayout pipelineLayout = nullptr;
VGPUPipeline renderPipeline = nullptr;

VGPUBuffer constantBuffer = nullptr;
VGPUBindGroup bindGroup = nullptr;

inline void vgpu_log(VGPULogLevel level, const char* message, void* user_data)
{
#if defined(_WIN32)
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
#endif
}

struct PushData {
    VGPUColor color;
};

std::string getAssetPath()
{
    return "assets/";
}

std::string getShadersPath()
{
    return getAssetPath() + "shaders/";
}

static std::vector<uint8_t> LoadShader(const char* fileName)
{
    std::string shaderExt = ".spv";
    if (vgpuDeviceGetBackend(device) == VGPUBackend_D3D12)
    {
        shaderExt = ".cso";
    }

    std::ifstream is(getShadersPath() + "/" + fileName + shaderExt, std::ios::binary | std::ios::in | std::ios::ate);

    if (is.is_open())
    {
        size_t size = is.tellg();
        is.seekg(0, std::ios::beg);

        std::vector<uint8_t> bytecode(size);
        is.read((char*)bytecode.data(), size);
        is.close();

        return bytecode;
    }
    else
    {
        std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << "\n";
        return {};
    }
}

void init_vgpu(GLFWwindow* window)
{
    VGPUDeviceDesc deviceDesc{};
    deviceDesc.label = "test device";
#ifdef _DEBUG
    deviceDesc.validationMode = VGPUValidationMode_Enabled;
#endif

    //deviceDesc.preferredBackend = VGPUBackend_D3D12;
    if (deviceDesc.preferredBackend == VGPUBackendType_Undefined
        && vgpuIsBackendSupported(VGPUBackend_Vulkan))
    {
        deviceDesc.preferredBackend = VGPUBackend_Vulkan;
    }

    device = vgpuCreateDevice(&deviceDesc);
    assert(device != nullptr);

    VGPUAdapterProperties adapterProps;
    vgpuDeviceGetAdapterProperties(device, &adapterProps);

    VGPULimits limits;
    vgpuDeviceGetLimits(device, &limits);

    int width = 0;
    int height = 0;
    glfwGetWindowSize(window, &width, &height);

    VGPUSwapChainDesc swapChainDesc{};
#if defined(GLFW_EXPOSE_NATIVE_WIN32)
    swapChainDesc.windowHandle = (uintptr_t)glfwGetWin32Window(window);
#elif defined(GLFW_EXPOSE_NATIVE_X11)
    swapChainDesc.displayHandle = (void*)(uintptr_t)glfwGetX11Display();
    swapChainDesc.windowHandle = (uintptr_t)glfwGetX11Window(window);
#endif
    swapChainDesc.width = width;
    swapChainDesc.height = height;
    swapChainDesc.format = VGPUTextureFormat_BGRA8UnormSrgb;
    swapChainDesc.presentMode = VGPUPresentMode_Fifo;
    swapChain = vgpuCreateSwapChain(device, &swapChainDesc);

    VGPUTextureDesc textureDesc = {};
    textureDesc.dimension = VGPUTextureDimension_2D;
    textureDesc.width = width;
    textureDesc.height = height;
    textureDesc.depthOrArrayLayers = 1u;
    textureDesc.format = VGPUTextureFormat_Depth32Float;
    textureDesc.usage = VGPUTextureUsage_RenderTarget;
    textureDesc.mipLevelCount = 1u;
    textureDesc.sampleCount = 1u;

    depthStencilTexture = vgpuCreateTexture(device, &textureDesc, nullptr);
    auto dimension = vgpuTextureGetDimension(depthStencilTexture);

    // Create vertex buffer
    const float vertices[] = {
        /* positions            colors */
        -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,
    };

    VGPUBufferDesc vertex_buffer_desc{};
    vertex_buffer_desc.label = "Vertex Buffer";
    vertex_buffer_desc.size = sizeof(vertices);
    vertex_buffer_desc.usage = VGPUBufferUsage_Vertex;
    vertexBuffer = vgpuCreateBuffer(device, &vertex_buffer_desc, vertices);

    const uint16_t indices[] = {
        0, 1, 2,    // first triangle
        0, 2, 3,    // second triangle
    };
    VGPUBufferDesc indexBufferDesc{};
    indexBufferDesc.label = "Index Buffer";
    indexBufferDesc.size = sizeof(indices);
    indexBufferDesc.usage = VGPUBufferUsage_Index;
    indexBuffer = vgpuCreateBuffer(device, &indexBufferDesc, indices);

    std::vector<uint8_t> vertexBytecode = LoadShader("triangleVertex");
    std::vector<uint8_t> fragmentBytecode = LoadShader("triangleFragment");

    // Stage info
    VGPUShaderStageDesc shaderStages[2] = {};
    shaderStages[0].stage = VGPUShaderStage_Vertex;
    shaderStages[0].bytecode = vertexBytecode.data();
    shaderStages[0].size = vertexBytecode.size();
    shaderStages[0].entryPointName = "vertexMain";

    shaderStages[1].stage = VGPUShaderStage_Fragment;
    shaderStages[1].bytecode = fragmentBytecode.data();
    shaderStages[1].size = fragmentBytecode.size();
    shaderStages[1].entryPointName = "fragmentMain";

    VGPUBindGroupLayoutEntry bindGroupLayoutEntry{};
    bindGroupLayoutEntry.binding = 0;
    bindGroupLayoutEntry.count = 1;
    bindGroupLayoutEntry.visibility = VGPUShaderStage_Fragment;
    bindGroupLayoutEntry.descriptorType = VGPUDescriptorType_ConstantBuffer;

    VGPUBindGroupLayoutDesc bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = 1;
    bindGroupLayoutDesc.entries = &bindGroupLayoutEntry;
    VGPUBindGroupLayout bindGroupLayout = vgpuCreateBindGroupLayout(device, &bindGroupLayoutDesc);

    // Buffer and Bind Group
    VGPUBufferDesc constantBufferDesc{};
    constantBufferDesc.label = "Constant Buffer";
    constantBufferDesc.size = sizeof(PushData);
    constantBufferDesc.usage = VGPUBufferUsage_Constant;

    PushData pushData{};
    pushData.color.r = 1.0f;
    constantBuffer = vgpuCreateBuffer(device, &constantBufferDesc, &pushData);

    VGPUBindGroupEntry bindGroupEntry{};
    bindGroupEntry.binding = 0;
    bindGroupEntry.arrayElement = 0;
    bindGroupEntry.buffer = constantBuffer;
    bindGroupEntry.offset = 0;
    bindGroupEntry.size = VGPU_WHOLE_SIZE;

    VGPUBindGroupDesc bindGroupDesc{};
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = &bindGroupEntry;
    bindGroup = vgpuCreateBindGroup(device, bindGroupLayout, &bindGroupDesc);

    //VGPUPushConstantRange pushConstantRange{};
    //pushConstantRange.visibility = VGPUShaderStage_Fragment;
    //pushConstantRange.size = sizeof(PushData);

    VGPUPipelineLayoutDesc pipelineLayoutDesc{};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
    //pipelineLayoutDesc.pushConstantRangeCount = 1;
    //pipelineLayoutDesc.pushConstantRanges = &pushConstantRange;
    pipelineLayout = vgpuCreatePipelineLayout(device, &pipelineLayoutDesc);

    VGPUTextureFormat colorFormat = vgpuSwapChainGetFormat(swapChain);

    std::array<VGPUVertexAttribute, 2> vertexAttributes = {};
    // Attribute location 0: Position
    vertexAttributes[0].format = VGPUVertexFormat_Float3;
    vertexAttributes[0].offset = 0;
    vertexAttributes[0].shaderLocation = 0;
    // Attribute location 1: Color
    vertexAttributes[1].format = VGPUVertexFormat_Float4;
    vertexAttributes[1].offset = 12;
    vertexAttributes[1].shaderLocation = 1;

    VGPUVertexBufferLayout vertexBufferLayout{};
    vertexBufferLayout.stride = 28;
    vertexBufferLayout.attributeCount = 2;
    vertexBufferLayout.attributes = vertexAttributes.data();

    VGPURenderPipelineDesc renderPipelineDesc{};
    renderPipelineDesc.label = "Triangle";
    renderPipelineDesc.layout = pipelineLayout;
    renderPipelineDesc.shaderStageCount = 2u;
    renderPipelineDesc.shaderStages = shaderStages;
    renderPipelineDesc.vertex.layoutCount = 1u;
    renderPipelineDesc.vertex.layouts = &vertexBufferLayout;

    renderPipelineDesc.colorFormatCount = 1u;
    renderPipelineDesc.colorFormats = &colorFormat;
    renderPipelineDesc.depthStencilFormat = VGPUTextureFormat_Depth32Float;
    renderPipelineDesc.depthStencilState.depthWriteEnabled = true;
    renderPipelineDesc.depthStencilState.depthCompareFunction = VGPUCompareFunction_LessEqual;
    renderPipelineDesc.blendState.renderTargets[0].colorWriteMask = VGPUColorWriteMask_All;
    renderPipeline = vgpuCreateRenderPipeline(device, &renderPipelineDesc);

    vgpuBindGroupLayoutRelease(bindGroupLayout);
}

void draw_frame()
{
#if defined(__EMSCRIPTEN__)
    return;
#else
    VGPUCommandBuffer commandBuffer = vgpuBeginCommandBuffer(device, VGPUCommandQueue_Graphics, "Frame");

    // When window minimized texture is null
    VGPUTexture swapChainTexture = vgpuAcquireSwapchainTexture(commandBuffer, swapChain);
    if (swapChainTexture != nullptr)
    {
        VGPURenderPassColorAttachment colorAttachment = {};
        colorAttachment.texture = swapChainTexture;
        colorAttachment.loadAction = VGPULoadAction_Clear;
        colorAttachment.storeAction = VGPUStoreAction_Store;
        colorAttachment.clearColor.r = 0.3f;
        colorAttachment.clearColor.g = 0.3f;
        colorAttachment.clearColor.b = 0.3f;
        colorAttachment.clearColor.a = 1.0f;

        VGPURenderPassDepthStencilAttachment depthStencilAttachment{};
        depthStencilAttachment.texture = depthStencilTexture;
        depthStencilAttachment.depthLoadAction = VGPULoadAction_Clear;
        depthStencilAttachment.depthStoreAction = VGPUStoreAction_Store;
        depthStencilAttachment.depthClearValue = 1.0f;

        VGPURenderPassDesc renderPass{};
        renderPass.label = "RenderPass";
        renderPass.colorAttachmentCount = 1u;
        renderPass.colorAttachments = &colorAttachment;
        renderPass.depthStencilAttachment = &depthStencilAttachment;
        vgpuBeginRenderPass(commandBuffer, &renderPass);
        vgpuSetPipeline(commandBuffer, renderPipeline);

        //PushData pushData{};
        //pushData.color.x = 1.0f;
        //vgpuSetPushConstants(commandBuffer, 0, &pushData, sizeof(pushData));

        vgpuSetBindGroup(commandBuffer, 0, bindGroup);

        vgpuSetVertexBuffer(commandBuffer, 0, vertexBuffer, 0);
        vgpuSetIndexBuffer(commandBuffer, indexBuffer, VGPUIndexType_Uint16, 0);
        vgpuDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
        vgpuEndRenderPass(commandBuffer);
    }

    vgpuDeviceSubmit(device, &commandBuffer, 1u);
#endif
}

int main()
{
    if (!glfwInit()) {
        return EXIT_FAILURE;
    }

#ifdef _DEBUG
    vgpuSetLogLevel(VGPULogLevel_Debug);
#else
    vgpuSetLogLevel(VGPULogLevel_Info);
#endif
    vgpuSetLogCallback(vgpu_log, nullptr);

    instance = vgpuCreateInstance(nullptr); 

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#if !defined(__EMSCRIPTEN__)
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#endif
    GLFWwindow* window = glfwCreateWindow(1280, 720, "VGPU Window", nullptr, nullptr);

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop(draw_frame, 0, false);
#else
    init_vgpu(window);
    glfwShowWindow(window);
    while (!glfwWindowShouldClose(window))
    {
        draw_frame();
        glfwPollEvents();
    }
#endif

    vgpuDeviceWaitIdle(device);
    vgpuBufferRelease(vertexBuffer);
    vgpuBufferRelease(indexBuffer);
    vgpuBufferRelease(constantBuffer);
    vgpuTextureRelease(depthStencilTexture);
    vgpuPipelineLayoutRelease(pipelineLayout);
    vgpuPipelineRelease(renderPipeline);
    vgpuBindGroupRelease(bindGroup);
    vgpuSwapChainRelease(swapChain);

    vgpuDeviceRelease(device);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
