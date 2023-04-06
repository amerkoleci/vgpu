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
VGPUPipeline* renderPipeline = nullptr;

inline void vgpu_log(VGPULogLevel level, const char* message)
{
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
    if (vgpuGetBackendType(device) == VGPUBackendType_D3D12)
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

        VGPUShaderModule shader = vgpuCreateShader(device, shaderCode, size);

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
        VGFXDevice device = (VGFXDevice)userData;
        vgfxFrame(device);
        return EM_TRUE;
    }
}

void init_gfx()
#else
void* os_get_gl_proc_address(const char* function) {
    return (void*)glfwGetProcAddress(function);
}

void init_gfx(GLFWwindow* window, bool opengl)
#endif
{
    VGPUDeviceDesc deviceDesc{};
    deviceDesc.label = "test device";
#ifdef _DEBUG
    deviceDesc.validationMode = VGPUValidationMode_Enabled;
#endif

    if (opengl)
    {
        deviceDesc.gl.gl_get_proc_address = os_get_gl_proc_address;
    }

    if (vgpuIsBackendSupported(VGPUBackendType_Vulkan))
    {
        deviceDesc.preferredBackend = VGPUBackendType_Vulkan;
    }

    if (vgpuIsBackendSupported(VGPUBackendType_OpenGL))
    {
        deviceDesc.preferredBackend = VGPUBackendType_OpenGL;
    }

    device = vgpuCreateDevice(&deviceDesc);

    VGPUAdapterProperties adapterProps;
    vgpuGetAdapterProperties(device, &adapterProps);

    VGPULimits limits;
    vgpuGetLimits(device, &limits);

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

    depthStencilTexture = vgpuCreateTexture2D(device,
        width, height,
        VGPUTextureFormat_Depth32Float,
        1,
        VGPUTextureUsage_RenderTarget,
        nullptr);

    // Create vertex buffer
    const float vertices[] = {
        /* positions            colors */
         0.0f, 0.5f, 0.5f,      1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f
    };

    vgpu_buffer_desc vertex_buffer_desc{};
    vertex_buffer_desc.label = "Vertex Buffer";
    vertex_buffer_desc.size = sizeof(vertices);
    vertex_buffer_desc.usage = VGPU_BUFFER_USAGE_VERTEX;
    vertex_buffer = vgpu_buffer_create(device, &vertex_buffer_desc, vertices);

    VGPUShaderModule vertexShader = LoadShader("triangleVertex");
    VGPUShaderModule fragmentShader = LoadShader("triangleFragment");

    RenderPipelineColorAttachmentDesc colorAttachment{};
    colorAttachment.format = vgpuSwapChainGetFormat(device, swapChain);

    VGPURenderPipelineDesc renderPipelineDesc{};
    renderPipelineDesc.label = "Triangle";
    renderPipelineDesc.vertex = vertexShader;
    renderPipelineDesc.fragment = fragmentShader;
    renderPipelineDesc.primitiveTopology = VGPUPrimitiveTopology_TriangleList;
    renderPipelineDesc.colorAttachmentCount = 1u;
    renderPipelineDesc.colorAttachments = &colorAttachment;
    renderPipelineDesc.depthAttachmentFormat = VGPUTextureFormat_Depth32Float;
    renderPipelineDesc.depthStencilState.depthWriteEnabled = true;
    renderPipelineDesc.depthStencilState.depthCompare = VGPUCompareFunction_Less;

    renderPipeline = vgpuCreateRenderPipeline(device, &renderPipelineDesc);
    vgpuDestroyShader(device, vertexShader);
    vgpuDestroyShader(device, fragmentShader);
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
    emscripten_request_animation_frame_loop(_app_emsc_frame, device);
}
#endif

void draw_frame()
{
    VGPUTextureFormat format = vgpuSwapChainGetFormat(device, swapChain);
    uint32_t width;
    uint32_t height;

    VGPUCommandBuffer commandBuffer = vgpuBeginCommandBuffer(device, VGPUCommandQueue_Graphics, "Frame");

    // When window minimized texture is null
    VGPUTexture swapChainTexture = vgpuAcquireSwapchainTexture(commandBuffer, swapChain, &width, &height);
    if (swapChainTexture != nullptr)
    {
        VGPURenderPassColorAttachment colorAttachment = {};
        colorAttachment.texture = swapChainTexture;
        colorAttachment.loadOp = VGPULoadAction_Clear;
        colorAttachment.storeOp = VGPUStoreOp_Store;
        colorAttachment.clearColor.r = 0.3f;
        colorAttachment.clearColor.g = 0.3f;
        colorAttachment.clearColor.b = 0.3f;
        colorAttachment.clearColor.a = 1.0f;

        VGPURenderPassDepthStencilAttachment depthStencilAttachment{};
        depthStencilAttachment.texture = depthStencilTexture;
        depthStencilAttachment.depthLoadOp = VGPULoadAction_Clear;
        depthStencilAttachment.depthStoreOp = VGPUStoreOp_Store;
        depthStencilAttachment.clearDepth = 1.0f;

        VGPURenderPassDesc renderPass{};
        renderPass.colorAttachmentCount = 1u;
        renderPass.colorAttachments = &colorAttachment;
        renderPass.depthStencilAttachment = &depthStencilAttachment;
        vgpuBeginRenderPass(commandBuffer, &renderPass);
        vgpuSetPipeline(commandBuffer, renderPipeline);
        vgpuSetVertexBuffer(commandBuffer, 0, vertex_buffer, 0);
        vgpuDraw(commandBuffer, 0, 3, 1, 0);
        vgpuEndRenderPass(commandBuffer);
    }

    vgpuSubmit(device, &commandBuffer, 1u);
    vgpuFrame(device);
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

    //vgpu_set_log_callback(vgpu_log);

    bool opengl = true;
    if (opengl)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }
    else
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Hello World", NULL, NULL);
    if (opengl)
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
    }
    init_gfx(window, opengl);

    glfwShowWindow(window);
    while (!glfwWindowShouldClose(window))
    {
        draw_frame();
        if (opengl)
        {
            glfwSwapBuffers(window);
        }
        glfwPollEvents();
    }

    vgpuWaitIdle(device);
    vgpu_buffer_destroy(device, vertex_buffer);
    vgpuDestroyTexture(device, depthStencilTexture);
    vgpuDestroyPipeline(device, renderPipeline);
    vgpuDestroySwapChain(device, swapChain);
    vgpuDestroyDevice(device);
    glfwDestroyWindow(window);
    glfwTerminate();
#endif
    return EXIT_SUCCESS;
}
