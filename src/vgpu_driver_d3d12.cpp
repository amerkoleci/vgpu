// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGPU_D3D12_DRIVER)

#include "vgpu_driver.h"

#include <directx/dxgiformat.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
//#include <windows.ui.xaml.media.dxinterop.h>
#include <wrl/client.h>
#include <unordered_map>
#include <vector>
#include <deque>
#include <mutex>
#include <sstream>

#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#include "D3D12MemAlloc.h"

#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   include <dxgidebug.h>
#endif

#ifdef USING_D3D12_AGILITY_SDK
extern "C"
{
    // Used to enable the "Agility SDK" components
    __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
    __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\";
}
#endif

#define VALID_COMPUTE_QUEUE_RESOURCE_STATES \
    ( D3D12_RESOURCE_STATE_UNORDERED_ACCESS \
    | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE \
    | D3D12_RESOURCE_STATE_COPY_DEST \
    | D3D12_RESOURCE_STATE_COPY_SOURCE )

using Microsoft::WRL::ComPtr;

#define VHR(hr) if (FAILED(hr)) { VGPU_ASSERT(0); }
#define SAFE_RELEASE(obj) if ((obj)) { (obj)->Release(); (obj) = nullptr; }

namespace
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    using PFN_CREATE_DXGI_FACTORY2 = decltype(&CreateDXGIFactory2);
    static PFN_CREATE_DXGI_FACTORY2 vgfxCreateDXGIFactory2 = nullptr;

    static PFN_D3D12_GET_DEBUG_INTERFACE vgfxD3D12GetDebugInterface = nullptr;
    static PFN_D3D12_CREATE_DEVICE vgfxD3D12CreateDevice = nullptr;
    static PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE vgfxD3D12SerializeVersionedRootSignature = nullptr;

#if defined(_DEBUG)
    // Declare debug guids to avoid linking with "dxguid.lib"
    static constexpr IID VGFX_DXGI_DEBUG_ALL = { 0xe48ae283, 0xda80, 0x490b, {0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8} };
    static constexpr IID VGFX_DXGI_DEBUG_DXGI = { 0x25cddaa4, 0xb1c6, 0x47e1, {0xac, 0x3e, 0x98, 0x87, 0x5b, 0x5a, 0x2e, 0x2a} };

    using PFN_DXGI_GET_DEBUG_INTERFACE1 = decltype(&DXGIGetDebugInterface1);
    static PFN_DXGI_GET_DEBUG_INTERFACE1 vgpuDXGIGetDebugInterface1 = nullptr;
#endif
#endif


    inline std::string WCharToUTF8(const wchar_t* input)
    {
        // The -1 argument asks WideCharToMultiByte to use the null terminator to know the size of
        // input. It will return a size that includes the null terminator.
        const int requiredSize = WideCharToMultiByte(CP_UTF8, 0, input, -1, nullptr, 0, nullptr, nullptr);
        if (requiredSize < 0)
            return "";

        char* char_buffer = (char*)alloca(sizeof(char) * requiredSize);
        WideCharToMultiByte(CP_UTF8, 0, input, -1, char_buffer, requiredSize, nullptr, nullptr);
        return std::string(char_buffer, requiredSize);
    }

    inline std::wstring UTF8ToWStr(const char* input)
    {
        // The -1 argument asks MultiByteToWideChar to use the null terminator to know the size of
        // input. It will return a size that includes the null terminator.
        const int requiredSize = MultiByteToWideChar(CP_UTF8, 0, input, -1, nullptr, 0);
        if (requiredSize < 0)
            return L"";

        wchar_t* char_buffer = (wchar_t*)alloca(sizeof(wchar_t) * requiredSize);
        MultiByteToWideChar(CP_UTF8, 0, input, -1, char_buffer, requiredSize);
        return std::wstring(char_buffer, requiredSize);
    }

    inline void D3D12SetName(ID3D12DeviceChild* obj, const char* name)
    {
        if (obj)
        {
            if (name != nullptr)
            {
                std::wstring wide_name = UTF8ToWStr(name);
                obj->SetName(wide_name.c_str());
            }
            else
            {
                obj->SetName(nullptr);
            }
        }
    }

    static_assert(sizeof(VGPUViewport) == sizeof(D3D12_VIEWPORT));
    static_assert(offsetof(VGPUViewport, x) == offsetof(D3D12_VIEWPORT, TopLeftX));
    static_assert(offsetof(VGPUViewport, y) == offsetof(D3D12_VIEWPORT, TopLeftY));
    static_assert(offsetof(VGPUViewport, width) == offsetof(D3D12_VIEWPORT, Width));
    static_assert(offsetof(VGPUViewport, height) == offsetof(D3D12_VIEWPORT, Height));
    static_assert(offsetof(VGPUViewport, minDepth) == offsetof(D3D12_VIEWPORT, MinDepth));
    static_assert(offsetof(VGPUViewport, maxDepth) == offsetof(D3D12_VIEWPORT, MaxDepth));

    static_assert(sizeof(VGPUDispatchIndirectCommand) == sizeof(D3D12_DISPATCH_ARGUMENTS), "DispatchIndirectCommand mismatch");
    static_assert(offsetof(VGPUDispatchIndirectCommand, x) == offsetof(D3D12_DISPATCH_ARGUMENTS, ThreadGroupCountX), "Layout mismatch");
    static_assert(offsetof(VGPUDispatchIndirectCommand, y) == offsetof(D3D12_DISPATCH_ARGUMENTS, ThreadGroupCountY), "Layout mismatch");
    static_assert(offsetof(VGPUDispatchIndirectCommand, z) == offsetof(D3D12_DISPATCH_ARGUMENTS, ThreadGroupCountZ), "Layout mismatch");

    static_assert(sizeof(VGPUDrawIndirectCommand) == sizeof(D3D12_DRAW_ARGUMENTS), "DrawIndirectCommand mismatch");
    static_assert(offsetof(VGPUDrawIndirectCommand, vertexCount) == offsetof(D3D12_DRAW_ARGUMENTS, VertexCountPerInstance), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndirectCommand, instanceCount) == offsetof(D3D12_DRAW_ARGUMENTS, InstanceCount), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndirectCommand, vertexStart) == offsetof(D3D12_DRAW_ARGUMENTS, StartVertexLocation), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndirectCommand, baseInstance) == offsetof(D3D12_DRAW_ARGUMENTS, StartInstanceLocation), "Layout mismatch");

    static_assert(sizeof(VGPUDrawIndexedIndirectCommand) == sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), "DrawIndexedIndirectCommand mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, indexCount) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, IndexCountPerInstance), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, instanceCount) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, InstanceCount), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, startIndex) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, StartIndexLocation), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, baseVertex) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, BaseVertexLocation), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, baseInstance) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, StartInstanceLocation), "Layout mismatch");

    constexpr DXGI_FORMAT ToDXGIFormat(VGPUTextureFormat format)
    {
        switch (format)
        {
            // 8-bit formats
            case VGPUTextureFormat_R8UNorm:         return DXGI_FORMAT_R8_UNORM;
            case VGPUTextureFormat_R8SNorm:         return DXGI_FORMAT_R8_SNORM;
            case VGPUTextureFormat_R8UInt:          return DXGI_FORMAT_R8_UINT;
            case VGPUTextureFormat_R8SInt:          return DXGI_FORMAT_R8_SINT;
                // 16-bit formats
            case VGPUTextureFormat_R16UInt:         return DXGI_FORMAT_R16_UINT;
            case VGPUTextureFormat_R16SInt:         return DXGI_FORMAT_R16_SINT;
            case VGPUTextureFormat_R16UNorm:        return DXGI_FORMAT_R16_UNORM;
            case VGPUTextureFormat_R16SNorm:        return DXGI_FORMAT_R16_SNORM;
            case VGPUTextureFormat_R16Float:        return DXGI_FORMAT_R16_FLOAT;
            case VGPUTextureFormat_RG8UNorm:        return DXGI_FORMAT_R8G8_UNORM;
            case VGPUTextureFormat_RG8SNorm:        return DXGI_FORMAT_R8G8_SNORM;
            case VGPUTextureFormat_RG8UInt:         return DXGI_FORMAT_R8G8_UINT;
            case VGPUTextureFormat_RG8SInt:         return DXGI_FORMAT_R8G8_SINT;
                // Packed 16-Bit Pixel Formats
            case VGPUTextureFormat_BGRA4UNorm:      return DXGI_FORMAT_B4G4R4A4_UNORM;
            case VGPUTextureFormat_B5G6R5UNorm:     return DXGI_FORMAT_B5G6R5_UNORM;
            case VGPUTextureFormat_B5G5R5A1UNorm:   return DXGI_FORMAT_B5G5R5A1_UNORM;
                // 32-bit formats
            case VGPUTextureFormat_R32UInt:          return DXGI_FORMAT_R32_UINT;
            case VGPUTextureFormat_R32SInt:          return DXGI_FORMAT_R32_SINT;
            case VGPUTextureFormat_R32Float:         return DXGI_FORMAT_R32_FLOAT;
            case VGPUTextureFormat_RG16UInt:         return DXGI_FORMAT_R16G16_UINT;
            case VGPUTextureFormat_RG16SInt:         return DXGI_FORMAT_R16G16_SINT;
            case VGPUTextureFormat_RG16UNorm:        return DXGI_FORMAT_R16G16_UNORM;
            case VGPUTextureFormat_RG16SNorm:        return DXGI_FORMAT_R16G16_SNORM;
            case VGPUTextureFormat_RG16Float:        return DXGI_FORMAT_R16G16_FLOAT;
            case VGPUTextureFormat_RGBA8UInt:        return DXGI_FORMAT_R8G8B8A8_UINT;
            case VGPUTextureFormat_RGBA8SInt:        return DXGI_FORMAT_R8G8B8A8_SINT;
            case VGPUTextureFormat_RGBA8UNorm:       return DXGI_FORMAT_R8G8B8A8_UNORM;
            case VGPUTextureFormat_RGBA8UNormSrgb:   return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case VGPUTextureFormat_RGBA8SNorm:       return DXGI_FORMAT_R8G8B8A8_SNORM;
            case VGPUTextureFormat_BGRA8UNorm:       return DXGI_FORMAT_B8G8R8A8_UNORM;
            case VGPUTextureFormat_BGRA8UNormSrgb:   return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                // Packed 32-Bit formats
            case VGPUTextureFormat_RGB10A2UNorm:     return DXGI_FORMAT_R10G10B10A2_UNORM;
            case VGPUTextureFormat_RG11B10Float:     return DXGI_FORMAT_R11G11B10_FLOAT;
            case VGPUTextureFormat_RGB9E5Float:      return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
                // 64-Bit formats
            case VGPUTextureFormat_RG32UInt:         return DXGI_FORMAT_R32G32_UINT;
            case VGPUTextureFormat_RG32SInt:         return DXGI_FORMAT_R32G32_SINT;
            case VGPUTextureFormat_RG32Float:        return DXGI_FORMAT_R32G32_FLOAT;
            case VGPUTextureFormat_RGBA16UInt:       return DXGI_FORMAT_R16G16B16A16_UINT;
            case VGPUTextureFormat_RGBA16SInt:       return DXGI_FORMAT_R16G16B16A16_SINT;
            case VGPUTextureFormat_RGBA16UNorm:      return DXGI_FORMAT_R16G16B16A16_UNORM;
            case VGPUTextureFormat_RGBA16SNorm:      return DXGI_FORMAT_R16G16B16A16_SNORM;
            case VGPUTextureFormat_RGBA16Float:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
                // 128-Bit formats
            case VGPUTextureFormat_RGBA32UInt:       return DXGI_FORMAT_R32G32B32A32_UINT;
            case VGPUTextureFormat_RGBA32SInt:       return DXGI_FORMAT_R32G32B32A32_SINT;
            case VGPUTextureFormat_RGBA32Float:      return DXGI_FORMAT_R32G32B32A32_FLOAT;
                // Depth-stencil formats
            case VGPUTextureFormat_Depth16UNorm:		    return DXGI_FORMAT_D16_UNORM;
            case VGPUTextureFormat_Depth24UNormStencil8:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case VGPUTextureFormat_Depth32Float:			return DXGI_FORMAT_D32_FLOAT;
            case VGPUTextureFormat_Depth32FloatStencil8:    return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
                // Compressed BC formats
            case VGPUTextureFormat_BC1UNorm:            return DXGI_FORMAT_BC1_UNORM;
            case VGPUTextureFormat_BC1UNormSrgb:        return DXGI_FORMAT_BC1_UNORM_SRGB;
            case VGPUTextureFormat_BC2UNorm:            return DXGI_FORMAT_BC2_UNORM;
            case VGPUTextureFormat_BC2UNormSrgb:        return DXGI_FORMAT_BC2_UNORM_SRGB;
            case VGPUTextureFormat_BC3UNorm:            return DXGI_FORMAT_BC3_UNORM;
            case VGPUTextureFormat_BC3UNormSrgb:        return DXGI_FORMAT_BC3_UNORM_SRGB;
            case VGPUTextureFormat_BC4SNorm:            return DXGI_FORMAT_BC4_SNORM;
            case VGPUTextureFormat_BC4UNorm:            return DXGI_FORMAT_BC4_UNORM;
            case VGPUTextureFormat_BC5SNorm:            return DXGI_FORMAT_BC5_SNORM;
            case VGPUTextureFormat_BC5UNorm:            return DXGI_FORMAT_BC5_UNORM;
            case VGPUTextureFormat_BC6HUFloat:          return DXGI_FORMAT_BC6H_UF16;
            case VGPUTextureFormat_BC6HSFloat:          return DXGI_FORMAT_BC6H_SF16;
            case VGPUTextureFormat_BC7UNorm:            return DXGI_FORMAT_BC7_UNORM;
            case VGPUTextureFormat_BC7UNormSrgb:        return DXGI_FORMAT_BC7_UNORM_SRGB;

            default:
                return DXGI_FORMAT_UNKNOWN;
        }
    }

    constexpr VGPUTextureFormat ToDXGISwapChainFormat(VGPUTextureFormat format)
    {
        switch (format)
        {
            case VGPUTextureFormat_RGBA16Float:
                return VGPUTextureFormat_RGBA16Float;

            case VGPUTextureFormat_BGRA8UNorm:
            case VGPUTextureFormat_BGRA8UNormSrgb:
                return VGPUTextureFormat_BGRA8UNorm;

            case VGPUTextureFormat_RGBA8UNorm:
            case VGPUTextureFormat_RGBA8UNormSrgb:
                return VGPUTextureFormat_RGBA8UNorm;

            case VGPUTextureFormat_RGB10A2UNorm:
                return VGPUTextureFormat_RGB10A2UNorm;
        }

        return VGPUTextureFormat_BGRA8UNorm;
    }

    constexpr DXGI_FORMAT GetTypelessFormatFromDepthFormat(VGPUTextureFormat format)
    {
        switch (format)
        {
            case VGPUTextureFormat_Depth16UNorm:
                return DXGI_FORMAT_R16_TYPELESS;
            case VGPUTextureFormat_Depth32Float:
                return DXGI_FORMAT_R32_TYPELESS;
            case VGPUTextureFormat_Depth24UNormStencil8:
                return DXGI_FORMAT_R24G8_TYPELESS;
            case VGPUTextureFormat_Depth32FloatStencil8:
                return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

            default:
                VGPU_ASSERT(vgpuIsDepthFormat(format) == false);
                return ToDXGIFormat(format);
        }
    }

    constexpr uint32_t PresentModeToBufferCount(VGPUPresentMode mode)
    {
        switch (mode)
        {
            case VGPUPresentMode_Immediate:
            case VGPUPresentMode_Fifo:
                return 2;
            case VGPUPresentMode_Mailbox:
                return 3;
            default:
                return 2;
        }
    }

    constexpr uint32_t PresentModeToSwapInterval(VGPUPresentMode mode)
    {
        switch (mode)
        {
            case VGPUPresentMode_Immediate:
            case VGPUPresentMode_Mailbox:
                return 0u;

            case VGPUPresentMode_Fifo:
            default:
                return 1u;
        }
    }

    constexpr D3D12_COMPARISON_FUNC ToD3D12ComparisonFunc(VGPUCompareFunction function)
    {
        switch (function)
        {
            case VGPUCompareFunction_Never:        return D3D12_COMPARISON_FUNC_NEVER;
            case VGPUCompareFunction_Less:         return D3D12_COMPARISON_FUNC_LESS;
            case VGPUCompareFunction_Equal:        return D3D12_COMPARISON_FUNC_EQUAL;
            case VGPUCompareFunction_LessEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
            case VGPUCompareFunction_Greater:      return D3D12_COMPARISON_FUNC_GREATER;
            case VGPUCompareFunction_NotEqual:     return D3D12_COMPARISON_FUNC_NOT_EQUAL;
            case VGPUCompareFunction_GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            case VGPUCompareFunction_Always:       return D3D12_COMPARISON_FUNC_ALWAYS;

            default:
                return D3D12_COMPARISON_FUNC_NEVER;
        }
    }

    constexpr D3D12_FILTER_TYPE ToD3D12FilterType(VGPUSamplerFilter value)
    {
        switch (value)
        {
            case VGPUSamplerFilter_Linear:
                return D3D12_FILTER_TYPE_LINEAR;
            default:
            case VGPUSamplerFilter_Nearest:
                return D3D12_FILTER_TYPE_POINT;
        }
    }

    constexpr D3D12_FILTER_TYPE ToD3D12FilterType(VGPUSamplerMipFilter value)
    {
        switch (value)
        {
            case VGPUSamplerMipFilter_Linear:
                return D3D12_FILTER_TYPE_LINEAR;
            default:
            case VGPUSamplerMipFilter_Nearest:
                return D3D12_FILTER_TYPE_POINT;
        }
    }

    constexpr D3D12_TEXTURE_ADDRESS_MODE ToD3D12AddressMode(VGPUSamplerAddressMode mode)
    {
        switch (mode)
        {
            case VGPUSamplerAddressMode_Mirror:
                return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            case VGPUSamplerAddressMode_Clamp:
                return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            case VGPUSamplerAddressMode_Border:
                return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
                //case VGPUSamplerAddressMode_MirrorOnce:
                //    return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;

            default:
            case VGPUSamplerAddressMode_Wrap:
                return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        }
    }

}

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define vgfxCreateDXGIFactory2 CreateDXGIFactory2
#define vgfxD3D12CreateDevice D3D12CreateDevice
#define vgfxD3D12GetDebugInterface D3D12GetDebugInterface
#endif

struct VGFXD3D12DescriptorAllocator
{
    ID3D12Device* device = nullptr;
    std::mutex locker;
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    std::vector<ID3D12DescriptorHeap*> heaps;
    uint32_t descriptorSize = 0;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> freelist;

    void Init(ID3D12Device* device_, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptorsPerBlock)
    {
        device = device_;

        desc.Type = type;
        desc.NumDescriptors = numDescriptorsPerBlock;
        descriptorSize = device->GetDescriptorHandleIncrementSize(type);
    }

    void Shutdown()
    {
        for (auto heap : heaps)
        {
            heap->Release();
        }
        heaps.clear();
    }

    void BlockAllocate()
    {
        heaps.emplace_back();
        HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heaps.back()));
        VGPU_ASSERT(SUCCEEDED(hr));
        D3D12_CPU_DESCRIPTOR_HANDLE heap_start = heaps.back()->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < desc.NumDescriptors; ++i)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heap_start;
            handle.ptr += i * descriptorSize;
            freelist.push_back(handle);
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Allocate()
    {
        locker.lock();
        if (freelist.empty())
        {
            BlockAllocate();
        }
        VGPU_ASSERT(!freelist.empty());
        D3D12_CPU_DESCRIPTOR_HANDLE handle = freelist.back();
        freelist.pop_back();
        locker.unlock();
        return handle;
    }

    void Free(D3D12_CPU_DESCRIPTOR_HANDLE index)
    {
        locker.lock();
        freelist.push_back(index);
        locker.unlock();
    }
};

struct D3D12_Renderer;

struct D3D12Resource
{
    ID3D12Resource* handle = nullptr;
    D3D12MA::Allocation* allocation = nullptr;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES transitioningState = (D3D12_RESOURCE_STATES)-1;
    D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress = 0u;

    uint32_t width = 0;
    uint32_t height = 0;
    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    uint32_t size = 0;
    uint64_t allocatedSize = 0;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = {};
    void* pMappedData{ nullptr };
    std::unordered_map<size_t, D3D12_CPU_DESCRIPTOR_HANDLE> rtvCache;
    std::unordered_map<size_t, D3D12_CPU_DESCRIPTOR_HANDLE> dsvCache;
};

struct D3D12Sampler
{
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor;
};

struct D3D12Shader
{
    std::vector<uint8_t> byteCode;
};

struct D3D12Pipeline
{
    ID3D12PipelineState* handle = nullptr;
    D3D_PRIMITIVE_TOPOLOGY primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
};

struct D3D12_SwapChain
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HWND window = nullptr;
#else
    IUnknown* window = nullptr;
#endif
    IDXGISwapChain3* handle = nullptr;
    VGPUTextureFormat format;
    VGPUTextureFormat textureFormat;
    uint32_t backBufferCount;
    uint32_t syncInterval;
    std::vector<D3D12Resource*> backbufferTextures;
};

struct D3D12_UploadContext
{
    ID3D12CommandAllocator* commandAllocator = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12Fence* fence = nullptr;

    uint32_t uploadBufferSize = 0;
    ID3D12Resource* uploadBuffer = nullptr;
    D3D12MA::Allocation* uploadBufferAllocation = nullptr;
    void* uploadBufferData = nullptr;
};

struct D3D12CommandBuffer
{
    D3D12_Renderer* renderer;
    D3D12_COMMAND_LIST_TYPE type;
    bool hasLabel;

    ID3D12CommandAllocator* commandAllocators[VGPU_MAX_INFLIGHT_FRAMES];
    ID3D12GraphicsCommandList4* commandList;

    D3D12_RESOURCE_BARRIER resourceBarriers[16];
    UINT numBarriersToFlush;

    bool insideRenderPass;

    D3D12_VERTEX_BUFFER_VIEW vboViews[VGPU_MAX_VERTEX_BUFFERS] = {};

    D3D12_RENDER_PASS_RENDER_TARGET_DESC RTVs[VGPU_MAX_COLOR_ATTACHMENTS] = {};
    // Due to a API bug, this resolve_subresources array must be kept alive between BeginRenderpass() and EndRenderpass()!
    D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS resolveSubresources[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};


    std::vector<D3D12_SwapChain*> swapChains;
};

struct D3D12_Renderer
{
    ComPtr<IDXGIFactory4> factory;
    bool tearingSupported = false;
    ID3D12Device5* device = nullptr;
    D3D_FEATURE_LEVEL featureLevel{};

    uint32_t vendorID;
    uint32_t deviceID;
    std::string adapterName;
    std::string driverDescription;
    VGPUAdapterType adapterType;

    ID3D12Fence* frameFence = nullptr;
    HANDLE frameFenceEvent = INVALID_HANDLE_VALUE;

    D3D12MA::Allocator* allocator = nullptr;
    CD3DX12FeatureSupport d3dFeatures;
    ID3D12CommandQueue* graphicsQueue = nullptr;

    /* Command contexts */
    std::mutex cmdBuffersLocker;
    uint32_t cmdBuffersCount{ 0 };
    std::vector<VGPUCommandBuffer_T*> commandBuffers;

    uint32_t frameIndex = 0;
    uint64_t frameCount = 0;
    uint64_t GPUFrameCount = 0;

    ID3D12CommandQueue* copyQueue = nullptr;
    std::mutex uploadLocker;
    std::vector<D3D12_UploadContext> uploadFreeList;

    VGFXD3D12DescriptorAllocator resourceAllocator;
    VGFXD3D12DescriptorAllocator samplerAllocator;
    VGFXD3D12DescriptorAllocator rtvAllocator;
    VGFXD3D12DescriptorAllocator dsvAllocator;

    ID3D12CommandSignature* dispatchIndirectCommandSignature = nullptr;
    ID3D12CommandSignature* drawIndirectCommandSignature = nullptr;
    ID3D12CommandSignature* drawIndexedIndirectCommandSignature = nullptr;
    ID3D12CommandSignature* dispatchMeshIndirectCommandSignature = nullptr;

    bool shuttingDown = false;
    std::mutex destroyMutex;
    std::deque<std::pair<D3D12MA::Allocation*, uint64_t>> deferredAllocations;
    std::deque<std::pair<IUnknown*, uint64_t>> deferredReleases;
};

static void d3d12_DeferDestroy(D3D12_Renderer* renderer, IUnknown* resource, D3D12MA::Allocation* allocation)
{
    if (resource == nullptr)
    {
        return;
    }

    renderer->destroyMutex.lock();

    if ((renderer->frameCount == renderer->GPUFrameCount)
        || renderer->shuttingDown
        || renderer->device == nullptr)
    {
        resource->Release();
        SAFE_RELEASE(allocation);

        renderer->destroyMutex.unlock();
        return;
    }

    renderer->deferredReleases.push_back(std::make_pair(resource, renderer->frameCount));
    if (allocation != nullptr)
    {
        renderer->deferredAllocations.push_back(std::make_pair(allocation, renderer->frameCount));
    }
    renderer->destroyMutex.unlock();
}

static void d3d12_ProcessDeletionQueue(D3D12_Renderer* renderer)
{
    renderer->destroyMutex.lock();

    while (!renderer->deferredAllocations.empty())
    {
        if (renderer->deferredAllocations.front().second + VGPU_MAX_INFLIGHT_FRAMES < renderer->frameCount)
        {
            auto item = renderer->deferredAllocations.front();
            renderer->deferredAllocations.pop_front();
            item.first->Release();
        }
        else
        {
            break;
        }
    }

    while (!renderer->deferredReleases.empty())
    {
        if (renderer->deferredReleases.front().second + VGPU_MAX_INFLIGHT_FRAMES < renderer->frameCount)
        {
            auto item = renderer->deferredReleases.front();
            renderer->deferredReleases.pop_front();
            item.first->Release();
        }
        else
        {
            break;
        }
    }

    renderer->destroyMutex.unlock();
}

static D3D12_UploadContext d3d12_AllocateUpload(D3D12_Renderer* renderer, uint32_t size)
{
    renderer->uploadLocker.lock();

    HRESULT hr = S_OK;

    // Create a new command list if there are no free ones.
    if (renderer->uploadFreeList.empty())
    {
        D3D12_UploadContext context;

        hr = renderer->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&context.commandAllocator));
        VGPU_ASSERT(SUCCEEDED(hr));
        hr = renderer->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, context.commandAllocator, nullptr, IID_PPV_ARGS(&context.commandList));
        VGPU_ASSERT(SUCCEEDED(hr));
        hr = context.commandList->Close();
        VGPU_ASSERT(SUCCEEDED(hr));
        hr = renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&context.fence));
        VGPU_ASSERT(SUCCEEDED(hr));

        renderer->uploadFreeList.push_back(context);
    }

    D3D12_UploadContext context = renderer->uploadFreeList.back();
    if (context.uploadBufferSize < size)
    {
        // Try to search for a staging buffer that can fit the request:
        for (size_t i = 0; i < renderer->uploadFreeList.size(); ++i)
        {
            if (renderer->uploadFreeList[i].uploadBufferSize >= size)
            {
                context = renderer->uploadFreeList[i];
                std::swap(renderer->uploadFreeList[i], renderer->uploadFreeList.back());
                break;
            }
        }
    }
    renderer->uploadFreeList.pop_back();
    renderer->uploadLocker.unlock();

    // If no buffer was found that fits the data, create one:
    if (context.uploadBufferSize < size)
    {
        // Release old one first
        d3d12_DeferDestroy(renderer, context.uploadBuffer, context.uploadBufferAllocation);

        uint64_t newSize = vgfxNextPowerOfTwo(size);

        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC resourceDesc;
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = newSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        hr = renderer->allocator->CreateResource(
            &allocationDesc,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            &context.uploadBufferAllocation,
            IID_PPV_ARGS(&context.uploadBuffer)
        );
        VGPU_ASSERT(SUCCEEDED(hr));

        D3D12_RANGE readRange = {};
        hr = context.uploadBuffer->Map(0, &readRange, &context.uploadBufferData);
        VGPU_ASSERT(SUCCEEDED(hr));
    }

    // Begin command list in valid state
    hr = context.commandAllocator->Reset();
    VGPU_ASSERT(SUCCEEDED(hr));
    hr = context.commandList->Reset(context.commandAllocator, nullptr);
    VGPU_ASSERT(SUCCEEDED(hr));

    return context;
}

static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_GetRTV(D3D12_Renderer* renderer, D3D12Resource* texture, uint32_t mipLevel, uint32_t slice)
{
    size_t hash = 0;
    //hash_combine(hash, format);
    hash_combine(hash, mipLevel);
    hash_combine(hash, slice);

    auto it = texture->rtvCache.find(hash);
    if (it == texture->rtvCache.end())
    {
        const D3D12_RESOURCE_DESC& resourceDesc = texture->handle->GetDesc();

        D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};
        viewDesc.Format = texture->dxgiFormat;

        switch (resourceDesc.Dimension)
        {
            case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            {
                if (resourceDesc.DepthOrArraySize > 1)
                {
                    viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                    viewDesc.Texture1DArray.MipSlice = mipLevel;
                    viewDesc.Texture1DArray.FirstArraySlice = slice;
                    viewDesc.Texture1DArray.ArraySize = 1;
                }
                else
                {
                    viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1D.MipSlice = mipLevel;
                }
            }
            break;

            case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            {
                if (resourceDesc.DepthOrArraySize > 1)
                {
                    if (resourceDesc.SampleDesc.Count > 1)
                    {
                        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                        viewDesc.Texture2DMSArray.FirstArraySlice = slice;
                        viewDesc.Texture2DMSArray.ArraySize = 1;
                    }
                    else
                    {
                        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                        viewDesc.Texture2DArray.MipSlice = mipLevel;
                        viewDesc.Texture2DArray.FirstArraySlice = slice;
                        viewDesc.Texture2DArray.ArraySize = 1;
                    }
                }
                else
                {
                    if (resourceDesc.SampleDesc.Count > 1)
                    {
                        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
                    }
                    else
                    {
                        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                        viewDesc.Texture2D.MipSlice = mipLevel;
                    }
                }
            }
            break;

            case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            {
                viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.MipSlice = mipLevel;
                viewDesc.Texture3D.FirstWSlice = slice;
                viewDesc.Texture3D.WSize = -1;
                break;
            }
            break;

            default:
                vgpuLogError("D3D12: Invalid texture dimension");
                return {};
        }

        D3D12_CPU_DESCRIPTOR_HANDLE newView = renderer->rtvAllocator.Allocate();
        renderer->device->CreateRenderTargetView(texture->handle, &viewDesc, newView);
        texture->rtvCache[hash] = newView;
        return newView;
    }

    return it->second;
}

static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_GetDSV(D3D12_Renderer* renderer, D3D12Resource* texture, uint32_t mipLevel, uint32_t slice)
{
    size_t hash = 0;
    hash_combine(hash, mipLevel);
    hash_combine(hash, slice);
    //hash_combine(hash, format);

    auto it = texture->dsvCache.find(hash);
    if (it == texture->dsvCache.end())
    {
        const D3D12_RESOURCE_DESC& resourceDesc = texture->handle->GetDesc();

        D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
        viewDesc.Format = texture->dxgiFormat;

        switch (resourceDesc.Dimension)
        {
            case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            {
                if (resourceDesc.DepthOrArraySize > 1)
                {
                    viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
                    viewDesc.Texture1DArray.MipSlice = mipLevel;
                    viewDesc.Texture1DArray.FirstArraySlice = slice;
                    viewDesc.Texture1DArray.ArraySize = 1;
                }
                else
                {
                    viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1D.MipSlice = mipLevel;
                }
            }
            break;

            case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            {
                if (resourceDesc.DepthOrArraySize > 1)
                {
                    if (resourceDesc.SampleDesc.Count > 1)
                    {
                        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                        viewDesc.Texture2DMSArray.FirstArraySlice = slice;
                        viewDesc.Texture2DMSArray.ArraySize = 1;
                    }
                    else
                    {
                        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                        viewDesc.Texture2DArray.MipSlice = mipLevel;
                        viewDesc.Texture2DArray.FirstArraySlice = slice;
                        viewDesc.Texture2DArray.ArraySize = 1;
                    }
                }
                else
                {
                    if (resourceDesc.SampleDesc.Count > 1)
                    {
                        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
                    }
                    else
                    {
                        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                        viewDesc.Texture2D.MipSlice = mipLevel;
                    }
                }
            }
            break;

            case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
                vgpuLogError("D3D12: Cannot create 3D texture DSV");
                return {};

            default:
                vgpuLogError("D3D12: Invalid texture dimension");
                return {};
        }

        D3D12_CPU_DESCRIPTOR_HANDLE newView = renderer->dsvAllocator.Allocate();
        renderer->device->CreateDepthStencilView(texture->handle, &viewDesc, newView);
        texture->dsvCache[hash] = newView;
        return newView;
    }

    return it->second;
}

static void d3d12_UploadSubmit(D3D12_Renderer* renderer, D3D12_UploadContext context)
{
    HRESULT hr = context.commandList->Close();
    VGPU_ASSERT(SUCCEEDED(hr));

    ID3D12CommandList* commandlists[] = {
        context.commandList
    };
    renderer->copyQueue->ExecuteCommandLists(1, commandlists);

    hr = renderer->copyQueue->Signal(context.fence, 1);
    VGPU_ASSERT(SUCCEEDED(hr));
    hr = context.fence->SetEventOnCompletion(1, nullptr);
    VGPU_ASSERT(SUCCEEDED(hr));
    hr = context.fence->Signal(0);
    VGPU_ASSERT(SUCCEEDED(hr));

    renderer->uploadLocker.lock();
    renderer->uploadFreeList.push_back(context);
    renderer->uploadLocker.unlock();
}

static void d3d12_destroyDevice(VGPUDevice device)
{
    // Wait idle
    vgpuWaitIdle(device);

    D3D12_Renderer* renderer = (D3D12_Renderer*)device->driverData;

    VGPU_ASSERT(renderer->frameCount == renderer->GPUFrameCount);
    renderer->shuttingDown = true;

    renderer->frameCount = UINT64_MAX;
    d3d12_ProcessDeletionQueue(renderer);
    renderer->frameCount = 0;

    // Frame fence
    SAFE_RELEASE(renderer->frameFence);
    CloseHandle(renderer->frameFenceEvent);

    // Copy queue + allocations
    {
        ComPtr<ID3D12Fence> fence;
        HRESULT hr = renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        VGPU_ASSERT(SUCCEEDED(hr));

        hr = renderer->copyQueue->Signal(fence.Get(), 1);
        VGPU_ASSERT(SUCCEEDED(hr));
        hr = fence->SetEventOnCompletion(1, nullptr);
        VGPU_ASSERT(SUCCEEDED(hr));

        for (auto& item : renderer->uploadFreeList)
        {
            item.uploadBuffer->Release();
            item.uploadBufferAllocation->Release();
            item.commandAllocator->Release();
            item.commandList->Release();
            item.fence->Release();
        }
        renderer->uploadFreeList.clear();

        SAFE_RELEASE(renderer->copyQueue);
    }

    // CPU descriptor allocators
    {
        renderer->resourceAllocator.Shutdown();
        renderer->samplerAllocator.Shutdown();
        renderer->rtvAllocator.Shutdown();
        renderer->dsvAllocator.Shutdown();
    }

    SAFE_RELEASE(renderer->dispatchIndirectCommandSignature);
    SAFE_RELEASE(renderer->drawIndirectCommandSignature);
    SAFE_RELEASE(renderer->drawIndexedIndirectCommandSignature);
    SAFE_RELEASE(renderer->dispatchMeshIndirectCommandSignature);

    for (size_t i = 0; i < renderer->commandBuffers.size(); ++i)
    {
        D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)renderer->commandBuffers[i]->driverData;
        for (uint32_t i = 0; i < VGPU_MAX_INFLIGHT_FRAMES; ++i)
        {
            SAFE_RELEASE(commandBuffer->commandAllocators[i]);
        }
        SAFE_RELEASE(commandBuffer->commandList);
    }
    renderer->commandBuffers.clear();
    SAFE_RELEASE(renderer->graphicsQueue);

    // Allocator.
    if (renderer->allocator != nullptr)
    {
        D3D12MA::TotalStatistics stats;
        renderer->allocator->CalculateStatistics(&stats);
        if (stats.Total.Stats.AllocationBytes > 0)
        {
            //LOGI("Total device memory leaked: {} bytes.", stats.Total.UsedBytes);
        }

        SAFE_RELEASE(renderer->allocator);
    }

    if (renderer->device)
    {
        const ULONG refCount = renderer->device->Release();

#if defined(_DEBUG)
        if (refCount)
        {
            //DebugString("There are %d unreleased references left on the D3D device!", ref_count);

            ComPtr<ID3D12DebugDevice> debugDevice = nullptr;
            if (SUCCEEDED(renderer->device->QueryInterface(debugDevice.GetAddressOf())))
            {
                debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
            }

        }
#else
        (void)refCount; // avoid warning
#endif
        renderer->device = nullptr;
    }

    renderer->factory.Reset();

#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    {
        ComPtr<IDXGIDebug1> dxgiDebug;
        if (vgpuDXGIGetDebugInterface1 != nullptr
            && SUCCEEDED(vgpuDXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            dxgiDebug->ReportLiveObjects(VGFX_DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
    }
#endif

    delete renderer;
    VGPU_FREE(device);
}

static void d3d12_updateSwapChain(D3D12_Renderer* renderer, D3D12_SwapChain* swapChain)
{
    HRESULT hr = S_OK;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    hr = swapChain->handle->GetDesc1(&swapChainDesc);
    VGPU_ASSERT(SUCCEEDED(hr));

    swapChain->backbufferTextures.resize(swapChainDesc.BufferCount);
    for (uint32_t i = 0; i < swapChainDesc.BufferCount; ++i)
    {
        D3D12Resource* texture = new D3D12Resource();
        texture->state = D3D12_RESOURCE_STATE_PRESENT;
        texture->width = swapChainDesc.Width;
        texture->height = swapChainDesc.Height;
        texture->dxgiFormat = ToDXGIFormat(swapChain->textureFormat);
        hr = swapChain->handle->GetBuffer(i, IID_PPV_ARGS(&texture->handle));
        VGPU_ASSERT(SUCCEEDED(hr));

        wchar_t name[25] = {};
        swprintf_s(name, L"Render target %u", i);
        texture->handle->SetName(name);
        swapChain->backbufferTextures[i] = texture;
    }
}

static uint64_t d3d12_frame(VGFXRenderer* driverData)
{
    HRESULT hr = S_OK;
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    renderer->frameCount++;
    renderer->frameIndex = renderer->frameCount % VGPU_MAX_INFLIGHT_FRAMES;

    // Signal the fence with the current frame number, so that we can check back on it
    hr = renderer->graphicsQueue->Signal(renderer->frameFence, renderer->frameCount);
    if (FAILED(hr))
    {
        vgpuLogError("Failed to signal frame");
        return (uint64_t)(-1);
    }

    // Wait for the GPU to catch up before we stomp an executing command buffer
    const uint64_t gpuLag = renderer->frameCount - renderer->GPUFrameCount;
    VGPU_ASSERT(gpuLag <= VGPU_MAX_INFLIGHT_FRAMES);
    if (gpuLag >= VGPU_MAX_INFLIGHT_FRAMES)
    {
        // Make sure that the previous frame is finished
        const uint64_t signalValue = renderer->GPUFrameCount + 1;
        if (renderer->frameFence->GetCompletedValue() < signalValue)
        {
            renderer->frameFence->SetEventOnCompletion(signalValue, renderer->frameFenceEvent);
            WaitForSingleObjectEx(renderer->frameFenceEvent, INFINITE, FALSE);
        }
        renderer->GPUFrameCount++;
    }

    // Begin new frame
    // Safe delete deferred destroys
    d3d12_ProcessDeletionQueue(renderer);



    // Return current frame
    return renderer->frameCount - 1;
}

static void d3d12_waitIdle(VGFXRenderer* driverData)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    // Wait for the GPU to fully catch up with the CPU
    VGPU_ASSERT(renderer->frameCount >= renderer->GPUFrameCount);
    if (renderer->frameCount > renderer->GPUFrameCount)
    {
        if (renderer->frameFence->GetCompletedValue() < renderer->frameCount)
        {
            renderer->frameFence->SetEventOnCompletion(renderer->frameCount, renderer->frameFenceEvent);
            WaitForSingleObjectEx(renderer->frameFenceEvent, INFINITE, FALSE);
        }

        renderer->GPUFrameCount = renderer->frameCount;
    }

    // Safe delete deferred destroys
    d3d12_ProcessDeletionQueue(renderer);
}

static VGPUBackendType d3d12_getBackendType(void)
{
    return VGPUBackendType_D3D12;
}

static bool d3d12_queryFeature(VGFXRenderer* driverData, VGPUFeature feature, void* pInfo, uint32_t infoSize)
{
    (void)pInfo;
    (void)infoSize;

    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;
    switch (feature)
    {
        case VGPUFeature_TextureCompressionBC:
        case VGPUFeature_ShaderFloat16:
        case VGPUFeature_PipelineStatisticsQuery:
        case VGPUFeature_TimestampQuery:
        case VGPUFeature_DepthClamping:
        case VGPUFeature_Depth24UNormStencil8:
        case VGPUFeature_Depth32FloatStencil8:
        case VGPUFeature_IndependentBlend:
        case VGPUFeature_TextureCubeArray:
        case VGPUFeature_Tessellation:
        case VGPUFeature_DescriptorIndexing:
        case VGPUFeature_ConditionalRendering:
        case VGPUFeature_DrawIndirectFirstInstance:
            return true;

        case VGPUFeature_TextureCompressionETC2:
        case VGPUFeature_TextureCompressionASTC:
            return false;

        case VGPUFeature_ShaderOutputViewportIndex:
            return renderer->d3dFeatures.VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation() == TRUE;

            // https://docs.microsoft.com/en-us/windows/win32/direct3d11/tiled-resources-texture-sampling-features
        case VGPUFeature_SamplerMinMax:
            return renderer->d3dFeatures.TiledResourcesTier() >= D3D12_TILED_RESOURCES_TIER_2;

        case VGPUFeature_MeshShader:
            if (renderer->d3dFeatures.MeshShaderTier() >= D3D12_MESH_SHADER_TIER_1)
            {
                return true;
            }

            return false;

        case VGPUFeature_RayTracing:
            if (renderer->d3dFeatures.RaytracingTier() >= D3D12_RAYTRACING_TIER_1_1)
            {
                if (infoSize == sizeof(uint32_t))
                {
                    auto* pShaderGroupHandleSize = reinterpret_cast<uint32_t*>(pInfo);
                    *pShaderGroupHandleSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
                }

                return true;
            }
            return false;


        default:
            return false;
    }
}

static void d3d12_getAdapterProperties(VGFXRenderer* driverData, VGPUAdapterProperties* properties)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    properties->vendorID = renderer->vendorID;
    properties->deviceID = renderer->deviceID;
    properties->name = renderer->adapterName.c_str();
    properties->driverDescription = renderer->driverDescription.c_str();
    properties->adapterType = renderer->adapterType;
}

static void d3d12_getLimits(VGFXRenderer* driverData, VGPULimits* limits)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    D3D12_FEATURE_DATA_D3D12_OPTIONS featureData = {};
    HRESULT hr = renderer->device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &featureData, sizeof(featureData));
    if (FAILED(hr))
    {
    }

    // Max(CBV+UAV+SRV)         1M    1M    1M+
    // Max CBV per stage        14    14   full
    // Max SRV per stage       128  full   full
    // Max UAV in all stages    64    64   full
    // Max Samplers per stage   16  2048   2048
    // https://docs.microsoft.com/en-us/windows-hardware/test/hlk/testref/efad06e8-51d1-40ce-ad5c-573a134b4bb6
    // "full" means the full heap can be used. This is tested
    // to work for 1 million descriptors, and 1.1M for tier 3.
    uint32_t maxCBVsPerStage;
    uint32_t maxSRVsPerStage;
    uint32_t maxUAVsAllStages;
    uint32_t maxSamplersPerStage;
    switch (featureData.ResourceBindingTier)
    {
        case D3D12_RESOURCE_BINDING_TIER_1:
            maxCBVsPerStage = 14;
            maxSRVsPerStage = 128;
            maxUAVsAllStages = 64;
            maxSamplersPerStage = 16;
            break;
        case D3D12_RESOURCE_BINDING_TIER_2:
            maxCBVsPerStage = 14;
            maxSRVsPerStage = 1'000'000;
            maxUAVsAllStages = 64;
            maxSamplersPerStage = 2048;
            break;
        case D3D12_RESOURCE_BINDING_TIER_3:
        default:
            maxCBVsPerStage = 1'100'000;
            maxSRVsPerStage = 1'100'000;
            maxUAVsAllStages = 1'100'000;
            maxSamplersPerStage = 2048;
            break;
    }

    uint32_t maxUAVsPerStage = maxUAVsAllStages / 2;

    limits->maxTextureDimension1D = D3D12_REQ_TEXTURE1D_U_DIMENSION;
    limits->maxTextureDimension2D = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    limits->maxTextureDimension3D = D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
    limits->maxTextureArrayLayers = D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    limits->maxBindGroups = 0;
    limits->maxDynamicUniformBuffersPerPipelineLayout = 0;
    limits->maxDynamicStorageBuffersPerPipelineLayout = 0;
    limits->maxSampledTexturesPerShaderStage = maxSRVsPerStage;
    limits->maxSamplersPerShaderStage = maxSamplersPerStage;
    limits->maxStorageBuffersPerShaderStage = maxUAVsPerStage - maxUAVsPerStage / 2;
    limits->maxStorageTexturesPerShaderStage = maxUAVsPerStage / 2;
    limits->maxUniformBuffersPerShaderStage = maxCBVsPerStage;
    limits->maxUniformBufferBindingSize = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
    // D3D12 has no documented limit on the size of a storage buffer binding.
    limits->maxStorageBufferBindingSize = 4294967295;
    limits->minUniformBufferOffsetAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    limits->minStorageBufferOffsetAlignment = 32;
    limits->maxVertexBuffers = 16;
    limits->maxVertexAttributes = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
    limits->maxVertexBufferArrayStride = 2048u;
    limits->maxInterStageShaderComponents = D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENTS_COMPONENTS;

    // https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-compute-shaders
    // Thread Group Shared Memory is limited to 16Kb on downlevel hardware. This is less than
    // the 32Kb that is available to Direct3D 11 hardware. D3D12 is also 32kb.
    limits->maxComputeWorkgroupStorageSize = 32768;

    // https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-attributes-numthreads
    limits->maxComputeInvocationsPerWorkGroup = D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;
    limits->maxComputeWorkGroupSizeX = D3D12_CS_THREAD_GROUP_MAX_X;
    limits->maxComputeWorkGroupSizeY = D3D12_CS_THREAD_GROUP_MAX_X;
    limits->maxComputeWorkGroupSizeZ = D3D12_CS_THREAD_GROUP_MAX_X;

    // https://docs.maxComputeWorkgroupSizeXmicrosoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_dispatch_arguments
    limits->maxComputeWorkGroupsPerDimension = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
}

/* Buffer */
static VGPUBuffer d3d12_createBuffer(VGFXRenderer* driverData, const VGPUBufferDesc* desc, const void* pInitialData)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
    if (desc->usage & VGPUBufferUsage_ShaderRead)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    if (!(desc->usage & VGPUBufferUsage_ShaderRead)/*
        && !CheckBitsAny(info.usage, BufferUsage::RayTracing)*/)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }

    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(desc->size, resourceFlags);

    if (desc->usage & VGPUBufferUsage_Constant)
    {
        resourceDesc.Width = AlignUp<UINT64>(resourceDesc.Width, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    }

    D3D12MA::ALLOCATION_DESC allocationDesc = {};
    D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;
    allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
    if (desc->cpuAccess == VGPUCpuAccessMode_Read)
    {
        allocationDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
        resourceState = D3D12_RESOURCE_STATE_COPY_DEST;
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }
    else if (desc->cpuAccess == VGPUCpuAccessMode_Write)
    {
        allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
    }

    D3D12Resource* buffer = new D3D12Resource();
    buffer->state = resourceState;
    buffer->size = desc->size;

    renderer->device->GetCopyableFootprints(&resourceDesc, 0, 1, 0, &buffer->footprint, nullptr, nullptr, &buffer->allocatedSize);

    HRESULT hr = renderer->allocator->CreateResource(
        &allocationDesc,
        &resourceDesc,
        resourceState,
        nullptr,
        &buffer->allocation,
        IID_PPV_ARGS(&buffer->handle)
    );

    if (FAILED(hr))
    {
        vgpuLogError("D3D12: Failed to create buffer");
        delete buffer;
        return nullptr;
    }

    if (desc->label)
    {
        D3D12SetName(buffer->handle, desc->label);
    }

    buffer->gpuAddress = buffer->handle->GetGPUVirtualAddress();

    if (desc->cpuAccess == VGPUCpuAccessMode_Read)
    {
        buffer->handle->Map(0, nullptr, &buffer->pMappedData);
    }
    else if (desc->cpuAccess == VGPUCpuAccessMode_Write)
    {
        D3D12_RANGE readRange = {};
        buffer->handle->Map(0, &readRange, &buffer->pMappedData);
    }

    // Issue data copy.
    if (pInitialData != nullptr)
    {
        if (buffer->pMappedData != nullptr)
        {
            memcpy(buffer->pMappedData, pInitialData, desc->size);
        }
        else
        {
            auto context = d3d12_AllocateUpload(renderer, desc->size);
            memcpy(context.uploadBufferData, pInitialData, desc->size);

            context.commandList->CopyBufferRegion(
                buffer->handle,
                0,
                context.uploadBuffer,
                0,
                desc->size
            );

            d3d12_UploadSubmit(renderer, context);
        }
    }

    if (desc->usage & VGPUBufferUsage_ShaderRead)
    {
        // Create Raw Buffer
        const uint64_t offset = 0;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = offset / sizeof(uint32_t);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        srvDesc.Buffer.NumElements = (UINT)(desc->size / sizeof(uint32_t));

        D3D12_CPU_DESCRIPTOR_HANDLE handle = renderer->resourceAllocator.Allocate();
        renderer->device->CreateShaderResourceView(buffer->handle, &srvDesc, handle);

        // bindless allocation.
        //const uint32_t bindlessSRV = AllocateBindlessResource(handle);
        //buffer->SetBindlessSRV(bindlessSRV);
    }

    if (desc->usage & VGPUBufferUsage_ShaderWrite)
    {
        // Create Raw Buffer
        const uint64_t offset = 0;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = offset / sizeof(uint32_t);
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        uavDesc.Buffer.NumElements = (UINT)(desc->size / sizeof(uint32_t));

        D3D12_CPU_DESCRIPTOR_HANDLE handle = renderer->resourceAllocator.Allocate();
        renderer->device->CreateUnorderedAccessView(buffer->handle, nullptr, &uavDesc, handle);

        // bindless allocation.
        //const uint32_t bindlessUAV = AllocateBindlessResource(handle);
        //buffer->SetBindlessUAV(bindlessUAV);
    }

    return (VGPUBuffer)buffer;
}

static void d3d12_destroyBuffer(VGFXRenderer* driverData, VGPUBuffer resource)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;
    D3D12Resource* d3dBuffer = (D3D12Resource*)resource;

    d3d12_DeferDestroy(renderer, d3dBuffer->handle, d3dBuffer->allocation);
    delete d3dBuffer;
}

/* Texture */
static VGPUTexture d3d12_createTexture(VGFXRenderer* driverData, const VGPUTextureDesc* desc, const void* pInitialData)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    D3D12MA::ALLOCATION_DESC allocationDesc = {};
    allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc;
    if (desc->type == VGPUTexture_Type1D)
    {
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    }
    else if (desc->type == VGPUTexture_Type3D)
    {
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    }
    else
    {
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    }
    resourceDesc.Alignment = 0;
    resourceDesc.Width = desc->width;
    resourceDesc.Height = desc->height;
    resourceDesc.DepthOrArraySize = (UINT16)desc->depthOrArraySize;
    resourceDesc.MipLevels = (UINT16)desc->mipLevels;
    resourceDesc.Format = ToDXGIFormat(desc->format);
    resourceDesc.SampleDesc.Count = desc->sampleCount;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;

    if (pInitialData == nullptr)
    {
        if (desc->usage & VGPUTextureUsage_RenderTarget)
        {
            if (vgpuIsDepthStencilFormat(desc->format))
            {
                resourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            }
            else
            {
                resourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
            }
        }
        if (desc->usage & VGPUTextureUsage_ShaderRead)
        {
            resourceState |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        }

        if (desc->usage & VGPUTextureUsage_ShaderWrite)
        {
            resourceState |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
    }

    if (desc->usage & VGPUTextureUsage_ShaderWrite)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    if (desc->usage & VGPUTextureUsage_RenderTarget)
    {
        if (vgpuIsDepthStencilFormat(desc->format))
        {
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            if (!(desc->usage & VGPUTextureUsage_ShaderRead))
            {
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
            }
        }
        else
        {
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
    }

    D3D12_CLEAR_VALUE clearValue = {};
    D3D12_CLEAR_VALUE* pClearValue = nullptr;

    if (desc->usage & VGPUTextureUsage_RenderTarget)
    {
        clearValue.Format = resourceDesc.Format;
        if (vgpuIsDepthStencilFormat(desc->format))
        {
            clearValue.DepthStencil.Depth = 1.0f;
        }
        pClearValue = &clearValue;
    }

    // If shader read/write and depth format, set to typeless
    if (vgpuIsDepthFormat(desc->format) && desc->usage & (VGPUTextureUsage_ShaderRead | VGPUTextureUsage_ShaderWrite))
    {
        resourceDesc.Format = GetTypelessFormatFromDepthFormat(desc->format);
        pClearValue = nullptr;
    }

    // TODO: TextureLayout/State
    D3D12Resource* texture = new D3D12Resource();
    texture->state = resourceState;
    texture->width = desc->width;
    texture->height = desc->height;
    texture->dxgiFormat = resourceDesc.Format;

    HRESULT hr = renderer->allocator->CreateResource(
        &allocationDesc,
        &resourceDesc,
        texture->state,
        pClearValue,
        &texture->allocation,
        IID_PPV_ARGS(&texture->handle)
    );

    if (FAILED(hr))
    {
        vgpuLogError("D3D12: Failed to create texture");
        delete texture;
        return nullptr;
    }

    if (desc->label)
    {
        D3D12SetName(texture->handle, desc->label);
    }

    return (VGPUTexture)texture;
}

static void d3d12_destroyTexture(VGFXRenderer* driverData, VGPUTexture texture)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;
    D3D12Resource* d3dTexture = (D3D12Resource*)texture;

    d3d12_DeferDestroy(renderer, d3dTexture->handle, d3dTexture->allocation);
    for (auto& it : d3dTexture->rtvCache)
    {
        renderer->rtvAllocator.Free(it.second);
    }
    d3dTexture->rtvCache.clear();
    for (auto& it : d3dTexture->dsvCache)
    {
        renderer->dsvAllocator.Free(it.second);
    }
    d3dTexture->dsvCache.clear();
    delete d3dTexture;
}

/* Sampler */
static VGPUSampler d3d12_createSampler(VGFXRenderer* driverData, const VGPUSamplerDesc* desc)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    const D3D12_FILTER_REDUCTION_TYPE d3dReductionType = desc->compareFunction != VGPUCompareFunction_Never ? D3D12_FILTER_REDUCTION_TYPE_COMPARISON : D3D12_FILTER_REDUCTION_TYPE_STANDARD;
    const D3D12_FILTER_TYPE d3dMinFilter = ToD3D12FilterType(desc->minFilter);
    const D3D12_FILTER_TYPE d3dMagFilter = ToD3D12FilterType(desc->magFilter);
    const D3D12_FILTER_TYPE d3dMipFilter = ToD3D12FilterType(desc->mipFilter);

    D3D12_SAMPLER_DESC samplerDesc{};

    // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_sampler_desc
    if (desc->maxAnisotropy > 1)
    {
        samplerDesc.Filter = D3D12_ENCODE_ANISOTROPIC_FILTER(d3dReductionType);
    }
    else
    {
        samplerDesc.Filter = D3D12_ENCODE_BASIC_FILTER(d3dMinFilter, d3dMagFilter, d3dMipFilter, d3dReductionType);
    }

    samplerDesc.AddressU = ToD3D12AddressMode(desc->addressU);
    samplerDesc.AddressV = ToD3D12AddressMode(desc->addressV);
    samplerDesc.AddressW = ToD3D12AddressMode(desc->addressW);
    samplerDesc.MipLODBias = desc->mipLodBias;
    samplerDesc.MaxAnisotropy = std::min<UINT>(desc->maxAnisotropy, 16u);
    samplerDesc.ComparisonFunc = ToD3D12ComparisonFunc(desc->compareFunction);
    switch (desc->borderColor)
    {
        case VGPUSamplerBorderColor_OpaqueBlack:
            samplerDesc.BorderColor[0] = 0.0f;
            samplerDesc.BorderColor[1] = 0.0f;
            samplerDesc.BorderColor[2] = 0.0f;
            samplerDesc.BorderColor[3] = 1.0f;
            break;

        case VGPUSamplerBorderColor_OpaqueWhite:
            samplerDesc.BorderColor[0] = 1.0f;
            samplerDesc.BorderColor[1] = 1.0f;
            samplerDesc.BorderColor[2] = 1.0f;
            samplerDesc.BorderColor[3] = 1.0f;
            break;
        default:
            samplerDesc.BorderColor[0] = 0.0f;
            samplerDesc.BorderColor[1] = 0.0f;
            samplerDesc.BorderColor[2] = 0.0f;
            samplerDesc.BorderColor[3] = 0.0f;
            break;
    }

    samplerDesc.MinLOD = desc->lodMinClamp;
    samplerDesc.MaxLOD = desc->lodMaxClamp;

    D3D12Sampler* sampler = new D3D12Sampler();
    sampler->descriptor = renderer->samplerAllocator.Allocate();
    renderer->device->CreateSampler(&samplerDesc, sampler->descriptor);
    //sampler->bindlessIndex = AllocateBindlessSampler(sampler->descriptor);

    return (VGPUSampler)sampler;
}

static void d3d12_destroySampler(VGFXRenderer* driverData, VGPUSampler resource)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;
    D3D12Sampler* sampler = (D3D12Sampler*)resource;
    renderer->samplerAllocator.Free(sampler->descriptor);
    delete sampler;
}

/* ShaderModule */
static VGPUShaderModule d3d12_createShaderModule(VGFXRenderer* driverData, const void* pCode, size_t codeSize)
{
    D3D12Shader* shader = new D3D12Shader();
    shader->byteCode.resize(codeSize);
    memcpy(shader->byteCode.data(), pCode, codeSize);
    return (VGPUShaderModule)shader;
}

static void d3d12_destroyShaderModule(VGFXRenderer* driverData, VGPUShaderModule resource)
{
    D3D12Shader* shader = (D3D12Shader*)resource;
    delete shader;
}

/* Pipeline */
static VGPUPipeline d3d12_createRenderPipeline(VGFXRenderer* driverData, const VGPURenderPipelineDesc* desc)
{
    D3D12Pipeline* pipeline = new D3D12Pipeline();
    return (VGPUPipeline)pipeline;
}

static VGPUPipeline d3d12_createComputePipeline(VGFXRenderer* driverData, const VGPUComputePipelineDesc* desc)
{
    D3D12Pipeline* pipeline = new D3D12Pipeline();
    return (VGPUPipeline)pipeline;
}

static VGPUPipeline d3d12_createRayTracingPipeline(VGFXRenderer* driverData, const VGPURayTracingPipelineDesc* desc)
{
    D3D12Pipeline* pipeline = new D3D12Pipeline();
    return (VGPUPipeline)pipeline;
}

static void d3d12_destroyPipeline(VGFXRenderer* driverData, VGPUPipeline resource)
{
    D3D12Pipeline* pipeline = (D3D12Pipeline*)resource;
    delete pipeline;
}

/* SwapChain */
static VGPUSwapChain d3d12_createSwapChain(VGFXRenderer* driverData, void* windowHandle, const VGPUSwapChainDesc* desc)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = desc->width;
    swapChainDesc.Height = desc->height;
    swapChainDesc.Format = ToDXGIFormat(ToDXGISwapChainFormat(desc->format));
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = PresentModeToBufferCount(desc->presentMode);
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = renderer->tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;

    IDXGISwapChain1* tempSwapChain = nullptr;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HWND window = static_cast<HWND>(windowHandle);
    VGPU_ASSERT(IsWindow(window));

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = !desc->isFullscreen;

    // Create a swap chain for the window.
    HRESULT hr = renderer->factory->CreateSwapChainForHwnd(
        renderer->graphicsQueue,
        window,
        &swapChainDesc,
        &fsSwapChainDesc,
        nullptr,
        &tempSwapChain
    );

    if (FAILED(hr))
    {
        return nullptr;
    }

    // This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
    hr = renderer->factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
#else
    swapChainDesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;

    IUnknown* window = static_cast<IUnknown*>(windowHandle);

    HRESULT hr = renderer->factory->CreateSwapChainForCoreWindow(
        renderer->graphicsQueue,
        window,
        &swapChainDesc,
        nullptr,
        &tempSwapChain
    );

    // SwapChain panel
    //ComPtr<ISwapChainPanelNative> swapChainPanelNative;
    //swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    //swapChainDesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;
    //hr = renderer->factory->CreateSwapChainForComposition(
    //    renderer->graphicsQueue,
    //    &swapChainDesc,
    //    nullptr,
    //    tempSwapChain.GetAddressOf()
    //);
    //
    //hr = tempSwapChain.As(&swapChainPanelNative);
    //if (FAILED(hr))
    //{
    //    vgpuLogError("Failed to get ISwapChainPanelNative from IDXGISwapChain1");
    //    return nullptr;
    //}
    //
    //hr = swapChainPanelNative->SetSwapChain(tempSwapChain.Get());
    //if (FAILED(hr))
    //{
    //    vgpuLogError("Failed to set ISwapChainPanelNative - SwapChain");
    //    return nullptr;
    //}
#endif
    if (FAILED(hr))
    {
        return nullptr;
    }

    IDXGISwapChain3* handle = nullptr;
    hr = tempSwapChain->QueryInterface(&handle);
    if (FAILED(hr))
    {
        return nullptr;
    }
    SAFE_RELEASE(tempSwapChain);

    D3D12_SwapChain* swapChain = new D3D12_SwapChain();
    swapChain->window = window;
    swapChain->handle = handle;
    swapChain->format = ToDXGISwapChainFormat(desc->format);
    swapChain->textureFormat = desc->format;
    swapChain->backBufferCount = swapChainDesc.BufferCount;
    swapChain->syncInterval = PresentModeToSwapInterval(desc->presentMode);
    d3d12_updateSwapChain(renderer, swapChain);
    return (VGPUSwapChain)swapChain;
}

static void d3d12_destroySwapChain(VGFXRenderer* driverData, VGPUSwapChain swapChain)
{
    D3D12_SwapChain* d3d12SwapChain = (D3D12_SwapChain*)swapChain;

    for (size_t i = 0, count = d3d12SwapChain->backbufferTextures.size(); i < count; ++i)
    {
        d3d12_destroyTexture(driverData, (VGPUTexture)d3d12SwapChain->backbufferTextures[i]);
    }
    d3d12SwapChain->backbufferTextures.clear();
    d3d12SwapChain->handle->Release();

    delete d3d12SwapChain;
}

static VGPUTextureFormat d3d12_getSwapChainFormat(VGFXRenderer*, VGPUSwapChain swapChain)
{
    D3D12_SwapChain* d3dSwapChain = (D3D12_SwapChain*)swapChain;
    return d3dSwapChain->format;
}

static constexpr UINT PIX_EVENT_UNICODE_VERSION = 0;

static void d3d12_pushDebugGroup(VGPUCommandBufferImpl* driverData, const char* groupLabel)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;

    std::wstring wide_name = UTF8ToWStr(groupLabel);
    UINT size = static_cast<UINT>((strlen(groupLabel) + 1) * sizeof(wchar_t));
    commandBuffer->commandList->BeginEvent(PIX_EVENT_UNICODE_VERSION, wide_name.c_str(), size);
}

static void d3d12_popDebugGroup(VGPUCommandBufferImpl* driverData)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    commandBuffer->commandList->EndEvent();
}

static void d3d12_insertDebugMarker(VGPUCommandBufferImpl* driverData, const char* markerLabel)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;

    std::wstring wide_name = UTF8ToWStr(markerLabel);
    UINT size = static_cast<UINT>((strlen(markerLabel) + 1) * sizeof(wchar_t));
    commandBuffer->commandList->SetMarker(PIX_EVENT_UNICODE_VERSION, wide_name.c_str(), size);
}

static void d3d12_FlushResourceBarriers(D3D12CommandBuffer* commandBuffer)
{
    if (commandBuffer->numBarriersToFlush > 0)
    {
        commandBuffer->commandList->ResourceBarrier(commandBuffer->numBarriersToFlush, commandBuffer->resourceBarriers);
        commandBuffer->numBarriersToFlush = 0;
    }
}

static void d3d12_TransitionResource(D3D12CommandBuffer* commandBuffer, D3D12Resource* resource, D3D12_RESOURCE_STATES newState, bool flushImmediate = false)
{
    D3D12_RESOURCE_STATES oldState = resource->state;

    if (commandBuffer->type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
    {
        VGPU_ASSERT((oldState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == oldState);
        VGPU_ASSERT((newState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == newState);
    }

    if (oldState != newState)
    {
        VGPU_ASSERT(commandBuffer->numBarriersToFlush < 16 && "Exceeded arbitrary limit on buffered barriers");
        D3D12_RESOURCE_BARRIER& BarrierDesc = commandBuffer->resourceBarriers[commandBuffer->numBarriersToFlush++];

        BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        BarrierDesc.Transition.pResource = resource->handle;
        BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        BarrierDesc.Transition.StateBefore = oldState;
        BarrierDesc.Transition.StateAfter = newState;

        // Check to see if we already started the transition
        if (newState == resource->transitioningState)
        {
            BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
            resource->transitioningState = (D3D12_RESOURCE_STATES)-1;
        }
        else
        {
            BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        }

        resource->state = newState;
    }
    else if (newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    {
        //d3d12_InsertUAVBarrier(commandBuffer, resource, flushImmediate);
    }

    if (flushImmediate || commandBuffer->numBarriersToFlush == 16)
    {
        d3d12_FlushResourceBarriers(commandBuffer);
    }
}

static void d3d12_setPipeline(VGPUCommandBufferImpl* driverData, VGPUPipeline pipeline)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    D3D12Pipeline* d3dPipeline = (D3D12Pipeline*)pipeline;
    commandBuffer->commandList->SetPipelineState(d3dPipeline->handle);
    commandBuffer->commandList->IASetPrimitiveTopology(d3dPipeline->primitiveTopology);
}

static void d3d12_prepareDispatch(D3D12CommandBuffer* commandBuffer)
{
    VGPU_ASSERT(commandBuffer->insideRenderPass);
}

static void d3d12_dispatch(VGPUCommandBufferImpl* driverData, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    d3d12_prepareDispatch(commandBuffer);
    commandBuffer->commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

static void d3d12_dispatchIndirect(VGPUCommandBufferImpl* driverData, VGPUBuffer indirectBuffer, uint32_t indirectBufferOffset)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    d3d12_prepareDispatch(commandBuffer);

    D3D12Resource* d3dBuffer = (D3D12Resource*)indirectBuffer;
    commandBuffer->commandList->ExecuteIndirect(commandBuffer->renderer->dispatchIndirectCommandSignature,
        1,
        d3dBuffer->handle, indirectBufferOffset,
        nullptr,
        0);
}

static VGPUTexture d3d12_acquireSwapchainTexture(VGPUCommandBufferImpl* driverData, VGPUSwapChain swapChain, uint32_t* pWidth, uint32_t* pHeight)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    D3D12_SwapChain* d3d12SwapChain = (D3D12_SwapChain*)swapChain;

    HRESULT hr = S_OK;

    /* Check for window size changes and resize the swapchain if needed. */
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    d3d12SwapChain->handle->GetDesc1(&swapChainDesc);

    uint32_t width = 0;
    uint32_t height = 0;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    RECT windowRect;
    GetClientRect(d3d12SwapChain->window, &windowRect);
    width = static_cast<uint32_t>(windowRect.right - windowRect.left);
    height = static_cast<uint32_t>(windowRect.bottom - windowRect.top);
#else

#endif

    // Check if window is minimized
    if (width == 0 ||
        height == 0)
    {
        return nullptr;
    }

    if (width != swapChainDesc.Width ||
        height != swapChainDesc.Height)
    {
        d3d12_waitIdle((VGFXRenderer*)commandBuffer->renderer);

        // Release resources that are tied to the swap chain and update fence values.
        for (size_t i = 0, count = d3d12SwapChain->backbufferTextures.size(); i < count; ++i)
        {
            d3d12_destroyTexture((VGFXRenderer*)commandBuffer->renderer, (VGPUTexture)d3d12SwapChain->backbufferTextures[i]);
        }
        d3d12SwapChain->backbufferTextures.clear();

        HRESULT hr = d3d12SwapChain->handle->ResizeBuffers(
            d3d12SwapChain->backBufferCount,
            width,
            height,
            DXGI_FORMAT_UNKNOWN, /* Keep the old format */
            (commandBuffer->renderer->tearingSupported) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u
        );

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
#ifdef _DEBUG
            char buff[64] = {};
            sprintf_s(buff, "Device Lost on ResizeBuffers: Reason code 0x%08X\n",
                static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? commandBuffer->renderer->device->GetDeviceRemovedReason() : hr));
            OutputDebugStringA(buff);
#endif
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            //HandleDeviceLost();

            // Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method
            // and correctly set up the new device.
            return nullptr;
        }
        else
        {
            if (FAILED(hr))
            {
                vgpuLogError("Could not resize swapchain");
                return nullptr;
            }

            d3d12_updateSwapChain(commandBuffer->renderer, d3d12SwapChain);
        }
    }

    D3D12Resource* swapChainTexture =
        d3d12SwapChain->backbufferTextures[d3d12SwapChain->handle->GetCurrentBackBufferIndex()];

    // Transition to RenderTarget state
    d3d12_TransitionResource(commandBuffer, swapChainTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

    if (pWidth) {
        *pWidth = swapChainTexture->width;
    }

    if (pHeight) {
        *pHeight = swapChainTexture->height;
    }

    commandBuffer->swapChains.push_back(d3d12SwapChain);
    return (VGPUTexture)swapChainTexture;
}

static void d3d12_beginRenderPass(VGPUCommandBufferImpl* driverData, const VGPURenderPassDesc* desc)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;

    uint32_t width = _VGPU_DEF(desc->width, UINT32_MAX);
    uint32_t height = _VGPU_DEF(desc->height, UINT32_MAX);
    uint32_t numRTVS = 0;
    D3D12_RENDER_PASS_FLAGS renderPassFlags = D3D12_RENDER_PASS_FLAG_NONE;
    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC DSV = {};

    for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i)
    {
        const VGPURenderPassColorAttachment* attachment = &desc->colorAttachments[i];
        D3D12Resource* texture = (D3D12Resource*)attachment->texture;
        const uint32_t level = attachment->level;
        const uint32_t slice = attachment->slice;

        commandBuffer->RTVs[i].cpuDescriptor = d3d12_GetRTV(commandBuffer->renderer, texture, level, slice);

        // Transition to RenderTarget
        d3d12_TransitionResource(commandBuffer, texture, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

        switch (attachment->loadOp)
        {
            default:
            case VGPULoadOp_Load:
                commandBuffer->RTVs[numRTVS].BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
                break;

            case VGPULoadOp_Clear:
                commandBuffer->RTVs[numRTVS].BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
                commandBuffer->RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Format = texture->dxgiFormat;
                commandBuffer->RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Color[0] = attachment->clearColor.r;
                commandBuffer->RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Color[1] = attachment->clearColor.g;
                commandBuffer->RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Color[2] = attachment->clearColor.b;
                commandBuffer->RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Color[3] = attachment->clearColor.a;
                break;

            case VGPULoadOp_Discard:
                commandBuffer->RTVs[numRTVS].BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
                break;
        }

        switch (attachment->storeOp)
        {
            default:
            case VGPUStoreOp_Store:
                commandBuffer->RTVs[numRTVS].EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
                break;

            case VGPUStoreOp_Discard:
                commandBuffer->RTVs[numRTVS].EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
                break;
        }

        width = _VGPU_MIN(width, _VGPU_MAX(1U, texture->width >> level));
        height = _VGPU_MIN(height, _VGPU_MAX(1U, texture->height >> level));

        numRTVS++;
    }

    const bool hasDepthStencil = desc->depthStencilAttachment != nullptr;
    if (hasDepthStencil)
    {
        const VGPURenderPassDepthStencilAttachment* attachment = desc->depthStencilAttachment;
        D3D12Resource* texture = (D3D12Resource*)attachment->texture;
        const uint32_t level = attachment->level;
        const uint32_t slice = attachment->slice;

        DSV.cpuDescriptor = d3d12_GetDSV(commandBuffer->renderer, texture, level, slice);
        DSV.StencilBeginningAccess.Clear.ClearValue.Format = texture->dxgiFormat;

        switch (attachment->depthLoadOp)
        {
            default:
            case VGPULoadOp_Load:
                DSV.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
                break;

            case VGPULoadOp_Clear:
                DSV.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
                DSV.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = attachment->clearDepth;
                break;

            case VGPULoadOp_Discard:
                DSV.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
                break;
        }

        switch (attachment->depthStoreOp)
        {
            default:
            case VGPUStoreOp_Store:
                DSV.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
                break;

            case VGPUStoreOp_Discard:
                DSV.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
                break;
        }

        switch (attachment->stencilLoadOp)
        {
            default:
            case VGPULoadOp_Load:
                DSV.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
                break;

            case VGPULoadOp_Clear:
                DSV.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
                DSV.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = attachment->clearStencil;
                break;

            case VGPULoadOp_Discard:
                DSV.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
                break;
        }

        switch (attachment->stencilStoreOp)
        {
            default:
            case VGPUStoreOp_Store:
                DSV.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
                break;

            case VGPUStoreOp_Discard:
                DSV.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
                break;
        }

        width = _VGPU_MIN(width, _VGPU_MAX(1U, texture->width >> level));
        height = _VGPU_MIN(height, _VGPU_MAX(1U, texture->height >> level));
    }

    commandBuffer->commandList->BeginRenderPass(numRTVS,
        commandBuffer->RTVs,
        hasDepthStencil ? &DSV : nullptr,
        renderPassFlags);

    // Set the viewport.
    static constexpr float defaultBlendFactor[4] = { 0, 0, 0, 0 };
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    D3D12_RECT scissorRect = { 0, 0, LONG(width), LONG(height) };
    commandBuffer->commandList->RSSetViewports(1, &viewport);
    commandBuffer->commandList->RSSetScissorRects(1, &scissorRect);
    commandBuffer->commandList->OMSetBlendFactor(defaultBlendFactor);
    commandBuffer->commandList->OMSetStencilRef(0);
    commandBuffer->insideRenderPass = true;
}

static void d3d12_endRenderPass(VGPUCommandBufferImpl* driverData)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    commandBuffer->commandList->EndRenderPass();
    commandBuffer->insideRenderPass = false;
}

static void d3d12_setViewports(VGPUCommandBufferImpl* driverData, const VGPUViewport* viewports, uint32_t count)
{
    VGPU_ASSERT(count < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);

    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    commandBuffer->commandList->RSSetViewports(count, (const D3D12_VIEWPORT*)viewports);
}

static void d3d12_setScissorRects(VGPUCommandBufferImpl* driverData, const VGPURect* scissorRects, uint32_t count)
{
    VGPU_ASSERT(count < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);

    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;

    D3D12_RECT d3dScissorRects[VGPU_MAX_VIEWPORTS_AND_SCISSORS];
    for (uint32_t i = 0; i < count; i++)
    {
        d3dScissorRects[i].left = scissorRects[i].x;
        d3dScissorRects[i].top = scissorRects[i].y;
        d3dScissorRects[i].right = scissorRects[i].x + scissorRects[i].width;
        d3dScissorRects[i].bottom = scissorRects[i].y + scissorRects[i].height;
    }
    commandBuffer->commandList->RSSetScissorRects(count, d3dScissorRects);
}

static void d3d12_setVertexBuffer(VGPUCommandBufferImpl* driverData, uint32_t index, VGPUBuffer buffer, uint64_t offset)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    D3D12Resource* d3d12Buffer = (D3D12Resource*)buffer;

    commandBuffer->vboViews[index].BufferLocation = d3d12Buffer->gpuAddress + (D3D12_GPU_VIRTUAL_ADDRESS)offset;
    commandBuffer->vboViews[index].SizeInBytes = (UINT)(d3d12Buffer->size - offset);
    commandBuffer->vboViews[index].StrideInBytes = 0;
}

static void d3d12_setIndexBuffer(VGPUCommandBufferImpl* driverData, VGPUBuffer buffer, uint64_t offset, VGPUIndexType indexType)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    D3D12Resource* d3d12Buffer = (D3D12Resource*)buffer;

    D3D12_INDEX_BUFFER_VIEW view;
    view.BufferLocation = d3d12Buffer->gpuAddress + (D3D12_GPU_VIRTUAL_ADDRESS)offset;
    view.SizeInBytes = (UINT)(d3d12Buffer->size - offset);
    view.Format = (indexType == VGPUIndexType_UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);
    commandBuffer->commandList->IASetIndexBuffer(&view);
}

static void d3d12_prepareDraw(D3D12CommandBuffer* commandBuffer)
{
    VGPU_ASSERT(commandBuffer->insideRenderPass);
}

static void d3d12_draw(VGPUCommandBufferImpl* driverData, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t baseInstance)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    d3d12_prepareDraw(commandBuffer);
    commandBuffer->commandList->DrawInstanced(vertexCount, instanceCount, vertexStart, baseInstance);
}

static VGPUCommandBuffer d3d12_beginCommandBuffer(VGFXRenderer* driverData, const char* label)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    HRESULT hr = S_OK;
    D3D12CommandBuffer* impl = nullptr;

    renderer->cmdBuffersLocker.lock();
    uint32_t cmd_current = renderer->cmdBuffersCount++;
    if (cmd_current >= renderer->commandBuffers.size())
    {
        impl = new D3D12CommandBuffer();
        impl->renderer = renderer;
        impl->type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        for (uint32_t i = 0; i < VGPU_MAX_INFLIGHT_FRAMES; ++i)
        {
            hr = renderer->device->CreateCommandAllocator(impl->type, IID_PPV_ARGS(&impl->commandAllocators[i]));
            VGPU_ASSERT(SUCCEEDED(hr));
        }

        hr = renderer->device->CreateCommandList1(0, impl->type, D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&impl->commandList)
        );
        VGPU_ASSERT(SUCCEEDED(hr));

        VGPUCommandBuffer_T* commandBuffer = (VGPUCommandBuffer_T*)VGPU_MALLOC(sizeof(VGPUCommandBuffer_T));
        ASSIGN_COMMAND_BUFFER(d3d12);
        commandBuffer->driverData = (VGPUCommandBufferImpl*)impl;

        renderer->commandBuffers.push_back(commandBuffer);
    }
    else
    {
        impl = (D3D12CommandBuffer*)renderer->commandBuffers.back()->driverData;
    }

    renderer->cmdBuffersLocker.unlock();

    // Start the command list in a default state.
    hr = impl->commandAllocators[renderer->frameIndex]->Reset();
    VGPU_ASSERT(SUCCEEDED(hr));
    hr = impl->commandList->Reset(impl->commandAllocators[renderer->frameIndex], nullptr);
    VGPU_ASSERT(SUCCEEDED(hr));

    //ID3D12DescriptorHeap* heaps[2] = {
    //    renderer->resourceHeap.handle,
    //    renderer->samplerHeap.handle
    //};
    //impl->commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    impl->insideRenderPass = false;

    static constexpr float defaultBlendFactor[4] = { 0, 0, 0, 0 };
    impl->commandList->OMSetBlendFactor(defaultBlendFactor);
    impl->commandList->OMSetStencilRef(0);
    impl->numBarriersToFlush = 0;

    impl->hasLabel = false;
    if (label)
    {
        d3d12_pushDebugGroup((VGPUCommandBufferImpl*)impl, label);
        impl->hasLabel = true;
    }

    return renderer->commandBuffers.back();
}

static void d3d12_submit(VGFXRenderer* driverData, VGPUCommandBuffer* commandBuffers, uint32_t count)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    HRESULT hr = S_OK;
    std::vector<ID3D12CommandList*> submitCommandLists;
    std::vector<D3D12_SwapChain*> presentSwapChains;
    for (uint32_t i = 0; i < count; i += 1)
    {
        D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)commandBuffers[i]->driverData;

        // Present acquired SwapChains
        for (size_t i = 0, count = commandBuffer->swapChains.size(); i < count && SUCCEEDED(hr); ++i)
        {
            D3D12_SwapChain* swapChain = commandBuffer->swapChains[i];

            /* Transition SwapChain textures to present */
            D3D12Resource* texture = (D3D12Resource*)swapChain->backbufferTextures[swapChain->handle->GetCurrentBackBufferIndex()];

            d3d12_TransitionResource(commandBuffer, texture, D3D12_RESOURCE_STATE_PRESENT);

            presentSwapChains.push_back(swapChain);
        }
        commandBuffer->swapChains.clear();

        /* Push debug group label -> if any */
        if (commandBuffer->hasLabel)
        {
            d3d12_popDebugGroup((VGPUCommandBufferImpl*)commandBuffer);
        }

        /* Flush any pending barriers */
        d3d12_FlushResourceBarriers(commandBuffer);

        hr = commandBuffer->commandList->Close();
        if (FAILED(hr))
        {
            vgpuLogError("Failed to close command list");
            return;
        }

        submitCommandLists.push_back(commandBuffer->commandList);
    }

    renderer->graphicsQueue->ExecuteCommandLists(
        (UINT)submitCommandLists.size(),
        submitCommandLists.data()
    );
    renderer->cmdBuffersCount = 0;

    // Present acquired SwapChains
    for (size_t i = 0, count = presentSwapChains.size(); i < count && SUCCEEDED(hr); ++i)
    {
        D3D12_SwapChain* swapChain = presentSwapChains[i];

        UINT presentFlags = 0;
        BOOL fullscreen = FALSE;
        swapChain->handle->GetFullscreenState(&fullscreen, nullptr);

        if (!swapChain->syncInterval && !fullscreen)
        {
            presentFlags = DXGI_PRESENT_ALLOW_TEARING;
        }

        hr = swapChain->handle->Present(swapChain->syncInterval, presentFlags);

        // If the device was reset we must completely reinitialize the renderer.
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
#ifdef _DEBUG
            char buff[64] = {};
            sprintf_s(buff, "Device Lost on Present: Reason code 0x%08X\n",
                static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? renderer->device->GetDeviceRemovedReason() : hr));
            OutputDebugStringA(buff);
#endif
            //HandleDeviceLost();
            return;
        }
    }

    // Signal
}

static bool d3d12_isSupported(void)
{
    static bool available_initialized = false;
    static bool available = false;

    if (available_initialized) {
        return available;
    }

    available_initialized = true;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HMODULE dxgiDLL = LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE d3d12DLL = LoadLibraryExW(L"d3d12.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (dxgiDLL == nullptr ||
        d3d12DLL == nullptr)
    {
        return false;
    }

    vgfxCreateDXGIFactory2 = (PFN_CREATE_DXGI_FACTORY2)GetProcAddress(dxgiDLL, "CreateDXGIFactory2");
    if (vgfxCreateDXGIFactory2 == nullptr)
    {
        return false;
    }

#if defined(_DEBUG)
    vgpuDXGIGetDebugInterface1 = (PFN_DXGI_GET_DEBUG_INTERFACE1)GetProcAddress(dxgiDLL, "DXGIGetDebugInterface1");
#endif

    vgfxD3D12GetDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(d3d12DLL, "D3D12GetDebugInterface");
    vgfxD3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12DLL, "D3D12CreateDevice");
    if (!vgfxD3D12CreateDevice)
    {
        return false;
    }

    vgfxD3D12SerializeVersionedRootSignature = (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(d3d12DLL, "D3D12SerializeVersionedRootSignature");
    if (!vgfxD3D12SerializeVersionedRootSignature) {
        return false;
    }
#endif

    ComPtr<IDXGIFactory4> dxgiFactory;
    if (FAILED(vgfxCreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory))))
    {
        return false;
    }

    bool foundCompatibleDevice = true;
    ComPtr<IDXGIAdapter1> dxgiAdapter;
    for (uint32_t i = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(i, dxgiAdapter.ReleaseAndGetAddressOf()); ++i)
    {
        DXGI_ADAPTER_DESC1 adapterDesc;
        dxgiAdapter->GetDesc1(&adapterDesc);

        // Don't select the Basic Render Driver adapter.
        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }

        // Check to see if the adapter supports Direct3D 12,
        // but don't create the actual device.
        if (SUCCEEDED(vgfxD3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
        {
            foundCompatibleDevice = true;
            break;
        }
    }

    if (foundCompatibleDevice)
    {
        available = true;
        return true;
    }

    return false;
}

static VGPUDevice d3d12_createDevice(const VGPUDeviceDesc* info)
{
    VGPU_ASSERT(info);

    D3D12_Renderer* renderer = new D3D12_Renderer();

    DWORD dxgiFactoryFlags = 0;
    if (info->validationMode != VGPUValidationMode_Disabled)
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(vgfxD3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
        {
            debugController->EnableDebugLayer();

            if (info->validationMode == VGPUValidationMode_GPU)
            {
                ComPtr<ID3D12Debug1> debugController1;
                if (SUCCEEDED(debugController.As(&debugController1)))
                {
                    debugController1->SetEnableGPUBasedValidation(TRUE);
                    debugController1->SetEnableSynchronizedCommandQueueValidation(TRUE);
                }

                ComPtr<ID3D12Debug2> debugController2;
                if (SUCCEEDED(debugController.As(&debugController2)))
                {
                    debugController2->SetGPUBasedValidationFlags(D3D12_GPU_BASED_VALIDATION_FLAGS_NONE);
                }
            }
        }
        else
        {
            OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
        }

#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (vgpuDXGIGetDebugInterface1 != nullptr &&
            SUCCEEDED(vgpuDXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
        {
            dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

            dxgiInfoQueue->SetBreakOnSeverity(VGFX_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(VGFX_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

            DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
            {
                80 /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
            };
            DXGI_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            dxgiInfoQueue->AddStorageFilterEntries(VGFX_DXGI_DEBUG_DXGI, &filter);
        }
#endif
    }

    if (FAILED(vgfxCreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(renderer->factory.ReleaseAndGetAddressOf()))))
    {
        delete renderer;
        return nullptr;
    }

    // Determines whether tearing support is available for fullscreen borderless windows.
    {
        BOOL allowTearing = FALSE;

        ComPtr<IDXGIFactory5> dxgiFactory5;
        HRESULT hr = renderer->factory.As(&dxgiFactory5);
        if (SUCCEEDED(hr))
        {
            hr = dxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        }

        if (FAILED(hr) || !allowTearing)
        {
            renderer->tearingSupported = false;
#ifdef _DEBUG
            OutputDebugStringA("WARNING: Variable refresh rate displays not supported");
#endif
        }
        else
        {
            renderer->tearingSupported = true;
        }
    }

    {
        ComPtr<IDXGIFactory6> dxgiFactory6;
        const bool queryByPreference = SUCCEEDED(renderer->factory.As(&dxgiFactory6));
        auto NextAdapter = [&](uint32_t index, IDXGIAdapter1** ppAdapter)
        {
            if (queryByPreference)
                return dxgiFactory6->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(ppAdapter));
            else
                return renderer->factory->EnumAdapters1(index, ppAdapter);
        };

        static constexpr D3D_FEATURE_LEVEL s_featureLevels[] =
        {
            D3D_FEATURE_LEVEL_12_2,
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };

        ComPtr<IDXGIAdapter1> dxgiAdapter = nullptr;
        for (uint32_t i = 0; NextAdapter(i, dxgiAdapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 adapterDesc;
            dxgiAdapter->GetDesc1(&adapterDesc);

            // Don't select the Basic Render Driver adapter.
            if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            for (auto& featurelevel : s_featureLevels)
            {
                if (SUCCEEDED(vgfxD3D12CreateDevice(dxgiAdapter.Get(), featurelevel, IID_PPV_ARGS(&renderer->device))))
                {
                    break;
                }
            }

            if (renderer->device != nullptr)
                break;
        }

        VGPU_ASSERT(dxgiAdapter != nullptr);
        if (dxgiAdapter == nullptr)
        {
            vgpuLogError("DXGI: No capable adapter found!");
            delete renderer;
            return nullptr;
        }

        // Init feature check (https://devblogs.microsoft.com/directx/introducing-a-new-api-for-checking-feature-support-in-direct3d-12/)
        VHR(renderer->d3dFeatures.Init(renderer->device));

        if (renderer->d3dFeatures.HighestRootSignatureVersion() < D3D_ROOT_SIGNATURE_VERSION_1_1)
        {
            vgpuLogError("Direct3D12: Root signature version 1.1 not supported!");
            delete renderer;
            return nullptr;
        }

        // Assign label object.
        if (info->label)
        {
            std::wstring wide_name = UTF8ToWStr(info->label);
            renderer->device->SetName(wide_name.c_str());
        }

        if (info->validationMode != VGPUValidationMode_Disabled)
        {
            // Configure debug device (if active).
            ComPtr<ID3D12InfoQueue> infoQueue;
            if (SUCCEEDED(renderer->device->QueryInterface(infoQueue.GetAddressOf())))
            {
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

                std::vector<D3D12_MESSAGE_SEVERITY> enabledSeverities;
                std::vector<D3D12_MESSAGE_ID> disabledMessages;

                // These severities should be seen all the time
                enabledSeverities.push_back(D3D12_MESSAGE_SEVERITY_CORRUPTION);
                enabledSeverities.push_back(D3D12_MESSAGE_SEVERITY_ERROR);
                enabledSeverities.push_back(D3D12_MESSAGE_SEVERITY_WARNING);
                enabledSeverities.push_back(D3D12_MESSAGE_SEVERITY_MESSAGE);

                if (info->validationMode == VGPUValidationMode_Verbose)
                {
                    // Verbose only filters
                    enabledSeverities.push_back(D3D12_MESSAGE_SEVERITY_INFO);
                }

#if defined (VGFX_DX12_USE_PIPELINE_LIBRARY)
                disabledMessages.push_back(D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND);
                disabledMessages.push_back(D3D12_MESSAGE_ID_STOREPIPELINE_DUPLICATENAME);
#endif

                disabledMessages.push_back(D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE);
                disabledMessages.push_back(D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE);
                disabledMessages.push_back(D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE);
                disabledMessages.push_back(D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE);
                disabledMessages.push_back(D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE);
                disabledMessages.push_back(D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE);
                //disabledMessages.push_back(D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED);

                D3D12_INFO_QUEUE_FILTER filter = {};
                filter.AllowList.NumSeverities = static_cast<UINT>(enabledSeverities.size());
                filter.AllowList.pSeverityList = enabledSeverities.data();
                filter.DenyList.NumIDs = static_cast<UINT>(disabledMessages.size());
                filter.DenyList.pIDList = disabledMessages.data();

                // Clear out the existing filters since we're taking full control of them
                infoQueue->PushEmptyStorageFilter();

                infoQueue->AddStorageFilterEntries(&filter);
            }
        }

        // Create allocator
        D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
        allocatorDesc.pDevice = renderer->device;
        allocatorDesc.pAdapter = dxgiAdapter.Get();
        allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
        if (FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &renderer->allocator)))
        {
            delete renderer;
            return nullptr;
        }

        // Init capabilities.
        DXGI_ADAPTER_DESC1 adapterDesc;
        dxgiAdapter->GetDesc1(&adapterDesc);

        renderer->vendorID = adapterDesc.VendorId;
        renderer->deviceID = adapterDesc.DeviceId;
        renderer->adapterName = WCharToUTF8(adapterDesc.Description);

        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            renderer->adapterType = VGPUAdapterType_Cpu;
        }
        else {
            renderer->adapterType = (renderer->d3dFeatures.UMA() == TRUE) ? VGPUAdapterType_IntegratedGPU : VGPUAdapterType_DiscreteGPU;
        }

        // Convert the adapter's D3D12 driver version to a readable string like "24.21.13.9793".
        LARGE_INTEGER umdVersion;
        if (dxgiAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umdVersion) != DXGI_ERROR_UNSUPPORTED)
        {
            uint64_t encodedVersion = umdVersion.QuadPart;
            std::ostringstream o;
            o << "D3D12 driver version ";
            uint16_t driverVersion[4] = {};

            for (size_t i = 0; i < 4; ++i) {
                driverVersion[i] = (encodedVersion >> (48 - 16 * i)) & 0xFFFF;
                o << driverVersion[i] << ".";
            }

            renderer->driverDescription = o.str();
        }

        renderer->featureLevel = renderer->d3dFeatures.MaxSupportedFeatureLevel();

        vgpuLogInfo("VGPU Driver: D3D12");
        vgpuLogInfo("D3D12 Adapter: %S", adapterDesc.Description);
    }

    // Create command queues
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        if (FAILED(renderer->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&renderer->graphicsQueue))))
        {
            delete renderer;
            return nullptr;
        }
        renderer->graphicsQueue->SetName(L"Graphics Queue");

        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        if (FAILED(renderer->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&renderer->copyQueue))))
        {
            delete renderer;
            return nullptr;
        }
        renderer->copyQueue->SetName(L"Copy Queue");
    }

    // Init CPU descriptor allocators
    renderer->resourceAllocator.Init(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096);
    renderer->samplerAllocator.Init(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256);
    renderer->rtvAllocator.Init(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 512);
    renderer->dsvAllocator.Init(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128);

    // Create frame data
    {
        HRESULT hr = renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderer->frameFence));
        if (FAILED(hr))
        {
            delete renderer;
            return nullptr;
        }
        renderer->frameFenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
        if (renderer->frameFence == INVALID_HANDLE_VALUE)
        {
            delete renderer;
            return nullptr;
        }
    }

    // Create common indirect command signatures
    {
        // DispatchIndirectCommand
        D3D12_INDIRECT_ARGUMENT_DESC dispatchArg{};
        dispatchArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

        D3D12_COMMAND_SIGNATURE_DESC cmdSignatureDesc = {};
        cmdSignatureDesc.ByteStride = sizeof(VGPUDispatchIndirectCommand);
        cmdSignatureDesc.NumArgumentDescs = 1;
        cmdSignatureDesc.pArgumentDescs = &dispatchArg;
        VHR(renderer->device->CreateCommandSignature(&cmdSignatureDesc, nullptr, IID_PPV_ARGS(&renderer->dispatchIndirectCommandSignature)));

        // DrawIndirectCommand
        D3D12_INDIRECT_ARGUMENT_DESC drawInstancedArg{};
        drawInstancedArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        cmdSignatureDesc.ByteStride = sizeof(VGPUDrawIndirectCommand);
        cmdSignatureDesc.NumArgumentDescs = 1;
        cmdSignatureDesc.pArgumentDescs = &drawInstancedArg;
        VHR(renderer->device->CreateCommandSignature(&cmdSignatureDesc, nullptr, IID_PPV_ARGS(&renderer->drawIndirectCommandSignature)));

        // DrawIndexedIndirectCommand
        D3D12_INDIRECT_ARGUMENT_DESC drawIndexedInstancedArg{};
        drawIndexedInstancedArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        cmdSignatureDesc.ByteStride = sizeof(VGPUDrawIndexedIndirectCommand);
        cmdSignatureDesc.NumArgumentDescs = 1;
        cmdSignatureDesc.pArgumentDescs = &drawIndexedInstancedArg;
        VHR(renderer->device->CreateCommandSignature(&cmdSignatureDesc, nullptr, IID_PPV_ARGS(&renderer->drawIndexedIndirectCommandSignature)));

        if (renderer->d3dFeatures.MeshShaderTier() >= D3D12_MESH_SHADER_TIER_1)
        {
            D3D12_INDIRECT_ARGUMENT_DESC dispatchMeshArg{};
            dispatchMeshArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;

            cmdSignatureDesc.ByteStride = sizeof(VGPUDispatchIndirectCommand);
            cmdSignatureDesc.NumArgumentDescs = 1;
            cmdSignatureDesc.pArgumentDescs = &dispatchMeshArg;
            VHR(renderer->device->CreateCommandSignature(&cmdSignatureDesc, nullptr, IID_PPV_ARGS(&renderer->dispatchMeshIndirectCommandSignature)));
        }
    }

    VGPUDevice_T* device = (VGPUDevice_T*)VGPU_MALLOC(sizeof(VGPUDevice_T));
    ASSIGN_DRIVER(d3d12);
    device->driverData = (VGFXRenderer*)renderer;
    return device;
}

VGFXDriver D3D12_Driver = {
    VGPUBackendType_D3D12,
    d3d12_isSupported,
    d3d12_createDevice
};

#undef VHR
#undef SAFE_RELEASE

#endif /* VGPU_D3D12_DRIVER */
