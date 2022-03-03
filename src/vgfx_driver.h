// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef _VGFX_DRIVER_H_
#define _VGFX_DRIVER_H_

#include "vgfx.h"
#include <stdio.h>
#include <stdarg.h>

#ifndef _VGFX_UNUSED
#define _VGFX_UNUSED(x) (void)(x)
#endif

extern void vgfxLogInfo(const char* format, ...);
extern void vgfxLogWarn(const char* format, ...);
extern void vgfxLogError(const char* format, ...);

typedef struct gfxRenderer gfxRenderer;

struct gfxDevice_T
{
    void (*destroyDevice)(gfxDevice device);

    /* Opaque pointer for the Driver */
    gfxRenderer* driverData;
};

#define ASSIGN_DRIVER_FUNC(func, name) \
	result->func = name##_##func;
#define ASSIGN_DRIVER(name) \
	ASSIGN_DRIVER_FUNC(destroyDevice, name) \

typedef struct gfxDriver
{
    VGFXAPI api;
    gfxDevice (*createDevice)(void);
} gfxDriver;

_VGFX_EXTERN gfxDriver vulkan_driver;
_VGFX_EXTERN gfxDriver d3d12_driver;

#endif /* _VGFX_DRIVER_H_ */
