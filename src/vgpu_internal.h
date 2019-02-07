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

#pragma once

#ifndef VGPU_IMPLEMENTATION
#error "Cannot include vgpu_internal.h without implementation"
#endif

#include "vgpu/vgpu.h"
#include <assert.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#   include <malloc.h>
#   undef    alloca
#   define   alloca _malloca
#   define   freea  _freea
#else
#   include <alloca.h>
#endif

#ifndef VGPU_D3D12
#   if defined(_WIN32) || defined(_WIN64)
#       define VGPU_D3D12 1
#   else
#       define VGPU_D3D12 0
#   endif
#endif // VGPU_D3D12

#ifndef VGPU_VULKAN
#   if defined(_WIN32) || defined(_WIN64) || defined(__ANDROID__) || defined(__linux__)
#       define VGPU_VULKAN 1
#   else
#       define VGPU_VULKAN 0
#   endif
#endif // VGPU_VULKAN

#ifndef VGPU_ASSERT
#   define VGPU_ASSERT(c) assert(c)
#endif



#ifndef VGPU_ALLOC
#   define VGPU_ALLOC(type) ((type*) malloc(sizeof(type)))
#   define VGPU_ALLOCN(type, n) ((type*) malloc(sizeof(type) * n))
#   define VGPU_FREE(ptr) free(ptr)
#   define VGPU_ALLOC_HANDLE(type) ((type) calloc(1, sizeof(type##_T)))
#endif


#if defined( __clang__ )
#   define VGPU_UNREACHABLE() __builtin_unreachable()
#   define VGPU_THREADLOCAL _Thread_local
#elif defined(__GNUC__)
#   define VGPU_UNREACHABLE() __builtin_unreachable()
#   define VGPU_THREADLOCAL __thread
#elif defined(_MSC_VER)
#   define VGPU_UNREACHABLE() __assume(false)
#   define VGPU_THREADLOCAL __declspec(thread)
#else
#   define VGPU_UNREACHABLE()((void)0)
#   define VGPU_THREADLOCAL
#endif

#define _vgpu_min(a,b) ((a<b)?a:b)
#define _vgpu_max(a,b) ((a>b)?a:b)
#define _vgpu_clamp(v,v0,v1) ((v<v0)?(v0):((v>v1)?(v1):(v)))

struct VgpuRendererI
{
    virtual ~VgpuRendererI() = 0;
    virtual VgpuResult initialize(const char* applicationName, const VgpuDescriptor* descriptor) = 0;
    virtual void shutdown() = 0;

    virtual VgpuResult beginFrame() = 0;
    virtual VgpuResult endFrame() = 0;
    virtual VgpuResult waitIdle() = 0;

    /* Sampler */
    virtual VgpuSampler createSampler(const VgpuSamplerDescriptor* descriptor) = 0;
    virtual void destroySampler(VgpuSampler sampler) = 0;

    /* CommandBuffer */
    virtual VgpuCommandBuffer createCommandBuffer(const VgpuCommandBufferDescriptor* descriptor) = 0;
    virtual void cmdBeginDefaultRenderPass(VgpuCommandBuffer commandBuffer, VgpuColor clearColor, float clearDepth, uint8_t clearStencil) = 0;
    virtual void cmdBeginRenderPass(VgpuCommandBuffer commandBuffer, VgpuFramebuffer framebuffer) = 0;
    virtual void cmdEndRenderPass(VgpuCommandBuffer commandBuffer) = 0;
};


inline VgpuRendererI::~VgpuRendererI()
{
}

#if VGPU_D3D12
VgpuBool32 vgpuIsD3D12Supported(bool headless);
VgpuRendererI* vgpuCreateD3D12Backend();
#endif // VGPU_D3D12


#if VGPU_VULKAN
VgpuBool32 vgpuIsVkSupported(bool headless);
VgpuRendererI* vgpuCreateVkBackend();
#endif // VGPU_VULKAN 
