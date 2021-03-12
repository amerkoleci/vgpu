//
// Copyright (c) 2019-2020 Amer Koleci.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <vgpu/vgpu.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__ANDROID__)
#elif defined(__EMSCRIPTEN__)
#   include <emscripten/emscripten.h>
#   include <emscripten/html5.h>
#else
#   define USE_GLFW
#   define GLFW_INCLUDE_NONE 
#   include <GLFW/glfw3.h>
#if defined(_WIN32) || defined(_WIN64)
#   define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#   if !defined(VGPU_GLFW_WAYLAND)
#       define GLFW_EXPOSE_NATIVE_X11
#   else
#       define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#elif defined(__APPLE__)
#   define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3native.h>

// GLFW3 Error Callback, runs on GLFW3 error
static void glfwErrorCallback(int error, const char* description)
{
    vgpu_log_error("[GLFW3 Error] Code: %d Decription: %s", error, description);
}
#endif

static void vgpu_log_fn(void* user_data, vgpu_log_level level, const char* message)
{
    printf("%s\n", message);
}

int main(int argc, char** argv)
{
    vgpu_set_log_callback(vgpu_log_fn, NULL);

#ifdef USE_GLFW
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit())
    {
        //ALIMER_LOGERROR("Failed to initialize GLFW");
        return EXIT_FAILURE;
    }

#if defined(__APPLE__)
    glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
#endif

    //VGPUBackendType preferredBackend = VGPUBackendType_Force32;
    //VGPUBackendType preferredBackend = VGPUBackendType_D3D11;
    //if (preferredBackend != VGPUBackendType_OpenGL)
    {
        // By default on non opengl context creation.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    /*else
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }*/

    GLFWwindow* window = glfwCreateWindow(1280, 720, "vgpu", 0, 0);
    /*if (preferredBackend == VGPUBackendType_OpenGL)
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
    }*/

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    VGPUDeviceDescriptor deviceDesc = {
#ifdef _DEBUG
        .flags = VGPUDeviceFlags_Debug,
#endif
        .swapchain = {
            .width = width,
            .height = height,
            .colorFormat = VGPUTextureFormat_BGRA8Unorm,
#if defined(_WIN32) || defined(_WIN64)
            .window_handle = (void*)glfwGetWin32Window(window),
#endif
      }
    };

    VGPUDevice device = vgpuCreateDevice(VGPU_BACKEND_TYPE_DEFAULT , &deviceDesc);
    if (!device) {
        return EXIT_FAILURE;
    }

    VGPUTexture depthTexture = vgpuCreateTexture(device, &(VGPUTextureDescription) {
        .usage = VGPUTextureUsage_RenderTarget,
        .width = width,
        .height = height,
        .format = vgpuGetDefaultDepthFormat(device)
    });


    VGPUColor clearColor = { 1.0f, 0.0f, 0.0f, 1.0f };

    while (!glfwWindowShouldClose(window))
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        if (!vgpuBeginFrame(device)) {
            return;
        }

        /* Begin default render pass and change its clear color */
        float g = clearColor.g + 0.01f;
        clearColor.g = (g > 1.0f) ? 0.0f : g;

        VGPURenderPassDescriptor renderPass;
        memset(&renderPass, 0, sizeof(VGPURenderPassDescriptor));
        renderPass.colorAttachments[0].texture = vgpuGetBackbufferTexture(device);
        renderPass.colorAttachments[0].mipLevel = 0;
        renderPass.colorAttachments[0].slice = 0;
        renderPass.colorAttachments[0].loadOp = VGPULoadOp_Clear;
        renderPass.colorAttachments[0].clearColor = clearColor;
        renderPass.depthStencilAttachment.texture = depthTexture;
        renderPass.depthStencilAttachment.depthLoadOp = VGPULoadOp_Clear;
        renderPass.depthStencilAttachment.clearDepth = 1.0f;

        vgpuCmdBeginRenderPass(device, &renderPass);
        vgpuCmdEndRenderPass(device);
        vgpuEndFrame(device);
        glfwPollEvents();
    }

    vgpuDestroyTexture(device, depthTexture);
    vgpuDestroyDevice(device);
    glfwTerminate();

#elif defined(__ANDROID__)
#elif defined(__EMSCRIPTEN__)
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    ctx = emscripten_webgl_create_context(0, &attrs);
    emscripten_webgl_make_context_current(ctx);

    setup_sokol();

    emscripten_set_main_loop(emscriptenLoop, 0, 1);
#endif

    return 0;
}
