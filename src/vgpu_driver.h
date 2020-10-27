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

#ifndef VGPU_DRIVER_H
#define VGPU_DRIVER_H

#include "vgpu/vgpu.h"
#include <stdbool.h>
#include <string.h> /* memset */

_VGPU_EXTERN const vgpu_allocation_callbacks* vgpu_alloc_cb;
_VGPU_EXTERN void* vgpu_allocation_user_data;

#define VGPU_ALLOC(T)     ((T*) vgpu_alloc_cb->allocate_memory(vgpu_allocation_user_data, sizeof(T)))
#define VGPU_FREE(ptr)       (vgpu_alloc_cb->free_memory(vgpu_allocation_user_data, (void*)(ptr)))
#define VGPU_ALLOC_HANDLE(T) ((T*) vgpu_alloc_cb->allocate_cleared_memory(vgpu_allocation_user_data, sizeof(T)))

#ifndef VGPU_ALLOCA
#   include <malloc.h>
#   if defined(_MSC_VER) || defined(__MINGW32__)
#       define VGPU_ALLOCA(type, count) ((type*)(_malloca(sizeof(type) * (count))))
#   else
#       define VGPU_ALLOCA(type, count) ((type*)(alloca(sizeof(type) * (count))))
#   endif
#endif

#ifndef VGPU_ASSERT
#   include <assert.h>
#   define VGPU_ASSERT(c) assert(c)
#endif

#define VGPU_CHECK(c, s) if (!(c)) { vgpuLogError(s); _VGPU_BREAKPOINT(); }

#if defined(__GNUC__) || defined(__clang__)
#   if defined(__i386__) || defined(__x86_64__)
#       define VGPU_BREAKPOINT() __asm__ __volatile__("int $3\n\t")
#   else
#       define VGPU_BREAKPOINT() ((void)0)
#   endif
#   define VGPU_UNREACHABLE() __builtin_unreachable()

#elif defined(_MSC_VER)
extern void __cdecl __debugbreak(void);
#   define VGPU_BREAKPOINT() __debugbreak()
#   define VGPU_UNREACHABLE() __assume(false)
#else
#   error "Unsupported compiler"
#endif

#define VGPU_UNUSED(x) do { (void)sizeof(x); } while(0)

#define _vgpu_def(val, def) (((val) == 0) ? (def) : (val))
#define _vgpu_def_flt(val, def) (((val) == 0.0f) ? (def) : (val))
#define _vgpu_min(a,b) ((a<b)?a:b)
#define _vgpu_max(a,b) ((a>b)?a:b)
#define _vgpu_clamp(v,v0,v1) ((v<v0)?(v0):((v>v1)?(v1):(v)))
#define _vgpu_count_of(x) (sizeof(x) / sizeof(x[0]))

typedef struct vgpu_renderer_t* vgpu_renderer;

typedef struct VGPUDeviceImpl {
    void(*destroy)(VGPUDevice device);
    void(*get_caps)(vgpu_renderer driver_data, VGPUDeviceCaps* caps);
    VGPUTextureFormat(*getDefaultDepthFormat)(vgpu_renderer driverData);
    VGPUTextureFormat(*getDefaultDepthStencilFormat)(vgpu_renderer driverData);

    vgpu_bool(*frame_begin)(vgpu_renderer driverData);
    void (*frame_end)(vgpu_renderer driverData);
    VGPUTexture (*getBackbufferTexture)(vgpu_renderer driverData, uint32_t swapchain);

    /* Texture */
    VGPUTexture(*createTexture)(vgpu_renderer driverData, const VGPUTextureDescription* desc);
    void (*destroyTexture)(vgpu_renderer driverData, VGPUTexture handle);

    /* Buffer */
    VGPUBuffer(*createBuffer)(vgpu_renderer driverData, const VGPUBufferDescriptor* desc);
    void (*destroyBuffer)(vgpu_renderer driverData, VGPUBuffer handle);
   
    /* Sampler */
    VGPUSampler(*createSampler)(vgpu_renderer driverData, const VGPUSamplerDescriptor* desc);
    void (*destroySampler)(vgpu_renderer driverData, VGPUSampler handle);

    /* Shader */
    VGPUShaderModule(*createShaderModule)(vgpu_renderer driverData, const VGPUShaderModuleDescriptor* desc);
    void (*destroyShaderModule)(vgpu_renderer driverData, VGPUShaderModule handle);

    /* Commands */
    void (*cmdBeginRenderPass)(vgpu_renderer driverData, const VGPURenderPassDescriptor* desc);
    void (*cmdEndRenderPass)(vgpu_renderer driverData);

    /* Opaque pointer for the renderer. */
    vgpu_renderer renderer;
} VGPUDeviceImpl;

typedef struct VGPU_Driver {
    VGPUBackendType backendType;
    bool(*is_supported)(void);
    VGPUDeviceImpl*(*createDevice)(const VGPUDeviceDescriptor* descriptor);
} VGPU_Driver;

_VGPU_EXTERN VGPU_Driver D3D12_Driver;
_VGPU_EXTERN VGPU_Driver D3D11_Driver;

#define ASSIGN_DRIVER_FUNC(func, name) device->func = name##_##func;
#define ASSIGN_DRIVER(name) \
ASSIGN_DRIVER_FUNC(destroy, name)\
ASSIGN_DRIVER_FUNC(get_caps, name)\
ASSIGN_DRIVER_FUNC(getDefaultDepthFormat, name)\
ASSIGN_DRIVER_FUNC(getDefaultDepthStencilFormat, name)\
ASSIGN_DRIVER_FUNC(frame_begin, name)\
ASSIGN_DRIVER_FUNC(frame_end, name)\
ASSIGN_DRIVER_FUNC(getBackbufferTexture, name)\
ASSIGN_DRIVER_FUNC(createTexture, name)\
ASSIGN_DRIVER_FUNC(destroyTexture, name)\
ASSIGN_DRIVER_FUNC(createBuffer, name)\
ASSIGN_DRIVER_FUNC(destroyBuffer, name)\
ASSIGN_DRIVER_FUNC(createSampler, name)\
ASSIGN_DRIVER_FUNC(destroySampler, name)\
ASSIGN_DRIVER_FUNC(createShaderModule, name)\
ASSIGN_DRIVER_FUNC(destroyShaderModule, name)\
ASSIGN_DRIVER_FUNC(cmdBeginRenderPass, name)\
ASSIGN_DRIVER_FUNC(cmdEndRenderPass, name)


#endif /* VGPU_DRIVER_H */
