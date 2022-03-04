// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef _VGFX_DRIVER_H_
#define _VGFX_DRIVER_H_

#include "vgfx.h"
#include <stdio.h>
#include <stdarg.h>

#ifndef VGFX_MALLOC
#   include <stdlib.h>
#   define VGFX_MALLOC(s) malloc(s)
#   define VGFX_FREE(p) free(p)
#endif

#ifndef VGFX_ASSERT
#   include <assert.h>
#   define VGFX_ASSERT(c) assert(c)
#endif

#ifndef _VGFX_UNUSED
#define _VGFX_UNUSED(x) (void)(x)
#endif

#if defined(__clang__)
// CLANG ENABLE/DISABLE WARNING DEFINITION
#define VGFX_DISABLE_WARNINGS() \
    _Pragma("clang diagnostic push")\
	_Pragma("clang diagnostic ignored \"-Wall\"") \
	_Pragma("clang diagnostic ignored \"-Wextra\"") \
	_Pragma("clang diagnostic ignored \"-Wtautological-compare\"")

#define VGFX_ENABLE_WARNINGS() _Pragma("clang diagnostic pop")
#elif defined(__GNUC__) || defined(__GNUG__)
// GCC ENABLE/DISABLE WARNING DEFINITION
#	define VGFX_DISABLE_WARNINGS() \
	_Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wall\"") \
	_Pragma("clang diagnostic ignored \"-Wextra\"") \
	_Pragma("clang diagnostic ignored \"-Wtautological-compare\"")

#define VGFX_ENABLE_WARNINGS() _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define VGFX_DISABLE_WARNINGS() __pragma(warning(push, 0))
#define VGFX_ENABLE_WARNINGS() __pragma(warning(pop))
#endif

_VGFX_EXTERN void vgfxLogInfo(const char* format, ...);
_VGFX_EXTERN void vgfxLogWarn(const char* format, ...);
_VGFX_EXTERN void vgfxLogError(const char* format, ...);

typedef struct gfxRenderer gfxRenderer;

struct gfxDevice_T
{
    void (*destroyDevice)(gfxDevice device);
    void (*frame)(gfxRenderer* driverData);

    /* Opaque pointer for the Driver */
    gfxRenderer* driverData;
};

#define ASSIGN_DRIVER_FUNC(func, name) \
	device->func = name##_##func;
#define ASSIGN_DRIVER(name) \
	ASSIGN_DRIVER_FUNC(destroyDevice, name) \
    ASSIGN_DRIVER_FUNC(frame, name) \

typedef struct gfxDriver
{
    VGFXAPI api;
    gfxDevice (*createDevice)(const VGFXDeviceInfo* info);
} gfxDriver;

_VGFX_EXTERN gfxDriver vulkan_driver;
_VGFX_EXTERN gfxDriver d3d12_driver;
_VGFX_EXTERN gfxDriver d3d11_driver;
_VGFX_EXTERN gfxDriver webgpu_driver;

#endif /* _VGFX_DRIVER_H_ */
