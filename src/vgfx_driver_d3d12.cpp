// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGFX_D3D12_DRIVER)

#include "vgfx_driver.h"

#define NOMINMAX
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#ifdef USING_DIRECTX_HEADERS
#   include <directx/dxgiformat.h>
#   include <directx/d3d12.h>
#else
#   include <d3d12.h>
#endif
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <deque>
#include <mutex>

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

#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   include <dxgidebug.h>
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

    // Declare debug guids to avoid linking with "dxguid.lib"
    static constexpr IID VGFX_DXGI_DEBUG_ALL = { 0xe48ae283, 0xda80, 0x490b, {0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8} };
    static constexpr IID VGFX_DXGI_DEBUG_DXGI = { 0x25cddaa4, 0xb1c6, 0x47e1, {0xac, 0x3e, 0x98, 0x87, 0x5b, 0x5a, 0x2e, 0x2a} };
#endif
#endif

    template <typename T>
    void SafeRelease(T& resource)
    {
        if (resource)
        {
            resource->Release();
            resource = nullptr;
        }
    }
}

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define vgfxCreateDXGIFactory2 CreateDXGIFactory2
#define vgfxD3D12CreateDevice D3D12CreateDevice
#define vgfxD3D12GetDebugInterface D3D12GetDebugInterface
#endif

struct VGFXD3D12SwapChain {
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HWND window = nullptr;
#else
    IUnknown* window = nullptr;
#endif

    IDXGISwapChain3* handle = nullptr;
};

struct VGFXD3D12Renderer
{
    ComPtr<IDXGIFactory4> factory;
    bool tearingSupported = false;
    ID3D12Device5* device = nullptr;
    D3D_FEATURE_LEVEL featureLevel{};

    ID3D12Fence* frameFence = nullptr;
    HANDLE frameFenceEvent = INVALID_HANDLE_VALUE;

    D3D12MA::Allocator* allocator = nullptr;
    ID3D12CommandQueue* graphicsQueue = nullptr;

    ID3D12GraphicsCommandList4* graphicsCommandList = nullptr;
    ID3D12CommandAllocator* commandAllocators[VGFX_MAX_INFLIGHT_FRAMES] = {};

    VGFXD3D12SwapChain swapChains[64];
    VGFXD3D12SwapChain* currentSwapChain = nullptr;

    uint32_t frameIndex = 0;
    uint64_t frameCount = 0;
    uint64_t GPUFrameCount = 0;

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

    if (renderer->shuttingDown)
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

static void d3d12_destroyDevice(VGFXDevice device)
{
    // Wait idle
    vgfxWaitIdle(device);

    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)device->driverData;

    VGFX_ASSERT(renderer->frameCount == renderer->GPUFrameCount);
    renderer->shuttingDown = true;

    // Frame fence
    SafeRelease(renderer->frameFence);
    CloseHandle(renderer->frameFenceEvent);

    for (VGFXD3D12SwapChain& swapChain : renderer->swapChains)
    {
        if (!swapChain.handle)
            continue;

        swapChain.handle->Release();
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

static bool d3d12_createSwapchain(VGFXD3D12Renderer* renderer, VGFXSurface surface, VGFXD3D12SwapChain& swapChain)
{
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = surface->width;
    swapChainDesc.Height = surface->height;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = VGFX_MAX_INFLIGHT_FRAMES;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = renderer->tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;

    ComPtr<IDXGISwapChain1> tempSwapChain;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = TRUE;

    // Create a swap chain for the window.
    HRESULT hr = renderer->factory->CreateSwapChainForHwnd(
        renderer->graphicsQueue,
        surface->window,
        &swapChainDesc,
        &fsSwapChainDesc,
        nullptr,
        tempSwapChain.GetAddressOf()
    );

    if (FAILED(hr))
    {
        return false;
    }

    // This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
    hr = renderer->factory->MakeWindowAssociation(surface->window, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr))
    {
        return false;
    }

#else
    swapChainDesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;

    HRESULT hr = renderer->factory->CreateSwapChainForCoreWindow(
        renderer->graphicsQueue,
        surface->window,
        &swapChainDesc,
        nullptr,
        tempSwapChain.GetAddressOf()
    );
#endif

    hr = tempSwapChain->QueryInterface(&swapChain.handle);
    if (FAILED(hr))
    {
        return false;
    }

    return true;
};

static void d3d12_frame(VGFXRenderer* driverData)
{
    HRESULT hr = S_OK;
    VGFXD3D12Renderer* renderer = (VGFXD3D12Renderer*)driverData;

    hr = renderer->graphicsCommandList->Close();
    if (FAILED(hr))
    {
        vgfxLogError("Failed to close command list");
        return;
    }

    ID3D12CommandList* commandLists[] = { renderer->graphicsCommandList };
    renderer->graphicsQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    // Present secondary windows without vertical sync
    uint32_t syncInterval = 0;
    uint32_t presentFlags = renderer->tearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;

    for (uint32_t i = 1, count = _VGFX_COUNT_OF(renderer->swapChains); i < count && SUCCEEDED(hr); ++i)
    {
        VGFXD3D12SwapChain* swapChain = &renderer->swapChains[i];
        if (!swapChain->handle)
            continue;

        hr = swapChain->handle->Present(syncInterval, presentFlags);
    }

    // Main SwapChain
    syncInterval = 1;
    presentFlags = 0;
    if (renderer->swapChains[0].handle)
    {
        hr = renderer->swapChains[0].handle->Present(syncInterval, presentFlags);
    }

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
            return;
        }

        renderer->frameCount++;

        // Signal the fence with the current frame number, so that we can check back on it
        hr = renderer->graphicsQueue->Signal(renderer->frameFence, renderer->frameCount);
        if (FAILED(hr))
        {
            vgfxLogError("Failed to signal frame");
            return;
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

        renderer->frameIndex = renderer->frameCount % VGFX_MAX_INFLIGHT_FRAMES;

        // Prepare the command buffers to be used for the next frame
        renderer->commandAllocators[renderer->frameIndex]->Reset();
        renderer->graphicsCommandList->Reset(renderer->commandAllocators[renderer->frameIndex], nullptr);

        // Safe delete deferred destroys
        d3d12_ProcessDeletionQueue(renderer);
    }
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

static VGFXDevice d3d12_createDevice(VGFXSurface surface, const VGFXDeviceInfo* info)
{
    VGFX_ASSERT(info);

    VGFXD3D12Renderer* renderer = new VGFXD3D12Renderer();

    DWORD dxgiFactoryFlags = 0;
    if (info->validationMode != VGFX_VALIDATION_MODE_DISABLED)
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(vgfxD3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
        {
            debugController->EnableDebugLayer();

            if (info->validationMode == VGFX_VALIDATION_MODE_GPU)
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

#ifdef _DEBUG
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

        // Create the DX12 API device object.
        renderer->device->SetName(L"vgfx-device");

        if (info->validationMode != VGFX_VALIDATION_MODE_DISABLED)
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

                if (info->validationMode == VGFX_VALIDATION_MODE_VERBOSE)
                {
                    // Verbose only filters
                    enabledSeverities.push_back(D3D12_MESSAGE_SEVERITY_INFO);
            }

#if defined (VGFX_DX12_USE_PIPELINE_LIBRARY)
                disabledMessages.push_back(D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND);
                disabledMessages.push_back(D3D12_MESSAGE_ID_STOREPIPELINE_DUPLICATENAME);
#endif

                disabledMessages.push_back(D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE);
                disabledMessages.push_back(D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE);
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

        // Check the maximum feature level, and make sure it's above our minimum
        D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels =
        {
            _countof(s_featureLevels), s_featureLevels, D3D_FEATURE_LEVEL_11_0
        };

        HRESULT hr = renderer->device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels));
        if (SUCCEEDED(hr))
        {
            renderer->featureLevel = featureLevels.MaxSupportedFeatureLevel;
        }
        else
        {
            renderer->featureLevel = D3D_FEATURE_LEVEL_11_0;
        }

        vgfxLogInfo("vgfx driver: D3D12");
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
    }

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

    // Create SwapChain
    if (!d3d12_createSwapchain(renderer, surface, renderer->swapChains[0]))
    {
        delete renderer;
        return nullptr;
    }
    renderer->currentSwapChain = &renderer->swapChains[0];

    VGFXDevice_T* device = (VGFXDevice_T*)VGFX_MALLOC(sizeof(VGFXDevice_T));
    ASSIGN_DRIVER(d3d12);
    device->driverData = (VGFXRenderer*)renderer;
    return device;
}

VGFXDriver d3d12_driver = {
    VGFX_API_D3D12,
    d3d12_isSupported,
    d3d12_createDevice
};

#endif /* VGFX_D3D12_DRIVER */
