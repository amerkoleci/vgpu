//
// Copyright (c) 2019 Amer Koleci.
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
    vgpu_log_format(VGPU_LOG_LEVEL_ERROR, "[GLFW3 Error] Code: %d Decription: %s", error, description);
}
#endif

static void vgpu_log_fn(void* user_data, vgpu_log_level level, const char* message)
{
}

int main(int argc, char** argv)
{
    vgpu_set_log_callback_function(vgpu_log_fn, NULL);

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
    VGPUBackendType preferredBackend = VGPUBackendType_D3D11;
    if (preferredBackend != VGPUBackendType_OpenGL)
    {
        // By default on non opengl context creation.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    else
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }

    GLFWwindow* window = glfwCreateWindow(1280, 720, "vgpu", 0, 0);
    if (preferredBackend == VGPUBackendType_OpenGL)
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
    }

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    VGpuDeviceDescriptor gpu_desc = {
        .preferredBackend = preferredBackend,
        .swapchain = &(VGPUSwapChainDescriptor) {
#if defined(_WIN32) || defined(_WIN64)
            .native_handle = (void*)glfwGetWin32Window(window),
#elif defined(__linux__)
#elif defined(__APPLE__)
#endif
            .width = width,
            .height = height,
            .fullscreen = false,
            .vsync = true
      },
    };

#ifdef _DEBUG
    gpu_desc.flags |= VGPU_CONFIG_FLAGS_VALIDATION;
#endif

    VGPUDevice device = vgpuDeviceCreate("00 - clear", &gpu_desc);

    while (!glfwWindowShouldClose(window))
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        vgpu_begin_frame(device);
        vgpu_begin_render_pass_desc begin_pass_desc;
        begin_pass_desc.render_pass = vgpu_get_default_render_pass(device);
        begin_pass_desc.color_attachments[0] = (vgpu_clear_value){ 1.0f, 0.0f, 0.0f, 0.0f };
        vgpu_cmd_begin_render_pass(&begin_pass_desc);
        /*VgpuColor clearColor = { 0.0f, 0.2f, 0.4f, 1.0f };
        vgpuCmdBeginDefaultRenderPass(commandBuffer, clearColor, 1.0f, 0);*/
        vgpu_cmd_end_render_pass();
        vgpu_end_frame(device);
        glfwPollEvents();
    }

    /*vgpuDestroyCommandBuffer(commandBuffer);*/
    vgpuDeviceDestroy(device);
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
