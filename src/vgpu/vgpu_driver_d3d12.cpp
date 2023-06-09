// Copyright © Amer Koleci.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGPU_D3D12_DRIVER)


#include <dxgi1_6.h>
#include <directx/d3d12.h>
#include <directx/d3dx12_default.h>
#include <directx/d3dx12_resource_helpers.h>
#include <directx/d3dx12_pipeline_state_stream.h>
#include <directx/d3dx12_check_feature_support.h>
//#include <windows.ui.xaml.media.dxinterop.h>
#include <wrl/client.h>
#include "vgpu_driver_d3d.h"

#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#include "D3D12MemAlloc.h"

#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   include <dxgidebug.h>
#endif

#include <sstream>

#define VALID_COMPUTE_QUEUE_RESOURCE_STATES \
    ( D3D12_RESOURCE_STATE_UNORDERED_ACCESS \
    | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE \
    | D3D12_RESOURCE_STATE_COPY_DEST \
    | D3D12_RESOURCE_STATE_COPY_SOURCE )

using namespace vgpu;
using Microsoft::WRL::ComPtr;

#define VHR(hr) if (FAILED(hr)) { VGPU_ASSERT(0); }
#define SAFE_RELEASE(obj) if ((obj)) { (obj)->Release(); (obj) = nullptr; }

namespace
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    using PFN_CREATE_DXGI_FACTORY2 = decltype(&CreateDXGIFactory2);
    static PFN_CREATE_DXGI_FACTORY2 vgpuCreateDXGIFactory2 = nullptr;

    static PFN_D3D12_GET_DEBUG_INTERFACE vgpuD3D12GetDebugInterface = nullptr;
    static PFN_D3D12_CREATE_DEVICE vgpuD3D12CreateDevice = nullptr;
    static PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE vgpuD3D12SerializeVersionedRootSignature = nullptr;

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

    inline void D3D12SetName(ID3D12Object* obj, const char* name)
    {
        if (obj)
        {
            if (name != nullptr)
            {
                std::wstring wide_name = UTF8ToWStr(name);
                obj->SetName(wide_name.c_str());
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
    static_assert(offsetof(VGPUDrawIndirectCommand, firstVertex) == offsetof(D3D12_DRAW_ARGUMENTS, StartVertexLocation), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndirectCommand, firstInstance) == offsetof(D3D12_DRAW_ARGUMENTS, StartInstanceLocation), "Layout mismatch");

    static_assert(sizeof(VGPUDrawIndexedIndirectCommand) == sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), "DrawIndexedIndirectCommand mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, indexCount) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, IndexCountPerInstance), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, instanceCount) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, InstanceCount), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, firstIndex) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, StartIndexLocation), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, baseVertex) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, BaseVertexLocation), "Layout mismatch");
    static_assert(offsetof(VGPUDrawIndexedIndirectCommand, firstInstance) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, StartInstanceLocation), "Layout mismatch");

    
    constexpr D3D12_COMMAND_LIST_TYPE ToD3D12(VGPUCommandQueue type)
    {
        switch (type)
        {
            case VGPUCommandQueue_Graphics:
                return D3D12_COMMAND_LIST_TYPE_DIRECT;

            case VGPUCommandQueue_Compute:
                return D3D12_COMMAND_LIST_TYPE_COMPUTE;

            case VGPUCommandQueue_Copy:
                return D3D12_COMMAND_LIST_TYPE_COPY;

            default:
                VGPU_UNREACHABLE();
        }
    }

    constexpr D3D12_COMPARISON_FUNC ToD3D12(VGPUCompareFunction function)
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

    constexpr D3D12_STENCIL_OP ToD3D12(VGPUStencilOperation op)
    {
        switch (op)
        {
            case VGPUStencilOperation_Keep:            return D3D12_STENCIL_OP_KEEP;
            case VGPUStencilOperation_Zero:            return D3D12_STENCIL_OP_ZERO;
            case VGPUStencilOperation_Replace:         return D3D12_STENCIL_OP_REPLACE;
            case VGPUStencilOperation_IncrementClamp:  return D3D12_STENCIL_OP_INCR_SAT;
            case VGPUStencilOperation_DecrementClamp:  return D3D12_STENCIL_OP_DECR_SAT;
            case VGPUStencilOperation_Invert:          return D3D12_STENCIL_OP_INVERT;
            case VGPUStencilOperation_IncrementWrap:   return D3D12_STENCIL_OP_INCR;
            case VGPUStencilOperation_DecrementWrap:   return D3D12_STENCIL_OP_DECR;
            default:
                return D3D12_STENCIL_OP_KEEP;
        }
    }

    constexpr D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE ToD3D12(VGPULoadAction action)
    {
        switch (action)
        {
            default:
            case VGPULoadAction_Load:
                return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;

            case VGPULoadAction_Clear:
                return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;

            case VGPULoadAction_DontCare:
                return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
        }
    }

    constexpr D3D12_RENDER_PASS_ENDING_ACCESS_TYPE ToD3D12(VGPUStoreAction action)
    {
        switch (action)
        {
            default:
            case VGPUStoreAction_Store:
                return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

            case VGPUStoreAction_DontCare:
                return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
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

    constexpr D3D12_BLEND D3D12Blend(VGPUBlendFactor factor)
    {
        switch (factor)
        {
            case VGPUBlendFactor_Zero:                      return D3D12_BLEND_ZERO;
            case VGPUBlendFactor_One:                       return D3D12_BLEND_ONE;
            case VGPUBlendFactor_SourceColor:               return D3D12_BLEND_SRC_COLOR;
            case VGPUBlendFactor_OneMinusSourceColor:       return D3D12_BLEND_INV_SRC_COLOR;
            case VGPUBlendFactor_SourceAlpha:               return D3D12_BLEND_SRC_ALPHA;
            case VGPUBlendFactor_OneMinusSourceAlpha:       return D3D12_BLEND_INV_SRC_ALPHA;
            case VGPUBlendFactor_DestinationColor:          return D3D12_BLEND_DEST_COLOR;
            case VGPUBlendFactor_OneMinusDestinationColor:  return D3D12_BLEND_INV_DEST_COLOR;
            case VGPUBlendFactor_DestinationAlpha:          return D3D12_BLEND_DEST_ALPHA;
            case VGPUBlendFactor_OneMinusDestinationAlpha:  return D3D12_BLEND_INV_DEST_ALPHA;
            case VGPUBlendFactor_SourceAlphaSaturated:      return D3D12_BLEND_SRC_ALPHA_SAT;
            case VGPUBlendFactor_BlendColor:                return D3D12_BLEND_BLEND_FACTOR;
            case VGPUBlendFactor_OneMinusBlendColor:        return D3D12_BLEND_INV_BLEND_FACTOR;
            //case VGPUBlendFactor_BlendAlpha:                return D3D12_BLEND_ALPHA_FACTOR;
            //case VGPUBlendFactor_OneMinusBlendAlpha:        return D3D12_BLEND_INV_ALPHA_FACTOR;
            //case VGPUBlendFactor_Source1Color:              return D3D12_BLEND_SRC1_COLOR;
            //case VGPUBlendFactor_OneMinusSource1Color:      return D3D12_BLEND_INV_SRC1_COLOR;
            //case VGPUBlendFactor_Source1Alpha:              return D3D12_BLEND_SRC1_ALPHA;
            //case VGPUBlendFactor_OneMinusSource1Alpha:      return D3D12_BLEND_INV_SRC1_ALPHA;
            default:
                return D3D12_BLEND_ZERO;
        }
    }

    constexpr D3D12_BLEND D3D12AlphaBlend(VGPUBlendFactor factor)
    {
        switch (factor)
        {
            case VGPUBlendFactor_SourceColor:
                return D3D12_BLEND_SRC_ALPHA;
            case VGPUBlendFactor_OneMinusSourceColor:
                return D3D12_BLEND_INV_SRC_ALPHA;
            case VGPUBlendFactor_DestinationColor:
                return D3D12_BLEND_DEST_ALPHA;
            case VGPUBlendFactor_OneMinusDestinationColor:
                return D3D12_BLEND_INV_DEST_ALPHA;
            //case VGPUBlendFactor_Source1Color:
            //    return D3D12_BLEND_SRC1_ALPHA;
            //case VGPUBlendFactor_OneMinusSource1Color:
            //    return D3D12_BLEND_INV_SRC1_ALPHA;
                // Other blend factors translate to the same D3D12 enum as the color blend factors.
            default:
                return D3D12Blend(factor);
        }
    }

    constexpr D3D12_BLEND_OP D3D12BlendOperation(VGPUBlendOperation operation)
    {
        switch (operation)
        {
            case VGPUBlendOperation_Add:                return D3D12_BLEND_OP_ADD;
            case VGPUBlendOperation_Subtract:           return D3D12_BLEND_OP_SUBTRACT;
            case VGPUBlendOperation_ReverseSubtract:    return D3D12_BLEND_OP_REV_SUBTRACT;
            case VGPUBlendOperation_Min:                return D3D12_BLEND_OP_MIN;
            case VGPUBlendOperation_Max:                return D3D12_BLEND_OP_MAX;
            default:                                    return D3D12_BLEND_OP_ADD;
        }
    }

    constexpr uint8_t D3D12RenderTargetWriteMask(VGPUColorWriteMaskFlags writeMask)
    {
        static_assert(static_cast<UINT>(VGPUColorWriteMask_Red) == D3D12_COLOR_WRITE_ENABLE_RED);
        static_assert(static_cast<UINT>(VGPUColorWriteMask_Green) == D3D12_COLOR_WRITE_ENABLE_GREEN);
        static_assert(static_cast<UINT>(VGPUColorWriteMask_Blue) == D3D12_COLOR_WRITE_ENABLE_BLUE);
        static_assert(static_cast<UINT>(VGPUColorWriteMask_Alpha) == D3D12_COLOR_WRITE_ENABLE_ALPHA);

        return static_cast<uint8_t>(writeMask);
    }

    D3D12_DEPTH_STENCILOP_DESC ToD3D12StencilOpDesc(const VGPUStencilFaceState& state)
    {
        D3D12_DEPTH_STENCILOP_DESC desc = {};
        desc.StencilFailOp = ToD3D12(state.failOperation);
        desc.StencilDepthFailOp = ToD3D12(state.depthFailOperation);
        desc.StencilPassOp = ToD3D12(state.passOperation);
        desc.StencilFunc = ToD3D12(state.compareFunction);
        return desc;
    }
}

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define vgpuCreateDXGIFactory2 CreateDXGIFactory2
#define vgpuD3D12CreateDevice D3D12CreateDevice
#define vgpuD3D12GetDebugInterface D3D12GetDebugInterface
#define vgpuD3D12SerializeVersionedRootSignature D3D12SerializeVersionedRootSignature
#endif

struct D3D12DescriptorAllocator final
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
        VGPU_UNUSED(hr);
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

struct D3D12GpuDescriptorHeap final
{
    uint32_t numDescriptors = 0;
    ID3D12DescriptorHeap* handle = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = {};

    // CPU status
    std::atomic<uint64_t> allocationOffset{ 0 };

    // GPU status
    ID3D12Fence* fence = nullptr;
    uint64_t fenceValue = 0;
    uint64_t cachedCompletedValue = 0;

    void SignalGPU(ID3D12CommandQueue* queue)
    {
        // Descriptor heaps' progress is recorded by the GPU:
        fenceValue = allocationOffset.load();
        VHR(queue->Signal(fence, fenceValue));
        cachedCompletedValue = fence->GetCompletedValue();
    }
};

struct D3D12_Renderer;

struct D3D12Resource
{
    D3D12_Renderer* renderer = nullptr;
    ID3D12Resource* handle = nullptr;
    D3D12MA::Allocation* allocation = nullptr;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES transitioningState = (D3D12_RESOURCE_STATES)-1;
};

struct D3D12Buffer : public VGPUBufferImpl, public D3D12Resource
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    uint64_t size = 0;
    uint64_t allocatedSize = 0;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = {};
    void* pMappedData{ nullptr };

    ~D3D12Buffer() override;
    void SetLabel(const char* label) override;

    uint64_t GetSize() const override { return size; }
    VGPUDeviceAddress GetGpuAddress() const override { return gpuAddress; }
};

struct D3D12Texture final : public VGPUTextureImpl, public D3D12Resource
{
    VGPUTextureDimension dimension{};
    uint32_t width = 0;
    uint32_t height = 0;
    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;

    std::unordered_map<size_t, D3D12_CPU_DESCRIPTOR_HANDLE> rtvCache;
    std::unordered_map<size_t, D3D12_CPU_DESCRIPTOR_HANDLE> dsvCache;

    ~D3D12Texture() override;
    void SetLabel(const char* label) override;

    VGPUTextureDimension GetDimension() const override { return dimension; }
};

struct D3D12Sampler
{
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor;
};

struct D3D12Shader
{
    void* byteCode;
    size_t byteCodeSize;
};

struct D3D12PipelineLayout
{
    ID3D12RootSignature* handle = nullptr;

    uint32_t pushConstantsBaseIndex = 0;
};

struct D3D12Pipeline
{
    VGPUPipelineType type = VGPUPipelineType_Render;
    D3D12PipelineLayout* pipelineLayout = nullptr;
    ID3D12PipelineState* handle = nullptr;
    uint32_t numVertexBindings = 0;
    uint32_t strides[D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT] = {};
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
    uint32_t backBufferCount;
    uint32_t syncInterval;
    std::vector<D3D12Texture*> backbufferTextures;
};

struct D3D12_UploadContext
{
    ID3D12CommandAllocator* commandAllocator = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12Fence* fence = nullptr;

    uint64_t uploadBufferSize = 0;
    ID3D12Resource* uploadBuffer = nullptr;
    D3D12MA::Allocation* uploadBufferAllocation = nullptr;
    void* uploadBufferData = nullptr;

    inline bool IsValid() const { return commandList != nullptr; }
};

struct D3D12CommandBuffer
{
    D3D12_Renderer* renderer;
    VGPUCommandQueue queue;
    bool hasLabel;

    ID3D12CommandAllocator* commandAllocators[VGPU_MAX_INFLIGHT_FRAMES];
    ID3D12GraphicsCommandList4* commandList;

    D3D12_RESOURCE_BARRIER resourceBarriers[16];
    UINT numBarriersToFlush;

    bool insideRenderPass = false;
    D3D12Pipeline* currentPipeline = nullptr;

    D3D12_VERTEX_BUFFER_VIEW vboViews[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};

    D3D12_RENDER_PASS_RENDER_TARGET_DESC RTVs[VGPU_MAX_COLOR_ATTACHMENTS] = {};
    // Due to a API bug, this resolve_subresources array must be kept alive between BeginRenderpass() and EndRenderpass()!
    D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS resolveSubresources[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};

    std::vector<D3D12_SwapChain*> swapChains;
};

struct D3D12Queue final
{
    ID3D12CommandQueue* handle = nullptr;
    ID3D12Fence* fence = nullptr;
    ID3D12Fence* frameFences[VGPU_MAX_INFLIGHT_FRAMES] = {};
    std::vector<ID3D12CommandList*> submitCommandLists;
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

    D3D12MA::Allocator* allocator = nullptr;
    CD3DX12FeatureSupport d3dFeatures;
    D3D12Queue queues[_VGPUCommandQueue_Count] = {};

    /* Command contexts */
    std::mutex cmdBuffersLocker;
    uint32_t cmdBuffersCount{ 0 };
    std::vector<VGPUCommandBuffer_T*> commandBuffers;

    uint32_t frameIndex = 0;
    uint64_t frameCount = 0;

    std::mutex uploadLocker;
    std::vector<D3D12_UploadContext> uploadFreeList;

    D3D12DescriptorAllocator resourceAllocator;
    D3D12DescriptorAllocator samplerAllocator;
    D3D12DescriptorAllocator rtvAllocator;
    D3D12DescriptorAllocator dsvAllocator;

    D3D12GpuDescriptorHeap resourceDescriptorHeap;
    D3D12GpuDescriptorHeap samplerDescriptorHeap;

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

    if (renderer->shuttingDown || renderer->device == nullptr)
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

static D3D12_UploadContext d3d12_AllocateUpload(D3D12_Renderer* renderer, uint64_t size)
{
    D3D12_UploadContext context;

    renderer->uploadLocker.lock();
    // Try to search for a staging buffer that can fit the request:
    for (size_t i = 0; i < renderer->uploadFreeList.size(); ++i)
    {
        if (renderer->uploadFreeList[i].uploadBuffer != nullptr &&
            renderer->uploadFreeList[i].uploadBufferSize >= size)
        {
            if (renderer->uploadFreeList[i].fence->GetCompletedValue() == 1)
            {
                VHR(renderer->uploadFreeList[i].fence->Signal(0));
                context = std::move(renderer->uploadFreeList[i]);
                std::swap(renderer->uploadFreeList[i], renderer->uploadFreeList.back());
                renderer->uploadFreeList.pop_back();
                break;
            }
        }
    }
    renderer->uploadLocker.unlock();

    // If no buffer was found that fits the data, create one:
    if (!context.IsValid())
    {
        VHR(renderer->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&context.commandAllocator)));
        VHR(renderer->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, context.commandAllocator, nullptr, IID_PPV_ARGS(&context.commandList)));
        VHR(context.commandList->Close());
        VHR(renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&context.fence)));

        context.uploadBufferSize = vgpuNextPowerOfTwo(size);

        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC resourceDesc;
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = context.uploadBufferSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        const HRESULT hr = renderer->allocator->CreateResource(
            &allocationDesc,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            &context.uploadBufferAllocation,
            IID_PPV_ARGS(&context.uploadBuffer)
        );
        VHR(hr);

        D3D12_RANGE readRange = {};
        VHR(context.uploadBuffer->Map(0, &readRange, &context.uploadBufferData));
    }

    // Begin command list in valid state
    VHR(context.commandAllocator->Reset());
    VHR(context.commandList->Reset(context.commandAllocator, nullptr));

    return context;
}

static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_GetRTV(D3D12_Renderer* renderer, D3D12Texture* texture, uint32_t mipLevel, uint32_t slice)
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
                viewDesc.Texture3D.WSize = static_cast<UINT>(-1);
                break;
            }
            break;

            default:
                vgpu_log_error("D3D12: Invalid texture dimension");
                return {};
        }

        D3D12_CPU_DESCRIPTOR_HANDLE newView = renderer->rtvAllocator.Allocate();
        renderer->device->CreateRenderTargetView(texture->handle, &viewDesc, newView);
        texture->rtvCache[hash] = newView;
        return newView;
    }

    return it->second;
}

static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_GetDSV(D3D12_Renderer* renderer, D3D12Texture* texture, uint32_t mipLevel, uint32_t slice)
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
                vgpu_log_error("D3D12: Cannot create 3D texture DSV");
                return {};

            default:
                vgpu_log_error("D3D12: Invalid texture dimension");
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
    VHR(context.commandList->Close());

    ID3D12CommandList* commandlists[] = {
        context.commandList
    };
    renderer->queues[VGPUCommandQueue_Copy].handle->ExecuteCommandLists(1, commandlists);
    VHR(renderer->queues[VGPUCommandQueue_Copy].handle->Signal(context.fence, 1));

    VHR(renderer->queues[VGPUCommandQueue_Graphics].handle->Wait(context.fence, 1));
    VHR(renderer->queues[VGPUCommandQueue_Compute].handle->Wait(context.fence, 1));

    renderer->uploadLocker.lock();
    renderer->uploadFreeList.push_back(context);
    renderer->uploadLocker.unlock();
}

/* D3D12Buffer */
D3D12Buffer::~D3D12Buffer()
{
    d3d12_DeferDestroy(renderer, handle, allocation);
}

void D3D12Buffer::SetLabel(const char* label)
{
    D3D12SetName(handle, label);
}

/* D3D12Texture */
D3D12Texture::~D3D12Texture()
{
    d3d12_DeferDestroy(renderer, handle, allocation);
    for (auto& it : rtvCache)
    {
        renderer->rtvAllocator.Free(it.second);
    }
    rtvCache.clear();
    for (auto& it : dsvCache)
    {
        renderer->dsvAllocator.Free(it.second);
    }
    dsvCache.clear();
}


void D3D12Texture::SetLabel(const char* label)
{
    D3D12SetName(handle, label);
}

static void d3d12_destroyDevice(VGPUDevice device)
{
    // Wait idle
    vgpuWaitIdle(device);

    D3D12_Renderer* renderer = (D3D12_Renderer*)device->driverData;
    renderer->shuttingDown = true;

    renderer->frameCount = UINT64_MAX;
    d3d12_ProcessDeletionQueue(renderer);
    renderer->frameCount = 0;

    // Upload/Copy allocations
    {
        for (auto& item : renderer->uploadFreeList)
        {
            item.uploadBuffer->Release();
            item.uploadBufferAllocation->Release();
            item.commandAllocator->Release();
            item.commandList->Release();
            item.fence->Release();
        }
        renderer->uploadFreeList.clear();
    }

    // CPU descriptor allocators
    {
        renderer->resourceAllocator.Shutdown();
        renderer->samplerAllocator.Shutdown();
        renderer->rtvAllocator.Shutdown();
        renderer->dsvAllocator.Shutdown();

        // GPU Heaps
        SAFE_RELEASE(renderer->resourceDescriptorHeap.handle);
        SAFE_RELEASE(renderer->resourceDescriptorHeap.fence);
        SAFE_RELEASE(renderer->samplerDescriptorHeap.handle);
        SAFE_RELEASE(renderer->samplerDescriptorHeap.fence);
    }

    SAFE_RELEASE(renderer->dispatchIndirectCommandSignature);
    SAFE_RELEASE(renderer->drawIndirectCommandSignature);
    SAFE_RELEASE(renderer->drawIndexedIndirectCommandSignature);
    SAFE_RELEASE(renderer->dispatchMeshIndirectCommandSignature);

    for (size_t i = 0; i < renderer->commandBuffers.size(); ++i)
    {
        D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)renderer->commandBuffers[i]->driverData;
        for (uint32_t frameIndex = 0; frameIndex < VGPU_MAX_INFLIGHT_FRAMES; ++frameIndex)
        {
            SAFE_RELEASE(commandBuffer->commandAllocators[frameIndex]);
        }
        SAFE_RELEASE(commandBuffer->commandList);
    }
    renderer->commandBuffers.clear();

    for (uint32_t queue = 0; queue < _VGPUCommandQueue_Count; ++queue)
    {
        SAFE_RELEASE(renderer->queues[queue].handle);
        SAFE_RELEASE(renderer->queues[queue].fence);

        for (uint32_t frameIndex = 0; frameIndex < VGPU_MAX_INFLIGHT_FRAMES; ++frameIndex)
        {
            SAFE_RELEASE(renderer->queues[queue].frameFences[frameIndex]);
        }
    }

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

            ID3D12DebugDevice* debugDevice = nullptr;
            if (SUCCEEDED(renderer->device->QueryInterface(&debugDevice)))
            {
                debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
                debugDevice->Release();
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
    delete device;
}

static void d3d12_updateSwapChain(D3D12_Renderer* renderer, D3D12_SwapChain* swapChain)
{
    VGPU_UNUSED(renderer);

    HRESULT hr = S_OK;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    hr = swapChain->handle->GetDesc1(&swapChainDesc);
    VGPU_ASSERT(SUCCEEDED(hr));

    swapChain->backbufferTextures.resize(swapChainDesc.BufferCount);
    for (uint32_t i = 0; i < swapChainDesc.BufferCount; ++i)
    {
        D3D12Texture* texture = new D3D12Texture();
        texture->renderer = renderer;
        texture->dimension = VGPUTextureDimension_2D;
        texture->state = D3D12_RESOURCE_STATE_PRESENT;
        texture->width = swapChainDesc.Width;
        texture->height = swapChainDesc.Height;
        texture->dxgiFormat = ToDxgiFormat(swapChain->format);
        hr = swapChain->handle->GetBuffer(i, IID_PPV_ARGS(&texture->handle));
        VGPU_ASSERT(SUCCEEDED(hr));

        wchar_t name[25] = {};
        swprintf_s(name, L"Render target %u", i);
        texture->handle->SetName(name);
        swapChain->backbufferTextures[i] = texture;
    }
}

static void d3d12_waitIdle(VGPURenderer* driverData)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    ComPtr<ID3D12Fence> fence;
    VHR(renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    // Wait for the GPU to fully catch up with the CPU
    for (uint32_t queue = 0; queue < _VGPUCommandQueue_Count; ++queue)
    {
        VHR(renderer->queues[queue].handle->Signal(fence.Get(), 1));

        if (fence->GetCompletedValue() < 1)
        {
            VHR(fence->SetEventOnCompletion(1, NULL));
        }

        VHR(fence->Signal(0));
    }

    // Safe delete deferred destroys
    d3d12_ProcessDeletionQueue(renderer);
}

static VGPUBackend d3d12_getBackendType(void)
{
    return VGPUBackend_D3D12;
}

static VGPUBool32 d3d12_queryFeature(VGPURenderer* driverData, VGPUFeature feature, void* pInfo, uint32_t infoSize)
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
        case VGPUFeature_Predication:
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

static void d3d12_getAdapterProperties(VGPURenderer* driverData, VGPUAdapterProperties* properties)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    properties->vendorID = renderer->vendorID;
    properties->deviceID = renderer->deviceID;
    properties->name = renderer->adapterName.c_str();
    properties->driverDescription = renderer->driverDescription.c_str();
    properties->adapterType = renderer->adapterType;
}

static void d3d12_getLimits(VGPURenderer* driverData, VGPULimits* limits)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    D3D12_FEATURE_DATA_D3D12_OPTIONS featureData = {};
    HRESULT hr = renderer->device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &featureData, sizeof(featureData));
    if (FAILED(hr))
    {
    }

    limits->maxTextureDimension1D = D3D12_REQ_TEXTURE1D_U_DIMENSION;
    limits->maxTextureDimension2D = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    limits->maxTextureDimension3D = D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
    limits->maxTextureDimensionCube = D3D12_REQ_TEXTURECUBE_DIMENSION;
    limits->maxTextureArrayLayers = D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    limits->maxConstantBufferBindingSize = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
    // D3D12 has no documented limit on the size of a storage buffer binding.
    limits->maxStorageBufferBindingSize = 4294967295;
    limits->minUniformBufferOffsetAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    limits->minStorageBufferOffsetAlignment = 32;
    limits->maxVertexBuffers = 16;
    limits->maxVertexAttributes = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
    limits->maxVertexBufferArrayStride = 2048u;

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

    limits->maxViewports = D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    limits->maxViewportDimensions[0] = D3D12_VIEWPORT_BOUNDS_MAX;
    limits->maxViewportDimensions[1] = D3D12_VIEWPORT_BOUNDS_MAX;
    limits->maxColorAttachments = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
}

static void d3d12_setLabel(VGPURenderer* driverData, const char* label)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;
    D3D12SetName(renderer->device, label);
}

/* Buffer */
static VGPUBuffer d3d12_createBuffer(VGPURenderer* driverData, const VGPUBufferDescriptor* desc, const void* pInitialData)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    if (desc->handle)
    {
        D3D12Buffer* buffer = new D3D12Buffer();
        buffer->renderer = renderer;
        buffer->handle = (ID3D12Resource*)desc->handle;
        buffer->handle->AddRef();
        buffer->allocation = nullptr;
        buffer->state = D3D12_RESOURCE_STATE_COMMON;
        buffer->size = desc->size;
        buffer->allocatedSize = 0u;

        if (desc->label)
        {
            D3D12SetName(buffer->handle, desc->label);
        }

        buffer->gpuAddress = buffer->handle->GetGPUVirtualAddress();

        return (VGPUBuffer)buffer;
    }

    UINT64 alignedSize = desc->size;
    if (desc->usage & VGPUBufferUsage_Constant)
    {
        alignedSize = AlignUp<UINT64>(alignedSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    }

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = alignedSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    if (desc->usage & VGPUBufferUsage_ShaderWrite)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    if (!(desc->usage & VGPUBufferUsage_ShaderRead) &&
        !(desc->usage & VGPUBufferUsage_RayTracing))
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
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

    D3D12Buffer* buffer = new D3D12Buffer();
    buffer->renderer = renderer;
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
        vgpu_log_error("D3D12: Failed to create buffer");
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
        if (desc->cpuAccess == VGPUCpuAccessMode_Write)
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

    return buffer;
}

static VGPUDeviceAddress d3d12_buffer_get_device_address(VGPUBuffer resource)
{
    D3D12Buffer* d3dBuffer = (D3D12Buffer*)resource;
    return d3dBuffer->gpuAddress;
}

/* Texture */
static VGPUTexture d3d12_createTexture(VGPURenderer* driverData, const VGPUTextureDesc* desc, const void* pInitialData)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    D3D12MA::ALLOCATION_DESC allocationDesc = {};
    allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc;
    if (desc->dimension == VGPUTextureDimension_1D)
    {
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    }
    else if (desc->dimension == VGPUTextureDimension_3D)
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
    resourceDesc.DepthOrArraySize = (UINT16)desc->depthOrArrayLayers;
    resourceDesc.MipLevels = (UINT16)desc->mipLevelCount;
    resourceDesc.Format = ToDxgiFormat(desc->format);
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
    D3D12Texture* texture = new D3D12Texture();
    texture->renderer = renderer;
    texture->dimension = desc->dimension;
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
        vgpu_log_error("D3D12: Failed to create texture");
        delete texture;
        return nullptr;
    }

    if (desc->label)
    {
        D3D12SetName(texture->handle, desc->label);
    }

    return (VGPUTexture)texture;
}


/* Sampler */
static VGPUSampler d3d12_createSampler(VGPURenderer* driverData, const VGPUSamplerDesc* desc)
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
    samplerDesc.MaxAnisotropy = _VGPU_MIN(desc->maxAnisotropy, 16u);
    samplerDesc.ComparisonFunc = ToD3D12(desc->compareFunction);
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

static void d3d12_destroySampler(VGPURenderer* driverData, VGPUSampler resource)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;
    D3D12Sampler* sampler = (D3D12Sampler*)resource;
    renderer->samplerAllocator.Free(sampler->descriptor);
    delete sampler;
}

/* ShaderModule */
static VGPUShaderModule d3d12_createShaderModule(VGPURenderer* driverData, const void* pCode, size_t codeSize)
{
    VGPU_UNUSED(driverData);

    D3D12Shader* shader = new D3D12Shader();
    shader->byteCode = malloc(codeSize);
    shader->byteCodeSize = codeSize;

    memcpy(shader->byteCode, pCode, codeSize);
    return (VGPUShaderModule)shader;
}

static void d3d12_destroyShaderModule(VGPURenderer* driverData, VGPUShaderModule resource)
{
    VGPU_UNUSED(driverData);

    D3D12Shader* shader = (D3D12Shader*)resource;
    free(shader->byteCode);
    delete shader;
}

/* PipelineLayout */
static HRESULT d3d12_CreateRootSignature(ID3D12Device* device, ID3D12RootSignature** rootSignature, const D3D12_ROOT_SIGNATURE_DESC1& desc)
{
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc = { };
    versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    versionedDesc.Desc_1_1 = desc;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = vgpuD3D12SerializeVersionedRootSignature(&versionedDesc, &signature, &error);
    if (FAILED(hr))
    {
        const char* errString = error ? reinterpret_cast<const char*>(error->GetBufferPointer()) : "";

        vgpu_log_error("Failed to create root signature: %S", errString);
    }

    return device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature));
}

static VGPUPipelineLayout d3d12_createPipelineLayout(VGPURenderer* driverData, const VGPUPipelineLayoutDescriptor* descriptor)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;
    D3D12PipelineLayout* layout = new D3D12PipelineLayout();

    uint32_t rangeMax = 0;
    for (uint32_t i = 0; i < descriptor->descriptorSetCount; i++)
    {
        rangeMax += descriptor->descriptorSets[i].rangeCount;
    }

    uint32_t totalRangeNum = 0;
    std::vector<D3D12_ROOT_PARAMETER1> rootParameters;
    std::vector<D3D12_DESCRIPTOR_RANGE1> descriptorRanges(rangeMax);
    std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;

    if (descriptor->pushConstantCount > 0)
    {
        layout->pushConstantsBaseIndex = (uint32_t)rootParameters.size();

        D3D12_ROOT_PARAMETER1 rootParameterLocal = {};
        rootParameterLocal.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        for (uint32_t i = 0; i < descriptor->pushConstantCount; i++)
        {
            rootParameterLocal.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // GetShaderVisibility(pipelineLayoutDesc.pushConstants[i].visibility);
            rootParameterLocal.Constants.ShaderRegister = descriptor->pushConstants[i].shaderRegister;
            rootParameterLocal.Constants.RegisterSpace = 0;
            rootParameterLocal.Constants.Num32BitValues = descriptor->pushConstants[i].size / 4;
            rootParameters.push_back(rootParameterLocal);
        }
    }
    VGPU_UNUSED(totalRangeNum);

    D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = { };
    rootSignatureDesc.NumParameters = (UINT)rootParameters.size();
    rootSignatureDesc.pParameters = rootParameters.data();
    rootSignatureDesc.NumStaticSamplers = (UINT)staticSamplers.size();
    rootSignatureDesc.pStaticSamplers = staticSamplers.data();
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

#ifdef USING_D3D12_AGILITY_SDK
    rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
#endif

    const HRESULT hr = d3d12_CreateRootSignature(renderer->device, &layout->handle, rootSignatureDesc);
    if (FAILED(hr))
    {
        delete layout;
        return nullptr;
    }

    return (VGPUPipelineLayout)layout;
}

static void d3d12_destroyPipelineLayout(VGPURenderer* driverData, VGPUPipelineLayout resource)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;
    D3D12PipelineLayout* layout = (D3D12PipelineLayout*)resource;

    d3d12_DeferDestroy(renderer, layout->handle, nullptr);
    delete layout;
}

/* Pipeline */
static VGPUPipeline d3d12_createRenderPipeline(VGPURenderer* driverData, const VGPURenderPipelineDesc* desc)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    D3D12Pipeline* pipeline = new D3D12Pipeline();
    pipeline->type = VGPUPipelineType_Render;
    pipeline->pipelineLayout = (D3D12PipelineLayout*)desc->layout;
    D3D12Shader* vertexShader = (D3D12Shader*)desc->vertex.module;
    D3D12Shader* fragmentShader = (D3D12Shader*)desc->fragment;

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;

    for (uint32_t binding = 0; binding < desc->vertex.layoutCount; ++binding)
    {
        const VGPUVertexBufferLayout& layout = desc->vertex.layouts[binding];

        for (uint32_t attributeIndex = 0; attributeIndex < layout.attributeCount; ++attributeIndex)
        {
            const VGPUVertexAttribute& attribute = layout.attributes[attributeIndex];

            D3D12_INPUT_ELEMENT_DESC inputElement;
            inputElement.SemanticName = "ATTRIBUTE";
            inputElement.SemanticIndex = attribute.shaderLocation;
            inputElement.Format = ToDxgiFormat(attribute.format);
            inputElement.InputSlot = binding;
            inputElement.AlignedByteOffset = attribute.offset;

            pipeline->numVertexBindings = _VGPU_MAX(binding + 1, pipeline->numVertexBindings);
            pipeline->strides[binding] = layout.stride;

            if (layout.stepMode == VGPUVertexStepMode_Vertex)
            {
                inputElement.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                inputElement.InstanceDataStepRate = 0;
            }
            else
            {
                inputElement.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                inputElement.InstanceDataStepRate = 1;
            }

            inputElementDescs.push_back(inputElement);
        }
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC d3dDesc = {};
    d3dDesc.pRootSignature = pipeline->pipelineLayout->handle;
    d3dDesc.VS = { vertexShader->byteCode, vertexShader->byteCodeSize };
    d3dDesc.PS = { fragmentShader->byteCode, fragmentShader->byteCodeSize };

    // Color Attachments + RTV
    D3D12_BLEND_DESC blendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    blendState.AlphaToCoverageEnable = desc->alphaToCoverageEnabled;
    blendState.IndependentBlendEnable = TRUE;
    for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i)
    {
        VGPU_ASSERT(desc->colorAttachments[i].format != VGPUTextureFormat_Undefined);

        const RenderPipelineColorAttachmentDesc& attachment = desc->colorAttachments[i];

        blendState.RenderTarget[i].BlendEnable = attachment.blendEnabled;
        blendState.RenderTarget[i].LogicOpEnable = FALSE;
        blendState.RenderTarget[i].SrcBlend = D3D12Blend(attachment.srcColorBlendFactor);
        blendState.RenderTarget[i].DestBlend = D3D12Blend(attachment.dstColorBlendFactor);
        blendState.RenderTarget[i].BlendOp = D3D12BlendOperation(attachment.colorBlendOperation);
        blendState.RenderTarget[i].SrcBlendAlpha = D3D12AlphaBlend(attachment.srcAlphaBlendFactor);
        blendState.RenderTarget[i].DestBlendAlpha = D3D12AlphaBlend(attachment.dstAlphaBlendFactor);
        blendState.RenderTarget[i].BlendOpAlpha = D3D12BlendOperation(attachment.alphaBlendOperation);
        blendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
        blendState.RenderTarget[i].RenderTargetWriteMask = D3D12RenderTargetWriteMask(attachment.colorWriteMask);

        // RTV
        d3dDesc.RTVFormats[d3dDesc.NumRenderTargets] = ToDxgiFormat(desc->colorAttachments[i].format);
        d3dDesc.NumRenderTargets++;
    }

    d3dDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    d3dDesc.SampleMask = UINT_MAX;
    d3dDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    // DepthStencilState
    const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    if (desc->depthStencilState.format != VGPUTextureFormat_Undefined)
    {
        d3dDesc.DepthStencilState.DepthEnable = desc->depthStencilState.depthCompareFunction != VGPUCompareFunction_Always || desc->depthStencilState.depthWriteEnabled;
        d3dDesc.DepthStencilState.DepthWriteMask = desc->depthStencilState.depthWriteEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        d3dDesc.DepthStencilState.DepthFunc = ToD3D12(desc->depthStencilState.depthCompareFunction);
        d3dDesc.DepthStencilState.StencilEnable = vgpuStencilTestEnabled(&desc->depthStencilState);
        d3dDesc.DepthStencilState.StencilReadMask = static_cast<UINT8>(desc->depthStencilState.stencilReadMask);
        d3dDesc.DepthStencilState.StencilWriteMask = static_cast<UINT8>(desc->depthStencilState.stencilWriteMask);
        d3dDesc.DepthStencilState.FrontFace = ToD3D12StencilOpDesc(desc->depthStencilState.stencilFront);
        d3dDesc.DepthStencilState.BackFace = ToD3D12StencilOpDesc(desc->depthStencilState.stencilBack);
    }
    else
    {
        d3dDesc.DepthStencilState.DepthEnable = FALSE;
        d3dDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        d3dDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        d3dDesc.DepthStencilState.StencilEnable = FALSE;
        d3dDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        d3dDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        d3dDesc.DepthStencilState.FrontFace = defaultStencilOp;
        d3dDesc.DepthStencilState.BackFace = defaultStencilOp;
        //d3dDesc.DepthStencilState.DepthBoundsTestEnable = FALSE;
    }
    d3dDesc.InputLayout = { inputElementDescs.data(), (UINT)inputElementDescs.size()};
    d3dDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

    switch (desc->primitive.topology)
    {
        case VGPUPrimitiveTopology_PointList:
            d3dDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            break;
        case VGPUPrimitiveTopology_LineList:
        case VGPUPrimitiveTopology_LineStrip:
            d3dDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            break;
        case VGPUPrimitiveTopology_TriangleList:
        case VGPUPrimitiveTopology_TriangleStrip:
            d3dDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            break;
        case VGPUPrimitiveTopology_PatchList:
            d3dDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
            break;
    }

    // DSV format
    d3dDesc.DSVFormat = ToDxgiFormat(desc->depthStencilState.format);
    d3dDesc.SampleDesc.Count = desc->sampleCount;

    if (FAILED(renderer->device->CreateGraphicsPipelineState(&d3dDesc, IID_PPV_ARGS(&pipeline->handle))))
    {
        delete pipeline;
        return nullptr;
    }

    D3D12SetName(pipeline->handle, desc->label);
    pipeline->primitiveTopology = ToD3DPrimitiveTopology(desc->primitive.topology, desc->primitive.patchControlPoints);
    return (VGPUPipeline)pipeline;
}

static VGPUPipeline d3d12_createComputePipeline(VGPURenderer* driverData, const VGPUComputePipelineDescriptor* desc)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    D3D12Pipeline* pipeline = new D3D12Pipeline();
    pipeline->type = VGPUPipelineType_Compute;
    pipeline->pipelineLayout = (D3D12PipelineLayout*)desc->layout;
    D3D12Shader* shader = (D3D12Shader*)desc->shader;

    D3D12_SHADER_BYTECODE cs = { shader->byteCode, shader->byteCodeSize };

    D3D12_COMPUTE_PIPELINE_STATE_DESC d3dDesc = {};
    d3dDesc.pRootSignature = pipeline->pipelineLayout->handle;
    d3dDesc.CS = cs;

    if (FAILED(renderer->device->CreateComputePipelineState(&d3dDesc, IID_PPV_ARGS(&pipeline->handle))))
    {
        delete pipeline;
        return nullptr;
    }

    D3D12SetName(pipeline->handle, desc->label);
    return (VGPUPipeline)pipeline;
}

static VGPUPipeline d3d12_createRayTracingPipeline(VGPURenderer* driverData, const VGPURayTracingPipelineDesc* desc)
{
    VGPU_UNUSED(desc);
    VGPU_UNUSED(driverData);

    //D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;
    D3D12Pipeline* pipeline = new D3D12Pipeline();
    pipeline->type = VGPUPipelineType_RayTracing;
    return (VGPUPipeline)pipeline;
}

static void d3d12_destroyPipeline(VGPURenderer* driverData, VGPUPipeline resource)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;
    D3D12Pipeline* pipeline = (D3D12Pipeline*)resource;

    d3d12_DeferDestroy(renderer, pipeline->handle, nullptr);
    delete pipeline;
}

/* SwapChain */
static VGPUSwapChain* d3d12_createSwapChain(VGPURenderer* driverData, void* windowHandle, const VGPUSwapChainDesc* desc)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = desc->width;
    swapChainDesc.Height = desc->height;
    swapChainDesc.Format = ToDxgiFormat(ToDXGISwapChainFormat(desc->format));
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
        renderer->queues[VGPUCommandQueue_Graphics].handle,
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
        renderer->queues[VGPUCommandQueue_Graphics].handle,
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
    swapChain->format = desc->format;
    swapChain->backBufferCount = swapChainDesc.BufferCount;
    swapChain->syncInterval = PresentModeToSwapInterval(desc->presentMode);
    d3d12_updateSwapChain(renderer, swapChain);
    return (VGPUSwapChain*)swapChain;
}

static void d3d12_destroySwapChain(VGPURenderer* driverData, VGPUSwapChain* swapChain)
{
    VGPU_UNUSED(driverData);
    D3D12_SwapChain* d3d12SwapChain = (D3D12_SwapChain*)swapChain;

    for (size_t i = 0, count = d3d12SwapChain->backbufferTextures.size(); i < count; ++i)
    {
        delete d3d12SwapChain->backbufferTextures[i];
    }
    d3d12SwapChain->backbufferTextures.clear();
    d3d12SwapChain->handle->Release();

    delete d3d12SwapChain;
}

static VGPUTextureFormat d3d12_getSwapChainFormat(VGPURenderer*, VGPUSwapChain* swapChain)
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

    if (commandBuffer->queue == VGPUCommandQueue_Compute)
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
    D3D12Pipeline* newPipeline = (D3D12Pipeline*)pipeline;

    if (commandBuffer->currentPipeline == newPipeline)
        return;

    commandBuffer->currentPipeline = newPipeline;

    commandBuffer->commandList->SetPipelineState(newPipeline->handle);
    if (newPipeline->type == VGPUPipelineType_Render)
    {
        commandBuffer->commandList->IASetPrimitiveTopology(newPipeline->primitiveTopology);
        commandBuffer->commandList->SetGraphicsRootSignature(newPipeline->pipelineLayout->handle);
    }
    else
    {
        commandBuffer->commandList->SetGraphicsRootSignature(newPipeline->pipelineLayout->handle);
    }
}

static void d3d12_prepareDispatch(D3D12CommandBuffer* commandBuffer)
{
    VGPU_VERIFY(commandBuffer->insideRenderPass);
}

static void d3d12_dispatch(VGPUCommandBufferImpl* driverData, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    d3d12_prepareDispatch(commandBuffer);
    commandBuffer->commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

static void d3d12_dispatchIndirect(VGPUCommandBufferImpl* driverData, VGPUBuffer buffer, uint64_t offset)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    d3d12_prepareDispatch(commandBuffer);

    D3D12Resource* d3dBuffer = (D3D12Resource*)buffer;
    commandBuffer->commandList->ExecuteIndirect(commandBuffer->renderer->dispatchIndirectCommandSignature,
        1,
        d3dBuffer->handle, offset,
        nullptr,
        0);
}

static VGPUTexture d3d12_acquireSwapchainTexture(VGPUCommandBufferImpl* driverData, VGPUSwapChain* swapChain, uint32_t* pWidth, uint32_t* pHeight)
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
        d3d12_waitIdle((VGPURenderer*)commandBuffer->renderer);

        // Release resources that are tied to the swap chain and update fence values.
        for (size_t i = 0, count = d3d12SwapChain->backbufferTextures.size(); i < count; ++i)
        {
            delete d3d12SwapChain->backbufferTextures[i];
        }
        d3d12SwapChain->backbufferTextures.clear();

        hr = d3d12SwapChain->handle->ResizeBuffers(
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
                vgpu_log_error("Could not resize swapchain");
                return nullptr;
            }

            d3d12_updateSwapChain(commandBuffer->renderer, d3d12SwapChain);
        }
    }

    D3D12Texture* swapChainTexture =
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

    uint32_t width = UINT32_MAX;
    uint32_t height = UINT32_MAX;
    uint32_t numRTVS = 0;
    D3D12_RENDER_PASS_FLAGS renderPassFlags = D3D12_RENDER_PASS_FLAG_NONE;
    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC DSV = {};

    for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i)
    {
        const VGPURenderPassColorAttachment* attachment = &desc->colorAttachments[i];
        D3D12Texture* texture = (D3D12Texture*)attachment->texture;
        const uint32_t level = attachment->level;
        const uint32_t slice = attachment->slice;

        commandBuffer->RTVs[i].cpuDescriptor = d3d12_GetRTV(commandBuffer->renderer, texture, level, slice);

        // Transition to RenderTarget
        d3d12_TransitionResource(commandBuffer, texture, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

        commandBuffer->RTVs[numRTVS].BeginningAccess.Type = ToD3D12(attachment->loadAction);
        if (attachment->loadAction == VGPULoadAction_Clear)
        {
            commandBuffer->RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Format = texture->dxgiFormat;
            commandBuffer->RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Color[0] = attachment->clearColor.r;
            commandBuffer->RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Color[1] = attachment->clearColor.g;
            commandBuffer->RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Color[2] = attachment->clearColor.b;
            commandBuffer->RTVs[numRTVS].BeginningAccess.Clear.ClearValue.Color[3] = attachment->clearColor.a;
        }

        // TODO: Resolve
        commandBuffer->RTVs[numRTVS].EndingAccess.Type = ToD3D12(attachment->storeAction);

        width = _VGPU_MIN(width, _VGPU_MAX(1U, texture->width >> level));
        height = _VGPU_MIN(height, _VGPU_MAX(1U, texture->height >> level));

        numRTVS++;
    }

    const bool hasDepthStencil = desc->depthStencilAttachment != nullptr;
    if (hasDepthStencil)
    {
        const VGPURenderPassDepthStencilAttachment* attachment = desc->depthStencilAttachment;
        D3D12Texture* texture = (D3D12Texture*)attachment->texture;
        const uint32_t level = attachment->level;
        const uint32_t slice = attachment->slice;

        width = _VGPU_MIN(width, _VGPU_MAX(1U, texture->width >> level));
        height = _VGPU_MIN(height, _VGPU_MAX(1U, texture->height >> level));

        DSV.cpuDescriptor = d3d12_GetDSV(commandBuffer->renderer, texture, level, slice);

        DSV.DepthBeginningAccess.Type = ToD3D12(attachment->depthLoadOp);
        if (attachment->depthLoadOp == VGPULoadAction_Clear)
        {
            DSV.DepthBeginningAccess.Clear.ClearValue.Format = texture->dxgiFormat;
            DSV.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = attachment->clearDepth;
        }
        DSV.DepthEndingAccess.Type = ToD3D12(attachment->depthStoreOp);

        DSV.StencilBeginningAccess.Type = ToD3D12(attachment->stencilLoadOp);
        if (attachment->stencilLoadOp == VGPULoadAction_Clear)
        {
            DSV.StencilBeginningAccess.Clear.ClearValue.Format = texture->dxgiFormat;
            DSV.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = attachment->clearStencil;
        }
        DSV.StencilEndingAccess.Type = ToD3D12(attachment->stencilStoreOp);
    }

    commandBuffer->commandList->BeginRenderPass(numRTVS,
        commandBuffer->RTVs,
        hasDepthStencil ? &DSV : nullptr,
        renderPassFlags);

    // Set the viewport.
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    D3D12_RECT scissorRect = { 0, 0, LONG(width), LONG(height) };
    commandBuffer->commandList->RSSetViewports(1, &viewport);
    commandBuffer->commandList->RSSetScissorRects(1, &scissorRect);
    commandBuffer->insideRenderPass = true;
}

static void d3d12_endRenderPass(VGPUCommandBufferImpl* driverData)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    commandBuffer->commandList->EndRenderPass();
    commandBuffer->insideRenderPass = false;
}

static void d3d12_setViewport(VGPUCommandBufferImpl* driverData, const VGPUViewport* viewport)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;

    commandBuffer->commandList->RSSetViewports(1, (const D3D12_VIEWPORT*)viewport);
}

static void d3d12_setViewports(VGPUCommandBufferImpl* driverData, uint32_t count, const VGPUViewport* viewports)
{
    VGPU_ASSERT(count < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);

    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;

    commandBuffer->commandList->RSSetViewports(count, (const D3D12_VIEWPORT*)viewports);
}

static void d3d12_setScissorRect(VGPUCommandBufferImpl* driverData, const VGPURect* rect)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;

    D3D12_RECT d3d_rect = {};
    d3d_rect.left = LONG(rect->x);
    d3d_rect.top = LONG(rect->y);
    d3d_rect.right = LONG(rect->x + rect->width);
    d3d_rect.bottom = LONG(rect->y + rect->height);
    commandBuffer->commandList->RSSetScissorRects(1u, &d3d_rect);
}

static void d3d12_setScissorRects(VGPUCommandBufferImpl* driverData, uint32_t count, const VGPURect* rects)
{
    VGPU_ASSERT(count < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);

    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;

    D3D12_RECT d3dScissorRects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    for (uint32_t i = 0; i < count; i++)
    {
        d3dScissorRects[i].left = LONG(rects[i].x);
        d3dScissorRects[i].top = LONG(rects[i].y);
        d3dScissorRects[i].right = LONG(rects[i].x + rects[i].width);
        d3dScissorRects[i].bottom = LONG(rects[i].y + rects[i].height);
    }
    commandBuffer->commandList->RSSetScissorRects(count, d3dScissorRects);
}

static void d3d12_setVertexBuffer(VGPUCommandBufferImpl* driverData, uint32_t index, VGPUBuffer buffer, uint64_t offset)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    D3D12Buffer* d3d12Buffer = (D3D12Buffer*)buffer;

    commandBuffer->vboViews[index].BufferLocation = d3d12Buffer->gpuAddress + (D3D12_GPU_VIRTUAL_ADDRESS)offset;
    commandBuffer->vboViews[index].SizeInBytes = (UINT)(d3d12Buffer->size - offset);
    commandBuffer->vboViews[index].StrideInBytes = 0;
}

static void d3d12_setIndexBuffer(VGPUCommandBufferImpl* driverData, VGPUBuffer buffer, uint64_t offset, VGPUIndexType type)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    D3D12Buffer* d3d12Buffer = (D3D12Buffer*)buffer;

    D3D12_INDEX_BUFFER_VIEW view;
    view.BufferLocation = d3d12Buffer->gpuAddress + (D3D12_GPU_VIRTUAL_ADDRESS)offset;
    view.SizeInBytes = (UINT)(d3d12Buffer->size - offset);
    view.Format = (type == VGPUIndexType_UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);
    commandBuffer->commandList->IASetIndexBuffer(&view);
}

static void d3d12_setStencilReference(VGPUCommandBufferImpl* driverData, uint32_t reference)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;

    commandBuffer->commandList->OMSetStencilRef(reference);
}

static void d3d12_prepareDraw(D3D12CommandBuffer* commandBuffer)
{
    VGPU_UNUSED(commandBuffer);

    VGPU_ASSERT(commandBuffer->insideRenderPass);

    if (commandBuffer->currentPipeline->numVertexBindings > 0)
    {
        for (uint32_t i = 0; i < commandBuffer->currentPipeline->numVertexBindings; ++i)
        {
            commandBuffer->vboViews[i].StrideInBytes = commandBuffer->currentPipeline->strides[i];
        }

        commandBuffer->commandList->IASetVertexBuffers(0, commandBuffer->currentPipeline->numVertexBindings, commandBuffer->vboViews);
    }
}

static void d3d12_draw(VGPUCommandBufferImpl* driverData, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstInstance)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    d3d12_prepareDraw(commandBuffer);
    commandBuffer->commandList->DrawInstanced(vertexCount, instanceCount, vertexStart, firstInstance);
}

static void d3d12_drawIndexed(VGPUCommandBufferImpl* driverData, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)driverData;
    d3d12_prepareDraw(commandBuffer);
    commandBuffer->commandList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

static VGPUCommandBuffer d3d12_beginCommandBuffer(VGPURenderer* driverData, VGPUCommandQueue queueType, const char* label)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    HRESULT hr = S_OK;
    D3D12CommandBuffer* impl = nullptr;

    renderer->cmdBuffersLocker.lock();
    uint32_t cmd_current = renderer->cmdBuffersCount++;
    if (cmd_current >= renderer->commandBuffers.size())
    {
        D3D12_COMMAND_LIST_TYPE d3dCommandListType = ToD3D12(queueType);

        impl = new D3D12CommandBuffer();
        impl->renderer = renderer;
        impl->queue = queueType;

        for (uint32_t i = 0; i < VGPU_MAX_INFLIGHT_FRAMES; ++i)
        {
            VHR(renderer->device->CreateCommandAllocator(d3dCommandListType, IID_PPV_ARGS(&impl->commandAllocators[i])));
        }

        hr = renderer->device->CreateCommandList1(0, d3dCommandListType, D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&impl->commandList)
        );
        VHR(hr);

        VGPUCommandBuffer_T* commandBuffer = new VGPUCommandBuffer_T();
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
    VHR(impl->commandAllocators[renderer->frameIndex]->Reset());
    VHR(impl->commandList->Reset(impl->commandAllocators[renderer->frameIndex], nullptr));

    if (queueType == VGPUCommandQueue_Graphics ||
        queueType == VGPUCommandQueue_Compute)
    {
        ID3D12DescriptorHeap* heaps[2] = {
            renderer->resourceDescriptorHeap.handle,
            renderer->samplerDescriptorHeap.handle
        };
        impl->commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);
    }

    if (queueType == VGPUCommandQueue_Graphics)
    {
        D3D12_RECT pRects[D3D12_VIEWPORT_AND_SCISSORRECT_MAX_INDEX + 1];
        for (uint32_t i = 0; i < _countof(pRects); ++i)
        {
            pRects[i].bottom = D3D12_VIEWPORT_BOUNDS_MAX;
            pRects[i].left = D3D12_VIEWPORT_BOUNDS_MIN;
            pRects[i].right = D3D12_VIEWPORT_BOUNDS_MAX;
            pRects[i].top = D3D12_VIEWPORT_BOUNDS_MIN;
        }
        impl->commandList->RSSetScissorRects(_countof(pRects), pRects);
    }

    impl->insideRenderPass = false;

    static constexpr float defaultBlendFactor[4] = { 0, 0, 0, 0 };
    impl->commandList->OMSetBlendFactor(defaultBlendFactor);
    impl->commandList->OMSetStencilRef(0);
    impl->numBarriersToFlush = 0;
    impl->currentPipeline = nullptr;

    impl->hasLabel = false;
    if (label)
    {
        d3d12_pushDebugGroup((VGPUCommandBufferImpl*)impl, label);
        impl->hasLabel = true;
    }

    return renderer->commandBuffers.back();
}

static uint64_t d3d12_submit(VGPURenderer* driverData, VGPUCommandBuffer* commandBuffers, uint32_t count)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;

    HRESULT hr = S_OK;
    std::vector<D3D12_SwapChain*> presentSwapChains;
    for (uint32_t i = 0; i < count; i += 1)
    {
        D3D12CommandBuffer* commandBuffer = (D3D12CommandBuffer*)commandBuffers[i]->driverData;

        // Present acquired SwapChains
        for (size_t swapChainIndex = 0; swapChainIndex < commandBuffer->swapChains.size(); ++swapChainIndex)
        {
            D3D12_SwapChain* swapChain = commandBuffer->swapChains[swapChainIndex];

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
            vgpu_log_error("Failed to close command list");
            return 0;
        }

        D3D12Queue& queue = renderer->queues[commandBuffer->queue];
        queue.submitCommandLists.push_back(commandBuffer->commandList);
    }

    for (uint32_t i = 0; i < _VGPUCommandQueue_Count; ++i)
    {
        D3D12Queue& queue = renderer->queues[i];

        if (!queue.submitCommandLists.empty())
        {
            queue.handle->ExecuteCommandLists(
                (UINT)queue.submitCommandLists.size(),
                queue.submitCommandLists.data()
            );
            queue.submitCommandLists.clear();
        }

        VHR(queue.handle->Signal(queue.frameFences[renderer->frameIndex], 1));
    }

    renderer->cmdBuffersCount = 0;

    // Present acquired SwapChains
    for (size_t i = 0; i < presentSwapChains.size() && SUCCEEDED(hr); ++i)
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
            return 0;
        }
    }

    renderer->resourceDescriptorHeap.SignalGPU(renderer->queues[VGPUCommandQueue_Graphics].handle);
    renderer->samplerDescriptorHeap.SignalGPU(renderer->queues[VGPUCommandQueue_Graphics].handle);

    renderer->frameCount++;
    renderer->frameIndex = renderer->frameCount % VGPU_MAX_INFLIGHT_FRAMES;

    for (uint32_t queue = 0; queue < _VGPUCommandQueue_Count; ++queue)
    {
        if (renderer->frameCount >= VGPU_MAX_INFLIGHT_FRAMES &&
            renderer->queues[queue].frameFences[renderer->frameIndex]->GetCompletedValue() < 1)
        {
            // NULL event handle will simply wait immediately:
            //	https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12fence-seteventoncompletion#remarks
            hr = renderer->queues[queue].frameFences[renderer->frameIndex]->SetEventOnCompletion(1, nullptr);
            VHR(hr);
        }
    }

    // Begin new frame
    // Safe delete deferred destroys
    d3d12_ProcessDeletionQueue(renderer);

    // Return current frame
    return renderer->frameCount - 1;
}

static uint64_t d3d12_getFrameCount(VGPURenderer* driverData)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;
    return renderer->frameCount;
}

static uint32_t d3d12_getFrameIndex(VGPURenderer* driverData)
{
    D3D12_Renderer* renderer = (D3D12_Renderer*)driverData;
    return renderer->frameIndex;
}

static VGPUBool32 d3d12_isSupported(void)
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

    vgpuCreateDXGIFactory2 = (PFN_CREATE_DXGI_FACTORY2)GetProcAddress(dxgiDLL, "CreateDXGIFactory2");
    if (vgpuCreateDXGIFactory2 == nullptr)
    {
        return false;
    }

#if defined(_DEBUG)
    vgpuDXGIGetDebugInterface1 = (PFN_DXGI_GET_DEBUG_INTERFACE1)GetProcAddress(dxgiDLL, "DXGIGetDebugInterface1");
#endif

    vgpuD3D12GetDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(d3d12DLL, "D3D12GetDebugInterface");
    vgpuD3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12DLL, "D3D12CreateDevice");
    if (!vgpuD3D12CreateDevice)
    {
        return false;
    }

    vgpuD3D12SerializeVersionedRootSignature = (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(d3d12DLL, "D3D12SerializeVersionedRootSignature");
    if (!vgpuD3D12SerializeVersionedRootSignature) {
        return false;
    }
#endif

    ComPtr<IDXGIFactory4> dxgiFactory;
    if (FAILED(vgpuCreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory))))
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
        if (SUCCEEDED(vgpuD3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
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

static VGPUDeviceImpl* d3d12_createDevice(const VGPUDeviceDescriptor* info)
{
    VGPU_ASSERT(info);

    D3D12_Renderer* renderer = new D3D12_Renderer();

    DWORD dxgiFactoryFlags = 0;
    if (info->validationMode != VGPUValidationMode_Disabled)
    {
        ComPtr<ID3D12Debug> debugController;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        if (vgpuD3D12GetDebugInterface != nullptr &&
            SUCCEEDED(vgpuD3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
#else
        if (SUCCEEDED(vgpuD3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
#endif
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

    if (FAILED(vgpuCreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(renderer->factory.ReleaseAndGetAddressOf()))))
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
                if (SUCCEEDED(vgpuD3D12CreateDevice(dxgiAdapter.Get(), featurelevel, IID_PPV_ARGS(&renderer->device))))
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
            vgpu_log_error("DXGI: No capable adapter found!");
            delete renderer;
            return nullptr;
        }

        // Init feature check (https://devblogs.microsoft.com/directx/introducing-a-new-api-for-checking-feature-support-in-direct3d-12/)
        VHR(renderer->d3dFeatures.Init(renderer->device));

        if (renderer->d3dFeatures.HighestRootSignatureVersion() < D3D_ROOT_SIGNATURE_VERSION_1_1)
        {
            vgpu_log_error("Direct3D12: Root signature version 1.1 not supported!");
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
            renderer->adapterType = VGPUAdapterType_CPU;
        }
        else
        {
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

        // Log some info
        vgpu_log_info("VGPU Driver: D3D12");
        vgpu_log_info("D3D12 Adapter: %S", adapterDesc.Description);
    }

    // Create command queues
    {
        for (uint32_t queue = 0; queue < _VGPUCommandQueue_Count; ++queue)
        {
            VGPUCommandQueue queueType = (VGPUCommandQueue)queue;

            D3D12_COMMAND_QUEUE_DESC queueDesc{};

            queueDesc.Type = ToD3D12(queueType);
            queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            queueDesc.NodeMask = 0;
            VHR(
                renderer->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&renderer->queues[queue].handle))
            );
            VHR(
                renderer->device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&renderer->queues[queue].fence))
            );

            switch (queueType)
            {
                case VGPUCommandQueue_Graphics:
                    renderer->queues[queue].handle->SetName(L"Graphics Queue");
                    renderer->queues[queue].fence->SetName(L"GraphicsQueue - Fence");
                    break;
                case VGPUCommandQueue_Compute:
                    renderer->queues[queue].handle->SetName(L"Compute Queue");
                    renderer->queues[queue].fence->SetName(L"ComputeQueue - Fence");
                    break;
                case VGPUCommandQueue_Copy:
                    renderer->queues[queue].handle->SetName(L"CopyQueue");
                    renderer->queues[queue].fence->SetName(L"CopyQueue - Fence");
                    break;
            }

            // Create frame-resident resources:
            for (uint32_t frameIndex = 0; frameIndex < VGPU_MAX_INFLIGHT_FRAMES; ++frameIndex)
            {
                VHR(
                    renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderer->queues[queue].frameFences[frameIndex]))
                );

#if defined(_DEBUG)
                wchar_t fenceName[64];

                switch (queueType)
                {
                    case VGPUCommandQueue_Graphics:
                        swprintf(fenceName, 64, L"GraphicsQueue - Frame Fence %u", frameIndex);
                        break;
                    case VGPUCommandQueue_Compute:
                        swprintf(fenceName, 64, L"ComputeQueue - Frame Fence %u", frameIndex);
                        break;
                    case VGPUCommandQueue_Copy:
                        swprintf(fenceName, 64, L"CopyQueue - Frame Fence %u", frameIndex);
                        break;
                }

                renderer->queues[queue].frameFences[frameIndex]->SetName(fenceName);
#endif
            }
        }
    }

    // Init CPU descriptor allocators
    renderer->resourceAllocator.Init(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096);
    renderer->samplerAllocator.Init(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256);
    renderer->rtvAllocator.Init(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 512);
    renderer->dsvAllocator.Init(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128);

    // Resource descriptor heap (shader visible)
    {
        renderer->resourceDescriptorHeap.numDescriptors = 1000000; // tier 2 limit

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = renderer->resourceDescriptorHeap.numDescriptors;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDesc.NodeMask = 0;
        VHR(renderer->device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&renderer->resourceDescriptorHeap.handle)));
        renderer->resourceDescriptorHeap.cpuStart = renderer->resourceDescriptorHeap.handle->GetCPUDescriptorHandleForHeapStart();
        renderer->resourceDescriptorHeap.gpuStart = renderer->resourceDescriptorHeap.handle->GetGPUDescriptorHandleForHeapStart();

        VHR(renderer->device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&renderer->resourceDescriptorHeap.fence)));
        renderer->resourceDescriptorHeap.fenceValue = renderer->resourceDescriptorHeap.fence->GetCompletedValue();

        //for (uint32_t i = 0; i < kBindlessResourceCapacity; ++i)
        //{
        //    freeBindlessResources.push_back(kBindlessResourceCapacity - i - 1);
        //}

        renderer->samplerDescriptorHeap.numDescriptors = 2048; // tier 2 limit

        heapDesc.NodeMask = 0;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        heapDesc.NumDescriptors = renderer->samplerDescriptorHeap.numDescriptors;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        VHR(renderer->device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&renderer->samplerDescriptorHeap.handle)));

        renderer->samplerDescriptorHeap.cpuStart = renderer->samplerDescriptorHeap.handle->GetCPUDescriptorHandleForHeapStart();
        renderer->samplerDescriptorHeap.gpuStart = renderer->samplerDescriptorHeap.handle->GetGPUDescriptorHandleForHeapStart();

        VHR(renderer->device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&renderer->samplerDescriptorHeap.fence)));
        renderer->samplerDescriptorHeap.fenceValue = renderer->samplerDescriptorHeap.fence->GetCompletedValue();

        //for (uint32_t i = 0; i < kBindlessSamplerCapacity; ++i)
        //{
        //    freeBindlessSamplers.push_back(kBindlessSamplerCapacity - i - 1);
        //}
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

    VGPUDeviceImpl* device = new VGPUDeviceImpl();
    ASSIGN_DRIVER(d3d12);
    device->driverData = (VGPURenderer*)renderer;
    return device;
}

VGPUDriver D3D12_Driver = {
    VGPUBackend_D3D12,
    d3d12_isSupported,
    d3d12_createDevice
};

#undef VHR
#undef SAFE_RELEASE

uint32_t vgpuToDxgiFormat(VGPUTextureFormat format)
{
    return (uint32_t)ToDxgiFormat(format);
}

VGPUTextureFormat vgpuFromDxgiFormat(uint32_t dxgiFormat)
{
    return FromDxgiFormat((DXGI_FORMAT)dxgiFormat);
}

#else

uint32_t vgpuToDxgiFormat(VGPUTextureFormat format)
{
    _VGPU_UNUSED(format);
    return 0;
}


VGPUTextureFormat vgpuFromDxgiFormat(uint32_t dxgiFormat)
{
    _VGPU_UNUSED(dxgiFormat);
    return VGPUTextureFormat_Undefined;
}

#endif /* VGPU_D3D12_DRIVER */
