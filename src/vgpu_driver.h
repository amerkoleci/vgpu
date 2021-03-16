// Copyright (c) Amer Koleci.
// Distributed under the MIT license. See the LICENSE file in the project root for more information.

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

typedef struct vgpu_renderer_t {
    bool(*init)(const vgpu_info* info);
    void(*shutdown)(void);
    void(*get_caps)(VGPUDeviceCaps* caps);
    VGPUTextureFormat(*getDefaultDepthFormat)(void);
    VGPUTextureFormat(*getDefaultDepthStencilFormat)(void);

    vgpu_bool(*frame_begin)(void);
    void (*frame_end)(void);
    VGPUTexture (*getBackbufferTexture)(uint32_t swapchain);

    /* Texture */
    VGPUTexture(*createTexture)(const VGPUTextureDescription* desc);
    void (*destroyTexture)(VGPUTexture handle);

    /* Buffer */
    VGPUBuffer(*createBuffer)(const VGPUBufferDescriptor* desc);
    void (*destroyBuffer)(VGPUBuffer handle);
   
    /* Sampler */
    VGPUSampler(*createSampler)(const VGPUSamplerDescriptor* desc);
    void (*destroySampler)(VGPUSampler handle);

    /* Shader */
    VGPUShaderModule(*createShaderModule)(const VGPUShaderModuleDescriptor* desc);
    void (*destroyShaderModule)(VGPUShaderModule handle);

    /* Commands */
    void (*cmdBeginRenderPass)(const VGPURenderPassDescriptor* desc);
    void (*cmdEndRenderPass)(void);
} vgpu_renderer_t;

typedef struct VGPU_Driver {
    vgpu_backend_type type;
    bool(*is_supported)(void);
    vgpu_renderer_t*(*init_renderer)(void);
} VGPU_Driver;

_VGPU_EXTERN VGPU_Driver Vulkan_Driver;
_VGPU_EXTERN VGPU_Driver D3D12_Driver;
_VGPU_EXTERN VGPU_Driver D3D11_Driver;

#define ASSIGN_DRIVER_FUNC(func, name) renderer.func = name##_##func;
#define ASSIGN_DRIVER(name) \
ASSIGN_DRIVER_FUNC(init, name)\
ASSIGN_DRIVER_FUNC(shutdown, name)\
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
