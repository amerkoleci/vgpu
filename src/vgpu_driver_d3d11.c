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

#if defined(VGPU_DRIVER_D3D11) 
#define CINTERFACE
#define COBJMACROS
#define D3D11_NO_HELPERS

#include <d3d11_1.h>
#include "vgpu_d3d_common.h"
#include "stb_ds.h"
#include <stdio.h>

/* D3D11 guids */
static const IID D3D11_IID_ID3D11DeviceContext1 = { 0xbb2c6faa, 0xb5fb, 0x4082, {0x8e, 0x6b, 0x38, 0x8b, 0x8c, 0xfa, 0x90, 0xe1} };
static const IID D3D11_IID_ID3D11Device1 = { 0xa04bfb29, 0x08ef, 0x43d6, {0xa4, 0x9c, 0xa9, 0xbd, 0xbd, 0xcb, 0xe6, 0x86} };
static const IID D3D11_IID_ID3DUserDefinedAnnotation = { 0xb2daad8b, 0x03d4, 0x4dbf, {0x95, 0xeb, 0x32, 0xab, 0x4b, 0x63, 0xd0, 0xab} };
static const IID D3D11_IID_ID3D11Texture2D = { 0x6f15aaf2,0xd208,0x4e89, {0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c } };

#ifdef _DEBUG
static const IID D3D11_WKPDID_D3DDebugObjectName = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87,0x42,0xac,0xb0,0xbf,0x85,0xc2,0x00 } };
static const IID D3D11_IID_ID3D11Debug = { 0x79cf2233, 0x7536, 0x4948, {0x9d, 0x36, 0x1e, 0x46, 0x92, 0xdc, 0x57, 0x60} };
static const IID D3D11_IID_ID3D11InfoQueue = { 0x6543dbb6, 0x1b48, 0x42f5, {0xab,0x82,0xe9,0x7e,0xc7,0x43,0x26,0xf6} };
#endif


typedef struct D3D11Texture {
    ID3D11Resource* handle;
    union {
        ID3D11RenderTargetView* rtv;
        ID3D11DepthStencilView* dsv;
    };
    DXGI_FORMAT dxgi_format;

    uint32_t width;
    uint32_t height;
    uint32_t depthOrArrayLayers;
    uint32_t mipLevels;
    VGPUTextureType type;
    uint32_t sample_count;
} D3D11Texture;

typedef struct d3d11_swapchain {
    vgpu_texture_format color_format;
    vgpu_texture_format depth_stencil_format;
    IDXGISwapChain1* handle;
    uint32_t width;
    uint32_t height;
    VGPUTexture backbuffer;
    VGPUTexture depthStencilTexture;
    //vgpu_framebuffer framebuffer;
} d3d11_swapchain;


typedef struct d3d11_renderer {
    VGPUDeviceCaps caps;

    bool validation;
    IDXGIFactory2* factory;
    uint32_t factory_caps;

    uint32_t  num_backbuffers;
    uint32_t  max_frame_latency;
    uint32_t sync_interval;
    uint32_t present_flags;

    ID3D11Device1* device;
    ID3D11DeviceContext1* context;
    ID3DUserDefinedAnnotation* d3d_annotation;
    D3D_FEATURE_LEVEL feature_level;
    bool is_lost;

    VGPUDevice gpuDevice;

    d3d11_swapchain swapchains[8];
} d3d11_renderer;

/* Global data */
static struct {
    bool available_initialized;
    bool available;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    struct
    {
        HMODULE instance;
        PFN_CREATE_DXGI_FACTORY1 CreateDXGIFactory1;
        PFN_CREATE_DXGI_FACTORY2 CreateDXGIFactory2;
        PFN_GET_DXGI_DEBUG_INTERFACE1 DXGIGetDebugInterface1;
    } dxgi;

    HMODULE instance;
    PFN_D3D11_CREATE_DEVICE D3D11CreateDevice;
#endif
} d3d11;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   define _vgpu_CreateDXGIFactory1 d3d11.dxgi.CreateDXGIFactory1
#   define _vgpu_CreateDXGIFactory2 d3d11.dxgi.CreateDXGIFactory2
#   define _vgpu_DXGIGetDebugInterface1 d3d11.dxgi.DXGIGetDebugInterface1
#   define _vgpu_D3D11CreateDevice d3d11.D3D11CreateDevice
#else
#   define _vgpu_CreateDXGIFactory1 CreateDXGIFactory1
#   define _vgpu_CreateDXGIFactory2 CreateDXGIFactory2
#   define _vgpu_DXGIGetDebugInterface1 DXGIGetDebugInterface1
#   define _vgpu_D3D11CreateDevice D3D11CreateDevice
#endif

/* Device functions */
static bool d3d11_create_factory(d3d11_renderer* renderer)
{
    if (renderer->factory) {
        IDXGIFactory2_Release(renderer->factory);
        renderer->factory = NULL;
    }

    HRESULT hr = S_OK;

#if defined(_DEBUG)
    bool debugDXGI = false;

    if (renderer->validation)
    {
        IDXGIInfoQueue* dxgi_info_queue;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        if (d3d11.dxgi.DXGIGetDebugInterface1 &&
            SUCCEEDED(_vgpu_DXGIGetDebugInterface1(0, &D3D_IID_IDXGIInfoQueue, (void**)&dxgi_info_queue)))
#else
        if (SUCCEEDED(_vgpu_DXGIGetDebugInterface1(0, &D3D_IID_IDXGIInfoQueue, (void**)&dxgi_info_queue)))
#endif
        {
            debugDXGI = true;
            hr = _vgpu_CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, &D3D_IID_IDXGIFactory2, (void**)&renderer->factory);
            if (FAILED(hr)) {
                return false;
            }

            VHR(IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info_queue, D3D_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE));
            VHR(IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info_queue, D3D_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE));
            VHR(IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info_queue, D3D_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, FALSE));

            DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
            {
                80 // IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides.
            };

            DXGI_INFO_QUEUE_FILTER filter;
            memset(&filter, 0, sizeof(filter));
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            IDXGIInfoQueue_AddStorageFilterEntries(dxgi_info_queue, D3D_DXGI_DEBUG_DXGI, &filter);
            IDXGIInfoQueue_Release(dxgi_info_queue);
        }
}

    if (!debugDXGI)
#endif
    {
        hr = _vgpu_CreateDXGIFactory1(&D3D_IID_IDXGIFactory2, (void**)&renderer->factory);
        if (FAILED(hr)) {
            return false;
        }
    }

    renderer->factory_caps = 0;

    // Disable FLIP if not on a supporting OS
    renderer->factory_caps |= DXGIFACTORY_CAPS_FLIP_PRESENT;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    {
        IDXGIFactory4* factory4;
        HRESULT hr = IDXGIFactory2_QueryInterface(renderer->factory, &D3D_IID_IDXGIFactory4, (void**)&factory4);
        if (FAILED(hr))
        {
            renderer->factory_caps &= DXGIFACTORY_CAPS_FLIP_PRESENT;
        }
        IDXGIFactory4_Release(factory4);
    }
#endif

    // Check tearing support.
    {
        BOOL allowTearing = FALSE;
        IDXGIFactory5* factory5;
        HRESULT hr = IDXGIFactory2_QueryInterface(renderer->factory, &D3D_IID_IDXGIFactory5, (void**)&factory5);
        if (SUCCEEDED(hr))
        {
            hr = IDXGIFactory5_CheckFeatureSupport(factory5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
            IDXGIFactory5_Release(factory5);
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
static IDXGIAdapter1* d3d11_get_adapter(d3d11_renderer* renderer, vgpu_device_preference device_preference)
{
    /* Detect adapter now. */
    IDXGIAdapter1* adapter = NULL;

#if defined(__dxgi1_6_h__) && defined(NTDDI_WIN10_RS4)
    if (device_preference != VGPU_DEVICE_PREFERENCE_DONT_CARE) {
        IDXGIFactory6* factory6;
        HRESULT hr = IDXGIFactory2_QueryInterface(renderer->factory, &D3D_IID_IDXGIFactory6, (void**)&factory6);
        if (SUCCEEDED(hr))
        {
            // By default prefer high performance
            DXGI_GPU_PREFERENCE gpuPreference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
            if (device_preference == VGPU_DEVICE_PREFERENCE_LOW_POWER) {
                gpuPreference = DXGI_GPU_PREFERENCE_MINIMUM_POWER;
            }

            for (uint32_t i = 0;
                DXGI_ERROR_NOT_FOUND != IDXGIFactory6_EnumAdapterByGpuPreference(factory6, i, gpuPreference, &D3D_IID_IDXGIAdapter1, (void**)&adapter); i++)
            {
                DXGI_ADAPTER_DESC1 desc;
                VHR(IDXGIAdapter1_GetDesc1(adapter, &desc));

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    IDXGIAdapter1_Release(adapter);
                    continue;
                }

                break;
            }

            IDXGIFactory6_Release(factory6);
        }
    }
#endif

    if (!adapter)
    {
        for (uint32_t i = 0; DXGI_ERROR_NOT_FOUND != IDXGIFactory2_EnumAdapters1(renderer->factory, i, &adapter); i++)
        {
            DXGI_ADAPTER_DESC1 desc;
            VHR(IDXGIAdapter1_GetDesc1(adapter, &desc));

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                IDXGIAdapter1_Release(adapter);

                continue;
            }

            break;
        }
    }

    return adapter;
}

static void d3d11_swapchain_destroy(d3d11_renderer* renderer, d3d11_swapchain* swapchain);
static void d3d11_swapchain_present(d3d11_renderer* renderer, d3d11_swapchain* swapchain);

static void d3d11_destroy(VGPUDevice device) {
    d3d11_renderer* renderer = (d3d11_renderer*)device->renderer;

    for (uint32_t i = 0; i < _vgpu_count_of(renderer->swapchains); i++) {
        if (!renderer->swapchains[i].handle)
            continue;

        d3d11_swapchain_destroy(renderer, &renderer->swapchains[i]);
    }

    ID3D11DeviceContext1_Release(renderer->context);
    ID3DUserDefinedAnnotation_Release(renderer->d3d_annotation);

    ULONG refCount = ID3D11Device1_Release(renderer->device);
#if !defined(NDEBUG)
    if (refCount > 0)
    {
        vgpu_log_error("Direct3D11: There are %d unreleased references left on the device", refCount);

        ID3D11Debug* d3d11_debug = NULL;
        if (SUCCEEDED(ID3D11Device1_QueryInterface(renderer->device, &D3D11_IID_ID3D11Debug, (void**)&d3d11_debug)))
        {
            ID3D11Debug_ReportLiveDeviceObjects(d3d11_debug, D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
            ID3D11Debug_Release(d3d11_debug);
}
    }
#else
    (void)refCount; // avoid warning
#endif

    IDXGIFactory2_Release(renderer->factory);

#ifdef _DEBUG
    IDXGIDebug1* dxgi_debug1;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    if (d3d11.dxgi.DXGIGetDebugInterface1 &&
        SUCCEEDED(_vgpu_DXGIGetDebugInterface1(0, &D3D_IID_IDXGIDebug1, (void**)&dxgi_debug1)))
#else
    if (SUCCEEDED(_vgpu_DXGIGetDebugInterface1(0, &D3D_IID_IDXGIDebug1, (void**)&dxgi_debug1)))
#endif
    {
        IDXGIDebug1_ReportLiveObjects(dxgi_debug1, D3D_DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL);
        IDXGIDebug1_Release(dxgi_debug1);
    }
#endif

    VGPU_FREE(renderer);
    VGPU_FREE(device);
}

static void d3d11_get_caps(vgpu_renderer driver_data, VGPUDeviceCaps* caps) {
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
    *caps = renderer->caps;
}

static bool d3d11_frame_begin(vgpu_renderer driver_data) {
    VGPU_UNUSED(driver_data);
    return true;
}

static void d3d11_frame_end(vgpu_renderer driverData) {
    d3d11_renderer* renderer = (d3d11_renderer*)driverData;
    for (uint32_t i = 0; i < _vgpu_count_of(renderer->swapchains); i++) {
        if (!renderer->swapchains[i].handle)
            continue;

        d3d11_swapchain_present(renderer, &renderer->swapchains[i]);
    }

    if (!renderer->is_lost) {
        if (!IDXGIFactory2_IsCurrent(renderer->factory))
        {
            // Output information is cached on the DXGI Factory. If it is stale we need to create a new factory.
            d3d11_create_factory(renderer);
        }
    }
}

static VGPUTexture d3d11_getBackbufferTexture(vgpu_renderer driverData, uint32_t swapchain) {
    d3d11_renderer* renderer = (d3d11_renderer*)driverData;
    return renderer->swapchains[swapchain].backbuffer;
}


/* Swapchain */
static void d3d11_swapchain_update(d3d11_renderer* renderer, d3d11_swapchain* swapchain) {
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc;
    VHR(IDXGISwapChain1_GetDesc1(swapchain->handle, &swapchain_desc));
    swapchain->width = swapchain_desc.Width;
    swapchain->height = swapchain_desc.Height;

    ID3D11Texture2D* backbuffer;
    VHR(IDXGISwapChain1_GetBuffer(swapchain->handle, 0, &D3D11_IID_ID3D11Texture2D, (void**)&backbuffer));

    const VGPUTextureDescription textureDesc = {
       .width = swapchain->width,
       .height = swapchain->height,
       .depth = 1u,
       .mipLevelCount = 1,
       .format = swapchain->color_format,
       .type = VGPU_TEXTURE_TYPE_2D,
       .usage = VGPU_TEXTURE_USAGE_RENDER_TARGET,
       .sampleCount = 1u,
       .externalHandle = backbuffer
    };

    swapchain->backbuffer = vgpuCreateTexture(renderer->gpuDevice, &textureDesc);
}

static void d3d11_swapchain_create(d3d11_renderer* renderer, d3d11_swapchain* swapchain, const vgpu_swapchain_info* info) {
    swapchain->handle = vgpu_d3d_create_swapchain(
        renderer->factory,
        renderer->factory_caps,
        (IUnknown*)renderer->device,
        info->window_handle,
        info->width, info->height,
        info->color_format,
        renderer->num_backbuffers,  /* 3 for triple buffering */
        !info->fullscreen
    );

    d3d11_swapchain_update(renderer, swapchain);
}

/* Texture */
static VGPUTexture d3d11_createTexture(vgpu_renderer driverData, const VGPUTextureDescription* desc) {
    d3d11_renderer* renderer = (d3d11_renderer*)driverData;
    D3D11Texture* texture = VGPU_ALLOC_HANDLE(D3D11Texture);
    texture->width = desc->width;
    texture->height = desc->height;
    texture->depthOrArrayLayers = desc->depth;
    texture->mipLevels = desc->mipLevelCount;
    texture->type = desc->type;
    texture->sample_count = desc->sampleCount;

    texture->dxgi_format = _vgpu_d3d_format_with_usage(desc->format, desc->usage);

    HRESULT hr = S_OK;
    if (desc->externalHandle) {
        texture->handle = (ID3D11Resource*)desc->externalHandle;
    }
    else {
        D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
        UINT bindFlags = 0;
        UINT CPUAccessFlags = 0;
        UINT miscFlags = 0;
        UINT arraySizeMultiplier = 1;
        if (desc->type == VGPU_TEXTURE_TYPE_CUBE) {
            arraySizeMultiplier = 6;
            miscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
        }

        if (desc->usage & VGPU_TEXTURE_USAGE_SAMPLED) {
            bindFlags |= D3D11_BIND_SHADER_RESOURCE;
        }
        if (desc->usage & VGPU_TEXTURE_USAGE_STORAGE) {
            bindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        }

        if (desc->usage & VGPU_TEXTURE_USAGE_RENDER_TARGET) {
            if (vgpu_is_depth_stencil_format(desc->format)) {
                bindFlags |= D3D11_BIND_DEPTH_STENCIL;
            }
            else {
                bindFlags |= D3D11_BIND_RENDER_TARGET;
            }
        }

        const bool generateMipmaps = false;
        if (generateMipmaps)
        {
            bindFlags |= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            miscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
        }

        if (desc->type == VGPU_TEXTURE_TYPE_2D || desc->type == VGPU_TEXTURE_TYPE_CUBE) {
            const D3D11_TEXTURE2D_DESC d3d11_desc = {
                .Width = desc->width,
                .Height = desc->height,
                .MipLevels = desc->mipLevelCount,
                .ArraySize = desc->depth * arraySizeMultiplier,
                .Format = texture->dxgi_format,
                .SampleDesc = {
                    .Count = 1,
                    .Quality = 0
                },
                .Usage = usage,
                .BindFlags = bindFlags,
                .CPUAccessFlags = CPUAccessFlags,
                .MiscFlags = miscFlags
            };

            hr = ID3D11Device_CreateTexture2D(
                renderer->device,
                &d3d11_desc,
                NULL,
                (ID3D11Texture2D**)&texture->handle
            );
        }
        else if (desc->type == VGPU_TEXTURE_TYPE_3D) {
            const D3D11_TEXTURE3D_DESC d3d11_desc = {
                .Width = desc->width,
                .Height = desc->height,
                .Depth = desc->depth,
                .MipLevels = desc->mipLevelCount,
                .Format = texture->dxgi_format,
                .Usage = usage,
                .BindFlags = bindFlags,
                .CPUAccessFlags = CPUAccessFlags,
                .MiscFlags = miscFlags
            };

            hr = ID3D11Device_CreateTexture3D(
                renderer->device,
                &d3d11_desc,
                NULL,
                (ID3D11Texture3D**)&texture->handle
            );
        }
    }

    if (desc->usage & VGPU_TEXTURE_USAGE_RENDER_TARGET) {
        ID3D11Device1_CreateRenderTargetView(
            renderer->device,
            texture->handle,
            NULL,
            &texture->rtv
        );
    }

    return (VGPUTexture)texture;
}

static void d3d11_destroyTexture(vgpu_renderer driverData, VGPUTexture handle) {
    D3D11Texture* texture = (D3D11Texture*)handle;
    ID3D11Resource_Release(texture->handle);
    VGPU_FREE(texture);
}


static void d3d11_swapchain_destroy(d3d11_renderer* renderer, d3d11_swapchain* swapchain) {
    //vgpu_framebuffer_destroy(swapchain->framebuffer);
    vgpuDestroyTexture(renderer->gpuDevice, swapchain->backbuffer);
    if (swapchain->depthStencilTexture) {
        vgpuDestroyTexture(renderer->gpuDevice, swapchain->depthStencilTexture);
    }
    IDXGISwapChain1_Release(swapchain->handle);
    //VGPU_FREE(swapchain);
}

static void d3d11_swapchain_present(d3d11_renderer* renderer, d3d11_swapchain* swapchain) {
    HRESULT hr = IDXGISwapChain1_Present(swapchain->handle, renderer->sync_interval, renderer->present_flags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED
        || hr == DXGI_ERROR_DEVICE_HUNG
        || hr == DXGI_ERROR_DEVICE_RESET
        || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR
        || hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
    {
        renderer->is_lost = true;
    }
}

/* Commands */
static void d3d11_cmdBeginRenderPass(vgpu_renderer driverData, const VGPURenderPassDescription* desc) {
    d3d11_renderer* renderer = (d3d11_renderer*)driverData;

    for (uint32_t i = 0; i < VGPU_MAX_COLOR_ATTACHMENTS; i++)
    {
        if (!desc->colorAttachments[i].texture)
            continue;

        D3D11Texture* texture = (D3D11Texture*)desc->colorAttachments[i].texture;

        if (desc->colorAttachments[i].loadOp == VGPU_LOAD_OP_CLEAR)
        {
            ID3D11DeviceContext1_ClearRenderTargetView(
                renderer->context,
                texture->rtv,
                &desc->colorAttachments[i].clearColor.r
            );
        }
    }

    if (desc->depthStencilAttachment.texture) {
        D3D11Texture* texture = (D3D11Texture*)desc->depthStencilAttachment.texture;

        UINT clearFlags = 0;
        if (desc->depthStencilAttachment.depthLoadOp == VGPU_LOAD_OP_CLEAR)
        {
            clearFlags |= D3D11_CLEAR_DEPTH;
        }

        if (desc->depthStencilAttachment.stencilLoadOp == VGPU_LOAD_OP_CLEAR)
        {
            clearFlags |= D3D11_CLEAR_STENCIL;
        }

        ID3D11DeviceContext1_ClearDepthStencilView(
            renderer->context,
            texture->dsv,
            clearFlags,
            desc->depthStencilAttachment.clearDepth,
            desc->depthStencilAttachment.clearStencil
        );
    }

    /*ID3D11DeviceContext1_OMSetRenderTargets(
        renderer->context,
        framebuffer.colorAttachmentCount,
        framebuffer.colorAttachments,
        framebuffer.depthStencilAttachment
    );*/
}

static void d3d11_cmdEndRenderPass(vgpu_renderer driverData) {
}

/* Driver functions */
static bool _vgpu_d3d11_sdk_layers_available() {
    HRESULT hr = _vgpu_D3D11CreateDevice(
        NULL,
        D3D_DRIVER_TYPE_NULL,
        NULL,
        D3D11_CREATE_DEVICE_DEBUG,
        NULL,
        0,
        D3D11_SDK_VERSION,
        NULL,
        NULL,
        NULL
    );

    return SUCCEEDED(hr);
}

static bool d3d11_is_supported(void) {
    if (d3d11.available_initialized) {
        return d3d11.available;
    }

    d3d11.available_initialized = true;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) 
    d3d11.dxgi.instance = LoadLibraryA("dxgi.dll");
    if (!d3d11.dxgi.instance) {
        return false;
    }

    d3d11.dxgi.CreateDXGIFactory1 = (PFN_CREATE_DXGI_FACTORY1)GetProcAddress(d3d11.dxgi.instance, "CreateDXGIFactory1");
    if (!d3d11.dxgi.CreateDXGIFactory1)
    {
        return false;
    }

    d3d11.dxgi.CreateDXGIFactory2 = (PFN_CREATE_DXGI_FACTORY2)GetProcAddress(d3d11.dxgi.instance, "CreateDXGIFactory2");
    if (!d3d11.dxgi.CreateDXGIFactory2)
    {
        return false;
    }
    d3d11.dxgi.DXGIGetDebugInterface1 = (PFN_GET_DXGI_DEBUG_INTERFACE1)GetProcAddress(d3d11.dxgi.instance, "DXGIGetDebugInterface1");

    d3d11.instance = LoadLibraryA("d3d11.dll");
    if (!d3d11.instance) {
        return false;
    }

    d3d11.D3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(d3d11.instance, "D3D11CreateDevice");
    if (!d3d11.D3D11CreateDevice) {
        return false;
    }
#endif

    static const D3D_FEATURE_LEVEL s_featureLevels[] =
    {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };

    HRESULT hr = _vgpu_D3D11CreateDevice(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        s_featureLevels,
        _vgpu_count_of(s_featureLevels),
        D3D11_SDK_VERSION,
        NULL,
        NULL,
        NULL
    );

    if (FAILED(hr))
    {
        return false;
    }

    d3d11.available = true;
    return true;
};

static VGPUDeviceImpl* d3d11_create_device(const vgpu_device_info* info) {
    /* Allocate and zero out the renderer */
    d3d11_renderer* renderer = VGPU_ALLOC_HANDLE(d3d11_renderer);
    renderer->validation = info->debug;

    if (!d3d11_create_factory(renderer)) {
        return NULL;
    }

    IDXGIAdapter1* dxgi_adapter = d3d11_get_adapter(renderer, info->device_preference);

    /* Setup present flags. */
    renderer->num_backbuffers = 2;
    renderer->max_frame_latency = 3;
    renderer->sync_interval = _vgpu_d3d_sync_interval(info->swapchain.present_mode);
    renderer->present_flags = 0;
    if (info->swapchain.present_mode == VGPU_PRESENT_MODE_IMMEDIATE
        && renderer->factory_caps & DXGIFACTORY_CAPS_TEARING) {
        renderer->present_flags |= DXGI_PRESENT_ALLOW_TEARING;
    }

    /* Create d3d11 device */
    {
        UINT creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

        if (renderer->validation && _vgpu_d3d11_sdk_layers_available())
        {
            // If the project is in a debug build, enable debugging via SDK Layers with this flag.
            creation_flags |= D3D11_CREATE_DEVICE_DEBUG;
        }
#if defined(_DEBUG)
        else
        {
            OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
        }
#endif

        static const D3D_FEATURE_LEVEL s_featureLevels[] =
        {
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0
        };

        // Create the Direct3D 11 API device object and a corresponding context.
        ID3D11Device* temp_d3d_device;
        ID3D11DeviceContext* temp_d3d_context;

        HRESULT hr = E_FAIL;
        if (dxgi_adapter)
        {
            hr = _vgpu_D3D11CreateDevice(
                (IDXGIAdapter*)dxgi_adapter,
                D3D_DRIVER_TYPE_UNKNOWN,
                NULL,
                creation_flags,
                s_featureLevels,
                _countof(s_featureLevels),
                D3D11_SDK_VERSION,
                &temp_d3d_device,
                &renderer->feature_level,
                &temp_d3d_context
            );
        }
#if defined(NDEBUG)
        else
        {
            vgpu_log_error("No Direct3D hardware device found");
            VGPU_UNREACHABLE();
        }
#else
        if (FAILED(hr))
        {
            // If the initialization fails, fall back to the WARP device.
            // For more information on WARP, see:
            // http://go.microsoft.com/fwlink/?LinkId=286690
            hr = _vgpu_D3D11CreateDevice(
                NULL,
                D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
                NULL,
                creation_flags,
                s_featureLevels,
                _countof(s_featureLevels),
                D3D11_SDK_VERSION,
                &temp_d3d_device,
                &renderer->feature_level,
                &temp_d3d_context
            );

            if (SUCCEEDED(hr))
            {
                OutputDebugStringA("Direct3D Adapter - WARP\n");
            }
    }
#endif

        if (FAILED(hr)) {
            return false;
        }

#ifndef NDEBUG
        ID3D11Debug* d3d_debug;
        if (SUCCEEDED(ID3D11Device_QueryInterface(temp_d3d_device, &D3D11_IID_ID3D11Debug, (void**)&d3d_debug)))
        {
            ID3D11InfoQueue* d3d_info_queue;
            if (SUCCEEDED(ID3D11Debug_QueryInterface(d3d_debug, &D3D11_IID_ID3D11InfoQueue, (void**)&d3d_info_queue)))
            {
#ifdef _DEBUG
                ID3D11InfoQueue_SetBreakOnSeverity(d3d_info_queue, D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
                ID3D11InfoQueue_SetBreakOnSeverity(d3d_info_queue, D3D11_MESSAGE_SEVERITY_ERROR, true);
#endif
                D3D11_MESSAGE_ID hide[] =
                {
                    D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
                };
                D3D11_INFO_QUEUE_FILTER filter;
                memset(&filter, 0, sizeof(D3D11_INFO_QUEUE_FILTER));
                filter.DenyList.NumIDs = _countof(hide);
                filter.DenyList.pIDList = hide;
                ID3D11InfoQueue_AddStorageFilterEntries(d3d_info_queue, &filter);
                ID3D11InfoQueue_Release(d3d_info_queue);
            }

            ID3D11Debug_Release(d3d_debug);
        }
#endif

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        {
            IDXGIDevice1* dxgiDevice1;
            if (SUCCEEDED(ID3D11Device_QueryInterface(temp_d3d_device, &D3D_IID_IDXGIDevice1, (void**)&dxgiDevice1)))
            {
                /* Default to 3. */
                VHR(IDXGIDevice1_SetMaximumFrameLatency(dxgiDevice1, renderer->max_frame_latency));
                IDXGIDevice1_Release(dxgiDevice1);
            }
        }
#endif

        VHR(ID3D11Device_QueryInterface(temp_d3d_device, &D3D11_IID_ID3D11Device1, (void**)&renderer->device));
        VHR(ID3D11DeviceContext_QueryInterface(temp_d3d_context, &D3D11_IID_ID3D11DeviceContext1, (void**)&renderer->context));
        VHR(ID3D11DeviceContext_QueryInterface(temp_d3d_context, &D3D11_IID_ID3DUserDefinedAnnotation, (void**)&renderer->d3d_annotation));
        ID3D11DeviceContext_Release(temp_d3d_context);
        ID3D11Device_Release(temp_d3d_device);
}

    /* Init caps first. */
    {
        DXGI_ADAPTER_DESC1 adapter_desc;
        VHR(IDXGIAdapter1_GetDesc1(dxgi_adapter, &adapter_desc));

        /* Log some info */
        vgpu_log_info("vgpu driver: D3D11");
        vgpu_log_info("Direct3D Adapter: VID:%04X, PID:%04X - %ls", adapter_desc.VendorId, adapter_desc.DeviceId, adapter_desc.Description);

        renderer->caps.backendType = VGPU_BACKEND_TYPE_D3D11;
        renderer->caps.vendorId = adapter_desc.VendorId;
        renderer->caps.deviceId = adapter_desc.DeviceId;

        renderer->caps.features.independentBlend = (renderer->feature_level >= D3D_FEATURE_LEVEL_10_0);
        renderer->caps.features.computeShader = (renderer->feature_level >= D3D_FEATURE_LEVEL_10_0);
        renderer->caps.features.tessellationShader = (renderer->feature_level >= D3D_FEATURE_LEVEL_11_0);
        renderer->caps.features.multiViewport = true;
        renderer->caps.features.indexUInt32 = true;
        renderer->caps.features.multiDrawIndirect = (renderer->feature_level >= D3D_FEATURE_LEVEL_11_0);
        renderer->caps.features.fillModeNonSolid = true;
        renderer->caps.features.samplerAnisotropy = true;
        renderer->caps.features.textureCompressionETC2 = false;
        renderer->caps.features.textureCompressionASTC_LDR = false;
        renderer->caps.features.textureCompressionBC = true;
        renderer->caps.features.textureCubeArray = (renderer->feature_level >= D3D_FEATURE_LEVEL_10_1);
        renderer->caps.features.raytracing = false;

        // Limits
        renderer->caps.limits.max_vertex_attributes = VGPU_MAX_VERTEX_ATTRIBUTES;
        renderer->caps.limits.max_vertex_bindings = VGPU_MAX_VERTEX_ATTRIBUTES;
        renderer->caps.limits.max_vertex_attribute_offset = VGPU_MAX_VERTEX_ATTRIBUTE_OFFSET;
        renderer->caps.limits.max_vertex_binding_stride = VGPU_MAX_VERTEX_BUFFER_STRIDE;

        renderer->caps.limits.max_texture_size_1d = D3D11_REQ_TEXTURE1D_U_DIMENSION;
        renderer->caps.limits.max_texture_size_2d = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        renderer->caps.limits.max_texture_size_3d = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        renderer->caps.limits.max_texture_size_cube = D3D11_REQ_TEXTURECUBE_DIMENSION;
        renderer->caps.limits.max_texture_array_layers = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        renderer->caps.limits.max_color_attachments = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
        renderer->caps.limits.max_uniform_buffer_size = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
        renderer->caps.limits.min_uniform_buffer_offset_alignment = 256u;
        renderer->caps.limits.max_storage_buffer_size = UINT32_MAX;
        renderer->caps.limits.min_storage_buffer_offset_alignment = 16;
        renderer->caps.limits.max_sampler_anisotropy = D3D11_MAX_MAXANISOTROPY;
        renderer->caps.limits.max_viewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        renderer->caps.limits.max_viewport_width = D3D11_VIEWPORT_BOUNDS_MAX;
        renderer->caps.limits.max_viewport_height = D3D11_VIEWPORT_BOUNDS_MAX;
        renderer->caps.limits.max_tessellation_patch_size = D3D11_IA_PATCH_MAX_CONTROL_POINT_COUNT;
        renderer->caps.limits.point_size_range_min = 1.0f;
        renderer->caps.limits.point_size_range_max = 1.0f;
        renderer->caps.limits.line_width_range_min = 1.0f;
        renderer->caps.limits.line_width_range_max = 1.0f;
        renderer->caps.limits.max_compute_shared_memory_size = D3D11_CS_THREAD_LOCAL_TEMP_REGISTER_POOL;
        renderer->caps.limits.max_compute_work_group_count_x = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        renderer->caps.limits.max_compute_work_group_count_y = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        renderer->caps.limits.max_compute_work_group_count_z = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        renderer->caps.limits.max_compute_work_group_invocations = D3D11_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;
        renderer->caps.limits.max_compute_work_group_size_x = D3D11_CS_THREAD_GROUP_MAX_X;
        renderer->caps.limits.max_compute_work_group_size_y = D3D11_CS_THREAD_GROUP_MAX_Y;
        renderer->caps.limits.max_compute_work_group_size_z = D3D11_CS_THREAD_GROUP_MAX_Z;

#if TODO
        /* see: https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_format_support */
        UINT dxgi_fmt_caps = 0;
        for (int fmt = (VGPUTextureFormat_Undefined + 1); fmt < VGPUTextureFormat_Count; fmt++)
        {
            DXGI_FORMAT dxgi_fmt = _vgpu_d3d_get_format((VGPUTextureFormat)fmt);
            HRESULT hr = ID3D11Device1_CheckFormatSupport(renderer->d3d_device, dxgi_fmt, &dxgi_fmt_caps);
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
#endif // TODO

    }

    /* Release adapter */
    IDXGIAdapter1_Release(dxgi_adapter);

    /* Allocate device as we use it swap chain logic */
    VGPUDeviceImpl* device = VGPU_ALLOC(VGPUDeviceImpl);
    device->renderer = (vgpu_renderer)renderer;
    ASSIGN_DRIVER(d3d11);
    renderer->gpuDevice = device;

    if (info->swapchain.window_handle) {
        d3d11_swapchain_create(renderer, &renderer->swapchains[0], &info->swapchain);
    }

    return device;
}

vgpu_driver d3d11_driver = {
    VGPU_BACKEND_TYPE_D3D11,
    d3d11_is_supported,
    d3d11_create_device
};

#endif /* defined(VGPU_DRIVER_D3D11)  */
