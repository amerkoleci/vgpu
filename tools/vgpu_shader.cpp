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

#include "vgpu_shader.h"
#include <assert.h>

#if defined(_WIN32)
#   include <d3dcompiler.h>
#endif

#if defined(_WIN32)
    static HINSTANCE d3dcompiler_dll = nullptr;
    static bool d3dcompiler_dll_load_failed = false;
    static pD3DCompile D3DCompile_func;

    inline bool _vgpu_shader_load_d3dcompiler_dll(void)
    {
        /* load DLL on demand */
        if (d3dcompiler_dll == nullptr && !d3dcompiler_dll_load_failed)
        {
            d3dcompiler_dll = LoadLibraryW(L"d3dcompiler_47.dll");
            if (d3dcompiler_dll == nullptr)
            {
                /* don't attempt to load missing DLL in the future */
                d3dcompiler_dll_load_failed = true;
                return false;
            }

            /* look up function pointers */
            D3DCompile_func = (pD3DCompile)GetProcAddress(d3dcompiler_dll, "D3DCompile");
            assert(D3DCompile_func != nullptr);
        }

        return d3dcompiler_dll != nullptr;
    }
#endif

vgpu_shader_blob_t vgpu_compile_shader(const char* source, const char* entry_point, uint32_t source_language, vgpu_shader_stage stage)
{
#if defined(_WIN32)
    if (!_vgpu_shader_load_d3dcompiler_dll())
    {
        return {};
    }

    ID3DBlob* output = nullptr;
    ID3DBlob* errors_or_warnings = nullptr;

    const char* target = "vs_5_0";
    switch (stage)
    {
    case VGPU_SHADER_STAGE_VERTEX:
        target = "vs_5_0";
        break;
    case VGPU_SHADER_STAGE_TESS_CONTROL:
        target = "hs_5_0";
        break;
    case VGPU_SHADER_STAGE_TESS_EVAL:
        target = "ds_5_0";
        break;
    case VGPU_SHADER_STAGE_GEOMETRY:
        target = "gs_5_0";
        break;
    case VGPU_SHADER_STAGE_FRAGMENT:
        target = "ps_5_0";
        break;
    case VGPU_SHADER_STAGE_COMPUTE:
        target = "cs_5_0";
        break;
    }

    UINT compileFlags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG;
    compileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    HRESULT hr = D3DCompile_func(
        source,
        strlen(source),
        nullptr,
        NULL,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry_point,
        target,
        compileFlags,
        0,
        &output,
        &errors_or_warnings);

    if (FAILED(hr)) {
        const char* errorMsg = (LPCSTR)errors_or_warnings->GetBufferPointer();
        //errors->Reset(errors_or_warnings->GetBufferPointer(), static_cast<uint32_t>(errors_or_warnings->GetBufferSize()));
        return {};
    }

    vgpu_shader_blob_t result = {};
    result.size = output->GetBufferSize();
    result.data = (uint8_t*)malloc(output->GetBufferSize());
    memcpy(result.data, output->GetBufferPointer(), result.size);
    return result;
#else
    return {};
#endif

}
