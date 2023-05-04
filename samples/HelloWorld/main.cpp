// Copyright © Amer Koleci and Contributors.
// Distributed under the MIT license. See the LICENSE file in the project root for more information.

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#ifndef KEEP_IN_MODULE
#define KEEP_IN_MODULE extern "C" __attribute__((used, visibility("default")))
#endif
#else
#include "GLFW/glfw3.h"
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
#include <assert.h>
#include <vgpu.h>

VGPUDevice device = nullptr;
VGPUSwapChain* swapChain = nullptr;
VGPUTexture depthStencilTexture = nullptr;
vgpu_buffer* vertex_buffer = nullptr;
vgpu_buffer* index_buffer = nullptr;
VGPUPipeline* renderPipeline = nullptr;

inline void vgpu_log(VGPULogLevel level, const char* message, void* user_data)
{
#if defined(_WIN32)
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
#endif
}

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
    if (vgpuDeviceGetBackend(device) == VGPU_BACKEND_D3D12)
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

        VGPUShaderModule shader = vgpuCreateShaderModule(shaderCode, size);

        delete[] shaderCode;

        return shader;
    }
    else
    {
        std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << "\n";
        return nullptr;
    }
}

#if defined(__EMSCRIPTEN__)
namespace
{
    static EM_BOOL _app_emsc_frame(double time, void* userData)
    {
        //emscripten_log(EM_LOG_CONSOLE, "%s", "frame");
        vgpu_frame();
        return EM_TRUE;
    }
}

void init_vgpu()
#else
void init_vgpu(GLFWwindow* window)
#endif
{
    VGPUDeviceDescriptor deviceDesc{};
    deviceDesc.label = "test device";
#ifdef _DEBUG
    deviceDesc.validationMode = VGPUValidationMode_Enabled;
#endif

    if (vgpuIsBackendSupported(VGPU_BACKEND_VULKAN))
    {
        deviceDesc.preferredBackend = VGPU_BACKEND_VULKAN;
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

    void* windowHandle = nullptr;
#if defined(__EMSCRIPTEN__)
    windowHandle = "canvas";
#elif defined(GLFW_EXPOSE_NATIVE_WIN32)
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

    depthStencilTexture = vgpu_texture_create_2d(width, height,
        VGPUTextureFormat_Depth32Float,
        1,
        VGPUTextureUsage_RenderTarget,
        nullptr);

    // Create vertex buffer
    const float vertices[] = {
        /* positions            colors */
        -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,
    };

    vgpu_buffer_desc vertex_buffer_desc{};
    vertex_buffer_desc.label = "Vertex Buffer";
    vertex_buffer_desc.size = sizeof(vertices);
    vertex_buffer_desc.usage = VGPU_BUFFER_USAGE_VERTEX;
    vertex_buffer = vgpu_buffer_create(&vertex_buffer_desc, vertices);

    uint16_t indices[] = {
        0, 1, 2,    // first triangle
        0, 2, 3,    // second triangle
    };
    vgpu_buffer_desc index_buffer_desc{};
    index_buffer_desc.label = "Vertex Buffer";
    index_buffer_desc.size = sizeof(indices);
    index_buffer_desc.usage = VGPU_BUFFER_USAGE_INDEX;
    index_buffer = vgpu_buffer_create(&index_buffer_desc, indices);

    VGPUShaderModule vertex_shader = LoadShader("triangleVertex");
    VGPUShaderModule fragment_shader = LoadShader("triangleFragment");

    RenderPipelineColorAttachmentDesc colorAttachment{};
    colorAttachment.format = vgpuGetSwapChainFormat(device, swapChain);

    VGPURenderPipelineDesc renderPipelineDesc{};
    renderPipelineDesc.label = "Triangle";
    renderPipelineDesc.vertex = vertex_shader;
    renderPipelineDesc.fragment = fragment_shader;
    renderPipelineDesc.primitiveTopology = VGPUPrimitiveTopology_TriangleList;
    renderPipelineDesc.colorAttachmentCount = 1u;
    renderPipelineDesc.colorAttachments = &colorAttachment;
    renderPipelineDesc.depthStencilFormat = VGPUTextureFormat_Depth32Float;
    renderPipelineDesc.depthStencilState.depthWriteEnabled = true;
    renderPipelineDesc.depthStencilState.depthCompare = VGPUCompareFunction_Less;

    renderPipeline = vgpuCreateRenderPipeline(&renderPipelineDesc);
    vgpuDestroyShaderModule(vertex_shader);
    vgpuDestroyShaderModule(fragment_shader);
}

#if defined(__EMSCRIPTEN__)
EM_JS(void, glue_preint, (), {
    var entry = __glue_main_;
    if (entry) {
        /*
         * None of the WebGPU properties appear to survive Closure, including
         * Emscripten's own `preinitializedWebGPUDevice` (which from looking at
         *`library_html5` is probably designed to be inited in script before
         * loading the Wasm).
         */
        if (navigator["gpu"]) {
            navigator["gpu"]["requestAdapter"]().then(function(adapter) {
                adapter["requestDevice"]().then(function(device) {
                    Module["preinitializedWebGPUDevice"] = device;
                    entry();
                });
            }, function() {
                console.error("No WebGPU adapter; not starting");
            });
        }
 else {
  console.error("No support for WebGPU; not starting");
}
}
else {
 console.error("Entry point not found; unable to start");
}
    });

KEEP_IN_MODULE void _glue_main_()
{
    init_gfx();
    emscripten_request_animation_frame_loop(_app_emsc_frame);
}
#endif

void draw_frame()
{
    VGPUCommandBuffer commandBuffer = vgpuBeginCommandBuffer(VGPUCommandQueue_Graphics, "Frame");

    // When window minimized texture is null
    uint32_t width;
    uint32_t height;
    VGPUTexture swapChainTexture = vgpuAcquireSwapchainTexture(commandBuffer, swapChain, &width, &height);
    if (swapChainTexture != nullptr)
    {
        VGPURenderPassColorAttachment colorAttachment = {};
        colorAttachment.texture = swapChainTexture;
        colorAttachment.load_action = VGPU_LOAD_ACTION_CLEAR;
        colorAttachment.store_action = VGPU_STORE_ACTION_STORE;
        colorAttachment.clearColor.r = 0.3f;
        colorAttachment.clearColor.g = 0.3f;
        colorAttachment.clearColor.b = 0.3f;
        colorAttachment.clearColor.a = 1.0f;

        VGPURenderPassDepthStencilAttachment depthStencilAttachment{};
        depthStencilAttachment.texture = depthStencilTexture;
        depthStencilAttachment.depthLoadOp = VGPU_LOAD_ACTION_CLEAR;
        depthStencilAttachment.depthStoreOp = VGPU_STORE_ACTION_STORE;
        depthStencilAttachment.clearDepth = 1.0f;

        VGPURenderPassDesc renderPass{};
        renderPass.colorAttachmentCount = 1u;
        renderPass.colorAttachments = &colorAttachment;
        renderPass.depthStencilAttachment = &depthStencilAttachment;
        vgpuBeginRenderPass(commandBuffer, &renderPass);
        vgpuSetPipeline(commandBuffer, renderPipeline);
        vgpuSetVertexBuffer(commandBuffer, 0, vertex_buffer, 0);
        vgpuSetIndexBuffer(commandBuffer, index_buffer, 0, VGPU_INDEX_TYPE_UINT16);
        vgpuDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
        vgpuEndRenderPass(commandBuffer);
    }

    vgpu_submit(&commandBuffer, 1u);
    vgpu_frame();
}

int main()
{
#if defined(__EMSCRIPTEN__)
    glue_preint();
#else
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
    vgpu_buffer_destroy(vertex_buffer);
    vgpu_buffer_destroy(index_buffer);
    vgpu_texture_destroy(depthStencilTexture);
    vgpuDestroyPipeline(device, renderPipeline);
    vgpuDestroySwapChain(device, swapChain);
    vgpuDeviceDestroy(device);
    glfwDestroyWindow(window);
    glfwTerminate();
#endif
    return EXIT_SUCCESS;
}
