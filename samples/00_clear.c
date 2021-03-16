// Copyright (c) Amer Koleci.
// Distributed under the MIT license. See the LICENSE file in the project root for more information.

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

    vgpu_info gpu_config = {
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

    if (!vgpu_init(VGPU_BACKEND_TYPE_DEFAULT, &gpu_config)) {
        return EXIT_FAILURE;
    }

    VGPUTexture depthTexture = vgpuCreateTexture(&(VGPUTextureDescription) {
        .usage = VGPUTextureUsage_RenderTarget,
        .width = width,
        .height = height,
        .format = vgpu_get_default_depth_format()
    });


    VGPUColor clearColor = { 1.0f, 0.0f, 0.0f, 1.0f };

    while (!glfwWindowShouldClose(window))
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        if (!vgpu_begin_frame()) {
            return;
        }

        /* Begin default render pass and change its clear color */
        float g = clearColor.g + 0.01f;
        clearColor.g = (g > 1.0f) ? 0.0f : g;

        VGPURenderPassDescriptor renderPass;
        memset(&renderPass, 0, sizeof(VGPURenderPassDescriptor));
        renderPass.colorAttachments[0].texture = vgpuGetBackbufferTexture();
        renderPass.colorAttachments[0].mipLevel = 0;
        renderPass.colorAttachments[0].slice = 0;
        renderPass.colorAttachments[0].loadOp = VGPULoadOp_Clear;
        renderPass.colorAttachments[0].clearColor = clearColor;
        renderPass.depthStencilAttachment.texture = depthTexture;
        renderPass.depthStencilAttachment.depthLoadOp = VGPULoadOp_Clear;
        renderPass.depthStencilAttachment.clearDepth = 1.0f;

        vgpuCmdBeginRenderPass(&renderPass);
        vgpuCmdEndRenderPass();
        vgpu_end_frame();
        glfwPollEvents();
    }

    vgpuDestroyTexture(depthTexture);
    vgpu_shutdown();
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
