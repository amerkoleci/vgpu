// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#ifndef _VGFX_H
#define _VGFX_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define REFRESHAPI __declspec(dllexport)
#define REFRESHCALL __cdecl
#else
#define REFRESHAPI
#define REFRESHCALL
#endif

#if defined(VGFX_SHARED_LIBRARY_BUILD)
#   if defined(_MSC_VER)
#       define VGFX_API __declspec(dllexport)
#   elif defined(__GNUC__)
#       define VGFX_API __attribute__((visibility("default")))
#   else
#       define VGFX_API
#       pragma warning "Unknown dynamic link import/export semantics."
#   endif
#elif defined(VGFX_SHARED_LIBRARY_INCLUDE)
#   if defined(VGFX_API)
#       define VGFX_API __declspec(dllimport)
#   else
#       define VGFX_API
#   endif
#else
#   define VGFX_API
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    typedef struct gfx_device_t* gfx_device;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _VGFX_H */
