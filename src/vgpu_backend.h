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

#define _VGPU_UNUSED(x) do { (void)sizeof(x); } while(0)
#define _VGPU_ALLOC_HANDLE(type) ((type*)(calloc(1, sizeof(type))))

#define _vgpu_min(a,b) ((a<b)?a:b)
#define _vgpu_max(a,b) ((a>b)?a:b)

typedef struct vgpu_renderer vgpu_renderer;

typedef struct vgpu_device {
    /* Opaque pointer for the renderer. */
    vgpu_renderer* renderer;

    bool (*init)(struct vgpu_device* device, const char* app_name, const vgpu_desc* desc);
    void (*destroy)(struct vgpu_device* device);
    vgpu_backend(*get_backend)();
    vgpu_features(*get_features)(vgpu_renderer* driver_data);
    vgpu_limits(*get_limits)(vgpu_renderer* driver_data);
    vgpu_render_pass* (*get_default_render_pass)(vgpu_renderer* driver_data);

    void (*begin_frame)(vgpu_renderer* driver_data);
    void (*end_frame)(vgpu_renderer* driver_data);

    /* Texture */
    vgpu_texture* (*texture_create)(vgpu_renderer* driver_data, const vgpu_texture_desc* desc, const void* initial_data);
    vgpu_texture* (*texture_create_external)(vgpu_renderer* driver_data, const vgpu_texture_desc* desc, void* handle);
    void (*texture_destroy)(vgpu_renderer* driver_data, vgpu_texture* texture);

    /* Sampler */
    vgpu_sampler* (*sampler_create)(vgpu_renderer* driver_data, const vgpu_sampler_desc* desc);
    void (*sampler_destroy)(vgpu_renderer* driver_data, vgpu_sampler* sampler);

    /* RenderPass */
    vgpu_render_pass* (*render_pass_create)(vgpu_renderer* driver_data, const vgpu_render_pass_desc* desc);
    void (*render_pass_destroy)(vgpu_renderer* driver_data, vgpu_render_pass* render_pass);
    void (*render_pass_get_extent)(vgpu_renderer* driver_data, vgpu_render_pass* render_pass, uint32_t* width, uint32_t* height);

    /* Commands */
    void (*cmd_begin_render_pass)(vgpu_renderer* driver_data, const vgpu_begin_render_pass_desc* begin_pass_desc);
    void (*cmd_end_render_pass)(vgpu_renderer* driver_data);
} vgpu_device;

/* d3d11 */
extern bool vgpu_d3d11_supported();
extern vgpu_device* d3d11_create_device();

#define ASSIGN_DRIVER_FUNC(func, name) \
	device->func = name##_##func;
#define ASSIGN_DRIVER(name) \
ASSIGN_DRIVER_FUNC(init, name)\
ASSIGN_DRIVER_FUNC(destroy, name)\
ASSIGN_DRIVER_FUNC(get_backend, name)\
ASSIGN_DRIVER_FUNC(get_features, name)\
ASSIGN_DRIVER_FUNC(get_limits, name)\
ASSIGN_DRIVER_FUNC(get_default_render_pass, name)\
ASSIGN_DRIVER_FUNC(begin_frame, name)\
ASSIGN_DRIVER_FUNC(end_frame, name)\
ASSIGN_DRIVER_FUNC(texture_create, name)\
ASSIGN_DRIVER_FUNC(texture_create_external, name)\
ASSIGN_DRIVER_FUNC(texture_destroy, name)\
ASSIGN_DRIVER_FUNC(sampler_create, name)\
ASSIGN_DRIVER_FUNC(sampler_destroy, name)\
ASSIGN_DRIVER_FUNC(render_pass_create, name)\
ASSIGN_DRIVER_FUNC(render_pass_destroy, name)\
ASSIGN_DRIVER_FUNC(render_pass_get_extent, name)\
ASSIGN_DRIVER_FUNC(cmd_begin_render_pass, name)\
ASSIGN_DRIVER_FUNC(cmd_end_render_pass, name)
