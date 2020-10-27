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

#ifndef VGPU_SHADER_H_
#define VGPU_SHADER_H_

#ifdef __cplusplus
#   define _VGPU_SHADER_EXTERN extern "C"
#else
#   define _VGPU_SHADER_EXTERN extern
#endif

#if defined(VGPU_SHARED_LIBRARY)
#   if defined(_WIN32)
#       if defined(VGPU_SHADER_IMPLEMENTATION)
#           define _VGPU_SHADER_API_DECL __declspec(dllexport)
#       else
#           define _VGPU_SHADER_API_DECL __declspec(dllimport)
#       endif
#   else  // defined(_WIN32)
#       if defined(VGPU_SHADER_IMPLEMENTATION)
#           define _VGPU_SHADER_API_DECL __attribute__((visibility("default")))
#       else
#           define _VGPU_SHADER_API_DECL
#       endif
#   endif  // defined(_WIN32)
#else       // defined(VGPU_SHARED_LIBRARY)
#   define _VGPU_SHADER_API_DECL
#endif  // defined(VGPU_SHARED_LIBRARY)

#define VGPU_SHADER_API _VGPU_SHADER_EXTERN _VGPU_SHADER_API_DECL

#include <stdint.h>

typedef enum vgpu_shader_stage {
    VGPU_SHADER_STAGE_VERTEX,
    VGPU_SHADER_STAGE_TESS_CONTROL,
    VGPU_SHADER_STAGE_TESS_EVAL,
    VGPU_SHADER_STAGE_GEOMETRY,
    VGPU_SHADER_STAGE_FRAGMENT,
    VGPU_SHADER_STAGE_COMPUTE,
    VGPU_SHADER_STAGE_MAX
} vgpu_shader_stage;

typedef struct vgpu_shader_blob_t
{
    uint64_t size;
    uint8_t* data;
} vgpu_shader_blob_t;

VGPU_SHADER_API vgpu_shader_blob_t vgpu_compile_shader(const char* source, const char* entry_point, uint32_t source_language, vgpu_shader_stage stage);

#endif // VGPU_SHADER_H_
