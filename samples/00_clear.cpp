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
#include <cstdio>
#include <cstdlib>
#include <sstream>

void platform_log(const char* str);
#define LOG(STR)  { std::stringstream ss; ss << STR << std::endl; \
                    platform_log(ss.str().c_str()); }
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
static void glfwErrorCallback(int error, const char *description)
{
    LOG("[GLFW3 Error] Code: " << error << " Decription: {}" << description);
}
#endif

static void platform_log(const char* str)
{
#if defined(_WIN32)
    OutputDebugStringA(str);
#else
    printf("%s", str);
#endif
}

static void vgpuLogCallback(VgpuLogLevel level, const char* context, const char* message)
{
}

int main(int argc, char** argv)
{
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

    if (vgpuGetBackend() != VGPU_BACKEND_OPENGL)
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

    GLFWwindow* window = glfwCreateWindow(640, 480, "vgpu", 0, 0);
    if (vgpuGetBackend() == VGPU_BACKEND_OPENGL)
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
    }

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    VgpuDescriptor descriptor = {};
#ifdef _DEBUG
    descriptor.validation = true;
#endif
    VgpuSwapchainDescriptor swapchainDescriptor = {};
    swapchainDescriptor.width = width;
    swapchainDescriptor.height = height;
    swapchainDescriptor.depthStencil = true;
    swapchainDescriptor.multisampling = false;
    swapchainDescriptor.tripleBuffer = false;
    swapchainDescriptor.vsync = true;
#if defined(_WIN32) || defined(_WIN64)
    swapchainDescriptor.nativeHandle = (uint64_t)glfwGetWin32Window(window);
#elif defined(__linux__)
#elif defined(__APPLE__)
#endif

    descriptor.swapchain = &swapchainDescriptor;
    descriptor.logCallback = vgpuLogCallback;

    if (vgpuInitialize("alimer", &descriptor) != VGPU_SUCCESS)
    {
        LOG("Error: Failed to initialize vgpu");
        return false;
    }

    while (!glfwWindowShouldClose(window))
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        glfwGetFramebufferSize(window, &width, &height);
        // Request command buffer.
        VgpuCommandBuffer commandBuffer = vgpuRequestCommandBuffer();
        vgpuFrame();
        glfwPollEvents();
    }

    vgpuShutdown();
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
