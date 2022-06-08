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
#   include <glad/glad.h>
#endif

#if defined(__EMSCRIPTEN__) || defined(__APPLE__)
#   define GL_NO_DEBUG 1
#else
#   define GL_NO_DEBUG 0
#endif

//#define _VGPU_GLES

namespace
{
#if !GL_NO_DEBUG
    void APIENTRY gl_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
    {
        // these are basically never useful
        if (severity == GL_DEBUG_SEVERITY_NOTIFICATION &&
            type == GL_DEBUG_TYPE_OTHER)
        {
            return;
        }

        const char* typeName = "";
        const char* severityName = "";

        switch (type)
        {
            case GL_DEBUG_TYPE_ERROR: typeName = "Error"; break;
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeName = "Deprecated behavior"; break;
            case GL_DEBUG_TYPE_MARKER: typeName = "Marker"; break;
            case GL_DEBUG_TYPE_OTHER: typeName = "Other"; break;
            case GL_DEBUG_TYPE_PERFORMANCE: typeName = "Performance"; break;
            case GL_DEBUG_TYPE_PORTABILITY: typeName = "Portability"; break;
            case GL_DEBUG_TYPE_PUSH_GROUP: typeName = "Push Group"; break;
            case GL_DEBUG_TYPE_POP_GROUP: typeName = "Pop Group"; break;
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: typeName = "Undefined behavior"; break;
        }

        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH: severityName = "HIGH"; break;
            case GL_DEBUG_SEVERITY_MEDIUM: severityName = "MEDIUM"; break;
            case GL_DEBUG_SEVERITY_LOW: severityName = "LOW"; break;
            case GL_DEBUG_SEVERITY_NOTIFICATION: severityName = "NOTIFICATION"; break;
        }

        if (type == GL_DEBUG_TYPE_ERROR)
        {
            if (severity == GL_DEBUG_SEVERITY_HIGH)
            {
                vgpuLogError("GL (%s:%s) %s", typeName, severityName, message);
            }
            else
            {
                vgpuLogError("GL (%s:%s) %s", typeName, severityName, message);
            }
        }
        else if (severity != GL_DEBUG_SEVERITY_NOTIFICATION)
        {
            vgpuLogWarn("GL (%s:%s) %s", typeName, severityName, message);
        }
        else
        {
            vgpuLogInfo("GL (%s:%s) %s", typeName, message);
        }
    }
#endif
}

struct GL_Renderer
{
    uint32_t frameIndex = 0;
    uint64_t frameCount = 0;
};

static void gl_destroyDevice(VGPUDevice device)
{
    GL_Renderer* renderer = (GL_Renderer*)device->driverData;
    delete renderer;
    VGPU_FREE(device);
}

static uint64_t gl_frame(VGFXRenderer* driverData)
{
    GL_Renderer* renderer = (GL_Renderer*)driverData;

    renderer->frameCount++;
    renderer->frameIndex = renderer->frameCount % VGPU_MAX_INFLIGHT_FRAMES;

    // Return current frame
    return renderer->frameCount - 1;
}

static void gl_waitIdle(VGFXRenderer* driverData)
{
    _VGPU_UNUSED(driverData);
    glFlush();
}

static VGPUBackendType gl_getBackendType(void)
{
    return VGPUBackendType_OpenGL;
}

static bool gl_hasFeature(VGFXRenderer* driverData, VGPUFeature feature)
{
    switch (feature)
    {
        case VGPU_FEATURE_COMPUTE:
        case VGPU_FEATURE_INDEPENDENT_BLEND:
        case VGPU_FEATURE_TEXTURE_CUBE_ARRAY:
        case VGPU_FEATURE_TEXTURE_COMPRESSION_BC:
            return true;

        case VGPU_FEATURE_TEXTURE_COMPRESSION_ETC2:
        case VGPU_FEATURE_TEXTURE_COMPRESSION_ASTC:
            return false;

        default:
            return false;
    }
}

static void gl_getAdapterProperties(VGFXRenderer* driverData, VGPUAdapterProperties* properties)
{
    //properties->vendorID = renderer->vendorID;
    //properties->deviceID = renderer->deviceID;
    //properties->name = renderer->adapterName.c_str();
    //properties->driverDescription = renderer->driverDescription.c_str();
    //properties->adapterType = renderer->adapterType;
}

static void gl_getLimits(VGFXRenderer* driverData, VGPULimits* limits)
{
    
}

/* Buffer */
static VGPUBuffer gl_createBuffer(VGFXRenderer* driverData, const VGPUBufferDesc* desc, const void* pInitialData)
{
    return nullptr;
}

static void gl_destroyBuffer(VGFXRenderer* driverData, VGPUBuffer resource)
{
}

/* Texture */
static VGPUTexture gl_createTexture(VGFXRenderer* driverData, const VGPUTextureDesc* desc)
{
    return nullptr;
}

static void gl_destroyTexture(VGFXRenderer*, VGPUTexture texture)
{
}

/* Sampler */
static VGPUSampler gl_createSampler(VGFXRenderer* driverData, const VGPUSamplerDesc* desc)
{
    return nullptr;
}

static void gl_destroySampler(VGFXRenderer* driverData, VGPUSampler resource)
{
}

/* ShaderModule */
static VGPUShaderModule gl_createShaderModule(VGFXRenderer* driverData, const void* pCode, size_t codeSize)
{
    return nullptr;
}

static void gl_destroyShaderModule(VGFXRenderer* driverData, VGPUShaderModule resource)
{
}

/* Pipeline */
static VGPUPipeline gl_createRenderPipeline(VGFXRenderer* driverData, const VGPURenderPipelineDesc* desc)
{
    return nullptr;
}

static VGPUPipeline gl_createComputePipeline(VGFXRenderer* driverData, const VGPUComputePipelineDesc* desc)
{
    return nullptr;
}

static VGPUPipeline gl_createRayTracingPipeline(VGFXRenderer* driverData, const VGPURayTracingPipelineDesc* desc)
{
    return nullptr;
}

static void gl_destroyPipeline(VGFXRenderer* driverData, VGPUPipeline resource)
{

}

static VGPUSwapChain gl_createSwapChain(VGFXRenderer* driverData, void* windowHandle, const VGPUSwapChainDesc* desc)
{
    return nullptr;
}

static void gl_destroySwapChain(VGFXRenderer* driverData, VGPUSwapChain swapChain)
{
}

static VGPUTextureFormat gl_getSwapChainFormat(VGFXRenderer*, VGPUSwapChain swapChain)
{
    return VGFXTextureFormat_BGRA8UNorm;
}

static VGPUCommandBuffer gl_beginCommandBuffer(VGFXRenderer* driverData, const char* label)
{
    return nullptr;
}

static void gl_submit(VGFXRenderer* driverData, VGPUCommandBuffer* commandBuffers, uint32_t count)
{
}

static bool gl_isSupported(void)
{
    return true;
}

static VGPUDevice gl_createDevice(const VGPUDeviceDesc* desc)
{
    VGPU_ASSERT(desc);

#if !defined(__EMSCRIPTEN__)
#if !defined(_VGPU_GLES)
    if (!gladLoadGLLoader((GLADloadproc)desc->gl.getProcAddress))
    {
        vgpuLogError("Failed to initialize GLAD");
        return nullptr;
    }
#else
    if (!gladLoadGLES2Loader((GLADloadproc)desc->gl.getProcAddress))
    {
        vgpuLogError("Failed to initialize GLAD");
        return nullptr;
    }
#endif
#endif

    GL_Renderer* renderer = new GL_Renderer();

    vgpuLogInfo("VGPU driver: OpenGL");

    VGPUDevice_T* device = (VGPUDevice_T*)VGPU_MALLOC(sizeof(VGPUDevice_T));
    ASSIGN_DRIVER(gl);

    device->driverData = (VGFXRenderer*)renderer;
    return device;
}

VGFXDriver GL_Driver = {
    VGPUBackendType_OpenGL,
    gl_isSupported,
    gl_createDevice
};

#endif /* VGPU_OPENGL_DRIVER */
