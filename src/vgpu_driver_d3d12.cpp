// Copyright (c) Amer Koleci.
// Distributed under the MIT license. See the LICENSE file in the project root for more information.


#if defined(VGPU_DRIVER_D3D12)

#if defined(NTDDI_WIN10_RS2)
#   include <dxgi1_6.h>
#else
#   include <dxgi1_5.h>
#endif

#include "d3d12/d3d12.h"
#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#include "d3d12/D3D12MemAlloc.h"
#include "vgpu_d3d_common.h"
#include <stdio.h>

static struct {
    bool available_initialized;
    bool available;
} d3d12;

static bool D3D12_IsSupported(void) {
    if (d3d12.available_initialized) {
        return d3d12.available;
    }

    d3d12.available_initialized = true;

    d3d12.available = true;
    return true;
};

static VGPUDeviceImpl* D3D12_CreateDevice(const VGPUDeviceDescriptor* info) {
    return NULL;
}

VGPU_Driver D3D12_Driver = {
    VGPU_BACKEND_TYPE_D3D12,
    D3D12_IsSupported,
    D3D12_CreateDevice
};

#if TODO
typedef struct D3D12Texture {
    ID3D12Resource* handle;
    D3D12MA::Allocation* allocation;
    DXGI_FORMAT dxgiFormat;

    VGPUTextureType type;
    uint32_t width;
    uint32_t height;
    uint32_t depthOrArrayLayers;
    VGPUTextureFormat format;
    uint32_t mipLevels;
    uint32_t sample_count;
} D3D12Texture;


typedef struct D3D12Buffer {
    ID3D12Resource* handle;
    D3D12MA::Allocation* allocation;
} D3D12Buffer;

typedef struct D3D12Swapchain {
    IDXGISwapChain3* handle;
    VGPUTextureFormat colorFormat;
    VGPUTextureFormat depthStencilFormat;
    uint32_t width;
    uint32_t height;
    uint32_t backbufferIndex;
    VGPUTexture backbufferTextures[VGPU_MAX_INFLIGHT_FRAMES];
    VGPUTexture depthStencilTexture;
} D3D12Swapchain;

typedef struct D3D12Renderer {
    VGPUDeviceCaps caps;

    bool debug;
    bool gpuBasedValidation;
    UINT dxgiFactoryFlags;
    IDXGIFactory4* factory;
    uint32_t factory_caps;

    uint32_t sync_interval;
    uint32_t present_flags;

    ID3D12Device* device;
    D3D12MA::Allocator* allocator;
    D3D_FEATURE_LEVEL featureLevel;
    VGPUTextureFormat defaultDepthFormat;
    VGPUTextureFormat defaultDepthStencilFormat;

    ID3D12CommandQueue* graphicsQueue;
    ID3D12CommandQueue* computeQueue;

    uint32_t frameIndex;
    uint64_t frameCount;
    ID3D12Fence* frameFence;
    HANDLE frameFenceEvent;

    bool is_lost;
    VGPUDevice gpuDevice;

    D3D12Swapchain swapchains[8];
} D3D12Renderer;

/* Global data */
static struct {
    bool available_initialized;
    bool available;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    struct
    {
        HMODULE instance;
        PFN_CREATE_DXGI_FACTORY2 CreateDXGIFactory2;
        PFN_GET_DXGI_DEBUG_INTERFACE1 DXGIGetDebugInterface1;
    } dxgi;

    HMODULE instance;
    PFN_D3D12_GET_DEBUG_INTERFACE D3D12GetDebugInterface;
    PFN_D3D12_CREATE_DEVICE D3D12CreateDevice;
#endif
} d3d12;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   define _vgpu_DXGIGetDebugInterface1 d3d12.dxgi.DXGIGetDebugInterface1
#   define _vgpu_CreateDXGIFactory2 d3d12.dxgi.CreateDXGIFactory2
#   define _vgpu_D3D12GetDebugInterface d3d12.D3D12GetDebugInterface
#   define _vgpu_D3D12CreateDevice d3d12.D3D12CreateDevice
#else
#   define _vgpu_DXGIGetDebugInterface1 DXGIGetDebugInterface1
#   define _vgpu_CreateDXGIFactory2 CreateDXGIFactory2
#   define _vgpu_D3D12GetDebugInterface D3D12GetDebugInterface
#   define _vgpu_D3D12CreateDevice D3D12CreateDevice
#endif

/* Device functions */
static bool _vgpu_d3d12_createFactory(D3D12Renderer* renderer)
{
    SAFE_RELEASE(renderer->factory);
    HRESULT hr = S_OK;

#if defined(_DEBUG)
    if (renderer->debug || renderer->gpuBasedValidation)
    {
        ID3D12Debug* d3d12Debug;
        if (SUCCEEDED(_vgpu_D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&d3d12Debug)))
        {
            d3d12Debug->EnableDebugLayer();

            ID3D12Debug1* d3d12Debug1;
            if (SUCCEEDED(d3d12Debug->QueryInterface(__uuidof(ID3D12Debug1), (void**)&d3d12Debug1)))
            {
                d3d12Debug1->SetEnableGPUBasedValidation(renderer->gpuBasedValidation);
                d3d12Debug1->Release();
            }

            d3d12Debug->Release();
        }
        else
        {
            OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
        }

        IDXGIInfoQueue* dxgiInfoQueue;
        if (SUCCEEDED(_vgpu_DXGIGetDebugInterface1(0, __uuidof(IDXGIInfoQueue), (void**)&dxgiInfoQueue)))
        {
            renderer->dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

            VHR(dxgiInfoQueue->SetBreakOnSeverity(D3D_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE));
            VHR(dxgiInfoQueue->SetBreakOnSeverity(D3D_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE));
            VHR(dxgiInfoQueue->SetBreakOnSeverity(D3D_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, FALSE));

            DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
            {
                80 // IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides.
            };

            DXGI_INFO_QUEUE_FILTER filter;
            memset(&filter, 0, sizeof(filter));
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            dxgiInfoQueue->AddStorageFilterEntries(D3D_DXGI_DEBUG_DXGI, &filter);
            dxgiInfoQueue->Release();
        }
    }

#endif

    hr = _vgpu_CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, __uuidof(IDXGIFactory2), (void**)&renderer->factory);
    if (FAILED(hr)) {
        return false;
    }

    renderer->factory_caps = DXGIFACTORY_CAPS_FLIP_PRESENT;

    // Check tearing support.
    {
        BOOL allowTearing = FALSE;
        IDXGIFactory5* factory5;
        HRESULT hr = renderer->factory->QueryInterface(__uuidof(IDXGIFactory5), (void**)&factory5);
        if (SUCCEEDED(hr))
        {
            hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
            factory5->Release();
        }

        if (FAILED(hr) || !allowTearing)
        {
#ifdef _DEBUG
            OutputDebugStringA("WARNING: Variable refresh rate displays not supported");
#endif
        }
        else
        {
            renderer->factory_caps |= DXGIFACTORY_CAPS_TEARING;
        }
    }

    return true;
}
static IDXGIAdapter1* d3d12_GetAdapter(D3D12Renderer* renderer, VGPUAdapterType adapterType)
{
    /* Detect adapter now. */
    IDXGIAdapter1* adapter = NULL;

#if defined(__dxgi1_6_h__) && defined(NTDDI_WIN10_RS4)
    {
        IDXGIFactory6* factory6;
        HRESULT hr = renderer->factory->QueryInterface(__uuidof(IDXGIFactory6), (void**)&factory6);
        if (SUCCEEDED(hr))
        {
            // By default prefer high performance
            DXGI_GPU_PREFERENCE gpuPreference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
            if (adapterType == VGPUAdapterType_IntegratedGPU)
            {
                gpuPreference = DXGI_GPU_PREFERENCE_MINIMUM_POWER;
            }

            for (uint32_t i = 0;
                DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(i, gpuPreference, __uuidof(IDXGIAdapter1), (void**)&adapter); i++)
            {
                DXGI_ADAPTER_DESC1 desc;
                VHR(adapter->GetDesc1(&desc));

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    adapter->Release();
                    continue;
                }

                break;
            }

            factory6->Release();
        }
    }
#endif

    if (!adapter)
    {
        for (uint32_t i = 0; DXGI_ERROR_NOT_FOUND != renderer->factory->EnumAdapters1(i, &adapter); i++)
        {
            DXGI_ADAPTER_DESC1 desc;
            VHR(adapter->GetDesc1(&desc));

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                adapter->Release();

                continue;
            }

            break;
        }
    }

    return adapter;
}

static void d3d12_swapchain_destroy(D3D12Renderer* renderer, D3D12Swapchain* swapchain);
static void d3d12_swapchain_present(D3D12Renderer* renderer, D3D12Swapchain* swapchain);

static void d3d12_ExecuteDeferredReleases()
{

}

static void d3d12_WaitIdle(vgpu_renderer driverData)
{
    D3D12Renderer* renderer = (D3D12Renderer*)driverData;

    VHR(renderer->graphicsQueue->Signal(renderer->frameFence, ++renderer->frameCount));
    VHR(renderer->frameFence->SetEventOnCompletion(renderer->frameCount, renderer->frameFenceEvent));
    WaitForSingleObject(renderer->frameFenceEvent, INFINITE);

    d3d12_ExecuteDeferredReleases();
}

static void d3d12_destroy(VGPUDevice device)
{
    d3d12_WaitIdle(device->renderer);

    D3D12Renderer* renderer = (D3D12Renderer*)device->renderer;

    // Swapchains
    for (uint32_t i = 0; i < _vgpu_count_of(renderer->swapchains); i++)
    {
        if (!renderer->swapchains[i].handle)
            continue;

        d3d12_swapchain_destroy(renderer, &renderer->swapchains[i]);
    }

    // Command queues
    SAFE_RELEASE(renderer->computeQueue);
    SAFE_RELEASE(renderer->graphicsQueue);

    // Frame fence
    CloseHandle(renderer->frameFenceEvent);
    SAFE_RELEASE(renderer->frameFence);

    // Allocator.
    D3D12MA::Stats stats;
    renderer->allocator->CalculateStats(&stats);

    if (stats.Total.UsedBytes > 0) {
        //vgpuLogError("Total device memory leaked: {} bytes.", stats.Total.UsedBytes);
    }

    SAFE_RELEASE(renderer->allocator);

#if !defined(NDEBUG)
    ULONG refCount = renderer->device->Release();
    if (refCount > 0)
    {
        vgpuLogError("Direct3D12: There are %d unreleased references left on the device", refCount);

        ID3D12DebugDevice* debugDevice = nullptr;
        if (SUCCEEDED(renderer->device->QueryInterface(__uuidof(ID3D12DebugDevice), (void**)&debugDevice)))
        {
            debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
            debugDevice->Release();
        }
    }
#else
    renderer->device->Release();
#endif
    renderer->device = nullptr;

    SAFE_RELEASE(renderer->factory);

#ifdef _DEBUG
    IDXGIDebug1* dxgi_debug1;
    if (SUCCEEDED(_vgpu_DXGIGetDebugInterface1(0, __uuidof(IDXGIDebug1), (void**)&dxgi_debug1)))
    {
        dxgi_debug1->ReportLiveObjects(D3D_DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        dxgi_debug1->Release();
    }
#endif

    VGPU_FREE(renderer);
    VGPU_FREE(device);
}

static void d3d12_get_caps(vgpu_renderer driver_data, VGPUDeviceCaps* caps)
{
    D3D12Renderer* renderer = (D3D12Renderer*)driver_data;
    *caps = renderer->caps;
}

static VGPUTextureFormat d3d12_getDefaultDepthFormat(vgpu_renderer driverData)
{
    D3D12Renderer* renderer = (D3D12Renderer*)driverData;
    return renderer->defaultDepthFormat;
}

static VGPUTextureFormat d3d12_getDefaultDepthStencilFormat(vgpu_renderer driverData)
{
    D3D12Renderer* renderer = (D3D12Renderer*)driverData;
    return renderer->defaultDepthStencilFormat;
}

static vgpu_bool d3d12_frame_begin(vgpu_renderer driverData) {
    VGPU_UNUSED(driverData);
    return true;
}

static void d3d12_frame_end(vgpu_renderer driverData) {
    D3D12Renderer* renderer = (D3D12Renderer*)driverData;
    for (uint32_t i = 0; i < _vgpu_count_of(renderer->swapchains); i++) {
        if (!renderer->swapchains[i].handle)
            continue;

        d3d12_swapchain_present(renderer, &renderer->swapchains[i]);
    }

    VHR(renderer->graphicsQueue->Signal(renderer->frameFence, ++renderer->frameCount));

    uint64_t GPUFrameCount = renderer->frameFence->GetCompletedValue();

    // Make sure that the previous frame is finished
    if ((renderer->frameCount - GPUFrameCount) >= VGPU_NUM_INFLIGHT_FRAMES)
    {
        VHR(renderer->frameFence->SetEventOnCompletion(GPUFrameCount + 1, renderer->frameFenceEvent));
        WaitForSingleObject(renderer->frameFenceEvent, INFINITE);
    }

    renderer->frameIndex = renderer->frameCount % VGPU_NUM_INFLIGHT_FRAMES;

    if (!renderer->is_lost)
    {
        // Output information is cached on the DXGI Factory. If it is stale we need to create a new factory.
        if (!renderer->factory->IsCurrent())
        {
            _vgpu_d3d12_createFactory(renderer);
        }
    }
}

static VGPUTexture d3d12_getBackbufferTexture(vgpu_renderer driverData, uint32_t swapchainIndex) {
    D3D12Renderer* renderer = (D3D12Renderer*)driverData;
    D3D12Swapchain* swapchain = &renderer->swapchains[swapchainIndex];
    return swapchain->backbufferTextures[swapchain->backbufferIndex];
}


/* Swapchain */
static void d3d12_swapchain_update(D3D12Renderer* renderer, D3D12Swapchain* swapchain) {
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc;
    VHR(swapchain->handle->GetDesc1(&swapchainDesc));
    swapchain->width = swapchainDesc.Width;
    swapchain->height = swapchainDesc.Height;
    swapchain->backbufferIndex = swapchain->handle->GetCurrentBackBufferIndex();

    // Get backbuffer
    {
        ID3D12Resource* backbuffer;
        for (uint32_t i = 0; i < swapchainDesc.BufferCount; i++)
        {
            VHR(swapchain->handle->GetBuffer(i, __uuidof(ID3D12Resource), (void**)&backbuffer));

            VGPUTextureDescription textureDesc = {};
            textureDesc.width = swapchain->width;
            textureDesc.height = swapchain->height;
            textureDesc.depth = 1u;
            textureDesc.mipLevelCount = 1;
            textureDesc.format = swapchain->colorFormat;
            textureDesc.type = VGPU_TEXTURE_TYPE_2D;
            textureDesc.usage = VGPUTextureUsage_RenderTarget;
            textureDesc.sampleCount = 1u;
            textureDesc.externalHandle = backbuffer;
            swapchain->backbufferTextures[i] = vgpuCreateTexture(renderer->gpuDevice, &textureDesc);
            backbuffer->Release();
        }
    }

}

static void d3d12_swapchain_create(D3D12Renderer* renderer, D3D12Swapchain* swapchain, const vgpu_swapchain_info* info)
{
    IDXGISwapChain1* tempSwapChain = vgpu_d3d_create_swapchain(
        renderer->factory,
        renderer->factory_caps,
        renderer->graphicsQueue,
        info->window_handle,
        info->width, info->height,
        info->colorFormat,
        VGPU_NUM_INFLIGHT_FRAMES,
        !info->fullscreen
    );

    VHR(tempSwapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapchain->handle));
    tempSwapChain->Release();

    d3d12_swapchain_update(renderer, swapchain);
}

/* Texture */
static VGPUTexture d3d12_createTexture(vgpu_renderer driverData, const VGPUTextureDescription* desc)
{
    D3D12Renderer* renderer = (D3D12Renderer*)driverData;
    D3D12Texture* texture = VGPU_ALLOC_HANDLE(D3D12Texture);
    texture->type = desc->type;
    texture->width = desc->width;
    texture->height = desc->height;
    texture->depthOrArrayLayers = desc->depth;
    texture->format = desc->format;
    texture->mipLevels = desc->mipLevelCount;
    texture->sample_count = desc->sampleCount;

    texture->dxgiFormat = _vgpu_d3d_format_with_usage(desc->format, desc->usage);

    HRESULT hr = S_OK;
    if (desc->externalHandle)
    {
        texture->handle = (ID3D12Resource*)desc->externalHandle;
        texture->handle->AddRef();
    }
    else
    {

    }

    if (desc->usage & VGPUTextureUsage_RenderTarget)
    {
        if (vgpuIsDepthStencilFormat(desc->format))
        {
            //VHR(renderer->device->CreateDepthStencilView(texture->handle, nullptr, &texture->dsv));
        }
        else
        {
            //VHR(renderer->device->CreateRenderTargetView(texture->handle, nullptr, &texture->rtv));
        }
    }

    return (VGPUTexture)texture;
}

static void d3d12_destroyTexture(vgpu_renderer driverData, VGPUTexture handle)
{
    D3D12Texture* texture = (D3D12Texture*)handle;
    SAFE_RELEASE(texture->handle);
    VGPU_FREE(texture);
}

/* Buffer */
static VGPUBuffer d3d12_createBuffer(vgpu_renderer driverData, const VGPUBufferDescriptor* descriptor)
{
    D3D12Renderer* renderer = (D3D12Renderer*)driverData;
    D3D12Buffer* buffer = VGPU_ALLOC_HANDLE(D3D12Buffer);
    return (VGPUBuffer)buffer;
}

static void d3d12_destroyBuffer(vgpu_renderer driverData, VGPUBuffer handle)
{
    D3D12Buffer* buffer = (D3D12Buffer*)handle;
    //SAFE_RELEASE(buffer->handle);
    VGPU_FREE(buffer);
}


/* Sampler */
static VGPUSampler d3d12_createSampler(vgpu_renderer driverData, const VGPUSamplerDescriptor* desc)
{
    return nullptr;
}

static void d3d12_destroySampler(vgpu_renderer driverData, VGPUSampler handle)
{

}

/* Shader */
static VGPUShaderModule d3d12_createShaderModule(vgpu_renderer driverData, const VGPUShaderModuleDescriptor* desc)
{
    return nullptr;
}

static void d3d12_destroyShaderModule(vgpu_renderer driverData, VGPUShaderModule handle)
{

}


/* SwapChain */
static void d3d12_swapchain_destroy(D3D12Renderer* renderer, D3D12Swapchain* swapchain)
{
    for (uint32_t i = 0; i < VGPU_NUM_INFLIGHT_FRAMES; i++)
    {
        vgpuDestroyTexture(renderer->gpuDevice, swapchain->backbufferTextures[i]);
    }

    if (swapchain->depthStencilTexture) {
        vgpuDestroyTexture(renderer->gpuDevice, swapchain->depthStencilTexture);
    }
    SAFE_RELEASE(swapchain->handle);
}

static void d3d12_swapchain_present(D3D12Renderer* renderer, D3D12Swapchain* swapchain)
{
    HRESULT hr = swapchain->handle->Present(renderer->sync_interval, renderer->present_flags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED
        || hr == DXGI_ERROR_DEVICE_HUNG
        || hr == DXGI_ERROR_DEVICE_RESET
        || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR
        || hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
    {
        renderer->is_lost = true;
        return;
    }

    swapchain->backbufferIndex = swapchain->handle->GetCurrentBackBufferIndex();
}

/* Commands */
static void d3d12_cmdBeginRenderPass(vgpu_renderer driverData, const VGPURenderPassDescriptor* desc)
{
    D3D12Renderer* renderer = (D3D12Renderer*)driverData;

    /*renderer->rtvs_count = 0;
    for (uint32_t i = 0; i < VGPU_MAX_COLOR_ATTACHMENTS; i++)
    {
        if (!desc->colorAttachments[i].texture)
            continue;

        D3D12Texture* texture = (D3D12Texture*)desc->colorAttachments[i].texture;

        if (desc->colorAttachments[i].loadOp == VGPULoadOp_Clear)
        {
            renderer->context->ClearRenderTargetView(texture->rtv, &desc->colorAttachments[i].clearColor.r);
        }

        renderer->cur_rtvs[renderer->rtvs_count++] = texture->rtv;
    }

    if (desc->depthStencilAttachment.texture)
    {
        D3D11Texture* texture = (D3D11Texture*)desc->depthStencilAttachment.texture;
        renderer->cur_dsv = texture->dsv;

        const bool hasDepth = vgpuIsDepthFormat(texture->format);
        const bool hasStencil = vgpuIsStencilFormat(texture->format);

        UINT clearFlags = 0;
        if (hasDepth && desc->depthStencilAttachment.depthLoadOp == VGPULoadOp_Clear)
        {
            clearFlags |= D3D11_CLEAR_DEPTH;
        }

        if (hasStencil && desc->depthStencilAttachment.stencilLoadOp == VGPULoadOp_Clear)
        {
            clearFlags |= D3D11_CLEAR_STENCIL;
        }

        renderer->context->ClearDepthStencilView(
            texture->dsv,
            clearFlags,
            desc->depthStencilAttachment.clearDepth,
            static_cast<uint8_t>(desc->depthStencilAttachment.clearStencil)
        );
    }

    renderer->context->OMSetRenderTargets(renderer->rtvs_count, renderer->cur_rtvs, renderer->cur_dsv);*/
}

static void d3d12_cmdEndRenderPass(vgpu_renderer driverData)
{
    D3D12Renderer* renderer = (D3D12Renderer*)driverData;
    /* renderer->context->OMSetRenderTargets(VGPU_MAX_COLOR_ATTACHMENTS, renderer->zero_rtvs, nullptr);

     for (uint32_t i = 0; i < renderer->rtvs_count; i++) {
         renderer->cur_rtvs[i] = nullptr;
     }

     renderer->rtvs_count = 0;
     renderer->cur_dsv = nullptr;*/
}

/* Driver functions */
static bool d3d12_is_supported(void) {
    if (d3d12.available_initialized) {
        return d3d12.available;
    }

    d3d12.available_initialized = true;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) 
    d3d12.dxgi.instance = LoadLibraryA("dxgi.dll");
    if (!d3d12.dxgi.instance) {
        return false;
    }

    d3d12.dxgi.CreateDXGIFactory2 = (PFN_CREATE_DXGI_FACTORY2)GetProcAddress(d3d12.dxgi.instance, "CreateDXGIFactory2");
    if (!d3d12.dxgi.CreateDXGIFactory2)
    {
        return false;
    }
    d3d12.dxgi.DXGIGetDebugInterface1 = (PFN_GET_DXGI_DEBUG_INTERFACE1)GetProcAddress(d3d12.dxgi.instance, "DXGIGetDebugInterface1");

    d3d12.instance = LoadLibraryA("d3d12.dll");
    if (!d3d12.instance) {
        return false;
    }

    d3d12.D3D12GetDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(d3d12.instance, "D3D12GetDebugInterface");
    d3d12.D3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12.instance, "D3D12CreateDevice");
    if (!d3d12.D3D12CreateDevice) {
        return false;
    }
#endif

    HRESULT hr = _vgpu_D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);

    if (FAILED(hr))
    {
        return false;
    }

    d3d12.available = true;
    return true;
};

static VGPUDeviceImpl* d3d12_create_device(const VGPUDeviceDescriptor* info) {
    /* Allocate and zero out the renderer */
    D3D12Renderer* renderer = VGPU_ALLOC_HANDLE(D3D12Renderer);
    renderer->debug = (info->flags & VGPUDeviceFlags_Debug) != 0;
    renderer->gpuBasedValidation = (info->flags & VGPUDeviceFlags_GPUBasedValidation) != 0;

    if (!_vgpu_d3d12_createFactory(renderer)) {
        return nullptr;
    }

    /* Setup present flags. */
    renderer->sync_interval = _vgpu_d3d_sync_interval(info->swapchain.presentMode);
    renderer->present_flags = 0;
    if (!renderer->sync_interval && renderer->factory_caps & DXGIFACTORY_CAPS_TEARING) {
        renderer->present_flags |= DXGI_PRESENT_ALLOW_TEARING;
    }

    /* Get DXGI adapter and create D3D12 device */
    IDXGIAdapter1* dxgiAdapter = d3d12_GetAdapter(renderer, info->adapterPreference);
    {
        HRESULT hr = _vgpu_D3D12CreateDevice(dxgiAdapter, D3D_FEATURE_LEVEL_11_1, __uuidof(ID3D12Device), (void**)&renderer->device);

        if (FAILED(hr)) {
            return false;
        }

#ifndef NDEBUG
        ID3D12InfoQueue* d3dInfoQueue;
        if (SUCCEEDED(renderer->device->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)&d3dInfoQueue)))
        {
#ifdef _DEBUG
            d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif

            D3D12_MESSAGE_ID hide[] =
            {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE
            };
            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            d3dInfoQueue->AddStorageFilterEntries(&filter);
            d3dInfoQueue->Release();
        }
#endif

        // Create memory allocator.
        D3D12MA::ALLOCATOR_DESC desc = {};
        desc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
        desc.pDevice = renderer->device;
        desc.pAdapter = dxgiAdapter;
        VHR(D3D12MA::CreateAllocator(&desc, &renderer->allocator));
    }

    /* Init caps first. */
    {
        DXGI_ADAPTER_DESC1 adapterDesc;
        VHR(dxgiAdapter->GetDesc1(&adapterDesc));

        /* Log some info */
        vgpuLogInfo("VGPU driver: D3D12");
        vgpuLogInfo("Direct3D Adapter: VID:%04X, PID:%04X - %ls", adapterDesc.VendorId, adapterDesc.DeviceId, adapterDesc.Description);

        renderer->caps.backendType = VGPUBackendType_D3D12;
        renderer->caps.vendorId = adapterDesc.VendorId;
        renderer->caps.adapterId = adapterDesc.DeviceId;
        _vgpu_StringConvert(adapterDesc.Description, renderer->caps.adapterName);

        // Detect adapter type.
        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            renderer->caps.adapterType = VGPUAdapterType_CPU;
        }
        else
        {
            D3D12_FEATURE_DATA_ARCHITECTURE arch = {};
            VHR(renderer->device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch, sizeof(arch)));
            renderer->caps.adapterType = arch.UMA ? VGPUAdapterType_IntegratedGPU : VGPUAdapterType_DiscreteGPU;
        }

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

        if (SUCCEEDED(renderer->device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels))))
        {
            renderer->featureLevel = featLevels.MaxSupportedFeatureLevel;
        }
        else
        {
            renderer->featureLevel = D3D_FEATURE_LEVEL_11_0;
        }

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = { };
        VHR(renderer->device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));

        renderer->caps.features.independentBlend = true;
        renderer->caps.features.computeShader = true;
        renderer->caps.features.tessellationShader = true;
        renderer->caps.features.multiViewport = true;
        renderer->caps.features.indexUInt32 = true;
        renderer->caps.features.multiDrawIndirect = true;
        renderer->caps.features.fillModeNonSolid = true;
        renderer->caps.features.samplerAnisotropy = true;
        renderer->caps.features.textureCompressionETC2 = false;
        renderer->caps.features.textureCompressionASTC_LDR = false;
        renderer->caps.features.textureCompressionBC = true;
        renderer->caps.features.textureCubeArray = true;
        renderer->caps.features.raytracing = options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;

        // Limits
        renderer->caps.limits.maxVertexAttributes = VGPU_MAX_VERTEX_ATTRIBUTES;
        renderer->caps.limits.maxVertexBindings = VGPU_MAX_VERTEX_ATTRIBUTES;
        renderer->caps.limits.maxVertexAttributeOffset = VGPU_MAX_VERTEX_ATTRIBUTE_OFFSET;
        renderer->caps.limits.maxVertexBindingStride = VGPU_MAX_VERTEX_BUFFER_STRIDE;

        renderer->caps.limits.maxTextureDimension2D = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        renderer->caps.limits.maxTextureDimension3D = D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        renderer->caps.limits.maxTextureDimensionCube = D3D12_REQ_TEXTURECUBE_DIMENSION;
        renderer->caps.limits.maxTextureArrayLayers = D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        renderer->caps.limits.maxColorAttachments = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
        renderer->caps.limits.maxUniformBufferRange = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
        renderer->caps.limits.maxStorageBufferRange = UINT32_MAX;
        renderer->caps.limits.minUniformBufferOffsetAlignment = 256u;
        renderer->caps.limits.minStorageBufferOffsetAlignment = 32u;
        renderer->caps.limits.maxSamplerAnisotropy = D3D12_MAX_MAXANISOTROPY;
        renderer->caps.limits.maxViewports = D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        renderer->caps.limits.maxViewportWidth = D3D12_VIEWPORT_BOUNDS_MAX;
        renderer->caps.limits.maxViewportHeight = D3D12_VIEWPORT_BOUNDS_MAX;
        renderer->caps.limits.maxTessellationPatchSize = D3D12_IA_PATCH_MAX_CONTROL_POINT_COUNT;
        renderer->caps.limits.maxComputeSharedMemorySize = D3D12_CS_THREAD_LOCAL_TEMP_REGISTER_POOL;
        renderer->caps.limits.maxComputeWorkGroupCountX = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        renderer->caps.limits.maxComputeWorkGroupCountY = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        renderer->caps.limits.maxComputeWorkGroupCountZ = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        renderer->caps.limits.maxComputeWorkGroupInvocations = D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;
        renderer->caps.limits.maxComputeWorkGroupSizeX = D3D12_CS_THREAD_GROUP_MAX_X;
        renderer->caps.limits.maxComputeWorkGroupSizeY = D3D12_CS_THREAD_GROUP_MAX_Y;
        renderer->caps.limits.maxComputeWorkGroupSizeZ = D3D12_CS_THREAD_GROUP_MAX_Z;

        /* see: https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_format_support */
        for (uint32_t format = (VGPU_TEXTURE_FORMAT_UNDEFINED + 1); format < VGPUTextureFormat_Count; format++)
        {
            D3D12_FEATURE_DATA_FORMAT_SUPPORT support;
            support.Format = _vgpu_to_dxgi_format((VGPUTextureFormat)format);
            if (support.Format == DXGI_FORMAT_UNKNOWN)
                continue;

            HRESULT hr = renderer->device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support));
            VGPU_ASSERT(SUCCEEDED(hr));
            /*sg_pixelformat_info* info = &_sg.formats[fmt];
            info->sample = 0 != (dxgi_fmt_caps & D3D11_FORMAT_SUPPORT_TEXTURE2D);
            info->filter = 0 != (dxgi_fmt_caps & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE);
            info->render = 0 != (dxgi_fmt_caps & D3D11_FORMAT_SUPPORT_RENDER_TARGET);
            info->blend = 0 != (dxgi_fmt_caps & D3D11_FORMAT_SUPPORT_BLENDABLE);
            info->msaa = 0 != (dxgi_fmt_caps & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET);
            info->depth = 0 != (dxgi_fmt_caps & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL);
            if (info->depth) {
                info->render = true;
            }*/
        }

        // Detect default depth format first.
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support;
        support.Format = _vgpu_to_dxgi_format(VGPUTextureFormat_Depth32Float);

        VHR(renderer->device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)));
        if (support.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)
        {
            renderer->defaultDepthFormat = VGPUTextureFormat_Depth32Float;
        }
        else
        {
            support.Format = _vgpu_to_dxgi_format(VGPUTextureFormat_Depth16Unorm);
            VHR(renderer->device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)));
            if (support.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)
            {
                renderer->defaultDepthFormat = VGPUTextureFormat_Depth16Unorm;
            }
        }

        // Detect default depth stencil format.
        support.Format = _vgpu_to_dxgi_format(VGPUTextureFormat_Depth24UnormStencil8);
        VHR(renderer->device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)));
        if (support.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)
        {
            renderer->defaultDepthStencilFormat = VGPUTextureFormat_Depth24UnormStencil8;
        }
        else
        {
            support.Format = _vgpu_to_dxgi_format(VGPUTextureFormat_Depth32FloatStencil8);
            VHR(renderer->device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)));
            if (support.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)
            {
                renderer->defaultDepthStencilFormat = VGPUTextureFormat_Depth32FloatStencil8;
            }
        }
    }

    /* Release adapter */
    dxgiAdapter->Release();

    // Command queues (we use compute queue for upload/generate mips logic)
    {
        D3D12_COMMAND_QUEUE_DESC graphicsQueueDesc = { D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0x0 };
        D3D12_COMMAND_QUEUE_DESC computeQueueDesc = { D3D12_COMMAND_LIST_TYPE_COMPUTE, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0x0 };

        VHR(renderer->device->CreateCommandQueue(&graphicsQueueDesc, __uuidof(ID3D12CommandQueue), (void**)&renderer->graphicsQueue));
        VHR(renderer->device->CreateCommandQueue(&computeQueueDesc, __uuidof(ID3D12CommandQueue), (void**)&renderer->computeQueue));
        renderer->graphicsQueue->SetName(L"Graphics Command Queue");
        renderer->computeQueue->SetName(L"Compute Command Queue");
    }

    // Frame fence
    {
        renderer->frameIndex = 0;
        renderer->frameCount = 0;
        VHR(renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&renderer->frameFence));
        renderer->frameFenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
        VGPU_ASSERT(renderer->frameFenceEvent != INVALID_HANDLE_VALUE);
        renderer->frameFence->SetName(L"Frame Fence");
    }

    /* Allocate device as we use it swap chain logic */
    VGPUDeviceImpl* device = VGPU_ALLOC(VGPUDeviceImpl);
    device->renderer = (vgpu_renderer)renderer;
    ASSIGN_DRIVER(d3d12);
    renderer->gpuDevice = device;

    if (info->swapchain.window_handle) {
        d3d12_swapchain_create(renderer, &renderer->swapchains[0], &info->swapchain);
    }

    return device;
}

VGPU_Driver D3D12_Driver = {
    VGPUBackendType_D3D12,
    d3d12_is_supported,
    d3d12_create_device
};
#endif // TODO

#endif /* defined(VGPU_DRIVER_D3D12)  */
