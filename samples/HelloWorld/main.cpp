// Copyright © Amer Koleci and Contributors.
// Distributed under the MIT license. See the LICENSE file in the project root for more information.

#include "GLFW/glfw3.h"
#if defined(__linux__)
#   define GLFW_EXPOSE_NATIVE_X11
#elif defined(_WIN32)
#   define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include "GLFW/glfw3native.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <array>
#include <assert.h>
#include <vgpu.h>

VGPUDevice device = nullptr;
VGPUSwapChain swapChain = nullptr;
VGPUTexture depthStencilTexture = nullptr;
VGPUBuffer vertexBuffer = nullptr;
VGPUBuffer index_buffer = nullptr;
VGPUPipelineLayout pipelineLayout = nullptr;
VGPUPipeline renderPipeline = nullptr;

inline void vgpu_log(VGPULogLevel level, const char* message, void* user_data)
{
#if defined(_WIN32)
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
#endif
}

struct float4 {
    float x;
    float y;
    float z;
    float w;
};

struct PushData {
    float4 color;
};

std::string getAssetPath()
{
    return "assets/";
}

std::string getShadersPath()
{
    return getAssetPath() + "shaders/";
}

VGPUShaderModule LoadShader(const char* fileName)
{
    std::string shaderExt = ".spv";
    if (vgpuGetBackend(device) == VGPUBackend_D3D12)
    {
        shaderExt = ".cso";
    }

    std::ifstream is(getShadersPath() + "/" + fileName + shaderExt, std::ios::binary | std::ios::in | std::ios::ate);

    if (is.is_open())
    {
        size_t size = is.tellg();
        is.seekg(0, std::ios::beg);
        char* shaderCode = new char[size];
        is.read(shaderCode, size);
        is.close();

        assert(size > 0);

        VGPUShaderModule shader = vgpuCreateShaderModule(device, shaderCode, size);

        delete[] shaderCode;

        return shader;
    }
    else
    {
        std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << "\n";
        return nullptr;
    }
}

void init_vgpu(GLFWwindow* window)
{
    VGPUDeviceDescriptor deviceDesc{};
    deviceDesc.label = "test device";
#ifdef _DEBUG
    deviceDesc.validationMode = VGPUValidationMode_Enabled;
#endif

    if (vgpuIsBackendSupported(VGPUBackend_Vulkan))
    {
        deviceDesc.preferredBackend = VGPUBackend_Vulkan;
    }

    device = vgpuCreateDevice(&deviceDesc);
    assert(device != nullptr);

    VGPUAdapterProperties adapterProps;
    vgpuGetAdapterProperties(device, &adapterProps);

    VGPULimits limits;
    vgpuGetLimits(device, &limits);

    int width = 0;
    int height = 0;
    glfwGetWindowSize(window, &width, &height);

    void* windowHandle = nullptr;
#if defined(GLFW_EXPOSE_NATIVE_WIN32)
    windowHandle = glfwGetWin32Window(window);
#elif defined(GLFW_EXPOSE_NATIVE_X11)
    windowHandle = (void*)(uintptr_t)glfwGetX11Window(window);
#endif

    VGPUSwapChainDesc swapChainDesc{};
    swapChainDesc.width = width;
    swapChainDesc.height = height;
    swapChainDesc.format = VGPUTextureFormat_BGRA8UnormSrgb;
    swapChainDesc.presentMode = VGPUPresentMode_Fifo;
    swapChain = vgpuCreateSwapChain(device, windowHandle, &swapChainDesc);

    VGPUTextureDesc textureDesc = {};
    textureDesc.dimension = VGPUTextureDimension_2D;
    textureDesc.width = width;
    textureDesc.height = height;
    //textureDesc.depthOrArrayLayers = 1u;
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
    VGPUBufferDesc index_buffer_desc{};
    index_buffer_desc.label = "Vertex Buffer";
    index_buffer_desc.size = sizeof(indices);
    index_buffer_desc.usage = VGPUBufferUsage_Index;
    index_buffer = vgpuCreateBuffer(device, &index_buffer_desc, indices);

    VGPUShaderModule vertex_shader = LoadShader("triangleVertex");
    VGPUShaderModule fragment_shader = LoadShader("triangleFragment");

    VGPUPushConstantRange pushConstantRange{};
    pushConstantRange.visibility = VGPUShaderStage_Fragment;
    pushConstantRange.size = sizeof(PushData);

    VGPUPipelineLayoutDesc pipelineLayoutDesc{};
    pipelineLayoutDesc.pushConstantRangeCount = 1;
    pipelineLayoutDesc.pushConstantRanges = &pushConstantRange;
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
    renderPipelineDesc.vertex.module = vertex_shader;
    renderPipelineDesc.vertex.layoutCount = 1u;
    renderPipelineDesc.vertex.layouts = &vertexBufferLayout;

    renderPipelineDesc.fragment = fragment_shader;
    renderPipelineDesc.colorFormatCount = 1u;
    renderPipelineDesc.colorFormats = &colorFormat;
    renderPipelineDesc.depthStencilFormat = VGPUTextureFormat_Depth32Float;
    renderPipelineDesc.depthStencilState.depthWriteEnabled = true;
    renderPipelineDesc.depthStencilState.depthCompareFunction = VGPUCompareFunction_LessEqual;

    renderPipeline = vgpuCreateRenderPipeline(device, &renderPipelineDesc);
    vgpuDestroyShaderModule(device, vertex_shader);
    vgpuDestroyShaderModule(device, fragment_shader);
}

void draw_frame()
{
    VGPUCommandBuffer commandBuffer = vgpuBeginCommandBuffer(device, VGPUCommandQueue_Graphics, "Frame");

    // When window minimized texture is null
    uint32_t width;
    uint32_t height;
    VGPUTexture swapChainTexture = vgpuAcquireSwapchainTexture(commandBuffer, swapChain, &width, &height);
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
        depthStencilAttachment.depthLoadOp = VGPULoadAction_Clear;
        depthStencilAttachment.depthStoreOp = VGPUStoreAction_Store;
        depthStencilAttachment.depthClearValue = 1.0f;

        VGPURenderPassDesc renderPass{};
        renderPass.colorAttachmentCount = 1u;
        renderPass.colorAttachments = &colorAttachment;
        renderPass.depthStencilAttachment = &depthStencilAttachment;
        vgpuBeginRenderPass(commandBuffer, &renderPass);
        vgpuSetPipeline(commandBuffer, renderPipeline);

        PushData pushData{};
        pushData.color.x = 1.0f;
        vgpuSetPushConstants(commandBuffer, 0, &pushData, sizeof(pushData));

        vgpuSetVertexBuffer(commandBuffer, 0, vertexBuffer, 0);
        vgpuSetIndexBuffer(commandBuffer, index_buffer, VGPUIndexType_Uint16, 0);
        vgpuDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
        vgpuEndRenderPass(commandBuffer);
    }

    vgpuSubmit(device, &commandBuffer, 1u);
}

int main()
{
    if (!glfwInit())
    {
        return EXIT_FAILURE;
    }

    vgpuSetLogCallback(vgpu_log, nullptr);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Hello World", NULL, NULL);
    init_vgpu(window);

    glfwShowWindow(window);
    while (!glfwWindowShouldClose(window))
    {
        draw_frame();
        glfwPollEvents();
    }

    vgpuWaitIdle(device);
    vgpuBufferRelease(vertexBuffer);
    vgpuBufferRelease(index_buffer);
    vgpuTextureRelease(depthStencilTexture);
    vgpuPipelineLayoutRelease(pipelineLayout);
    vgpuPipelineRelease(renderPipeline);
    vgpuSwapChainRelease(swapChain);
    vgpuDestroyDevice(device);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
