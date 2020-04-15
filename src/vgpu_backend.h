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

#include "vgpu/vgpu.h"
#include <string.h> /* memset */

#if defined(_MSC_VER) || defined(__MINGW32__)
#   include <malloc.h>
#else
#   include <alloca.h>
#endif

#ifndef __cplusplus
#  define nullptr ((void*)0)
#endif

#ifndef VGPU_ASSERT
#   include <assert.h>
#   define VGPU_ASSERT(c) assert(c)
#endif

#ifndef VGPU_ALLOCA
#   if defined(_MSC_VER) || defined(__MINGW32__)
#       define VGPU_ALLOCA(type, count) ((type*)(_malloca(sizeof(type) * (count))))
#   else
#       define VGPU_ALLOCA(type, count) ((type*)(alloca(sizeof(type) * (count))))
#   endif
#endif

#ifndef VGPU_MALLOC
#   include <stdlib.h>
#   define VGPU_MALLOC(s) malloc(s)
#   define VGPU_FREE(p) free(p)
#endif

#define VGPU_CHECK(c, s) if (!(c)) { vgpu_log_error(s); _VGPU_BREAKPOINT(); }

#if defined(__GNUC__) || defined(__clang__)
#   if defined(__i386__) || defined(__x86_64__)
#       define _VGPU_BREAKPOINT() __asm__ __volatile__("int $3\n\t")
#   else
#       define _VGPU_BREAKPOINT() ((void)0)
#   endif
#   define _VGPU_UNREACHABLE() __builtin_unreachable()

#elif defined(_MSC_VER)
extern void __cdecl __debugbreak(void);
#   define _VGPU_BREAKPOINT() __debugbreak()
#   define _VGPU_UNREACHABLE() __assume(false)
#else
#   error "Unsupported compiler"
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

#define _VGPU_UNUSED(x) do { (void)sizeof(x); } while(0)
#define _VGPU_ALLOC_HANDLE(type) ((type*)(calloc(1, sizeof(type))))

#define _vgpu_min(a,b) ((a<b)?a:b)
#define _vgpu_max(a,b) ((a>b)?a:b)
#define _vgpu_clamp(v,v0,v1) ((v<v0)?(v0):((v>v1)?(v1):(v)))

typedef struct VGPUDeviceImpl* VGPUDevice;
typedef struct VGPURenderer VGPURenderer;

typedef struct VGPUDeviceImpl {
    /* Opaque pointer for the renderer. */
    VGPURenderer* renderer;

    bool (*init)(VGPUDevice device, const char* app_name, const VGpuDeviceDescriptor* descriptor);
    void (*destroy)(VGPUDevice device);
    VGPUBackendType(*getBackend)(void);
    vgpu_features(*get_features)(VGPURenderer* driver_data);
    vgpu_limits(*get_limits)(VGPURenderer* driver_data);
    VGPURenderPass (*get_default_render_pass)(VGPURenderer* driver_data);

    void (*begin_frame)(VGPURenderer* driver_data);
    void (*end_frame)(VGPURenderer* driver_data);

    /* Texture */
    VGPUTexture (*textureCreate)(VGPURenderer* driver_data, const VGPUTextureDescriptor* descriptor, const void* initial_data);
    VGPUTexture (*textureCreateExternal)(VGPURenderer* driver_data, const VGPUTextureDescriptor* descriptor, void* handle);
    void (*textureDestroy)(VGPURenderer* driver_data, VGPUTexture handle);

    /* Sampler */
    VGPUSampler (*samplerCreate)(VGPURenderer* driver_data, const VGPUSamplerDescriptor* descriptor);
    void (*samplerDestroy)(VGPURenderer* driver_data, VGPUSampler handle);

    /* RenderPass */
    VGPURenderPass (*renderPassCreate)(VGPURenderer* driverData, const vgpu_render_pass_desc* desc);
    void (*renderPassDestroy)(VGPURenderer* driverData, VGPURenderPass handle);
    void (*renderPassGetExtent)(VGPURenderer* driverData, VGPURenderPass handle, uint32_t* width, uint32_t* height);

    /* Commands */
    void (*cmd_begin_render_pass)(VGPURenderer* driver_data, const vgpu_begin_render_pass_desc* begin_pass_desc);
    void (*cmd_end_render_pass)(VGPURenderer* driver_data);
} VGPUDeviceImpl;

/* d3d11 */
extern bool vgpu_d3d11_supported(void);
extern VGPUDevice d3d11_create_device(void);

/* vulkan */
extern bool vgpu_vk_supported(void);
extern VGPUDevice vk_create_device(void);

#define ASSIGN_DRIVER_FUNC(func, name) device->func = name##_##func;
#define ASSIGN_DRIVER(name) \
ASSIGN_DRIVER_FUNC(init, name)\
ASSIGN_DRIVER_FUNC(destroy, name)\
ASSIGN_DRIVER_FUNC(getBackend, name)\
ASSIGN_DRIVER_FUNC(get_features, name)\
ASSIGN_DRIVER_FUNC(get_limits, name)\
ASSIGN_DRIVER_FUNC(get_default_render_pass, name)\
ASSIGN_DRIVER_FUNC(begin_frame, name)\
ASSIGN_DRIVER_FUNC(end_frame, name)\
ASSIGN_DRIVER_FUNC(textureCreate, name)\
ASSIGN_DRIVER_FUNC(textureCreateExternal, name)\
ASSIGN_DRIVER_FUNC(textureDestroy, name)\
ASSIGN_DRIVER_FUNC(samplerCreate, name)\
ASSIGN_DRIVER_FUNC(samplerDestroy, name)\
ASSIGN_DRIVER_FUNC(renderPassCreate, name)\
ASSIGN_DRIVER_FUNC(renderPassDestroy, name)\
ASSIGN_DRIVER_FUNC(renderPassGetExtent, name)\
ASSIGN_DRIVER_FUNC(cmd_begin_render_pass, name)\
ASSIGN_DRIVER_FUNC(cmd_end_render_pass, name)
