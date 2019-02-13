//
// Copyright (c) 2017-2019 Amer Koleci and contributors.
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

#define VGPU_IMPLEMENTATION
#include "vgpu_internal.h"

#if VGPU_D3D12

#if TODO_D3D12
#include <wrl/client.h>
#include <wrl/event.h>
#include "../Base/HashMap.h"
#include "../Base/MurmurHash.h"
#include "../Base/Vector.h"
#include "../Math/MathUtil.h"
#include "../Core/Platform.h"
#include "../IO/FileStream.h"
#include "../IO/FileSystem.h"
#include "../Core/Log.h"
#include <spirv-cross/spirv_hlsl.hpp>

#if defined(_DEBUG)
#   include <dxgidebug.h>
#endif

#if defined(_DEBUG) || defined(PROFILE)
#   if !defined(_XBOX_ONE) || !defined(_TITLE) || !defined(_DURANGO)
#       pragma comment(lib,"dxguid.lib")
#   endif
#endif

#include <d3dcompiler.h>
#if !(defined(WINAPI_FAMILY_PARTITION) && !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP))
#   pragma comment (lib, "d3dcompiler.lib")
#endif

#if defined(_DURANGO) || defined(_XBOX_ONE)
#   define AGPU_D3D12_DYNAMIC_LIB 0
#elif defined(WINAPI_FAMILY) && (WINAPI_FAMILY != WINAPI_FAMILY_DESKTOP_APP)
#   define AGPU_D3D12_DYNAMIC_LIB 0
#elif defined(_WIN64) || defined(_WIN32)
#   define AGPU_D3D12_DYNAMIC_LIB 1
#endif

#if AGPU_D3D12_DYNAMIC_LIB
typedef HRESULT(WINAPI* PFN_CREATE_DXGI_FACTORY2)(UINT flags, REFIID _riid, void** _factory);
typedef HRESULT(WINAPI* PFN_GET_DXGI_DEBUG_INTERFACE1)(UINT Flags, REFIID riid, _COM_Outptr_ void** pDebug);
#endif

DXGI_FORMAT agpuD3DConvertPixelFormat(AgpuPixelFormat format)
{
    switch (format)
    {
    case AGPU_PIXEL_FORMAT_UNKNOWN:
        return DXGI_FORMAT_UNKNOWN;

    case AGPU_PIXEL_FORMAT_R8_UNORM:
        return DXGI_FORMAT_R8_UNORM;

    case AGPU_PIXEL_FORMAT_R8_SNORM:
        return DXGI_FORMAT_R8_SNORM;

    case AGPU_PIXEL_FORMAT_R16_UNORM:
        return DXGI_FORMAT_R16_UNORM;

    case AGPU_PIXEL_FORMAT_R16_SNORM:
        return DXGI_FORMAT_R16_SNORM;

    case AGPU_PIXEL_FORMAT_RG8_UNORM:
        return DXGI_FORMAT_R8G8_UNORM;

    case AGPU_PIXEL_FORMAT_RG8_SNORM:
        return DXGI_FORMAT_R8G8_SNORM;

    case AGPU_PIXEL_FORMAT_RG16_UNORM:
        return DXGI_FORMAT_R16G16_UNORM;

    case AGPU_PIXEL_FORMAT_RG16_SNORM:
        return DXGI_FORMAT_R16G16_SNORM;

    case AGPU_PIXEL_FORMAT_RGB16_UNORM:
        return DXGI_FORMAT_UNKNOWN;

    case AGPU_PIXEL_FORMAT_RGB16_SNORM:
        return DXGI_FORMAT_UNKNOWN;

    case AGPU_PIXEL_FORMAT_RGBA8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case AGPU_PIXEL_FORMAT_RGBA8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    case AGPU_PIXEL_FORMAT_RGBA8_SNORM:
        return DXGI_FORMAT_R8G8B8A8_SNORM;

    case AGPU_PIXEL_FORMAT_BGRA8_UNORM:
        return DXGI_FORMAT_B8G8R8A8_UNORM;

    case AGPU_PIXEL_FORMAT_BGRA8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

    case AGPU_PIXEL_FORMAT_D32_FLOAT:
        return DXGI_FORMAT_D32_FLOAT;

    case AGPU_PIXEL_FORMAT_D16_UNORM:
        return DXGI_FORMAT_D16_UNORM;

    case AGPU_PIXEL_FORMAT_D24_UNORM_S8:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;

    case AGPU_PIXEL_FORMAT_D32_FLOAT_S8:
        return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

    case AGPU_PIXEL_FORMAT_BC1_UNORM:
        return DXGI_FORMAT_BC1_UNORM;

    case AGPU_PIXEL_FORMAT_BC1_UNORM_SRGB:
        return DXGI_FORMAT_BC1_UNORM_SRGB;

    case AGPU_PIXEL_FORMAT_BC2_UNORM:
        return DXGI_FORMAT_BC2_UNORM;

    case AGPU_PIXEL_FORMAT_BC2_UNORM_SRGB:
        return DXGI_FORMAT_BC2_UNORM_SRGB;

    case AGPU_PIXEL_FORMAT_BC3_UNORM:
        return DXGI_FORMAT_BC3_UNORM;

    case AGPU_PIXEL_FORMAT_BC3_UNORM_SRGB:
        return DXGI_FORMAT_BC3_UNORM_SRGB;

    case AGPU_PIXEL_FORMAT_BC4_UNORM:
        return DXGI_FORMAT_BC4_UNORM;

    case AGPU_PIXEL_FORMAT_BC4_SNORM:
        return DXGI_FORMAT_BC4_SNORM;

    case AGPU_PIXEL_FORMAT_BC5_UNORM:
        return DXGI_FORMAT_BC5_UNORM;

    case AGPU_PIXEL_FORMAT_BC5_SNORM:
        return DXGI_FORMAT_BC5_SNORM;

    case AGPU_PIXEL_FORMAT_BC6HS16:
        return DXGI_FORMAT_BC6H_SF16;

    case AGPU_PIXEL_FORMAT_BC6HU16:
        return DXGI_FORMAT_BC6H_UF16;

    case AGPU_PIXEL_FORMAT_BC7_UNORM:
        return DXGI_FORMAT_BC7_UNORM;

    case AGPU_PIXEL_FORMAT_BC7_UNORM_SRGB:
        return DXGI_FORMAT_BC7_UNORM_SRGB;
    default:
        ALIMER_UNREACHABLE();
    }
}

DXGI_FORMAT agpuD3DConvertVertexFormat(AgpuVertexFormat format)
{
    switch (format)
    {
    case AGPU_VERTEX_FORMAT_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;

    case AGPU_VERTEX_FORMAT_FLOAT2:
        return DXGI_FORMAT_R32G32_FLOAT;

    case AGPU_VERTEX_FORMAT_FLOAT3:
        return DXGI_FORMAT_R32G32B32_FLOAT;

    case AGPU_VERTEX_FORMAT_FLOAT4:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;

    case AGPU_VERTEX_FORMAT_BYTE4:
        return DXGI_FORMAT_R8G8B8A8_SINT;

    case AGPU_VERTEX_FORMAT_BYTE4N:
        return DXGI_FORMAT_R8G8B8A8_SNORM;

    case AGPU_VERTEX_FORMAT_UBYTE4:
        return DXGI_FORMAT_R8G8B8A8_UINT;

    case AGPU_VERTEX_FORMAT_UBYTE4N:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case AGPU_VERTEX_FORMAT_SHORT2:
        return DXGI_FORMAT_R16G16_SINT;

    case AGPU_VERTEX_FORMAT_SHORT2N:
        return DXGI_FORMAT_R16G16_SNORM;

    case AGPU_VERTEX_FORMAT_SHORT4:
        return DXGI_FORMAT_R16G16B16A16_SINT;

    case AGPU_VERTEX_FORMAT_SHORT4N:
        return DXGI_FORMAT_R16G16B16A16_SNORM;

    default:
        ALIMER_UNREACHABLE();
    }
}

D3D_PRIMITIVE_TOPOLOGY agpuD3DConvertPrimitiveTopology(AgpuPrimitiveTopology topology, uint32_t patchCount)
{
    switch (topology)
    {
    case AGPU_PRIMITIVE_TOPOLOGY_POINT_LIST:
        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;

    case AGPU_PRIMITIVE_TOPOLOGY_LINE_LIST:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;

    case AGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP:
        return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;

    case AGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    case AGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

    case AGPU_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;

    case AGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;

    case AGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;

    case AGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;

    case AGPU_PRIMITIVE_TOPOLOGY_PATCH_LIST:
        return (D3D_PRIMITIVE_TOPOLOGY)(uint32_t(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST) + patchCount);

    default:
        ALIMER_UNREACHABLE();
    }
}

using namespace alimer;

namespace d3d12
{
    using Microsoft::WRL::ComPtr;

    template<typename T> void Release(T*& resource)
    {
        if (resource != nullptr) {
            resource->Release();
            resource = nullptr;
        }
    }

#ifdef _DEBUG
#define DXCall(hr) \
    do \
    { \
        ALIMER_ASSERT_MSG(SUCCEEDED(hr), alimer::GetDXErrorStringAnsi(hr).CString()); \
    } \
    while(0)
#else
    // Throws a DXException on failing HRESULT
    inline void DXCall(HRESULT hr)
    {
        if (FAILED(hr))
        {
            ALIMER_LOGCRITICAL("DirectX Error: {}", alimer::GetDXErrorStringAnsi(hr));
        }
    }

#endif

#define ArraySize_(x) ((sizeof(x) / sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#ifdef AGPU_D3D12_DYNAMIC_LIB
    HMODULE s_dxgiLib = nullptr;
    HMODULE s_d3d12Lib = nullptr;

    PFN_CREATE_DXGI_FACTORY2                        CreateDXGIFactory2 = nullptr;
    PFN_GET_DXGI_DEBUG_INTERFACE1                   DXGIGetDebugInterface1 = nullptr;

    PFN_D3D12_GET_DEBUG_INTERFACE                   D3D12GetDebugInterface = nullptr;
    PFN_D3D12_CREATE_DEVICE                         D3D12CreateDevice = nullptr;
    PFN_D3D12_SERIALIZE_ROOT_SIGNATURE              D3D12SerializeRootSignature = nullptr;
    PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE    D3D12SerializeVersionedRootSignature = nullptr;
#endif

    static constexpr uint64_t RenderLatency = 2;
    static constexpr uint64_t NumCmdAllocators = RenderLatency;

    DWORD           _dxgiFactoryFlags = 0;
    IDXGIFactory4*  _dxgiFactory = nullptr;
    BOOL            _dxgiAllowTearing = FALSE;

    void GetDXGIAdapter(IDXGIAdapter1** ppAdapter)
    {
        *ppAdapter = nullptr;

        HRESULT hr = S_OK;
        bool releaseFactory = false;
        if (_dxgiFactory == nullptr)
        {
            hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&_dxgiFactory));
            if (FAILED(hr))
            {
                ALIMER_LOGERROR("Unable to create a DXGI factory. Make sure that your OS and driver support DirectX 12");
                return;
            }
            releaseFactory = true;
        }

        ComPtr<IDXGIAdapter1> adapter;
#if defined(__dxgi1_6_h__) && defined(NTDDI_WIN10_RS4)
        ComPtr<IDXGIFactory6> factory6;
        hr = _dxgiFactory->QueryInterface(factory6.ReleaseAndGetAddressOf());
        if (SUCCEEDED(hr))
        {
            for (UINT adapterIndex = 0;
                DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(
                    adapterIndex,
                    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()));
                adapterIndex++)
            {
                DXGI_ADAPTER_DESC1 desc;
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    continue;
                }

                // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
#ifdef _DEBUG
                    wchar_t buff[256] = {};
                    swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
                    OutputDebugStringW(buff);
#endif
                    break;
                }
            }
        }
        else
#endif
            for (UINT adapterIndex = 0;
                DXGI_ERROR_NOT_FOUND != _dxgiFactory->EnumAdapters1(
                    adapterIndex,
                    adapter.ReleaseAndGetAddressOf());
                ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            if (FAILED(adapter->GetDesc1(&desc)))
            {
                ALIMER_LOGERROR("DXGI - Failed to get desc");
            }

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                continue;
            }

            // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
#ifdef _DEBUG
                wchar_t buff[256] = {};
                swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
                OutputDebugStringW(buff);
#endif
                break;
            }
        }

#if !defined(NDEBUG)
        if (!adapter)
        {
            // Try WARP12 instead
            if (FAILED(_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))))
            {
                throw std::exception("WARP12 not available. Enable the 'Graphics Tools' optional feature");
            }

            OutputDebugStringA("Direct3D Adapter - WARP12\n");
        }
#endif

        if (!adapter)
        {
            ALIMER_ASSERT_MSG(false, "No Direct3D 12 device found");
        }

        *ppAdapter = adapter.Detach();

        if (releaseFactory)
        {
            Release(_dxgiFactory);
        }
    }

    // Resource Barriers
    void TransitionResource(ID3D12GraphicsCommandList* cmdList,
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after,
        uint32_t subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
    {
        D3D12_RESOURCE_BARRIER barrier = { };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = subResource;
        cmdList->ResourceBarrier(1, &barrier);
    }

    //------------------------------------------------------------------------------------------------
    // D3D12 exports a new method for serializing root signatures in the Windows 10 Anniversary Update.
    // To help enable root signature 1.1 features when they are available and not require maintaining
    // two code paths for building root signatures, this helper method reconstructs a 1.0 signature when
    // 1.1 is not supported.
    inline HRESULT D3DX12SerializeVersionedRootSignature(
        _In_ const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* pRootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION MaxVersion,
        _Outptr_ ID3DBlob** ppBlob,
        _Always_(_Outptr_opt_result_maybenull_) ID3DBlob** ppErrorBlob)
    {
        if (ppErrorBlob != nullptr)
        {
            *ppErrorBlob = nullptr;
        }

        switch (MaxVersion)
        {
        case D3D_ROOT_SIGNATURE_VERSION_1_0:
            switch (pRootSignatureDesc->Version)
            {
            case D3D_ROOT_SIGNATURE_VERSION_1_0:
                return D3D12SerializeRootSignature(&pRootSignatureDesc->Desc_1_0, D3D_ROOT_SIGNATURE_VERSION_1, ppBlob, ppErrorBlob);

            case D3D_ROOT_SIGNATURE_VERSION_1_1:
            {
                HRESULT hr = S_OK;
                const D3D12_ROOT_SIGNATURE_DESC1& desc_1_1 = pRootSignatureDesc->Desc_1_1;

                const SIZE_T ParametersSize = sizeof(D3D12_ROOT_PARAMETER) * desc_1_1.NumParameters;
                void* pParameters = (ParametersSize > 0) ? HeapAlloc(GetProcessHeap(), 0, ParametersSize) : nullptr;
                if (ParametersSize > 0 && pParameters == nullptr)
                {
                    hr = E_OUTOFMEMORY;
                }
                auto pParameters_1_0 = reinterpret_cast<D3D12_ROOT_PARAMETER*>(pParameters);

                if (SUCCEEDED(hr))
                {
                    for (UINT n = 0; n < desc_1_1.NumParameters; n++)
                    {
                        __analysis_assume(ParametersSize == sizeof(D3D12_ROOT_PARAMETER) * desc_1_1.NumParameters);
                        pParameters_1_0[n].ParameterType = desc_1_1.pParameters[n].ParameterType;
                        pParameters_1_0[n].ShaderVisibility = desc_1_1.pParameters[n].ShaderVisibility;

                        switch (desc_1_1.pParameters[n].ParameterType)
                        {
                        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
                            pParameters_1_0[n].Constants.Num32BitValues = desc_1_1.pParameters[n].Constants.Num32BitValues;
                            pParameters_1_0[n].Constants.RegisterSpace = desc_1_1.pParameters[n].Constants.RegisterSpace;
                            pParameters_1_0[n].Constants.ShaderRegister = desc_1_1.pParameters[n].Constants.ShaderRegister;
                            break;

                        case D3D12_ROOT_PARAMETER_TYPE_CBV:
                        case D3D12_ROOT_PARAMETER_TYPE_SRV:
                        case D3D12_ROOT_PARAMETER_TYPE_UAV:
                            pParameters_1_0[n].Descriptor.RegisterSpace = desc_1_1.pParameters[n].Descriptor.RegisterSpace;
                            pParameters_1_0[n].Descriptor.ShaderRegister = desc_1_1.pParameters[n].Descriptor.ShaderRegister;
                            break;

                        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
                            const D3D12_ROOT_DESCRIPTOR_TABLE1& table_1_1 = desc_1_1.pParameters[n].DescriptorTable;

                            const SIZE_T DescriptorRangesSize = sizeof(D3D12_DESCRIPTOR_RANGE) * table_1_1.NumDescriptorRanges;
                            void* pDescriptorRanges = (DescriptorRangesSize > 0 && SUCCEEDED(hr)) ? HeapAlloc(GetProcessHeap(), 0, DescriptorRangesSize) : nullptr;
                            if (DescriptorRangesSize > 0 && pDescriptorRanges == nullptr)
                            {
                                hr = E_OUTOFMEMORY;
                            }
                            auto pDescriptorRanges_1_0 = reinterpret_cast<D3D12_DESCRIPTOR_RANGE*>(pDescriptorRanges);

                            if (SUCCEEDED(hr))
                            {
                                for (UINT x = 0; x < table_1_1.NumDescriptorRanges; x++)
                                {
                                    __analysis_assume(DescriptorRangesSize == sizeof(D3D12_DESCRIPTOR_RANGE) * table_1_1.NumDescriptorRanges);
                                    pDescriptorRanges_1_0[x].BaseShaderRegister = table_1_1.pDescriptorRanges[x].BaseShaderRegister;
                                    pDescriptorRanges_1_0[x].NumDescriptors = table_1_1.pDescriptorRanges[x].NumDescriptors;
                                    pDescriptorRanges_1_0[x].OffsetInDescriptorsFromTableStart = table_1_1.pDescriptorRanges[x].OffsetInDescriptorsFromTableStart;
                                    pDescriptorRanges_1_0[x].RangeType = table_1_1.pDescriptorRanges[x].RangeType;
                                    pDescriptorRanges_1_0[x].RegisterSpace = table_1_1.pDescriptorRanges[x].RegisterSpace;
                                }
                            }

                            D3D12_ROOT_DESCRIPTOR_TABLE& table_1_0 = pParameters_1_0[n].DescriptorTable;
                            table_1_0.NumDescriptorRanges = table_1_1.NumDescriptorRanges;
                            table_1_0.pDescriptorRanges = pDescriptorRanges_1_0;
                        }
                    }
                }

                if (SUCCEEDED(hr))
                {
                    D3D12_ROOT_SIGNATURE_DESC desc_1_0 = {};
                    desc_1_0.NumParameters = desc_1_1.NumParameters;
                    desc_1_0.pParameters = pParameters_1_0;
                    desc_1_0.NumStaticSamplers = desc_1_1.NumStaticSamplers;
                    desc_1_0.pStaticSamplers = desc_1_1.pStaticSamplers;
                    desc_1_0.Flags = desc_1_1.Flags;
                    hr = D3D12SerializeRootSignature(&desc_1_0, D3D_ROOT_SIGNATURE_VERSION_1, ppBlob, ppErrorBlob);
                }

                if (pParameters)
                {
                    for (UINT n = 0; n < desc_1_1.NumParameters; n++)
                    {
                        if (desc_1_1.pParameters[n].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
                        {
                            HeapFree(GetProcessHeap(), 0, reinterpret_cast<void*>(const_cast<D3D12_DESCRIPTOR_RANGE*>(pParameters_1_0[n].DescriptorTable.pDescriptorRanges)));
                        }
                    }
                    HeapFree(GetProcessHeap(), 0, pParameters);
                }
                return hr;
            }
            }
            break;

        case D3D_ROOT_SIGNATURE_VERSION_1_1:
            return D3D12SerializeVersionedRootSignature(pRootSignatureDesc, ppBlob, ppErrorBlob);
        }

        return E_INVALIDARG;
    }

    static Hash MakeCompilerHash()
    {
#if AGPU_COMPILER_DXC
        HMODULE module = LoadLibraryW(L"dxcompiler.dll");
#else
        HMODULE module = LoadLibraryW(L"d3dcompiler_47.dll");
#endif

        if (module == nullptr)
        {
            ALIMER_LOGCRITICAL("Failed to load D3D shader compiler DLL");
        }

        wchar_t dllPath[1024] = { };
        GetModuleFileNameW(module, dllPath, ArraySize_(dllPath));

        FileStream dllFile(String(dllPath), FileAccess::ReadOnly);
        uint64_t fileSize = dllFile.Size();
        Vector<uint8_t> fileData(static_cast<uint32_t>(fileSize));
        dllFile.Read(fileData.Data(), uint32_t(fileSize));
        return GenerateHash(fileData.Data(), int32_t(fileSize));
}

    static Hash CompilerHash = MakeCompilerHash();

    static const String baseCacheDir = "ShaderCache\\";

#if _DEBUG
    static const String cacheSubDir = "Debug\\";
#else
    static const String cacheSubDir = "Release\\";
#endif

    static const String cacheDir = baseCacheDir + cacheSubDir;

    static const uint64_t CacheVersion = 0;

    static String MakeDefinesString(const D3D_SHADER_MACRO* defines)
    {
        String definesString = "";
        while (defines && defines->Name != nullptr && defines != nullptr)
        {
            if (definesString.Length() > 0)
                definesString += "|";
            definesString += defines->Name;
            definesString += "=";
            definesString += defines->Definition;
            ++defines;
        }

        return definesString;
    }

    static String MakeShaderCacheName(
        const String& shaderCode,
        const char* entryPoint,
        const char* profile,
        const D3D_SHADER_MACRO* defines)
    {
        String hashString = shaderCode;
        hashString += "\n";
        if (entryPoint != nullptr)
        {
            hashString += entryPoint;
            hashString += "\n";
        }
        hashString += profile;
        hashString += "\n";

        hashString += MakeDefinesString(defines);

        hashString += String(CacheVersion);

        Hash codeHash = GenerateHash(hashString.CString(), int(hashString.Length()), 0);
        codeHash = CombineHashes(codeHash, CompilerHash);

        return cacheDir + codeHash.ToString() + ".cache";
    }

    // Heap helpers
    const D3D12_HEAP_PROPERTIES* agpuD3D12GetDefaultHeapProps()
    {
        static D3D12_HEAP_PROPERTIES heapProps =
        {
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN,
            0,
            0,
        };

        return &heapProps;
    }

    const D3D12_HEAP_PROPERTIES* agpuD3D12GetUploadHeapProps()
    {
        static D3D12_HEAP_PROPERTIES heapProps =
        {
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN,
            0,
            0,
        };

        return &heapProps;
    }

    const D3D12_HEAP_PROPERTIES* agpuD3D12GetReadbackHeapProps()
    {
        static D3D12_HEAP_PROPERTIES heapProps =
        {
            D3D12_HEAP_TYPE_READBACK,
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN,
            0,
            0,
        };

        return &heapProps;
    }

    struct AgpuFence
    {
        ID3D12Fence*        Fence;
        HANDLE              Event;
    };

    struct PersistentDescriptorAlloc
    {
        D3D12_CPU_DESCRIPTOR_HANDLE Handles[RenderLatency] = { };
        uint32_t Index = static_cast<uint32_t>(-1);
    };

    struct TempDescriptorAlloc
    {
        D3D12_CPU_DESCRIPTOR_HANDLE StartCPUHandle = { };
        D3D12_GPU_DESCRIPTOR_HANDLE StartGPUHandle = { };
        uint32_t StartIndex = static_cast<uint32_t>(-1);
    };

    class DescriptorHeap
    {
    public:
        ~DescriptorHeap();

        void Initialize(ID3D12Device* device, uint32_t numPersistent, uint32_t numTemporary, D3D12_DESCRIPTOR_HEAP_TYPE heapType, bool shaderVisible);
        void Shutdown();
        void EndFrame();

        PersistentDescriptorAlloc AllocatePersistent();
        void FreePersistent(uint32_t& index);
        void FreePersistent(D3D12_CPU_DESCRIPTOR_HANDLE& handle);
        void FreePersistent(D3D12_GPU_DESCRIPTOR_HANDLE& handle);

        D3D12_CPU_DESCRIPTOR_HANDLE CPUHandleFromIndex(uint32_t descriptorIndex) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GPUHandleFromIndex(uint32_t descriptorIndex) const;

        D3D12_CPU_DESCRIPTOR_HANDLE CPUHandleFromIndex(uint32_t descriptorIndex, uint64_t heapIndex) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GPUHandleFromIndex(uint32_t descriptorIndex, uint64_t heapIndex) const;

        uint32_t IndexFromHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle);
        uint32_t IndexFromHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle);

        ID3D12DescriptorHeap* CurrentHeap() const;
        uint32_t TotalNumDescriptors() const { return NumPersistent + NumTemporary; }

    private:
        uint32_t _heapIndex = 0;
        uint32_t _heapCount = 0;
        ID3D12DescriptorHeap* _heaps[RenderLatency] = { };
        uint32_t NumPersistent = 0;
        uint32_t PersistentAllocated = 0;
        std::vector<uint32_t> DeadList;
        uint32_t NumTemporary = 0;
        volatile int64_t TemporaryAllocated = 0;
        uint32_t DescriptorSize = 0;
        AgpuBool32 ShaderVisible = false;
        D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        D3D12_CPU_DESCRIPTOR_HANDLE CPUStart[RenderLatency] = { };
        D3D12_GPU_DESCRIPTOR_HANDLE GPUStart[RenderLatency] = { };
        SRWLOCK Lock = SRWLOCK_INIT;
    };

    class ShaderModule final
    {
    public:
        ShaderModule();
    };

    class Shader final
    {
    public:
        Shader(const AgpuShaderDescriptor* descriptor);
        ~Shader();

        bool isCompute = false;
        AgpuShaderStageFlags stages;
        Vector<AgpuShaderModule> shaderModules;
    };

    class GraphicsStateD3D12
    {
    public:
        GraphicsStateD3D12();

        void Reset();

        void SetPrimitiveTopology(AgpuPrimitiveTopology primitiveTopology);

        AgpuPrimitiveTopology GetPrimitiveTopology() const { return _primitiveTopology; }

        bool IsDirty() const { return _dirty; }
        void ClearDirty() { _dirty = false; }
        void SetDirty() { _dirty = true; }

    private:
        AgpuPrimitiveTopology _primitiveTopology;

        bool _dirty = false;
    };

    class CommandBufferD3D12
    {
    public:
        CommandBufferD3D12(ID3D12Device* device, ID3D12CommandAllocator* allocator);
        ~CommandBufferD3D12();

        void Begin(ID3D12CommandAllocator* allocator);
        void End();

        void CmdSetShader(AgpuShader shader);
        void CmdSetVertexBuffer(uint32_t binding, AgpuBuffer buffer, uint64_t offset, uint32_t stride, AgpuVertexInputRate inputRate);
        void CmdSetIndexBuffer(AgpuBuffer buffer, uint64_t offset, AgpuIndexType indexType);

        void CmdSetPrimitiveTopology(AgpuPrimitiveTopology topology);

        void CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        void CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

        ID3D12GraphicsCommandList* GetCommandList() const { return _commandList; }

    private:
        void FlushGraphicsState();

        bool _isRecording = false;
        ID3D12GraphicsCommandList* _commandList = nullptr;

        GraphicsStateD3D12 _graphicsState;

        struct VertexBindingState
        {
            AgpuBuffer              buffers[AGPU_MAX_VERTEX_BUFFER_BINDINGS];
            uint64_t                offsets[AGPU_MAX_VERTEX_BUFFER_BINDINGS];
            AgpuVertexInputRate     inputRates[AGPU_MAX_VERTEX_BUFFER_BINDINGS];
        };

        struct IndexState
        {
            AgpuBuffer buffer;
            uint64_t offset;
            AgpuIndexType indexType;
        };

        VertexBindingState _vbo = {};
        D3D12_VERTEX_BUFFER_VIEW _d3dVbViews[AGPU_MAX_VERTEX_BUFFER_BINDINGS] = {};
        IndexState _index = {};
        uint32_t _dirtyVbos = 0;

        ID3D12PipelineState* _currentPipelineState = nullptr;
        AgpuShader _currentShader = nullptr;
    };

    struct AGpuRendererD3D12 : public AGpuRendererI
    {
        AGpuRendererD3D12()
        {
        }

        ~AGpuRendererD3D12()
        {
        }

        AgpuResult Initialize(const AgpuDescriptor* descriptor) override;
        void Shutdown() override;
        void WaitIdle();
        void BeginFrame();
        uint64_t Frame() override;

        /* Fence */
        AgpuFence* CreateFence(uint64_t initialValue);
        void DestroyFence(AgpuFence* fence);
        void SignalFence(AgpuFence* fence, ID3D12CommandQueue* queue, uint64_t fenceValue);
        void WaitFence(AgpuFence* fence, uint64_t fenceValue);
        bool IsFenceSignaled(AgpuFence* fence, uint64_t fenceValue);
        void ClearFence(AgpuFence* fence, uint64_t fenceValue);

        /* Swapchain */
        AgpuSwapchain CreateSwapchain(const AgpuSwapchainDescriptor* descriptor);
        void DestroySwapchain(AgpuSwapchain swapchain);

        /* Buffer */
        AgpuBuffer CreateBuffer(const AgpuBufferDescriptor* descriptor, const void* initialData, void* externalHandle) override;
        void DestroyBuffer(AgpuBuffer buffer) override;

        /* Texture */
        AgpuTexture CreateTexture(const AgpuTextureDescriptor* descriptor, void* externalHandle) override;
        void DestroyTexture(AgpuTexture texture) override;

        /* Framebuffer */
        AgpuFramebuffer CreateFramebuffer(const AgpuFramebufferDescriptor* descriptor) override;
        void DestroyFramebuffer(AgpuFramebuffer framebuffer) override;

        /* ShaderModule */
        AgpuShaderModule CreateShaderModule(const AgpuShaderModuleDescriptor* descriptor) override;
        AgpuShaderModule CompileShaderModule(AgpuShaderStageFlagBits stage, const char* source, const char* entryPoint);
        void DestroyShaderModule(AgpuShaderModule shaderModule) override;

        /* Shader */
        AgpuShader CreateShader(const AgpuShaderDescriptor* descriptor) override;
        void DestroyShader(AgpuShader shader) override;

        /* Pipeline */
        AgpuPipeline CreateRenderPipeline(const AgpuRenderPipelineDescriptor* descriptor) override;
        AgpuPipeline CreateComputePipeline(const AgpuComputePipelineDescriptor* descriptor) override;
        void DestroyPipeline(AgpuPipeline pipeline) override;

        /* CommandList */
        void BeginCommandBuffer(AgpuCommandBuffer commandBuffer) override;
        void EndCommandBuffer() override;
        void CmdBeginRenderPass(AgpuFramebuffer framebuffer) override;
        void CmdEndRenderPass() override;
        void CmdSetShader(AgpuShader shader) override;
        void CmdSetVertexBuffer(uint32_t binding, AgpuBuffer buffer, uint64_t offset, uint32_t stride, AgpuVertexInputRate inputRate) override;
        void CmdSetIndexBuffer(AgpuBuffer buffer, uint64_t offset, AgpuIndexType indexType) override;

        void CmdSetViewport(AgpuViewport viewport) override;
        void CmdSetViewports(uint32_t count, const AgpuViewport* pViewports) override;

        void CmdSetScissor(AgpuRect2D scissor) override;
        void CmdSetScissors(uint32_t count, const AgpuRect2D* pScissors) override;

        void CmdSetPrimitiveTopology(AgpuPrimitiveTopology topology) override;
        void CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
        void CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override;

        void CreateRootSignature(ID3D12RootSignature** rootSignature, const D3D12_ROOT_SIGNATURE_DESC1& desc);

        template<typename T> void DeferredRelease(T*& resource, bool forceDeferred = false)
        {
            IUnknown* base = resource;
            DeferredRelease_(base, forceDeferred);
            resource = nullptr;
        }

    private:
        void SetResourceName(ID3D12Object* object, const char* name);
        void DeferredRelease_(IUnknown* resource, bool forceDeferred = false);
        void ProcessDeferredReleases(uint64_t frameIndex);
        void InitializeHelpers();
        void ShutdownHelpers();
        void EndFrameHelpers();

    private:
        IDXGIAdapter1*              _dxgiAdapter = nullptr;
        ID3D12Device*               _d3dDevice = nullptr;
        D3D_FEATURE_LEVEL           _d3dFeatureLevel = D3D_FEATURE_LEVEL_11_0;
        D3D_ROOT_SIGNATURE_VERSION  _d3dRootSignatureVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        AgpuFence*                  _frameFence = nullptr;
        uint64_t                    _fenceValues[AGPU_MAX_BACK_BUFFER_COUNT] = {};
        ID3D12CommandQueue*         _graphicsQueue = nullptr;
        CommandBufferD3D12*         _primaryCommandBuffer = nullptr;
        ID3D12CommandAllocator*     _commandAllocators[AGPU_MAX_BACK_BUFFER_COUNT];

        uint64_t                    _currentCPUFrame = 0;
        uint64_t                    _currentGPUFrame = 0;
        uint64_t                    _currentFrameIndex = 0;
        bool                        _shuttingDown = false;
        std::vector<IUnknown*>      _deferredReleases[RenderLatency];
        bool                        _headless = false;
        AgpuSwapchain               _mainSwapchain = nullptr;
        AgpuBool32                  _raytracingSupported = AGPU_FALSE;

        DescriptorHeap              _RTVDescriptorHeap;
        DescriptorHeap              _SRVDescriptorHeap;
        DescriptorHeap              _DSVDescriptorHeap;
        DescriptorHeap              _UAVDescriptorHeap;
        AgpuFramebuffer             _currentFramebuffer = nullptr;
    };

    /* DescriptorHeap */
    DescriptorHeap::~DescriptorHeap()
    {
        ALIMER_ASSERT(_heaps[0] == nullptr);
    }

    void DescriptorHeap::Initialize(ID3D12Device* device, uint32_t numPersistent, uint32_t numTemporary, D3D12_DESCRIPTOR_HEAP_TYPE heapType, bool shaderVisible)
    {
        Shutdown();

        uint32_t totalNumDescriptors = numPersistent + numTemporary;
        ALIMER_ASSERT(totalNumDescriptors > 0);

        NumPersistent = numPersistent;
        NumTemporary = numTemporary;
        HeapType = heapType;
        ShaderVisible = shaderVisible;
        if (heapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || heapType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
            ShaderVisible = false;

        _heapCount = ShaderVisible ? 2 : 1;

        DeadList.resize(numPersistent);
        for (uint32_t i = 0u; i < numPersistent; ++i)
        {
            DeadList[i] = i;
        }

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
        heapDesc.Type = heapType;
        heapDesc.NumDescriptors = totalNumDescriptors;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heapDesc.NodeMask = 0;
        if (ShaderVisible)
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        for (uint32_t i = 0; i < _heapCount; ++i)
        {
            DXCall(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_heaps[i])));
            CPUStart[i] = _heaps[i]->GetCPUDescriptorHandleForHeapStart();
            if (ShaderVisible)
            {
                GPUStart[i] = _heaps[i]->GetGPUDescriptorHandleForHeapStart();
            }
        }

        DescriptorSize = device->GetDescriptorHandleIncrementSize(heapType);
    }

    void DescriptorHeap::Shutdown()
    {
        ALIMER_ASSERT(PersistentAllocated == 0);
        for (uint64_t i = 0; i < ArraySize_(_heaps); ++i)
        {
            Release(_heaps[i]);
        }
    }

    void DescriptorHeap::EndFrame()
    {
        ALIMER_ASSERT(_heaps[0] != nullptr);
        TemporaryAllocated = 0;
        _heapIndex = (_heapIndex + 1) % _heapCount;
    }

    PersistentDescriptorAlloc DescriptorHeap::AllocatePersistent()
    {
        ALIMER_ASSERT(_heaps[0] != nullptr);

        AcquireSRWLockExclusive(&Lock);

        ALIMER_ASSERT(PersistentAllocated < NumPersistent);
        uint32_t index = DeadList[PersistentAllocated];
        ++PersistentAllocated;

        ReleaseSRWLockExclusive(&Lock);

        PersistentDescriptorAlloc alloc;
        alloc.Index = index;
        for (uint32_t i = 0; i < _heapCount; ++i)
        {
            alloc.Handles[i] = CPUStart[i];
            alloc.Handles[i].ptr += index * DescriptorSize;
        }

        return alloc;
    }

    void DescriptorHeap::FreePersistent(uint32_t& index)
    {
        if (index == static_cast<uint32_t>(-1))
            return;

        ALIMER_ASSERT(index < NumPersistent);
        ALIMER_ASSERT(_heaps[0] != nullptr);

        AcquireSRWLockExclusive(&Lock);

        ALIMER_ASSERT(PersistentAllocated > 0);
        DeadList[PersistentAllocated - 1] = index;
        --PersistentAllocated;

        ReleaseSRWLockExclusive(&Lock);

        index = static_cast<uint32_t>(-1);
    }

    void DescriptorHeap::FreePersistent(D3D12_CPU_DESCRIPTOR_HANDLE& handle)
    {
        ALIMER_ASSERT(_heapCount == 1);
        if (handle.ptr != 0)
        {
            uint32_t index = IndexFromHandle(handle);
            FreePersistent(index);
            handle = { };
        }
    }

    void DescriptorHeap::FreePersistent(D3D12_GPU_DESCRIPTOR_HANDLE& handle)
    {
        ALIMER_ASSERT(_heapCount == 1);
        if (handle.ptr != 0)
        {
            uint32_t index = IndexFromHandle(handle);
            FreePersistent(index);
            handle = { };
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CPUHandleFromIndex(uint32_t descriptorIndex) const
    {
        return CPUHandleFromIndex(descriptorIndex, _heapIndex);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GPUHandleFromIndex(uint32_t descriptorIndex) const
    {
        return GPUHandleFromIndex(descriptorIndex, _heapIndex);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CPUHandleFromIndex(uint32_t descriptorIndex, uint64_t heapIndex) const
    {
        ALIMER_ASSERT(_heaps[0] != nullptr);
        ALIMER_ASSERT(heapIndex < _heapCount);
        ALIMER_ASSERT(descriptorIndex < TotalNumDescriptors());
        D3D12_CPU_DESCRIPTOR_HANDLE handle = CPUStart[heapIndex];
        handle.ptr += descriptorIndex * DescriptorSize;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GPUHandleFromIndex(uint32_t descriptorIndex, uint64_t heapIndex) const
    {
        ALIMER_ASSERT(_heaps[0] != nullptr);
        ALIMER_ASSERT(heapIndex < _heapCount);
        ALIMER_ASSERT(descriptorIndex < TotalNumDescriptors());
        ALIMER_ASSERT(ShaderVisible);
        D3D12_GPU_DESCRIPTOR_HANDLE handle = GPUStart[heapIndex];
        handle.ptr += descriptorIndex * DescriptorSize;
        return handle;
    }

    uint32_t DescriptorHeap::IndexFromHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        ALIMER_ASSERT(_heaps[0] != nullptr);
        ALIMER_ASSERT(handle.ptr >= CPUStart[_heapIndex].ptr);
        ALIMER_ASSERT(handle.ptr < CPUStart[_heapIndex].ptr + DescriptorSize * TotalNumDescriptors());
        ALIMER_ASSERT((handle.ptr - CPUStart[_heapIndex].ptr) % DescriptorSize == 0);
        return uint32_t(handle.ptr - CPUStart[_heapIndex].ptr) / DescriptorSize;
    }

    uint32_t DescriptorHeap::IndexFromHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle)
    {
        ALIMER_ASSERT(_heaps[0] != nullptr);
        ALIMER_ASSERT(handle.ptr >= GPUStart[_heapIndex].ptr);
        ALIMER_ASSERT(handle.ptr < GPUStart[_heapIndex].ptr + DescriptorSize * TotalNumDescriptors());
        ALIMER_ASSERT((handle.ptr - GPUStart[_heapIndex].ptr) % DescriptorSize == 0);
        return uint32_t(handle.ptr - GPUStart[_heapIndex].ptr) / DescriptorSize;
    }

    ID3D12DescriptorHeap* DescriptorHeap::CurrentHeap() const
    {
        ALIMER_ASSERT(_heaps[0] != nullptr);
        return _heaps[_heapIndex];
    }

    /* ShaderModule */
    ShaderModule::ShaderModule()
    {

    }

    /* Shader */
    Shader::Shader(const AgpuShaderDescriptor* descriptor)
    {
        isCompute = false;
        shaderModules.Resize(descriptor->stageCount);
        for (uint32_t i = 0; i < descriptor->stageCount; i++)
        {
            uint32_t stage_mask = 1u << i;
            stages |= stage_mask;
            shaderModules[i] = descriptor->stages[i].shaderModule;
        }
    }

    Shader::~Shader()
    {

    }

    /* GraphicsStateD3D12 */
    GraphicsStateD3D12::GraphicsStateD3D12()
    {
        Reset();
    }

    void GraphicsStateD3D12::Reset()
    {
        _primitiveTopology = AGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        _dirty = false;
    }

    void GraphicsStateD3D12::SetPrimitiveTopology(AgpuPrimitiveTopology primitiveTopology)
    {
        if (_primitiveTopology != primitiveTopology)
        {
            _primitiveTopology = primitiveTopology;
            _dirty = true;
        }
    }

    /* CommandBufferD3D12 */
    CommandBufferD3D12::CommandBufferD3D12(ID3D12Device* device, ID3D12CommandAllocator* allocator)
    {
        DXCall(device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator,
            nullptr,
            IID_PPV_ARGS(&_commandList)
        ));
        DXCall(_commandList->Close());
    }

    CommandBufferD3D12::~CommandBufferD3D12()
    {
        Release(_commandList);
    }

    void CommandBufferD3D12::Begin(ID3D12CommandAllocator* allocator)
    {
        if (_isRecording)
            return;

        _isRecording = true;
        // Prepare the command buffers to be used for the next frame
        DXCall(_commandList->Reset(allocator, nullptr));

        /* Reset cache states. */
        _graphicsState.Reset();
        _dirtyVbos = ~0u;
        _currentPipelineState = nullptr;
        _currentShader = nullptr;
        memset(&_index, 0, sizeof(_index));
        memset(_vbo.buffers, 0, sizeof(_vbo.buffers));
    }

    void CommandBufferD3D12::End()
    {
        HRESULT hr = _commandList->Close();
        if (FAILED(hr))
        {
            ALIMER_LOGERROR("Failed to end command buffer");
        }

        _isRecording = false;
    }

    void CommandBufferD3D12::CmdSetShader(AgpuShader shader)
    {
        if (_currentShader == shader)
            return;

        _currentShader = shader;
        _currentPipelineState = nullptr;
        _graphicsState.SetDirty();
    }

    void CommandBufferD3D12::CmdSetVertexBuffer(uint32_t binding, AgpuBuffer buffer, uint64_t offset, uint32_t stride, AgpuVertexInputRate inputRate)
    {
        if (_vbo.buffers[binding] != buffer || _vbo.offsets[binding] != offset)
        {
            _dirtyVbos |= 1u << binding;
        }

        if (_vbo.inputRates[binding] != inputRate)
        {
            _graphicsState.SetDirty();
        }

        _vbo.buffers[binding] = buffer;
        _vbo.offsets[binding] = offset;
        _vbo.inputRates[binding] = inputRate;

        _d3dVbViews[binding].BufferLocation = buffer->d3d12GPUAddress + offset;
        _d3dVbViews[binding].SizeInBytes = static_cast<UINT>(buffer->size - offset);
        _d3dVbViews[binding].StrideInBytes = stride;
    }

    void CommandBufferD3D12::CmdSetIndexBuffer(AgpuBuffer buffer, uint64_t offset, AgpuIndexType indexType)
    {
        if (_index.buffer == buffer
            && _index.offset == offset
            && _index.indexType == indexType)
        {
            return;
        }

        _index.buffer = buffer;
        _index.offset = offset;
        _index.indexType = indexType;

        D3D12_INDEX_BUFFER_VIEW bufferView = { };
        bufferView.BufferLocation = buffer->d3d12GPUAddress + offset;
        bufferView.SizeInBytes = static_cast<UINT>(buffer->size - offset);
        bufferView.Format = indexType == AGPU_INDEX_TYPE_UINT16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        _commandList->IASetIndexBuffer(&bufferView);
    }

    void CommandBufferD3D12::CmdSetPrimitiveTopology(AgpuPrimitiveTopology topology)
    {
        _graphicsState.SetPrimitiveTopology(topology);
    }

    void CommandBufferD3D12::CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        FlushGraphicsState();

        _commandList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void CommandBufferD3D12::CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
        FlushGraphicsState();

        _commandList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void CommandBufferD3D12::FlushGraphicsState()
    {
        // ALIMER_ASSERT(_currentProgram);

        uint32_t active_vbos = 1;

        // We've invalidated pipeline state, update the VkPipeline.
        if (_graphicsState.IsDirty())
        {
            /*ID3D12PipelineState* oldPipelineState = _currentPipelineState;
            Hasher hasher;

            _currentPipelineState = _currentShader->d3d12PipelineState;
            if (oldPipelineState != _currentPipelineState)
            {
                if (_currentShader->isCompute)
                {
                    _commandList->SetComputeRootSignature(_currentShader->d3d12RootSignature);
                }
                else
                {
                    _commandList->SetGraphicsRootSignature(_currentShader->d3d12RootSignature);
                }

                _commandList->SetPipelineState(_currentPipelineState);
            }*/

            // Set primitive topology 
            D3D_PRIMITIVE_TOPOLOGY d3dPrimitiveTopology = agpuD3DConvertPrimitiveTopology(_graphicsState.GetPrimitiveTopology(), 1);
            _commandList->IASetPrimitiveTopology(d3dPrimitiveTopology);

            // Reset graphics state dirty bit.
            _graphicsState.ClearDirty();
        }


        uint32_t update_vbo_mask = _dirtyVbos & active_vbos;
        ForEachBitRange(update_vbo_mask, [&](uint32_t binding, uint32_t count)
            {
#ifdef ALIMER_DEV
                for (uint32_t i = binding; i < binding + count; i++)
                {
                    ALIMER_ASSERT(_vbo.buffers[i] != nullptr);
    }
#endif

                _commandList->IASetVertexBuffers(binding, count, _d3dVbViews);
        });

        _dirtyVbos &= ~update_vbo_mask;
    }

    // Per thread command buffer currently being recorded.
    static thread_local CommandBufferD3D12* s_pActiveCommandBuffer = nullptr;

    /* AGpuRendererD3D12 */
    AgpuResult AGpuRendererD3D12::Initialize(const AgpuDescriptor* descriptor)
    {
        _shuttingDown = false;
        GetDXGIAdapter(&_dxgiAdapter);

        DXCall(D3D12CreateDevice(_dxgiAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_d3dDevice)));

#ifndef NDEBUG
        if (descriptor->validation)
        {
            // Configure debug device (if active).
            ID3D12InfoQueue* d3dInfoQueue;
            if (SUCCEEDED(_d3dDevice->QueryInterface(&d3dInfoQueue)))
            {
#ifdef _DEBUG
                d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
                d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif
                D3D12_MESSAGE_ID hide[] =
                {
                    D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                    D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                    D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
                };
                D3D12_INFO_QUEUE_FILTER filter = {};
                filter.DenyList.NumIDs = _countof(hide);
                filter.DenyList.pIDList = hide;
                d3dInfoQueue->AddStorageFilterEntries(&filter);

                Release(d3dInfoQueue);
            }
        }
#endif

        // Determine maximum supported feature level for this device
        static const D3D_FEATURE_LEVEL s_featureLevels[] =
        {
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };

        D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels =
        {
            _countof(s_featureLevels), s_featureLevels, D3D_FEATURE_LEVEL_11_0
        };

        HRESULT hr = _d3dDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels));
        if (SUCCEEDED(hr))
        {
            _d3dFeatureLevel = featLevels.MaxSupportedFeatureLevel;
        }
        else
        {
            _d3dFeatureLevel = D3D_FEATURE_LEVEL_11_0;
        }

        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = _d3dRootSignatureVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            _d3dRootSignatureVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }


#if ALIMER_DXR
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options;
        if (SUCCEEDED(_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options, sizeof(options)))
            && options.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
        {
            _raytracingSupported = AGPU_TRUE;
    }
#endif

        // Create the command queue.
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        DXCall(_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_graphicsQueue)));
        _graphicsQueue->SetName(L"Main Gfx Queue");

        // Create a command allocator for each back buffer that will be rendered to.
        for (uint32_t n = 0; n < NumCmdAllocators; n++)
        {
            DXCall(_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocators[n])));

            wchar_t name[25] = {};
            swprintf_s(name, L"Frame CommandAllocator %u", n);
            _commandAllocators[n]->SetName(name);
        }

        // Create a command list for recording graphics commands.
        _primaryCommandBuffer = new CommandBufferD3D12(_d3dDevice, _commandAllocators[0]);
        _primaryCommandBuffer->GetCommandList()->SetName(L"Primary Graphics Command List");

        // Create fence
        _frameFence = CreateFence(0);
        _fenceValues[_currentFrameIndex]++;
        _currentFrameIndex = 0;

        // Initialize helpers and upload.
        InitializeHelpers();

        WaitIdle();
        for (UINT n = 0; n < NumCmdAllocators; n++)
        {
            _fenceValues[n] = _fenceValues[_currentFrameIndex];
        }

        // Create main swap chain if not headless and valid width and height.
        _headless = descriptor->headless;
        if (!_headless
            && descriptor->swapchain.width > 0
            && descriptor->swapchain.height > 0)
        {
            _mainSwapchain = CreateSwapchain(&descriptor->swapchain);
        }

        BeginFrame();

        return AGPU_OK;
    }

    void AGpuRendererD3D12::InitializeHelpers()
    {
        _RTVDescriptorHeap.Initialize(_d3dDevice, 256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
        _SRVDescriptorHeap.Initialize(_d3dDevice, 1024, 1024, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
        _DSVDescriptorHeap.Initialize(_d3dDevice, 256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);
        _UAVDescriptorHeap.Initialize(_d3dDevice, 256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false);
    }

    void AGpuRendererD3D12::ShutdownHelpers()
    {
        _RTVDescriptorHeap.Shutdown();
        _SRVDescriptorHeap.Shutdown();
        _DSVDescriptorHeap.Shutdown();
        _UAVDescriptorHeap.Shutdown();
    }

    void AGpuRendererD3D12::EndFrameHelpers()
    {
        _RTVDescriptorHeap.EndFrame();
        _SRVDescriptorHeap.EndFrame();
        _DSVDescriptorHeap.EndFrame();
        _UAVDescriptorHeap.EndFrame();
    }

    void AGpuRendererD3D12::Shutdown()
    {
        WaitIdle();
        _shuttingDown = true;

        if (_mainSwapchain)
        {
            DestroySwapchain(_mainSwapchain);
        }

        for (uint64_t i = 0; i < RenderLatency; ++i)
        {
            ProcessDeferredReleases(i);
        }

        DestroyFence(_frameFence);

        for (uint64_t i = 0; i < RenderLatency; ++i)
        {
            Release(_commandAllocators[i]);
        }

        // Free primary command buffer.
        delete _primaryCommandBuffer;
        _primaryCommandBuffer = nullptr;

        Release(_graphicsQueue);

        ShutdownHelpers();

        Release(_dxgiFactory);
        Release(_dxgiAdapter);
        Release(_d3dDevice);

#ifdef _DEBUG
        {
            IDXGIDebug1* dxgiDebug;
            if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
            {
                dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
                Release(dxgiDebug);
            }
        }
#endif
    }

    void AGpuRendererD3D12::WaitIdle()
    {
        // Schedule a Signal command in the queue.
        const uint64_t fenceValue = _fenceValues[_currentFrameIndex];
        SignalFence(_frameFence, _graphicsQueue, fenceValue);

        // Wait until the fence has been processed.
        DXCall(_frameFence->Fence->SetEventOnCompletion(fenceValue, _frameFence->Event));
        WaitForSingleObjectEx(_frameFence->Event, INFINITE, FALSE);

        // Increment the fence value for the current frame.
        _fenceValues[_currentFrameIndex]++;
    }

    void AGpuRendererD3D12::BeginFrame()
    {
        // Prepare the command buffers to be used for the next frame
        DXCall(_commandAllocators[_currentFrameIndex]->Reset());
        _primaryCommandBuffer->Begin(_commandAllocators[_currentFrameIndex]);
        s_pActiveCommandBuffer = _primaryCommandBuffer;

        if (!_headless)
        {
            _mainSwapchain->backBufferIndex = _mainSwapchain->d3d12SwapChain->GetCurrentBackBufferIndex();
            CmdBeginRenderPass(_mainSwapchain->backBufferFramebuffers[_mainSwapchain->backBufferIndex]);
        }
    }

    uint64_t AGpuRendererD3D12::Frame()
    {
        if (!_headless)
        {
            CmdEndRenderPass();
        }

        // Send the command list off to the GPU for processing.
        _primaryCommandBuffer->End();
        s_pActiveCommandBuffer = nullptr;

        /* End frame for upload */
        //EndFrameUpload();

        ID3D12CommandList* ppCommandLists[] = { _primaryCommandBuffer->GetCommandList() };
        _graphicsQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        // Present the frame.
        if (_mainSwapchain)
        {
            const bool vsync = true;
            uint32_t syncIntervals = vsync ? 1 : 0;
            DXCall(_mainSwapchain->d3d12SwapChain->Present(syncIntervals, syncIntervals == 0 ? DXGI_PRESENT_ALLOW_TEARING : 0));
        }

        ++_currentCPUFrame;

        // Prepare to render the next frame.
        // Schedule a Signal command in the queue.
        const uint64_t currentFenceValue = _fenceValues[_currentFrameIndex];
        SignalFence(_frameFence, _graphicsQueue, currentFenceValue);

        // Update frame index.
        _currentFrameIndex = (_currentFrameIndex + 1) % NumCmdAllocators;

        // If the next frame is not ready to be rendered yet, wait until it is ready.
        WaitFence(_frameFence, _fenceValues[_currentFrameIndex]);

        // Set the fence value for the next frame.
        _fenceValues[_currentFrameIndex] = currentFenceValue + 1;

        EndFrameHelpers();

        // See if we have any deferred releases to process
        ProcessDeferredReleases(_currentFrameIndex);

        // Begin new frame.
        BeginFrame();

        // Return fence value.
        return _fenceValues[_currentFrameIndex];
    }

    void AGpuRendererD3D12::SetResourceName(ID3D12Object* object, const char* name)
    {
        size_t length = strlen(name);
        std::vector<wchar_t> szBuffer(length);
        if (MultiByteToWideChar(CP_UTF8, 0, name, -1, szBuffer.data(), static_cast<int>(szBuffer.size())) == 0)
            return;

        object->SetName(szBuffer.data());
    }

    void AGpuRendererD3D12::DeferredRelease_(IUnknown* resource, bool forceDeferred)
    {
        if (resource == nullptr)
            return;

        if ((forceDeferred == false)
            || _shuttingDown || _d3dDevice == nullptr)
        {
            // Free-for-all!
            resource->Release();
            return;
        }

        _deferredReleases[_currentFrameIndex].push_back(resource);
    }

    void AGpuRendererD3D12::ProcessDeferredReleases(uint64_t frameIndex)
    {

    }

    AgpuFence* AGpuRendererD3D12::CreateFence(uint64_t initialValue)
    {
        AgpuFence* fence = new AgpuFence();
        _d3dDevice->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence->Fence));

        fence->Event = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
        ALIMER_ASSERT(fence->Event != INVALID_HANDLE_VALUE);

        return fence;
    }

    void AGpuRendererD3D12::DestroyFence(AgpuFence* fence)
    {
        DeferredRelease(fence->Fence);
        delete fence;
        fence = nullptr;
    }

    void AGpuRendererD3D12::SignalFence(AgpuFence* fence, ID3D12CommandQueue* queue, uint64_t fenceValue)
    {
        ALIMER_ASSERT(fence->Fence != nullptr);
        DXCall(queue->Signal(fence->Fence, fenceValue));
    }

    void AGpuRendererD3D12::WaitFence(AgpuFence* fence, uint64_t fenceValue)
    {
        ALIMER_ASSERT(fence->Fence != nullptr);
        if (fence->Fence->GetCompletedValue() < fenceValue)
        {
            DXCall(fence->Fence->SetEventOnCompletion(fenceValue, fence->Event));
            WaitForSingleObjectEx(fence->Event, INFINITE, FALSE);
        }
    }

    bool AGpuRendererD3D12::IsFenceSignaled(AgpuFence* fence, uint64_t fenceValue)
    {
        ALIMER_ASSERT(fence->Fence != nullptr);
        return fence->Fence->GetCompletedValue() >= fenceValue;
    }

    void AGpuRendererD3D12::ClearFence(AgpuFence* fence, uint64_t fenceValue)
    {
        ALIMER_ASSERT(fence->Fence != nullptr);
        DXCall(fence->Fence->Signal(fenceValue));
    }

    AgpuSwapchain AGpuRendererD3D12::CreateSwapchain(const AgpuSwapchainDescriptor* descriptor)
    {
        // Create a descriptor for the swap chain.
        AgpuPixelFormat backBufferFormat = AGPU_PIXEL_FORMAT_BGRA8_UNORM;
        DXGI_FORMAT dxgiBackBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        uint32_t backbufferCount = 2;
        if (descriptor->preferredColorFormat != AGPU_PIXEL_FORMAT_UNKNOWN)
        {
            switch (descriptor->preferredColorFormat)
            {
            case AGPU_PIXEL_FORMAT_BGRA8_UNORM_SRGB:
                backBufferFormat = AGPU_PIXEL_FORMAT_BGRA8_UNORM_SRGB;
                dxgiBackBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                break;

            default:
                backBufferFormat = AGPU_PIXEL_FORMAT_BGRA8_UNORM;
                dxgiBackBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
                break;
            }
        }

        if (descriptor->bufferCount != 0)
        {
            backbufferCount = descriptor->bufferCount;
            if (backbufferCount > 3)
                backbufferCount = 3;
        }

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = descriptor->width;
        swapChainDesc.Height = descriptor->height;
        swapChainDesc.Format = dxgiBackBufferFormat;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = backbufferCount;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swapChainDesc.Flags =
            DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
            DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        /* TODO: DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT*/

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
        fsSwapChainDesc.Windowed = TRUE;

        // Create a swap chain for the window.
        IDXGISwapChain1* swapChain;
        DXCall(_dxgiFactory->CreateSwapChainForHwnd(
            _graphicsQueue,
            static_cast<HWND>(descriptor->windowHandle),
            &swapChainDesc,
            &fsSwapChainDesc,
            nullptr,
            &swapChain
        ));

        IDXGISwapChain3* swapChain3;
        DXCall(swapChain->QueryInterface(&swapChain3));
        swapChain->Release();

        // Create engine instance and assign.
        AgpuSwapchain swapchain = new AgpuSwapchain_T();
        swapchain->backBufferIndex = swapChain3->GetCurrentBackBufferIndex();
        swapchain->backbufferCount = backbufferCount;
        swapchain->backBufferFormat = backBufferFormat;
        swapchain->dxgiBackBufferFormat = dxgiBackBufferFormat;
        swapchain->d3d12SwapChain = swapChain3;

        AgpuTextureDescriptor textureDescriptor = {};
        textureDescriptor.type = AGPU_TEXTURE_TYPE_2D;
        textureDescriptor.width = descriptor->width;
        textureDescriptor.height = descriptor->height;
        textureDescriptor.depthOrArraySize = 1;
        textureDescriptor.mipLevels = 1;
        textureDescriptor.format = backBufferFormat;
        textureDescriptor.usage = AgpuTextureUsage(AGPU_TEXTURE_USAGE_OUTPUT_ATTACHMENT | AGPU_TEXTURE_USAGE_PRESENT);
        textureDescriptor.samples = AGPU_SAMPLE_COUNT1;

        AgpuFramebufferDescriptor fboDescriptor = {};

        for (uint32_t i = 0; i < swapchain->backbufferCount; i++)
        {
            ID3D12Resource* resource;
            DXCall(swapChain3->GetBuffer(i, IID_PPV_ARGS(&resource)));

            wchar_t name[25] = {};
            swprintf_s(name, L"Back Buffer %u", i);
            resource->SetName(name);

            auto d3dTextureDesc = resource->GetDesc();
            ALIMER_UNUSED(d3dTextureDesc);

            // Create external texture.
            swapchain->backBufferTexture[i] = agpuCreateExternalTexture(&textureDescriptor, resource);

            // Create framebuffer.
            fboDescriptor.colorAttachments[0].texture = swapchain->backBufferTexture[i];
            swapchain->backBufferFramebuffers[i] = CreateFramebuffer(&fboDescriptor);
        }

        return swapchain;
    }

    void AGpuRendererD3D12::DestroySwapchain(AgpuSwapchain swapchain)
    {
        for (uint32_t i = 0; i < swapchain->backbufferCount; ++i)
        {
            DestroyTexture(swapchain->backBufferTexture[i]);
            DestroyFramebuffer(swapchain->backBufferFramebuffers[i]);
        }

        DeferredRelease(swapchain->d3d12SwapChain);
        delete swapchain;
        swapchain = nullptr;
    }

    AgpuTexture AGpuRendererD3D12::CreateTexture(const AgpuTextureDescriptor* descriptor, void* externalHandle)
    {
        AgpuTexture texture = new AgpuTexture_T();
        texture->dxgiFormat = agpuD3DConvertPixelFormat(descriptor->format);
        if (externalHandle == nullptr)
        {

        }
        else
        {
            texture->d3d12Resource = static_cast<ID3D12Resource*>(externalHandle);
        }

        if (descriptor->usage & AGPU_TEXTURE_USAGE_PRESENT)
        {
            texture->d3d12ResourceState = D3D12_RESOURCE_STATE_PRESENT;
        }
        else
        {
            if (descriptor->usage & AGPU_TEXTURE_USAGE_TRANSFER_SRC)
            {
                texture->d3d12ResourceState |= D3D12_RESOURCE_STATE_COPY_SOURCE;
            }

            if (descriptor->usage & AGPU_TEXTURE_USAGE_TRANSFER_DEST)
            {
                texture->d3d12ResourceState |= D3D12_RESOURCE_STATE_COPY_DEST;
            }

            if (descriptor->usage & AGPU_TEXTURE_USAGE_SAMPLED)
            {
                texture->d3d12ResourceState |= (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            }

            if (descriptor->usage & AGPU_TEXTURE_USAGE_STORAGE)
            {
                texture->d3d12ResourceState |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            }
            if (descriptor->usage & AGPU_TEXTURE_USAGE_OUTPUT_ATTACHMENT)
            {
                if (agpuIsDepthFormat(descriptor->format) || agpuIsStencilFormat(descriptor->format))
                {
                    texture->d3d12ResourceState |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
                }
                else
                {
                    texture->d3d12ResourceState |= D3D12_RESOURCE_STATE_RENDER_TARGET;
                }
            }
        }

        return texture;
    }

    AgpuBuffer AGpuRendererD3D12::CreateBuffer(const AgpuBufferDescriptor* descriptor, const void* initialData, void* externalHandle)
    {
        AgpuBuffer buffer = new AgpuBuffer_T();
        buffer->frameIndex = _currentCPUFrame;

        uint64_t size = descriptor->size;
        size = AlignTo(size, uint64_t(descriptor->stride));

        const bool allowUAV = false;
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_GENERIC_READ;

        const bool dynamic = descriptor->usage & AGPU_BUFFER_USAGE_DYNAMIC;
        const bool cpuAccessible = descriptor->usage & AGPU_BUFFER_USAGE_CPU_ACCESSIBLE;

        D3D12_RESOURCE_DESC resourceDesc = { };
        if (externalHandle == nullptr)
        {
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Width = dynamic ? size * RenderLatency : size;
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            resourceDesc.Flags = allowUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.SampleDesc.Quality = 0;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resourceDesc.Alignment = 0;

            const D3D12_HEAP_PROPERTIES* heapProps = cpuAccessible
                ? agpuD3D12GetUploadHeapProps() : agpuD3D12GetDefaultHeapProps();
            D3D12_RESOURCE_STATES resourceState = initialState;
            if (cpuAccessible)
            {
                resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
            }
            else if (initialData)
            {
                resourceState = D3D12_RESOURCE_STATE_COMMON;
            }

            ID3D12Heap* heap = nullptr;
            const uint64_t heapOffset = 0;
            if (heap)
            {
                DXCall(_d3dDevice->CreatePlacedResource(
                    heap,
                    heapOffset,
                    &resourceDesc,
                    resourceState,
                    nullptr,
                    IID_PPV_ARGS(&buffer->d3d12Resource))
                );
            }
            else
            {
                DXCall(_d3dDevice->CreateCommittedResource(
                    heapProps,
                    D3D12_HEAP_FLAG_NONE,
                    &resourceDesc,
                    resourceState,
                    nullptr,
                    IID_PPV_ARGS(&buffer->d3d12Resource))
                );
            }
        }
        else
        {
            buffer->d3d12Resource = static_cast<ID3D12Resource*>(externalHandle);
            resourceDesc = buffer->d3d12Resource->GetDesc();
        }

        if (descriptor->name)
        {
            SetResourceName(buffer->d3d12Resource, descriptor->name);
        }

        buffer->d3d12GPUAddress = buffer->d3d12Resource->GetGPUVirtualAddress();

        if (cpuAccessible)
        {
            D3D12_RANGE readRange = { };
            DXCall(buffer->d3d12Resource->Map(0, &readRange, reinterpret_cast<void**>(&buffer->d3d12CPUAddress)));
        }

        if (initialData && cpuAccessible)
        {
            for (uint64_t i = 0; i < RenderLatency; ++i)
            {
                uint8_t* dstMem = buffer->d3d12CPUAddress + size * i;
                memcpy(dstMem, initialData, size);
            }

        }
        else if (initialData)
        {
            /*UploadContext uploadContext = ResourceUploadBegin(resourceDesc.Width);

            memcpy(uploadContext.CPUAddress, initialData, size);
            if (dynamic)
                memcpy((uint8_t*)uploadContext.CPUAddress + size, initialData, size);

            uploadContext.commandList->CopyBufferRegion(
                buffer->d3d12Resource,
                0,
                uploadContext.resource,
                uploadContext.resourceOffset,
                size);

            ResourceUploadEnd(uploadContext);*/
        }

        return buffer;
    }

    void AGpuRendererD3D12::DestroyBuffer(AgpuBuffer buffer)
    {
        DeferredRelease(buffer->d3d12Resource);
        delete buffer;
        buffer = nullptr;
    }

    void AGpuRendererD3D12::DestroyTexture(AgpuTexture texture)
    {
        DeferredRelease(texture->d3d12Resource);
        delete texture;
        texture = nullptr;
    }

    /* Framebuffer */
    AgpuFramebuffer AGpuRendererD3D12::CreateFramebuffer(const AgpuFramebufferDescriptor* descriptor)
    {
        AgpuFramebuffer framebuffer = new AgpuFramebuffer_T();
        framebuffer->width = UINT32_MAX;
        framebuffer->height = UINT32_MAX;
        framebuffer->numRTVs = 0;
        for (int i = 0; i < AGPU_MAX_COLOR_ATTACHMENTS; i++)
        {
            if (descriptor->colorAttachments[i].texture == nullptr)
                continue;

            framebuffer->colorAttachments[framebuffer->numRTVs] = descriptor->colorAttachments[i];
            framebuffer->d3d12RTVs[framebuffer->numRTVs] = _RTVDescriptorHeap.AllocatePersistent().Handles[0];

            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Format = descriptor->colorAttachments[i].texture->dxgiFormat;
            rtvDesc.Texture2D.MipSlice = descriptor->colorAttachments[i].mipLevel;
            rtvDesc.Texture2D.PlaneSlice = 0;

            _d3dDevice->CreateRenderTargetView(
                descriptor->colorAttachments[i].texture->d3d12Resource,
                nullptr,
                framebuffer->d3d12RTVs[framebuffer->numRTVs]
            );

            uint32_t mipLevel = descriptor->colorAttachments[i].mipLevel;
            framebuffer->width = std::min(framebuffer->width, agpuGetTextureLevelWidth(descriptor->colorAttachments[i].texture, mipLevel));
            framebuffer->height = std::min(framebuffer->height, agpuGetTextureLevelHeight(descriptor->colorAttachments[i].texture, mipLevel));
            framebuffer->numRTVs++;
        }

        // Depth stencil.
        if (descriptor->depthStencilAttachment.texture)
        {
            uint32_t mipLevel = descriptor->depthStencilAttachment.mipLevel;
            framebuffer->width = std::min(framebuffer->width, agpuGetTextureLevelWidth(descriptor->depthStencilAttachment.texture, mipLevel));
            framebuffer->height = std::min(framebuffer->height, agpuGetTextureLevelHeight(descriptor->depthStencilAttachment.texture, mipLevel));
        }

        return framebuffer;
    }

    void AGpuRendererD3D12::DestroyFramebuffer(AgpuFramebuffer framebuffer)
    {
        for (uint32_t i = 0; i < framebuffer->numRTVs; i++)
        {
            _RTVDescriptorHeap.FreePersistent(framebuffer->d3d12RTVs[i]);
        }

        delete framebuffer;
        framebuffer = nullptr;
    }

    AgpuShaderModule AGpuRendererD3D12::CreateShaderModule(const AgpuShaderModuleDescriptor* descriptor)
    {
        if (descriptor->source)
        {
            // Compiling from GLSL source requires the entry point.
            if (!descriptor->entryPoint)
            {
                return nullptr;
            }

            return CompileShaderModule(descriptor->stage, descriptor->source, descriptor->entryPoint);
        }

        // Check if dx bycode.
        if (descriptor->codeSize > 4
            && descriptor->pCode[0] == 'D'
            && descriptor->pCode[1] == 'X'
            && descriptor->pCode[2] == 'B'
            && descriptor->pCode[3] == 'C')
        {
            /* TODO: Reflect D3D shader */
            AgpuShaderModule shaderModule = new AgpuShaderModule_T();
            //shaderModule->stage = stage;
            shaderModule->d3d12Bytecode.BytecodeLength = descriptor->codeSize;
            shaderModule->d3d12Bytecode.pShaderBytecode = new uint8_t[descriptor->codeSize];
            memcpy((void*)shaderModule->d3d12Bytecode.pShaderBytecode, descriptor->pCode, descriptor->codeSize);
            return shaderModule;
        }
        else
        {
            spirv_cross::CompilerGLSL::Options options_glsl;
            //options_glsl.vertex.fixup_clipspace = true;
            //options_glsl.vertex.flip_vert_y = true;
            options_glsl.flatten_multidimensional_arrays = true;

            spirv_cross::CompilerHLSL compiler(reinterpret_cast<const uint32_t*>(descriptor->pCode), descriptor->codeSize / 4);
            compiler.set_common_options(options_glsl);

            spirv_cross::CompilerHLSL::Options options_hlsl;
            options_hlsl.shader_model = 51;
            compiler.set_hlsl_options(options_hlsl);

            uint32_t new_builtin = compiler.remap_num_workgroups_builtin();
            if (new_builtin)
            {
                compiler.set_decoration(new_builtin, spv::DecorationDescriptorSet, 0);
                compiler.set_decoration(new_builtin, spv::DecorationBinding, 0);
            }

            auto resources = compiler.get_shader_resources();

            AgpuShaderStageFlagBits stage = AGPU_SHADER_STAGE_NONE;
            switch (compiler.get_execution_model())
            {
            case spv::ExecutionModelVertex:
                stage = AGPU_SHADER_STAGE_VERTEX_BIT;
                break;
            case spv::ExecutionModelTessellationControl:
                stage = AGPU_SHADER_STAGE_TESS_CONTROL_BIT;
                break;
            case spv::ExecutionModelTessellationEvaluation:
                stage = AGPU_SHADER_STAGE_TESS_EVAL_BIT;
                break;
            case spv::ExecutionModelGeometry:
                stage = AGPU_SHADER_STAGE_GEOMETRY_BIT;
                break;
            case spv::ExecutionModelFragment:
                stage = AGPU_SHADER_STAGE_FRAGMENT_BIT;
                break;
            case spv::ExecutionModelGLCompute:
                stage = AGPU_SHADER_STAGE_COMPUTE_BIT;
                break;
            default:
                ALIMER_LOGCRITICAL("Invalid shader execution model");
            }

            std::string convertedHlslSource = compiler.compile();
            return CompileShaderModule(stage, convertedHlslSource.c_str(), "main");
        }
    }

    void AGpuRendererD3D12::DestroyShaderModule(AgpuShaderModule shaderModule)
    {
        delete[] shaderModule->d3d12Bytecode.pShaderBytecode;
        delete shaderModule;
        shaderModule = nullptr;
    }

    AgpuShaderModule AGpuRendererD3D12::CompileShaderModule(AgpuShaderStageFlagBits stage, const char* source, const char* entryPoint)
    {
        const char* compileTarget = nullptr;
        switch (stage)
        {
        case AGPU_SHADER_STAGE_VERTEX_BIT:
#ifdef AGPU_COMPILER_DXC
            compileTarget = "vs_6_1";
#else
            compileTarget = "vs_5_1";
#endif
            break;
        case AGPU_SHADER_STAGE_TESS_CONTROL_BIT:
#ifdef AGPU_COMPILER_DXC
            compileTarget = "hs_6_1";
#else
            compileTarget = "hs_5_1";
#endif
            break;

        case AGPU_SHADER_STAGE_TESS_EVAL_BIT:
#ifdef AGPU_COMPILER_DXC
            compileTarget = "ds_6_1";
#else
            compileTarget = "ds_5_1";
#endif
            break;

        case AGPU_SHADER_STAGE_GEOMETRY_BIT:
#ifdef AGPU_COMPILER_DXC
            compileTarget = "gs_6_1";
#else
            compileTarget = "gs_5_1";
#endif
            break;

        case AGPU_SHADER_STAGE_FRAGMENT_BIT:
#ifdef AGPU_COMPILER_DXC
            compileTarget = "ps_6_1";
#else
            compileTarget = "ps_5_1";
#endif
            break;

        case AGPU_SHADER_STAGE_COMPUTE_BIT:
#ifdef AGPU_COMPILER_DXC
            compileTarget = "cs_6_1";
#else
            compileTarget = "cs_5_1";
#endif
            break;

        default:
            break;
        }

        String cacheName = MakeShaderCacheName(source, entryPoint, compileTarget, nullptr);
        if (FileSystem::FileExists(cacheName))
        {
            ALIMER_LOGDEBUG("Load compiled shader from cache '{}'", cacheName.CString());

            FileStream cacheFile(cacheName, FileAccess::ReadOnly);
            const uint64_t shaderSize = cacheFile.Size();

#ifdef AGPU_COMPILER_DXC

#else
            Vector<uint8_t> compressedShader(static_cast<uint32_t>(shaderSize));
            cacheFile.Read(compressedShader.Data(), shaderSize);

            ID3DBlob* decompressedShader[1] = { nullptr };
            uint32_t indices[1] = { 0 };
            DXCall(D3DDecompressShaders(
                compressedShader.Data(), shaderSize, 1, 0,
                indices, 0, decompressedShader, nullptr));

            AgpuShaderModule shaderModule = new AgpuShaderModule_T();
            shaderModule->stage = stage;
            shaderModule->d3d12Bytecode.BytecodeLength = decompressedShader[0]->GetBufferSize();
            shaderModule->d3d12Bytecode.pShaderBytecode = new uint8_t[decompressedShader[0]->GetBufferSize()];
            memcpy((void*)shaderModule->d3d12Bytecode.pShaderBytecode, decompressedShader[0]->GetBufferPointer(), decompressedShader[0]->GetBufferSize());
            decompressedShader[0]->Release();
            return shaderModule;
#endif
        }

#ifdef AGPU_COMPILER_DXC
#else

        // TODO: Cache.
        UINT flags = D3DCOMPILE_WARNINGS_ARE_ERRORS;
        flags |= D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;
        flags |= D3DCOMPILE_ALL_RESOURCES_BOUND;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG;
#endif

        if (!entryPoint
            || !strlen(entryPoint))
        {
            entryPoint = "main";
        }

        ComPtr<ID3DBlob> compiledShader;
        ID3DBlob* errorMessages;
        HRESULT hr = D3DCompile(
            source,
            strlen(source),
            nullptr,
            nullptr,
            nullptr,
            entryPoint,
            compileTarget,
            flags,
            0,
            compiledShader.ReleaseAndGetAddressOf(),
            &errorMessages);

        if (FAILED(hr))
        {
            if (errorMessages)
            {
                wchar_t message[1024] = { 0 };
                char* blobdata = reinterpret_cast<char*>(errorMessages->GetBufferPointer());

                MultiByteToWideChar(CP_ACP, 0, blobdata, static_cast<int>(errorMessages->GetBufferSize()), message, 1024);
                std::wstring fullMessage = L"Error compiling shader \"";
                fullMessage += L"\" - ";
                fullMessage += message;

                // Pop up a message box allowing user to retry compilation
                int retVal = MessageBoxW(nullptr, fullMessage.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
                if (retVal != IDRETRY)
                {
                    ALIMER_BREAKPOINT();
                }

                errorMessages->Release();
                errorMessages = nullptr;
            }

            return {};
        }
        else
        {
            const bool saveCache = true;
            if (saveCache)
            {
                // Compress shader and save in cache
                ComPtr<ID3DBlob> compressedShader;

                // Compress the shader
                D3D_SHADER_DATA shaderData;
                shaderData.pBytecode = compiledShader->GetBufferPointer();
                shaderData.BytecodeLength = compiledShader->GetBufferSize();
                DXCall(D3DCompressShaders(1, &shaderData, D3D_COMPRESS_SHADER_KEEP_ALL_PARTS, &compressedShader));

                // Create the cache directory if it doesn't exist
                if (FileSystem::DirectoryExists(baseCacheDir) == false)
                    FileSystem::CreateDir(baseCacheDir);

                if (FileSystem::DirectoryExists(cacheDir) == false)
                    FileSystem::CreateDir(cacheDir);

                FileStream cacheFile(cacheName, FileAccess::WriteOnly);

                // Write the compiled shader to disk
                uint64_t shaderSize = compressedShader->GetBufferSize();
                cacheFile.Write(compressedShader->GetBufferPointer(), shaderSize);
            }

            AgpuShaderModule shaderModule = new AgpuShaderModule_T();
            shaderModule->stage = stage;
            shaderModule->d3d12Bytecode.BytecodeLength = compiledShader->GetBufferSize();
            shaderModule->d3d12Bytecode.pShaderBytecode = new uint8_t[compiledShader->GetBufferSize()];
            memcpy((void*)shaderModule->d3d12Bytecode.pShaderBytecode, compiledShader->GetBufferPointer(), compiledShader->GetBufferSize());
            return shaderModule;
        }
#endif /* AGPU_COMPILER_DXC */
    }

    AgpuShader AGpuRendererD3D12::CreateShader(const AgpuShaderDescriptor* descriptor)
    {
        return reinterpret_cast<AgpuShader>(new Shader(descriptor));
    }

    void AGpuRendererD3D12::DestroyShader(AgpuShader shader)
    {
        delete reinterpret_cast<Shader*>(shader);
        shader = nullptr;
    }

    AgpuPipeline AGpuRendererD3D12::CreateRenderPipeline(const AgpuRenderPipelineDescriptor* descriptor)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

        // Root signature
        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = { };
        rootSignatureDesc.NumParameters = 0;
        rootSignatureDesc.pParameters = nullptr;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        /*if ((descriptor->shader->stages & AGPU_SHADER_STAGE_FRAGMENT_BIT) == AGPU_SHADER_STAGE_FRAGMENT_BIT)
        {
            rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
        }

        for (uint32_t i = 0; i < descriptor->shader->shaderModulesCount; i++)
        {
            AgpuShaderModule shaderModule =  descriptor->shader->shaderModules[i];
            switch (shaderModule->stage)
            {
            case AGPU_SHADER_STAGE_VERTEX_BIT:
                psoDesc.VS = shaderModule->d3d12Bytecode;
                rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
                break;

            case AGPU_SHADER_STAGE_FRAGMENT_BIT:
                psoDesc.PS = shaderModule->d3d12Bytecode;
                break;

            default:
                break;
            }
        }*/

        /*if (descriptor->domain)
        {
            psoDesc.DS = descriptor->domain->d3d12Bytecode;
        }
        else*/
        {
            rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
        }

        /*if (descriptor->hull)
        {
            psoDesc.HS = descriptor->hull->d3d12Bytecode;
        }
        else*/
        {
            rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
        }

        /*if (descriptor->geometry)
        {
            psoDesc.GS = descriptor->geometry->d3d12Bytecode;
        }
        else*/
        {
            rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
        }

        CreateRootSignature(&psoDesc.pRootSignature, rootSignatureDesc);

        // StreamOutput
        //psoDesc.StreamOutput;

        // BlendState
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
        {
            FALSE,FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            psoDesc.BlendState.RenderTarget[i] = defaultRenderTargetBlendDesc;
        }
        psoDesc.SampleMask = UINT_MAX;

        // RasterizerState
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // DepthStencilState
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        // InputLayout
        /* resolve vertex attributes */
        int autoOffset[AGPU_MAX_VERTEX_BUFFER_BINDINGS];
        for (int i = 0; i < AGPU_MAX_VERTEX_BUFFER_BINDINGS; i++)
        {
            autoOffset[i] = 0;
        }

        bool useAutoOffset = true;
        for (int i = 0; i < AGPU_MAX_VERTEX_ATTRIBUTES; i++)
        {
            if (descriptor->vertexDescriptor.attributes[i].offset != 0) {
                useAutoOffset = false;
            }
        }

        UINT inputElementsCount = 0u;
        D3D12_INPUT_ELEMENT_DESC inputElements[AGPU_MAX_VERTEX_ATTRIBUTES];

        for (uint32_t i = 0u; i < AGPU_MAX_VERTEX_ATTRIBUTES; i++)
        {
            const AgpuVertexAttributeDescriptor* attributeDesc = &descriptor->vertexDescriptor.attributes[i];
            if (attributeDesc->format == AGPU_VERTEX_FORMAT_UNKNOWN) {
                break;
            }

            ALIMER_ASSERT((attributeDesc->bufferIndex >= 0) && (attributeDesc->bufferIndex < AGPU_MAX_VERTEX_BUFFER_BINDINGS));
            const AgpuVertexBufferLayoutDescriptor* layoutDesc = &descriptor->vertexDescriptor.layouts[attributeDesc->bufferIndex];

            // If the HLSL semantic is TEXCOORDN the SemanticName should be "TEXCOORD" and the
            // SemanticIndex N
            D3D12_INPUT_ELEMENT_DESC* inputElement = &inputElements[inputElementsCount++];
            inputElement->SemanticName = "TEXCOORD";
            inputElement->SemanticIndex = i;
            inputElement->Format = agpuD3DConvertVertexFormat(attributeDesc->format);
            inputElement->InputSlot = attributeDesc->bufferIndex;
            inputElement->AlignedByteOffset = useAutoOffset ? autoOffset[attributeDesc->bufferIndex] : attributeDesc->offset;

            if (layoutDesc->inputRate == AGPU_VERTEX_INPUT_RATE_VERTEX)
            {
                inputElement->InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                inputElement->InstanceDataStepRate = 0;
            }
            else
            {
                inputElement->InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                inputElement->InstanceDataStepRate = 1;
            }

            //autoOffset[attributeDesc->bufferIndex] += agpuGetVertexFormatSize(attributeDesc->format);
        }

        psoDesc.InputLayout.pInputElementDescs = inputElements;
        psoDesc.InputLayout.NumElements = inputElementsCount;

        psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = _mainSwapchain->dxgiBackBufferFormat;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.NodeMask = 0;
        //psoDesc.CachedPSO;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        ID3D12PipelineState* pipelineState;
        DXCall(_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));

        // Create pipeline.
        AgpuPipeline pipeline = new AgpuPipeline_T();
        pipeline->isCompute = AGPU_FALSE;
        pipeline->d3dPrimitiveTopology = agpuD3DConvertPrimitiveTopology(descriptor->primitiveTopology, 1);
        pipeline->d3d12RootSignature = psoDesc.pRootSignature;
        pipeline->d3d12PipelineState = pipelineState;
        return pipeline;
    }

    AgpuPipeline AGpuRendererD3D12::CreateComputePipeline(const AgpuComputePipelineDescriptor* descriptor)
    {
        AgpuPipeline pipeline = new AgpuPipeline_T();
        pipeline->isCompute = AGPU_TRUE;

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = nullptr;
        psoDesc.CS;
        psoDesc.NodeMask = 0;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        DXCall(_d3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipeline->d3d12PipelineState)));
        pipeline->d3d12RootSignature = psoDesc.pRootSignature;
        return pipeline;
    }

    void AGpuRendererD3D12::DestroyPipeline(AgpuPipeline pipeline)
    {
        DeferredRelease(pipeline->d3d12PipelineState);
        delete pipeline;
        pipeline = nullptr;
    }

    void AGpuRendererD3D12::BeginCommandBuffer(AgpuCommandBuffer commandBuffer)
    {
        auto commandBufferD3D12{ reinterpret_cast<CommandBufferD3D12*>(commandBuffer) };

        s_pActiveCommandBuffer = commandBufferD3D12;
        s_pActiveCommandBuffer->Begin(_commandAllocators[_currentFrameIndex]);
    }

    void AGpuRendererD3D12::EndCommandBuffer()
    {
        if (!s_pActiveCommandBuffer)
            return;

        s_pActiveCommandBuffer->End();
        s_pActiveCommandBuffer = nullptr;
    }

    void AGpuRendererD3D12::CmdBeginRenderPass(AgpuFramebuffer framebuffer)
    {
        _currentFramebuffer = framebuffer;

        for (uint32_t i = 0; i < framebuffer->numRTVs; ++i)
        {
            // Indicate that the resource will be used as a render target.
            TransitionResource(s_pActiveCommandBuffer->GetCommandList(),
                framebuffer->colorAttachments[i].texture->d3d12Resource,
                framebuffer->colorAttachments[i].texture->d3d12ResourceState,
                D3D12_RESOURCE_STATE_RENDER_TARGET);

            const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
            s_pActiveCommandBuffer->GetCommandList()->ClearRenderTargetView(framebuffer->d3d12RTVs[i], clearColor, 0, nullptr);
        }

        if (framebuffer->d3d12DSV.ptr)
        {
            s_pActiveCommandBuffer->GetCommandList()->OMSetRenderTargets(framebuffer->numRTVs, framebuffer->d3d12RTVs, FALSE, &framebuffer->d3d12DSV);
        }
        else
        {
            s_pActiveCommandBuffer->GetCommandList()->OMSetRenderTargets(framebuffer->numRTVs, framebuffer->d3d12RTVs, FALSE, nullptr);
        }

        // Set viewport and scissor to framebuffer size.
        uint32_t width = framebuffer->width;
        uint32_t height = framebuffer->height;
        AgpuViewport viewport = {
            0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
        AgpuRect2D scissorRect = { 0, 0, width, height };

        CmdSetViewport(viewport);
        CmdSetScissor(scissorRect);
    }

    void AGpuRendererD3D12::CmdEndRenderPass()
    {
        for (uint32_t i = 0; i < _currentFramebuffer->numRTVs; ++i)
        {
            // Indicate that the back buffer will now be used to present.
            TransitionResource(s_pActiveCommandBuffer->GetCommandList(),
                _currentFramebuffer->colorAttachments[i].texture->d3d12Resource,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                _currentFramebuffer->colorAttachments[i].texture->d3d12ResourceState);
        }

        _currentFramebuffer = nullptr;
    }

    void AGpuRendererD3D12::CmdSetShader(AgpuShader shader)
    {
        s_pActiveCommandBuffer->CmdSetShader(shader);
    }

    void AGpuRendererD3D12::CmdSetVertexBuffer(uint32_t binding, AgpuBuffer buffer, uint64_t offset, uint32_t stride, AgpuVertexInputRate inputRate)
    {
        s_pActiveCommandBuffer->CmdSetVertexBuffer(binding, buffer, offset, stride, inputRate);
    }

    void AGpuRendererD3D12::CmdSetIndexBuffer(AgpuBuffer buffer, uint64_t offset, AgpuIndexType indexType)
    {
        s_pActiveCommandBuffer->CmdSetIndexBuffer(buffer, offset, indexType);
    }

    void AGpuRendererD3D12::CmdSetViewport(AgpuViewport viewport)
    {
        D3D12_VIEWPORT d3dViewport;
        d3dViewport.TopLeftX = viewport.x;
        d3dViewport.TopLeftY = viewport.y;
        d3dViewport.Width = viewport.width;
        d3dViewport.Height = viewport.height;
        d3dViewport.MinDepth = viewport.minDepth;
        d3dViewport.MaxDepth = viewport.maxDepth;

        // Set viewport
        s_pActiveCommandBuffer->GetCommandList()->RSSetViewports(1, &d3dViewport);
    }

    void AGpuRendererD3D12::CmdSetViewports(uint32_t count, const AgpuViewport* pViewports)
    {
    }

    void AGpuRendererD3D12::CmdSetScissor(AgpuRect2D scissor)
    {
        D3D12_RECT d3dRect = {};
        d3dRect.left = static_cast<long>(scissor.x);
        d3dRect.top = static_cast<long>(scissor.y);
        d3dRect.right = static_cast<long>(scissor.x + scissor.width);
        d3dRect.bottom = static_cast<long>(scissor.y + scissor.height);
        s_pActiveCommandBuffer->GetCommandList()->RSSetScissorRects(1, &d3dRect);
    }

    void AGpuRendererD3D12::CmdSetScissors(uint32_t count, const AgpuRect2D* pScissors)
    {
    }

    void AGpuRendererD3D12::CmdSetPrimitiveTopology(AgpuPrimitiveTopology topology)
    {
        s_pActiveCommandBuffer->CmdSetPrimitiveTopology(topology);
    }

    void AGpuRendererD3D12::CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        s_pActiveCommandBuffer->CmdDraw(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void AGpuRendererD3D12::CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
        s_pActiveCommandBuffer->CmdDrawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void AGpuRendererD3D12::CreateRootSignature(
        ID3D12RootSignature** rootSignature,
        const D3D12_ROOT_SIGNATURE_DESC1& desc)
    {
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc = { };
        versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        versionedDesc.Desc_1_1 = desc;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3DX12SerializeVersionedRootSignature(
            &versionedDesc,
            _d3dRootSignatureVersion,
            &signature,
            &error);

        if (FAILED(hr))
        {
            const char* errString = error ? reinterpret_cast<const char*>(error->GetBufferPointer()) : "";
            ALIMER_ASSERT_MSG(false, "Failed to create root signature: %s", errString);
        }

        hr = _d3dDevice->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(rootSignature));

        if (FAILED(hr))
        {
            ALIMER_LOGERROR("Failed to create root signature, hr={}", hr);
        }
    }

    AgpuBool32 isSupported()
    {
        static AgpuBool32 availableCheck = AGPU_FALSE;
        static AgpuBool32 isAvailable = AGPU_FALSE;

        if (availableCheck)
            return isAvailable;

        availableCheck = AGPU_TRUE;
#if AGPU_D3D12_DYNAMIC_LIB
        // Load libraries first.
        s_dxgiLib = LoadLibraryW(L"dxgi.dll");
        if (!s_dxgiLib)
        {
            OutputDebugStringW(L"Failed to load dxgi.dll");
            availableCheck = AGPU_FALSE;
            return availableCheck;
        }

        s_d3d12Lib = LoadLibraryW(L"d3d12.dll");
        if (!s_d3d12Lib)
        {
            OutputDebugStringW(L"Failed to load d3d12.dll");
            availableCheck = AGPU_FALSE;
            return availableCheck;
        }

        /* DXGI entry points */
        CreateDXGIFactory2 = (PFN_CREATE_DXGI_FACTORY2)GetProcAddress(s_dxgiLib, "CreateDXGIFactory2");
        DXGIGetDebugInterface1 = (PFN_GET_DXGI_DEBUG_INTERFACE1)GetProcAddress(s_dxgiLib, "DXGIGetDebugInterface1");
        if (CreateDXGIFactory2 == nullptr)
        {
            OutputDebugStringW(L"Cannot find CreateDXGIFactory2 entry point.");
            availableCheck = AGPU_FALSE;
            return availableCheck;
        }

        /* D3D12 entry points */
        D3D12GetDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(s_d3d12Lib, "D3D12GetDebugInterface");
        D3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(s_d3d12Lib, "D3D12CreateDevice");
        D3D12SerializeRootSignature = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(s_d3d12Lib, "D3D12SerializeRootSignature");
        D3D12SerializeVersionedRootSignature = (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(s_d3d12Lib, "D3D12SerializeVersionedRootSignature");

        if (!D3D12CreateDevice)
        {
            OutputDebugStringW(L"Cannot find D3D12CreateDevice entry point.");
            availableCheck = AGPU_FALSE;
            return availableCheck;
        }

        if (!D3D12SerializeRootSignature)
        {
            OutputDebugStringW(L"Cannot find D3D12SerializeRootSignature entry point.");
            availableCheck = AGPU_FALSE;
            return availableCheck;
        }
#endif

        IDXGIAdapter1* adapter;
        GetDXGIAdapter(&adapter);
        isAvailable = adapter != nullptr;
        Release(adapter);
        return isAvailable;
    }

    AGpuRendererI* createBackend(bool validation)
    {
#if defined(_DEBUG)
        // Enable the debug layer (requires the Graphics Tools "optional feature").
        if (validation)
        {
            ID3D12Debug* d3d12debug;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12debug))))
            {
                d3d12debug->EnableDebugLayer();

                ID3D12Debug1* d3d12debug1;
                if (SUCCEEDED(d3d12debug->QueryInterface(&d3d12debug1)))
                {
                    d3d12debug1->SetEnableGPUBasedValidation(true);
                    Release(d3d12debug1);
                }

                Release(d3d12debug);
            }
            else
            {
                ALIMER_LOGWARN("Direct3D Debug Device is not available");
            }

            IDXGIInfoQueue* dxgiInfoQueue;
            if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
            {
                _dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

                dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
                dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

                Release(dxgiInfoQueue);
            }
        }
#endif

        // Create global dxgi factory.
        HRESULT hr = CreateDXGIFactory2(_dxgiFactoryFlags, IID_PPV_ARGS(&_dxgiFactory));
        if (FAILED(hr))
        {
            ALIMER_LOGERROR("Unable to create a DXGI 1.4 device. Make sure that your OS and driver support DirectX 12");
            return nullptr;
        }

        IDXGIFactory5* factory5;
        if (SUCCEEDED(_dxgiFactory->QueryInterface(&factory5)))
        {
            BOOL allowTearing = FALSE;
            hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
            if (SUCCEEDED(hr) && allowTearing)
            {
                _dxgiAllowTearing = TRUE;
            }
        }

        AGpuRendererD3D12* renderD3D12 = new AGpuRendererD3D12();
        return renderD3D12;
    }
}
#endif // TODO_D3D12

struct VgpuRendererD3D12 : public VgpuRendererI
{
    VgpuRendererD3D12()  {}
    ~VgpuRendererD3D12() {}

    VgpuResult initialize(const char* applicationName, const VgpuDescriptor* descriptor) override;
    void shutdown() override;
    VgpuBackend getBackend() const { return VGPU_BACKEND_D3D12; }
    VgpuResult beginFrame() override;
    VgpuResult endFrame() override;
    VgpuResult waitIdle() override;

    /* Sampler */
    VgpuSampler createSampler(const VgpuSamplerDescriptor* descriptor) override;
    void destroySampler(VgpuSampler sampler) override;
    //VkSampler getVkSampler(VgpuSampler sampler);

    /* Command Buffer */
    VgpuCommandBuffer createCommandBuffer(const VgpuCommandBufferDescriptor* descriptor) override;
    void cmdBeginDefaultRenderPass(VgpuCommandBuffer commandBuffer, VgpuColor clearColor, float clearDepth, uint8_t clearStencil) override;
    void cmdBeginRenderPass(VgpuCommandBuffer commandBuffer, VgpuFramebuffer framebuffer) override;
    void cmdEndRenderPass(VgpuCommandBuffer commandBuffer) override;
};

VgpuResult VgpuRendererD3D12::initialize(const char* applicationName, const VgpuDescriptor* descriptor) {
    return VGPU_ERROR_GENERIC;
}

void VgpuRendererD3D12::shutdown() {

}

VgpuResult VgpuRendererD3D12::beginFrame() {
    return VGPU_ERROR_GENERIC;
}

VgpuResult VgpuRendererD3D12::endFrame() {
    return VGPU_ERROR_GENERIC;
}

VgpuResult VgpuRendererD3D12::waitIdle() {
    return VGPU_ERROR_GENERIC;
}


/* Sampler */
VgpuSampler VgpuRendererD3D12::createSampler(const VgpuSamplerDescriptor* descriptor) {
    return {};
}

void VgpuRendererD3D12::destroySampler(VgpuSampler sampler) {

}

//VkSampler getVkSampler(VgpuSampler sampler);

/* Command Buffer */
VgpuCommandBuffer VgpuRendererD3D12::createCommandBuffer(const VgpuCommandBufferDescriptor* descriptor) {
    return nullptr;
}

void VgpuRendererD3D12::cmdBeginDefaultRenderPass(VgpuCommandBuffer commandBuffer, VgpuColor clearColor, float clearDepth, uint8_t clearStencil) {

}

void VgpuRendererD3D12::cmdBeginRenderPass(VgpuCommandBuffer commandBuffer, VgpuFramebuffer framebuffer) {

}

void VgpuRendererD3D12::cmdEndRenderPass(VgpuCommandBuffer commandBuffer) {

}


VgpuBool32 vgpuIsD3D12Supported(VgpuBool32 headless)
{
    return false; // d3d12::isSupported();
}

VgpuRendererI* vgpuCreateD3D12Backend()
{
    return new VgpuRendererD3D12();
}

#endif // VGPU_D3D12
