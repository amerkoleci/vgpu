// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGPU_OPENGL_DRIVER)

#include "vgpu_driver.h"

#if defined(__EMSCRIPTEN__)
#   include <emscripten.h>
#   include <GLES3/gl3.h>
#   include <GLES2/gl2ext.h>
#   include <GL/gl.h>
#   include <GL/glext.h>
#else
//#   include <glad/glad.h>
#endif

struct GL_Renderer
{
};

static bool gl_isSupported(void)
{
    return true;
}

static VGPUDevice gl_createDevice(const VGPUDeviceDesc* desc)
{
    VGPU_ASSERT(desc);

    GL_Renderer* renderer = new GL_Renderer();

    vgpuLogInfo("VGPU driver: OpenGL");

    VGPUDevice_T* device = (VGPUDevice_T*)VGPU_MALLOC(sizeof(VGPUDevice_T));
    //ASSIGN_DRIVER(gl);

    device->driverData = (VGFXRenderer*)renderer;
    return device;
}

VGFXDriver GL_Driver = {
    VGPUBackendType_OpenGL,
    gl_isSupported,
    gl_createDevice
};

#endif /* VGPU_OPENGL_DRIVER */
