// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGPU_D3D12_DRIVER)

#include "vgpu_driver_d3d.h"

#ifdef USING_DIRECTX_HEADERS
#   include <directx/d3d12.h>
#else
#   include <d3d12.h>
#endif
#include <dxgi1_6.h>
#include <windows.ui.xaml.media.dxinterop.h>
#include <wrl/client.h>
#include <vector>
#include <deque>
#include <mutex>
#include <sstream>

#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#include "D3D12MemAlloc.h"

#ifdef USING_D3D12_AGILITY_SDK
extern "C"
{
    // Used to enable the "Agility SDK" components
    __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
    __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\";
}
#endif

using Microsoft::WRL::ComPtr;

namespace
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    using PFN_CREATE_DXGI_FACTORY2 = decltype(&CreateDXGIFactory2);
    static PFN_CREATE_DXGI_FACTORY2 vgfxCreateDXGIFactory2 = nullptr;

    static PFN_D3D12_GET_DEBUG_INTERFACE vgfxD3D12GetDebugInterface = nullptr;
    static PFN_D3D12_CREATE_DEVICE vgfxD3D12CreateDevice = nullptr;
    static PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE vgfxD3D12SerializeVersionedRootSignature = nullptr;

#if defined(_DEBUG)
    using PFN_DXGI_GET_DEBUG_INTERFACE1 = decltype(&DXGIGetDebugInterface1);
    static PFN_DXGI_GET_DEBUG_INTERFACE1 vgfxDXGIGetDebugInterface1 = nullptr;
#endif
#endif

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
        VGFX_ASSERT(SUCCEEDED(hr));
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
        VGFX_ASSERT(!freelist.empty());
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

struct VGFXD3D12Texture
{
    ID3D12Resource* handle = nullptr;
    D3D12MA::Allocation* allocation = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
};

struct VGFXD3D12SwapChain
{
    IDXGISwapChain3* handle = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    bool vsync = false;
    std::vector<VGFXTexture> backbufferTextures;
};

struct VGFXD3D12UploadContext
{
    ID3D12CommandAllocator* commandAllocator = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12Fence* fence = nullptr;

    uint64_t uploadBufferSize = 0;
    ID3D12Resource* uploadBuffer = nullptr;
    D3D12MA::Allocation* uploadBufferAllocation = nullptr;
    void* uploadBufferData = nullptr;
};

struct VGFXD3D12Renderer
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
    ID3D12CommandQueue* graphicsQueue = nullptr;

    ID3D12GraphicsCommandList4* graphicsCommandList = nullptr;
    ID3D12CommandAllocator* commandAllocators[VGFX_MAX_INFLIGHT_FRAMES] = {};

    std::vector<VGFXD3D12SwapChain*> swapChains;

    uint32_t frameIndex = 0;
    uint64_t frameCount = 0;
    uint64_t GPUFrameCount = 0;

    ID3D12CommandQueue* copyQueue = nullptr;
    std::mutex uploadLocker;
    std::vector<VGFXD3D12UploadContext> uploadFreeList;

    VGFXD3D12DescriptorAllocator resourceAllocator;
    VGFXD3D12DescriptorAllocator samplerAllocator;
    VGFXD3D12DescriptorAllocator rtvAllocator;
    VGFXD3D12DescriptorAllocator dsvAllocator;

    bool shuttingDown = false;
    std::mutex destroyMutex;
    std::deque<std::pair<D3D12MA::Allocation*, uint64_t>> deferredAllocations;
    std::deque<std::pair<IUnknown*, uint64_t>> deferredReleases;
};

static void d3d12_DeferDestroy(VGFXD3D12Renderer* renderer, IUnknown* resource, D3D12MA::Allocation* allocation)
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
        SafeRelease(allocation);

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

static void d3d12_ProcessDeletionQueue(VGFXD3D12Renderer* renderer)
{
    renderer->destroyMutex.lock();

    while (!renderer->deferredAllocations.empty())
    {
        if (renderer->deferredAllocations.front().second + VGFX_MAX_INFLIGHT_FRAMES < renderer->frameCount)
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
        if (renderer->deferredReleases.front().second + VGFX_MAX_INFLIGHT_FRAMES < renderer->frameCount)
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

static VGFXD3D12UploadContext d3d12_AllocateUpload(VGFXD3D12Renderer* renderer, uint64_t size)
{
    renderer->uploadLocker.lock();

    HRESULT hr = S_OK;

    // Create a new command list if there are no free ones.
    if (renderer->uploadFreeList.empty())
    {
        VGFXD3D12UploadContext context;

        hr = renderer->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&context.commandAllocator));
        VGFX_ASSERT(SUCCEEDED(hr));
        hr = renderer->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, context.commandAllocator, nullptr, IID_PPV_ARGS(&context.commandList));
        VGFX_ASSERT(SUCCEEDED(hr));
        hr = context.commandList->Close();
        VGFX_ASSERT(SUCCEEDED(hr));
        hr = renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&context.fence));
        VGFX_ASSERT(SUCCEEDED(hr));

        renderer->uploadFreeList.push_back(context);
    }

    VGFXD3D12UploadContext context = renderer->uploadFreeList.back();
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
        VGFX_ASSERT(SUCCEEDED(hr));

        D3D12_RANGE readRange = {};
        hr = context.uploadBuffer->Map(0, &readRange, &context.uploadBufferData);
        VGFX_ASSERT(SUCCEEDED(hr));
    }

    // Begin command list in valid state
    hr = context.commandAllocator->Reset();
    VGFX_ASSERT(SUCCEEDED(hr));
    hr = context.commandList->Reset(context.commandAllocator, nullptr);
    VGFX_ASSERT(SUCCEEDED(hr));

    return context;
}

static void d3d12_UploadSubmit(VGFXD3D12Renderer* renderer, VGFXD3D12UploadContext context)
{
    HRESULT hr = context.commandList->Close();
    VGFX_ASSERT(SUCCEEDED(hr));

    ID3D12CommandList* commandlists[] = {
        context.commandList
    };
    renderer->copyQueue->ExecuteCommandLists(1, commandlists);

    hr = renderer->copyQueue->Signal(context.fence, 1);
    VGFX_ASSERT(SUCCEEDED(hr));
    hr = context.fence->SetEventOnCompletion(1, nullptr);
    VGFX_ASSERT(SUCCEEDED(hr));
    hr = context.fence->Signal(0);
    VGFX_ASSERT(SUCCEEDED(hr));

    renderer->uploadLocker.lock();
    renderer->uploadFreeList.push_back(context);
    renderer->uploadLocker.unlock();
}

static void d3d12_destroyDevice(VGPUDevice device)
{
    // Wait idle
    vgpuWaitIdle(device);

    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)device->driverData;

    VGFX_ASSERT(renderer->frameCount == renderer->GPUFrameCount);
    renderer->shuttingDown = true;

    renderer->frameCount = UINT64_MAX;
    d3d12_ProcessDeletionQueue(renderer);
    renderer->frameCount = 0;

    // Frame fence
    SafeRelease(renderer->frameFence);
    CloseHandle(renderer->frameFenceEvent);

    // Copy queue + allocations
    {
        ComPtr<ID3D12Fence> fence;
        HRESULT hr = renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        VGFX_ASSERT(SUCCEEDED(hr));

        hr = renderer->copyQueue->Signal(fence.Get(), 1);
        VGFX_ASSERT(SUCCEEDED(hr));
        hr = fence->SetEventOnCompletion(1, nullptr);
        VGFX_ASSERT(SUCCEEDED(hr));

        for (auto& item : renderer->uploadFreeList)
        {
            item.uploadBuffer->Release();
            item.uploadBufferAllocation->Release();
            item.commandAllocator->Release();
            item.commandList->Release();
            item.fence->Release();
        }
        renderer->uploadFreeList.clear();

        SafeRelease(renderer->copyQueue);
    }

    // CPU descriptor allocators
    {
        renderer->resourceAllocator.Shutdown();
        renderer->samplerAllocator.Shutdown();
        renderer->rtvAllocator.Shutdown();
        renderer->dsvAllocator.Shutdown();
    }

    for (uint32_t i = 0; i < VGFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        SafeRelease(renderer->commandAllocators[i]);
    }
    SafeRelease(renderer->graphicsCommandList);
    SafeRelease(renderer->graphicsQueue);

    // Allocator.
    if (renderer->allocator != nullptr)
    {
        //D3D12MA::TotalStatistics stats;
        //renderer->allocator->CalculateStatistics(&stats);
        //
        //if (stats.Total.Stats.AllocationBytes > 0)
        //{
        //    //LOGI("Total device memory leaked: {} bytes.", stats.Total.UsedBytes);
        //}

        SafeRelease(renderer->allocator);
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
        if (vgfxDXGIGetDebugInterface1 != nullptr
            && SUCCEEDED(vgfxDXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            dxgiDebug->ReportLiveObjects(VGFX_DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
    }
#endif

    delete renderer;
    VGFX_FREE(device);
    }

static void d3d12_updateSwapChain(VGFXD3D12Renderer* renderer, VGFXD3D12SwapChain* swapChain)
{
    HRESULT hr = S_OK;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    hr = swapChain->handle->GetDesc1(&swapChainDesc);
    VGFX_ASSERT(SUCCEEDED(hr));

    swapChain->width = swapChainDesc.Width;
    swapChain->height = swapChainDesc.Height;
    swapChain->backbufferTextures.resize(swapChainDesc.BufferCount);
    for (uint32_t i = 0; i < swapChainDesc.BufferCount; ++i)
    {
        VGFXD3D12Texture* texture = new VGFXD3D12Texture();
        hr = swapChain->handle->GetBuffer(i, IID_PPV_ARGS(&texture->handle));
        VGFX_ASSERT(SUCCEEDED(hr));

        wchar_t name[25] = {};
        swprintf_s(name, L"Render target %u", i);
        texture->handle->SetName(name);

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = swapChainDesc.Format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        texture->rtv = renderer->rtvAllocator.Allocate();
        renderer->device->CreateRenderTargetView(texture->handle, &rtvDesc, texture->rtv);

        swapChain->backbufferTextures[i] = (VGFXTexture)texture;
    }
}

static uint64_t d3d12_frame(VGFXRenderer* driverData)
{
    HRESULT hr = S_OK;
    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)driverData;

    /* Transition SwapChain textures to present */
    for (size_t i = 0, count = renderer->swapChains.size(); i < count; ++i)
    {
        VGFXD3D12SwapChain* swapChain = renderer->swapChains[i];
        VGFXD3D12Texture* texture = (VGFXD3D12Texture*)swapChain->backbufferTextures[swapChain->handle->GetCurrentBackBufferIndex()];

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = texture->handle;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        renderer->graphicsCommandList->ResourceBarrier(1, &barrier);
    }

    hr = renderer->graphicsCommandList->Close();
    if (FAILED(hr))
    {
        vgfxLogError("Failed to close command list");
        return (uint64_t)(-1);
    }

    ID3D12CommandList* commandLists[] = { renderer->graphicsCommandList };
    renderer->graphicsQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    // Present acquired SwapChains
    for (size_t i = 0, count = renderer->swapChains.size(); i < count && SUCCEEDED(hr); ++i)
    {
        VGFXD3D12SwapChain* swapChain = renderer->swapChains[i];

        if (swapChain->vsync)
        {
            hr = swapChain->handle->Present(1, 0);
        }
        else
        {
            hr = swapChain->handle->Present(0, renderer->tearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0);
        }
    }
    renderer->swapChains.clear();

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
    }
    else
    {
        if (FAILED(hr))
        {
            vgfxLogError("Failed to process frame");
            return (uint64_t)(-1);
        }

        renderer->frameCount++;

        // Signal the fence with the current frame number, so that we can check back on it
        hr = renderer->graphicsQueue->Signal(renderer->frameFence, renderer->frameCount);
        if (FAILED(hr))
        {
            vgfxLogError("Failed to signal frame");
            return (uint64_t)(-1);
        }

        // Wait for the GPU to catch up before we stomp an executing command buffer
        const uint64_t gpuLag = renderer->frameCount - renderer->GPUFrameCount;
        VGFX_ASSERT(gpuLag <= VGFX_MAX_INFLIGHT_FRAMES);
        if (gpuLag >= VGFX_MAX_INFLIGHT_FRAMES)
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
    }

    // Begin new frame
    {
        // Safe delete deferred destroys
        d3d12_ProcessDeletionQueue(renderer);

        renderer->frameIndex = renderer->frameCount % VGFX_MAX_INFLIGHT_FRAMES;

        // Prepare the command buffers to be used for the next frame
        renderer->commandAllocators[renderer->frameIndex]->Reset();
        renderer->graphicsCommandList->Reset(renderer->commandAllocators[renderer->frameIndex], nullptr);
    }

    // Return current frame
    return renderer->frameCount - 1;
}

static void d3d12_waitIdle(VGFXRenderer* driverData)
{
    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)driverData;

    // Wait for the GPU to fully catch up with the CPU
    VGFX_ASSERT(renderer->frameCount >= renderer->GPUFrameCount);
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

static bool d3d12_hasFeature(VGFXRenderer* driverData, VGPUFeature feature) {
    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)driverData;
    switch (feature)
    {
        case VGPU_FEATURE_COMPUTE:
        case VGPU_FEATURE_INDEPENDENT_BLEND:
        case VGPU_FEATURE_TEXTURE_CUBE_ARRAY:
        case VGPU_FEATURE_TEXTURE_COMPRESSION_BC:
            return true;

        case VGPU_FEATURE_TEXTURE_COMPRESSION_ETC2:
        case VGPU_FEATURE_TEXTURE_COMPRESSION_ASTC:
            return false;

        default:
            return false;
    }
}

static void d3d12_getAdapterProperties(VGFXRenderer* driverData, VGPUAdapterProperties* properties) {
    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)driverData;

    properties->vendorID = renderer->vendorID;
    properties->deviceID = renderer->deviceID;
    properties->name = renderer->adapterName.c_str();
    properties->driverDescription = renderer->driverDescription.c_str();
    properties->adapterType = renderer->adapterType;
    properties->backendType = VGPU_BACKEND_TYPE_D3D12;
}

static void d3d12_getLimits(VGFXRenderer* driverData, VGPULimits* limits)
{
    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)driverData;

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
static VGFXBuffer d3d12_createBuffer(VGFXRenderer* driverData, const VGFXBufferDesc* desc, const void* pInitialData)
{
    return nullptr;
}

static void d3d12_destroyBuffer(VGFXRenderer* driverData, VGFXBuffer resource)
{
}

/* Texture */
static VGFXTexture d3d12_createTexture(VGFXRenderer* driverData, const VGFXTextureDesc* desc)
{
    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)driverData;

    D3D12MA::ALLOCATION_DESC allocationDesc = {};
    allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc;
    if (desc->type == VGPU_TEXTURE_TYPE_3D)
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
    resourceDesc.MipLevels = (UINT16)desc->mipLevelCount;
    resourceDesc.Format = ToDXGIFormat(desc->format);
    resourceDesc.SampleDesc.Count = desc->sampleCount;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (desc->usage & VGFXTextureUsage_ShaderWrite)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    if (desc->usage & VGFXTextureUsage_RenderTarget)
    {
        if (vgfxIsDepthStencilFormat(desc->format))
        {
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            if (!(desc->usage & VGFXTextureUsage_ShaderRead))
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

    if (desc->usage & VGFXTextureUsage_RenderTarget)
    {
        clearValue.Format = resourceDesc.Format;
        if (vgfxIsDepthStencilFormat(desc->format))
        {
            clearValue.DepthStencil.Depth = 1.0f;
        }
        pClearValue = &clearValue;
    }

    // If shader read/write and depth format, set to typeless
    if (vgfxIsDepthFormat(desc->format) && desc->usage & (VGFXTextureUsage_ShaderRead | VGFXTextureUsage_ShaderWrite))
    {
        resourceDesc.Format = GetTypelessFormatFromDepthFormat(desc->format);
        pClearValue = nullptr;
    }

    // TODO: TextureLayout/State
    D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;

    VGFXD3D12Texture* texture = new VGFXD3D12Texture();
    HRESULT hr = renderer->allocator->CreateResource(
        &allocationDesc,
        &resourceDesc,
        resourceState,
        pClearValue,
        &texture->allocation,
        IID_PPV_ARGS(&texture->handle)
    );

    if (FAILED(hr))
    {
        vgfxLogError("D3D11: Failed to create texture");
        delete texture;
        return nullptr;
    }

    if (desc->label)
    {
        D3D12SetName(texture->handle, desc->label);
    }

    return (VGFXTexture)texture;
}

static void d3d12_destroyTexture(VGFXRenderer* driverData, VGFXTexture texture)
{
    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)driverData;
    VGFXD3D12Texture* d3dTexture = (VGFXD3D12Texture*)texture;

    d3d12_DeferDestroy(renderer, d3dTexture->handle, d3dTexture->allocation);
    renderer->rtvAllocator.Free(d3dTexture->rtv);
    delete d3dTexture;
}

/* SwapChain */
static VGPUSwapChain d3d12_createSwapChain(VGFXRenderer* driverData, void* windowHandle, const VGPUSwapChainDesc* info)
{
    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)driverData;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = info->width;
    swapChainDesc.Height = info->height;
    swapChainDesc.Format = ToDXGISwapChainFormat(info->format);
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = VGFX_MAX_INFLIGHT_FRAMES;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = renderer->tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;

    HRESULT hr = S_OK;
    ComPtr<IDXGISwapChain1> tempSwapChain;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = TRUE;

    hr = renderer->factory->CreateSwapChainForHwnd(
        renderer->graphicsQueue,
        (HWND)windowHandle,
        &swapChainDesc,
        &fsSwapChainDesc,
        nullptr,
        tempSwapChain.GetAddressOf()
    );

    if (FAILED(hr))
    {
        return nullptr;
    }

    // This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
    hr = renderer->factory->MakeWindowAssociation((HWND)windowHandle, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr))
    {
        return nullptr;
    }
#else
    swapChainDesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;

    hr = renderer->factory->CreateSwapChainForCoreWindow(
        renderer->graphicsQueue,
        surface->coreWindowOrSwapChainPanel,
        &swapChainDesc,
        nullptr,
        tempSwapChain.GetAddressOf()
    );

    if (FAILED(hr))
    {
        return nullptr;
    }

    // SwapChain panel
    ComPtr<ISwapChainPanelNative> swapChainPanelNative;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;
    hr = renderer->factory->CreateSwapChainForComposition(
        renderer->graphicsQueue,
        &swapChainDesc,
        nullptr,
        tempSwapChain.GetAddressOf()
    );

    hr = tempSwapChain.As(&swapChainPanelNative);
    if (FAILED(hr))
    {
        vgfxLogError("Failed to get ISwapChainPanelNative from IDXGISwapChain1");
        return nullptr;
    }

    hr = swapChainPanelNative->SetSwapChain(tempSwapChain.Get());
    if (FAILED(hr))
    {
        vgfxLogError("Failed to set ISwapChainPanelNative - SwapChain");
        return nullptr;
    }
#endif

    IDXGISwapChain3* handle = nullptr;
    hr = tempSwapChain->QueryInterface(&handle);
    if (FAILED(hr))
    {
        return nullptr;
    }

    VGFXD3D12SwapChain* swapChain = new VGFXD3D12SwapChain();
    swapChain->handle = handle;
    swapChain->vsync = info->presentMode == VGFXPresentMode_Fifo;
    d3d12_updateSwapChain(renderer, swapChain);
    return (VGPUSwapChain)swapChain;
}

static void d3d12_destroySwapChain(VGFXRenderer* driverData, VGPUSwapChain swapChain)
{
    VGFXD3D12SwapChain* d3d12SwapChain = (VGFXD3D12SwapChain*)swapChain;

    for (size_t i = 0, count = d3d12SwapChain->backbufferTextures.size(); i < count; ++i)
    {
        d3d12_destroyTexture(driverData, d3d12SwapChain->backbufferTextures[i]);
    }
    d3d12SwapChain->backbufferTextures.clear();
    d3d12SwapChain->handle->Release();

    delete d3d12SwapChain;
}

static void d3d12_getSwapChainSize(VGFXRenderer*, VGPUSwapChain swapChain, VGPUSize2D* pSize)
{
    VGFXD3D12SwapChain* d3dSwapChain = (VGFXD3D12SwapChain*)swapChain;
    pSize->width = d3dSwapChain->width;
    pSize->height = d3dSwapChain->height;
}

static VGFXTexture d3d12_acquireNextTexture(VGFXRenderer* driverData, VGPUSwapChain swapChain)
{
    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)driverData;
    VGFXD3D12SwapChain* d3d12SwapChain = (VGFXD3D12SwapChain*)swapChain;

    renderer->swapChains.push_back(d3d12SwapChain);
    return d3d12SwapChain->backbufferTextures[d3d12SwapChain->handle->GetCurrentBackBufferIndex()];
}

static VGPUCommandBuffer d3d12_beginCommandBuffer(VGFXRenderer* driverData, const char* label)
{
    return nullptr;
}

static void d3d12_submit(VGFXRenderer* driverData, VGPUCommandBuffer* commandBuffers, uint32_t count)
{
}

static void d3d12_beginRenderPass(VGFXRenderer* driverData, const VGFXRenderPassDesc* info)
{
    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)driverData;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[VGFX_MAX_COLOR_ATTACHMENTS] = {};

    for (uint32_t i = 0; i < info->colorAttachmentCount; ++i)
    {
        VGFXRenderPassColorAttachment attachment = info->colorAttachments[i];
        VGFXD3D12Texture* texture = (VGFXD3D12Texture*)attachment.texture;

        D3D12_RESOURCE_BARRIER resource_barrier = {};
        resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        resource_barrier.Transition.pResource = texture->handle;
        resource_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        resource_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        resource_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        renderer->graphicsCommandList->ResourceBarrier(1, &resource_barrier);

        rtvs[i] = texture->rtv;

        switch (attachment.loadOp)
        {
            case VGFXLoadOp_Clear:
                renderer->graphicsCommandList->ClearRenderTargetView(rtvs[i], &attachment.clearColor.r, 0, nullptr);
                break;

            default:
                break;
        }
    }

    renderer->graphicsCommandList->OMSetRenderTargets(info->colorAttachmentCount, rtvs, FALSE, nullptr);
}

static void d3d12_endRenderPass(VGFXRenderer* driverData)
{
    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)driverData;
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
    vgfxDXGIGetDebugInterface1 = (PFN_DXGI_GET_DEBUG_INTERFACE1)GetProcAddress(dxgiDLL, "DXGIGetDebugInterface1");
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
    VGFX_ASSERT(info);

    VGFXD3D12Renderer* renderer = new VGFXD3D12Renderer();

    DWORD dxgiFactoryFlags = 0;
    if (info->validationMode != VGPU_VALIDATION_MODE_DISABLED)
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(vgfxD3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
        {
            debugController->EnableDebugLayer();

            if (info->validationMode == VGPU_VALIDATION_MODE_GPU)
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
        if (SUCCEEDED(vgfxDXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
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

        VGFX_ASSERT(dxgiAdapter != nullptr);
        if (dxgiAdapter == nullptr)
        {
            vgfxLogError("DXGI: No capable adapter found!");
            delete renderer;
            return nullptr;
        }

        // Assign label object.
        if (info->label)
        {
            std::wstring wide_name = UTF8ToWStr(info->label);
            renderer->device->SetName(wide_name.c_str());
        }

        if (info->validationMode != VGPU_VALIDATION_MODE_DISABLED)
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

                if (info->validationMode == VGPU_VALIDATION_MODE_VERBOSE)
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
                infoQueue->AddApplicationMessage(D3D12_MESSAGE_SEVERITY_MESSAGE, "D3D12 Debug Filters setup");
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

        D3D12_FEATURE_DATA_ARCHITECTURE arch = {};
        HRESULT hr = renderer->device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch, sizeof(arch));

        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            renderer->adapterType = VGPU_ADAPTER_TYPE_CPU;
        }
        else {
            renderer->adapterType = (arch.UMA == TRUE) ? VGPU_ADAPTER_TYPE_INTEGRATED_GPU : VGPU_ADAPTER_TYPE_DISCRETE_GPU;
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

        // Check the maximum feature level, and make sure it's above our minimum
        D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels =
        {
            _countof(s_featureLevels), s_featureLevels, D3D_FEATURE_LEVEL_11_0
        };

        hr = renderer->device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels));
        if (SUCCEEDED(hr))
        {
            renderer->featureLevel = featureLevels.MaxSupportedFeatureLevel;
        }
        else
        {
            renderer->featureLevel = D3D_FEATURE_LEVEL_11_0;
        }

        vgfxLogInfo("VGPU Driver: D3D12");
        vgfxLogInfo("D3D12 Adapter: %S", adapterDesc.Description);
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

        for (uint32_t i = 0; i < VGFX_MAX_INFLIGHT_FRAMES; ++i)
        {
            if (!SUCCEEDED(renderer->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&renderer->commandAllocators[i]))))
            {
                vgfxLogError("Unable to create command allocator");
                delete renderer;
                return nullptr;
            }
        }

        hr = renderer->device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            renderer->commandAllocators[0],
            nullptr,
            IID_PPV_ARGS(&renderer->graphicsCommandList)
        );
        if (FAILED(hr))
        {
            delete renderer;
            return nullptr;
        }
    }

    VGFXDevice_T* device = (VGFXDevice_T*)VGFX_MALLOC(sizeof(VGFXDevice_T));
    ASSIGN_DRIVER(d3d12);
    device->driverData = (VGFXRenderer*)renderer;
    return device;
}

VGFXDriver D3D12_Driver = {
    VGPU_BACKEND_TYPE_D3D12,
    d3d12_isSupported,
    d3d12_createDevice
};

#endif /* VGPU_D3D12_DRIVER */
