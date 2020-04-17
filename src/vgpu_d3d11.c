//
// Copyright (c) 2019 Amer Koleci.
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

#include "vgpu_backend.h"

#ifndef CINTERFACE
#   define CINTERFACE
#endif
#ifndef COBJMACROS
#   define COBJMACROS
#endif
#ifndef NOMINMAX
#   define NOMINMAX
#endif
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP
#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#ifndef D3D11_NO_HELPERS
#   define D3D11_NO_HELPERS
#endif
#include <windows.h>
#include <d3d11_1.h>
#include "vgpu_d3d.h"
#include <stdio.h>

#if defined(NTDDI_WIN10_RS2)
#   include <dxgi1_6.h>
#else
#   include <dxgi1_5.h>
#endif

/* DXGI guids */
static const GUID vgpu_IID_IDXGIFactory2 = { 0x50c83a1c, 0xe072, 0x4c48, {0x87,0xb0,0x36,0x30,0xfa,0x36,0xa6,0xd0} };
static const GUID vgpu_IID_IDXGIFactory4 = { 0x1bc6ea02, 0xef36, 0x464f, {0xbf,0x0c,0x21,0xca,0x39,0xe5,0x16,0x8a} };
static const GUID vgpu_IID_IDXGIFactory5 = { 0x7632e1f5, 0xee65, 0x4dca, {0x87,0xfd,0x84,0xcd,0x75,0xf8,0x83,0x8d} };
static const GUID vgpu_IID_IDXGIFactory6 = { 0xc1b6694f, 0xff09, 0x44a9, {0xb0,0x3c,0x77,0x90,0x0a,0x0a,0x1d,0x17} };

#ifdef _DEBUG
#include <dxgidebug.h>

static const GUID vgpu_IID_IDXGIInfoQueue = { 0xD67441C7, 0x672A, 0x476f, {0x9E, 0x82, 0xCD, 0x55, 0xB4, 0x49, 0x49, 0xCE} };
static const GUID vgpu_IID_IDXGIDebug = { 0x119E7452, 0xDE9E, 0x40fe, {0x88, 0x06, 0x88, 0xF9, 0x0C, 0x12, 0xB4, 0x41 } };
static const GUID vgpu_IID_IDXGIDebug1 = { 0xc5a05f0c,0x16f2,0x4adf, {0x9f,0x4d,0xa8,0xc4,0xd5,0x8a,0xc5,0x50 } };

// Declare debug guids to avoid linking with "dxguid.lib"
static const GUID vgpu_DXGI_DEBUG_ALL = { 0xe48ae283, 0xda80, 0x490b, {0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8} };
static const GUID vgpu_DXGI_DEBUG_DXGI = { 0x25cddaa4, 0xb1c6, 0x47e1, {0xac, 0x3e, 0x98, 0x87, 0x5b, 0x5a, 0x2e, 0x2a} };
#endif

/* D3D11 guids */
static const GUID vgpu_IID_ID3D11BlendState1 = { 0xcc86fabe, 0xda55, 0x401d, {0x85, 0xe7, 0xe3, 0xc9, 0xde, 0x28, 0x77, 0xe9} };
static const GUID vgpu_IID_ID3D11RasterizerState1 = { 0x1217d7a6, 0x5039, 0x418c, {0xb0, 0x42, 0x9c, 0xbe, 0x25, 0x6a, 0xfd, 0x6e} };
static const GUID vgpu_IID_ID3DDeviceContextState = { 0x5c1e0d8a, 0x7c23, 0x48f9, {0x8c, 0x59, 0xa9, 0x29, 0x58, 0xce, 0xff, 0x11} };
static const GUID vgpu_IID_ID3D11DeviceContext1 = { 0xbb2c6faa, 0xb5fb, 0x4082, {0x8e, 0x6b, 0x38, 0x8b, 0x8c, 0xfa, 0x90, 0xe1} };
static const GUID vgpu_IID_ID3D11Device1 = { 0xa04bfb29, 0x08ef, 0x43d6, {0xa4, 0x9c, 0xa9, 0xbd, 0xbd, 0xcb, 0xe6, 0x86} };
static const GUID vgpu_IID_ID3DUserDefinedAnnotation = { 0xb2daad8b, 0x03d4, 0x4dbf, {0x95, 0xeb, 0x32, 0xab, 0x4b, 0x63, 0xd0, 0xab} };
static const GUID vgpu_IID_ID3D11Texture2D = { 0x6f15aaf2,0xd208,0x4e89, {0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c } };

#ifdef _DEBUG
static const GUID vgpu_IID_ID3D11Debug = { 0x79cf2233, 0x7536, 0x4948, {0x9d, 0x36, 0x1e, 0x46, 0x92, 0xdc, 0x57, 0x60} };
static const GUID vgpu_IID_ID3D11InfoQueue = { 0x6543dbb6, 0x1b48, 0x42f5, {0xab,0x82,0xe9,0x7e,0xc7,0x43,0x26,0xf6} };
#endif

#define VHR(hr) if (FAILED(hr)) { VGPU_ASSERT(0); }
#define SAFE_RELEASE(obj) if ((obj)) { (obj)->lpVtbl->Release(obj); (obj) = nullptr; }

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
typedef HRESULT(WINAPI* PFN_CREATE_DXGI_FACTORY1)(REFIID _riid, void** _factory);
typedef HRESULT(WINAPI* PFN_CREATE_DXGI_FACTORY2)(UINT flags, REFIID _riid, void** _factory);
typedef HRESULT(WINAPI* PFN_GET_DXGI_DEBUG_INTERFACE1)(UINT flags, REFIID _riid, void** _debug);
#endif

#define _VGPU_MAX_SWAPCHAINS (16u)
typedef struct {
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HWND window;
#else
    IUnknown* window;
#endif

    uint32_t width;
    uint32_t height;

    VGPUTextureFormat colorFormat;
    VGPUColor clearColor;
    IDXGISwapChain1* handle;
    UINT sync_interval;
    UINT present_flags;

    VGPUTexture backbufferTexture;
    VGPUTextureFormat depthStencilFormat;
    VGPUTexture depthStencilTexture;
    VGPURenderPass renderPass;

} VGPUSwapchainD3D11;

typedef struct {
    ID3D11Buffer* handle;
} VGPUBufferD3D11;

typedef struct {
    union
    {
        ID3D11Resource* resource;
        ID3D11Texture1D* tex1d;
        ID3D11Texture2D* tex2d;
        ID3D11Texture3D* tex3d;
    } handle;
    VGPUTextureLayout layout;
    vgpu_texture_desc desc;
} VGPUTextureD3D11;

typedef struct VGPUSamplerD3D11 {
    ID3D11SamplerState* handle;
} VGPUSamplerD3D11;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t num_color_rtvs;
    ID3D11RenderTargetView* color_rtvs[VGPU_MAX_COLOR_ATTACHMENTS];
    ID3D11DepthStencilView* dsv;
    VGPUColor               clearColors[VGPU_MAX_COLOR_ATTACHMENTS];
} VGPURenderPassD3D11;

typedef struct VGPURendererD3D11 {
    /* Associated vgpu_device */
    VGPUDevice gpu_device;

    bool headless;
    bool validation;
    IDXGIFactory2* dxgi_factory;
    bool flip_present_supported;
    bool tearing_supported;

    ID3D11Device1* d3d_device;
    ID3D11DeviceContext1* d3d_context;
    ID3DUserDefinedAnnotation* d3d_annotation;
    D3D_FEATURE_LEVEL feature_level;

    VGPUFeatures features;
    VGPULimits limits;

    VGPUSwapchainD3D11 swapchains[_VGPU_MAX_SWAPCHAINS];
} VGPURendererD3D11;

static struct {
    bool                    available_initialized;
    bool                    available;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    bool can_use_new_features;
    HMODULE dxgi_handle;
    HMODULE d3d11_handle;

    // DXGI functions
    PFN_CREATE_DXGI_FACTORY1 CreateDXGIFactory1;
    PFN_CREATE_DXGI_FACTORY2 CreateDXGIFactory2;
    PFN_GET_DXGI_DEBUG_INTERFACE1 DXGIGetDebugInterface1;
    // D3D11 functions.
    PFN_D3D11_CREATE_DEVICE D3D11CreateDevice;
#endif
} d3d11;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   define vgpuCreateDXGIFactory1 d3d11.CreateDXGIFactory1
#   define vgpuCreateDXGIFactory2 d3d11.CreateDXGIFactory2
#   define vgpuDXGIGetDebugInterface1 d3d11.DXGIGetDebugInterface1
#   define vgpuD3D11CreateDevice d3d11.D3D11CreateDevice
#else
#   define vgpuCreateDXGIFactory2 CreateDXGIFactory2
#   define vgpuDXGIGetDebugInterface1 DXGIGetDebugInterface1
#   define vgpuD3D11CreateDevice D3D11CreateDevice
#endif

#if defined(_DEBUG)
static const GUID g_WKPDID_D3DDebugObjectName = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87,0x42,0xac,0xb0,0xbf,0x85,0xc2,0x00 } };

static bool vgpu_sdk_layers_available() {
    HRESULT hr = vgpuD3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
        nullptr,
        D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
        nullptr,                    // Any feature level will do.
        0,
        D3D11_SDK_VERSION,
        nullptr,                    // No need to keep the D3D device reference.
        nullptr,                    // No need to know the feature level.
        nullptr                     // No need to keep the D3D device context reference.
    );

    return SUCCEEDED(hr);
}
#endif


/* Helper functions */
static void _vgpu_d3d11_set_name(VGPURendererD3D11* renderer, ID3D11DeviceChild* handle, const char* name)
{
#if defined(_DEBUG)
    if (!renderer->validation)
        return;

    if (name)
    {
        const size_t length = strlen(name);
        ID3D11DeviceChild_SetPrivateData(handle, &g_WKPDID_D3DDebugObjectName, (UINT)length, name);
    }
    else
    {
        ID3D11DeviceChild_SetPrivateData(handle, &g_WKPDID_D3DDebugObjectName, 0, NULL);
    }
#endif /* defined(_DEBUG) */
}

/* Conversion functions */
static vgpu_texture_usage _vgpu_d3d11_get_texture_usage(UINT bind_flags) {
    vgpu_texture_usage usage = 0;
    if (bind_flags & D3D11_BIND_SHADER_RESOURCE) {
        usage |= VGPU_TEXTURE_USAGE_SAMPLED;
    }
    if (bind_flags & D3D11_BIND_UNORDERED_ACCESS) {
        usage |= VGPU_TEXTURE_USAGE_STORAGE;
    }
    if (bind_flags & D3D11_BIND_RENDER_TARGET ||
        bind_flags & D3D11_BIND_DEPTH_STENCIL) {
        usage |= VGPU_TEXTURE_USAGE_RENDERTARGET;
    }

    return usage;
}

static D3D11_COMPARISON_FUNC getD3D11Comparison(VGPUCompareFunction function, D3D11_COMPARISON_FUNC defaultValue)
{
    switch (function)
    {
    case VGPUCompareFunction_Never:
        return D3D11_COMPARISON_NEVER;
    case VGPUCompareFunction_Less:
        return D3D11_COMPARISON_LESS;
    case VGPUCompareFunction_LessEqual:
        return D3D11_COMPARISON_LESS_EQUAL;
    case VGPUCompareFunction_Greater:
        return D3D11_COMPARISON_GREATER;
    case VGPUCompareFunction_GreaterEqual:
        return D3D11_COMPARISON_GREATER_EQUAL;
    case VGPUCompareFunction_Equal:
        return D3D11_COMPARISON_EQUAL;
    case VGPUCompareFunction_NotEqual:
        return D3D11_COMPARISON_NOT_EQUAL;
    case VGPUCompareFunction_Always:
        return D3D11_COMPARISON_ALWAYS;

    default:
        return defaultValue;
    }
}

static void vgpu_d3d11_init_or_resize_swapchain(
    VGPURendererD3D11* renderer,
    VGPUSwapchainD3D11* swapchain,
    uint32_t width, uint32_t height, bool fullscreen)
{
    const uint32_t sample_count = 1u;
    const DXGI_FORMAT back_buffer_dxgi_format = _vgpu_d3d_swapchain_pixel_format(swapchain->colorFormat);

    if (swapchain->handle != nullptr)
    {
    }
    else
    {
        DXGI_SWAP_CHAIN_DESC1 dxgi_swap_chain_desc = {
            .Width = width,
            .Height = height,
            .Format = back_buffer_dxgi_format,
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = 2u,
            .SampleDesc = {
                .Count = sample_count,
                .Quality = sample_count > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0
            },
            .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
        };

        if (!swapchain->sync_interval && renderer->tearing_supported)
        {
            dxgi_swap_chain_desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }


#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        dxgi_swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
        if (!renderer->flip_present_supported)
        {
            dxgi_swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        }

        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC dxgi_swap_chain_fullscreen_desc = {
            .Windowed = !fullscreen
        };

        // Create a SwapChain from a Win32 window.
        VHR(IDXGIFactory2_CreateSwapChainForHwnd(
            renderer->dxgi_factory,
            (IUnknown*)renderer->d3d_device,
            swapchain->window,
            &dxgi_swap_chain_desc,
            &dxgi_swap_chain_fullscreen_desc,
            nullptr,
            &swapchain->handle
        ));

        // This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
        VHR(IDXGIFactory2_MakeWindowAssociation(renderer->dxgi_factory, swapchain->window, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER));
#else
        swapChainDesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;

        // Create a swap chain for the window.
        IDXGISwapChain1* tempSwapChain;
        VHR(d3d11.dxgiFactory->CreateSwapChainForCoreWindow(
            d3d11.device,
            swapChain->window,
            &swapChainDesc,
            nullptr,
            &tempSwapChain
        ));

        IDXGISwapChain3* handle;
        VHR(tempSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain->handle)));
        VHR(swapChain.handle->SetRotation(DXGI_MODE_ROTATION_IDENTITY));
        SAFE_RELEASE(tempSwapChain);
#endif
    }

    ID3D11Texture2D* render_target;
    VHR(IDXGISwapChain1_GetBuffer(swapchain->handle, 0, &vgpu_IID_ID3D11Texture2D, (void**)&render_target));

    D3D11_TEXTURE2D_DESC d3d_texture_desc;
    ID3D11Texture2D_GetDesc(render_target, &d3d_texture_desc);

    const vgpu_texture_desc texture_desc = {
        .type = VGPU_TEXTURE_TYPE_2D,
        .usage = _vgpu_d3d11_get_texture_usage(d3d_texture_desc.BindFlags),
        .width = d3d_texture_desc.Width,
        .height = d3d_texture_desc.Height,
        .layers = d3d_texture_desc.ArraySize,
        .format = VGPUTextureFormat_BGRA8Unorm,
        .mip_levels = d3d_texture_desc.MipLevels,
        .sample_count = d3d_texture_desc.SampleDesc.Count,
        .external_handle = (void*)render_target
    };
    swapchain->backbufferTexture = vgpu_create_texture(&texture_desc);

    if (swapchain->depthStencilFormat != VGPUTextureFormat_Undefined)
    {
        const vgpu_texture_desc depth_texture_desc = {
            .type = VGPU_TEXTURE_TYPE_2D,
            .usage = VGPU_TEXTURE_USAGE_RENDERTARGET,
            .width = d3d_texture_desc.Width,
            .height = d3d_texture_desc.Height,
            .layers = 1u,
            .format = swapchain->depthStencilFormat,
            .mip_levels = 1u,
            .sample_count = 1u
        };

        swapchain->depthStencilTexture = vgpu_create_texture(&depth_texture_desc);
    }

    VGPURenderPassDescriptor pass_desc = {
        .colorAttachments[0].texture = swapchain->backbufferTexture,
        .colorAttachments[0].clearColor = swapchain->clearColor
    };

    if (swapchain->depthStencilFormat != VGPUTextureFormat_Undefined)
    {
        pass_desc.depthStencilAttachment.texture = swapchain->depthStencilTexture;
    }

    swapchain->renderPass = vgpuCreateRenderPass(&pass_desc);
    ID3D11Texture2D_Release(render_target);
}

static bool d3d11_create_factory(VGPURendererD3D11* renderer)
{
    HRESULT hr = S_OK;

    SAFE_RELEASE(renderer->dxgi_factory);

#if defined(_DEBUG)
    bool debug_dxgi = false;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    if (d3d11.can_use_new_features)
#endif
    {
        if (renderer->validation)
        {
            IDXGIInfoQueue* dxgi_info_queue;
            if (SUCCEEDED(vgpuDXGIGetDebugInterface1(0, &vgpu_IID_IDXGIInfoQueue, (void**)&dxgi_info_queue)))
            {
                debug_dxgi = true;

                hr = vgpuCreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, &vgpu_IID_IDXGIFactory2, (void**)&renderer->dxgi_factory);
                if (FAILED(hr)) {
                    return false;
                }

                IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info_queue, vgpu_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
                IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info_queue, vgpu_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

                DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
                {
                    80 // IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides.
                };
                DXGI_INFO_QUEUE_FILTER filter;
                memset(&filter, 0, sizeof(DXGI_INFO_QUEUE_FILTER));
                filter.DenyList.NumIDs = _countof(hide);
                filter.DenyList.pIDList = hide;
                IDXGIInfoQueue_AddStorageFilterEntries(dxgi_info_queue, vgpu_DXGI_DEBUG_DXGI, &filter);
                IDXGIInfoQueue_Release(dxgi_info_queue);
            }
        }
    }

    if (!debug_dxgi)
#endif
    {
        hr = vgpuCreateDXGIFactory1(&vgpu_IID_IDXGIFactory2, (void**)&renderer->dxgi_factory);
        if (FAILED(hr)) {
            return false;
        }
    }

    return true;
}

static void vgpu_d3d11_destroy_swapchain(VGPURendererD3D11* renderer, VGPUSwapchainD3D11* swapchain)
{
    if (swapchain->depthStencilTexture) {
        vgpu_destroy_texture(swapchain->depthStencilTexture);
    }

    vgpu_destroy_texture(swapchain->backbufferTexture);
    vgpuDestroyRenderPass(swapchain->renderPass);

    SAFE_RELEASE(swapchain->handle);
}

static bool d3d11_init(VGPUDevice device, const VGpuDeviceDescriptor* desc)
{
    if (!vgpu_d3d11_supported()) {
        return false;
    }

    VGPURendererD3D11* renderer = (VGPURendererD3D11*)device->renderer;
    renderer->headless = desc->swapchain == NULL;
    renderer->validation = (desc->flags & VGPU_CONFIG_FLAGS_VALIDATION);
    if (!d3d11_create_factory(renderer)) {
        vgpuShutdown();
        return false;
    }

    renderer->flip_present_supported = true;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    // Disable FLIP if not on a supporting OS
    {
        IDXGIFactory4* dxgi_factory4;
        HRESULT hr = IDXGIFactory2_QueryInterface(renderer->dxgi_factory, &vgpu_IID_IDXGIFactory4, (void**)&dxgi_factory4);
        if (FAILED(hr))
        {
            renderer->flip_present_supported = false;
#ifdef _DEBUG
            OutputDebugStringA("INFO: Flip swap effects not supported");
#endif
        }
        SAFE_RELEASE(dxgi_factory4);
    }
#endif

    // Check tearing support.
    {
        BOOL allowTearing = FALSE;
        IDXGIFactory5* dxgi_factory5;
        HRESULT hr = IDXGIFactory2_QueryInterface(renderer->dxgi_factory, &vgpu_IID_IDXGIFactory5, (void**)&dxgi_factory5);
        if (SUCCEEDED(hr))
        {
            hr = IDXGIFactory5_CheckFeatureSupport(dxgi_factory5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        }

        if (FAILED(hr) || !allowTearing)
        {
            renderer->tearing_supported = false;
#ifdef _DEBUG
            OutputDebugStringA("WARNING: Variable refresh rate displays not supported");
#endif
        }
        else
        {
            renderer->tearing_supported = true;
        }
        SAFE_RELEASE(dxgi_factory5);
    }

    /* TODO: Enum adapters */

    /* Create d3d11 device */
    {
        UINT creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
        if (renderer->validation && vgpu_sdk_layers_available())
        {
            // If the project is in a debug build, enable debugging via SDK Layers with this flag.
            creation_flags |= D3D11_CREATE_DEVICE_DEBUG;
        }
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
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };

        // Create the Direct3D 11 API device object and a corresponding context.
        ID3D11Device* temp_d3d_device;
        ID3D11DeviceContext* temp_d3d_context;

        HRESULT hr = vgpuD3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            creation_flags,
            s_featureLevels,
            _countof(s_featureLevels),
            D3D11_SDK_VERSION,
            &temp_d3d_device,
            &renderer->feature_level,
            &temp_d3d_context
        );

        if (FAILED(hr)) {
            vgpuShutdown();
            return false;
        }

#ifndef NDEBUG
        ID3D11Debug* d3d_debug;
        if (SUCCEEDED(ID3D11Device_QueryInterface(temp_d3d_device, &vgpu_IID_ID3D11Debug, (void**)&d3d_debug)))
        {
            ID3D11InfoQueue* d3d_info_queue;
            if (SUCCEEDED(ID3D11Debug_QueryInterface(d3d_debug, &vgpu_IID_ID3D11InfoQueue, (void**)&d3d_info_queue)))
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
            }
            SAFE_RELEASE(d3d_info_queue);
            SAFE_RELEASE(d3d_debug);
        }
#endif

        VHR(ID3D11Device_QueryInterface(temp_d3d_device, &vgpu_IID_ID3D11Device1, (void**)&renderer->d3d_device));
        VHR(ID3D11DeviceContext_QueryInterface(temp_d3d_context, &vgpu_IID_ID3D11DeviceContext1, (void**)&renderer->d3d_context));
        VHR(ID3D11DeviceContext_QueryInterface(temp_d3d_context, &vgpu_IID_ID3DUserDefinedAnnotation, (void**)&renderer->d3d_annotation));
        SAFE_RELEASE(temp_d3d_context);
        SAFE_RELEASE(temp_d3d_device);
    }

    // Init features and limits.
    {
        renderer->features.independentBlend = (renderer->feature_level >= D3D_FEATURE_LEVEL_10_0);
        renderer->features.computeShader = (renderer->feature_level >= D3D_FEATURE_LEVEL_10_0);
        renderer->features.geometryShader = (renderer->feature_level >= D3D_FEATURE_LEVEL_11_0);
        renderer->features.tessellationShader = (renderer->feature_level >= D3D_FEATURE_LEVEL_11_0);
        renderer->features.multiViewport = true;
        renderer->features.indexUint32 = true;
        renderer->features.multiDrawIndirect = (renderer->feature_level >= D3D_FEATURE_LEVEL_11_0);
        renderer->features.fillModeNonSolid = true;
        renderer->features.samplerAnisotropy = true;
        renderer->features.textureCompressionETC2 = false;
        renderer->features.textureCompressionASTC_LDR = false;
        renderer->features.textureCompressionBC = true;
        renderer->features.textureCubeArray = (renderer->feature_level >= D3D_FEATURE_LEVEL_10_1);
        renderer->features.raytracing = false;

        // Limits
        renderer->limits.max_vertex_attributes = VGPU_MAX_VERTEX_ATTRIBUTES;
        renderer->limits.max_vertex_bindings = VGPU_MAX_VERTEX_ATTRIBUTES;
        renderer->limits.max_vertex_attribute_offset = VGPU_MAX_VERTEX_ATTRIBUTE_OFFSET;
        renderer->limits.max_vertex_binding_stride = VGPU_MAX_VERTEX_BUFFER_STRIDE;

        renderer->limits.max_texture_size_1d = D3D11_REQ_TEXTURE1D_U_DIMENSION;
        renderer->limits.max_texture_size_2d = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        renderer->limits.max_texture_size_3d = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        renderer->limits.max_texture_size_cube = D3D11_REQ_TEXTURECUBE_DIMENSION;
        renderer->limits.max_texture_array_layers = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        renderer->limits.max_color_attachments = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
        renderer->limits.max_uniform_buffer_size = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
        renderer->limits.min_uniform_buffer_offset_alignment = 256u;
        renderer->limits.max_storage_buffer_size = UINT32_MAX;
        renderer->limits.min_storage_buffer_offset_alignment = 16;
        renderer->limits.max_sampler_anisotropy = D3D11_MAX_MAXANISOTROPY;
        renderer->limits.max_viewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        renderer->limits.max_viewport_width = D3D11_VIEWPORT_BOUNDS_MAX;
        renderer->limits.max_viewport_height = D3D11_VIEWPORT_BOUNDS_MAX;
        renderer->limits.max_tessellation_patch_size = D3D11_IA_PATCH_MAX_CONTROL_POINT_COUNT;
        renderer->limits.point_size_range_min = 1.0f;
        renderer->limits.point_size_range_max = 1.0f;
        renderer->limits.line_width_range_min = 1.0f;
        renderer->limits.line_width_range_max = 1.0f;
        renderer->limits.max_compute_shared_memory_size = D3D11_CS_THREAD_LOCAL_TEMP_REGISTER_POOL;
        renderer->limits.max_compute_work_group_count_x = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        renderer->limits.max_compute_work_group_count_y = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        renderer->limits.max_compute_work_group_count_z = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        renderer->limits.max_compute_work_group_invocations = D3D11_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;
        renderer->limits.max_compute_work_group_size_x = D3D11_CS_THREAD_GROUP_MAX_X;
        renderer->limits.max_compute_work_group_size_y = D3D11_CS_THREAD_GROUP_MAX_Y;
        renderer->limits.max_compute_work_group_size_z = D3D11_CS_THREAD_GROUP_MAX_Z;

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
    }

    /* Create main swap chain if required */
    if (desc->swapchain)
    {
        renderer->swapchains[0].width = desc->swapchain->width;
        renderer->swapchains[0].height = desc->swapchain->height;
        renderer->swapchains[0].clearColor = desc->swapchain->colorClearValue;
        renderer->swapchains[0].depthStencilFormat = desc->swapchain->depthStencilFormat;
        renderer->swapchains[0].sync_interval = vgpuD3DGetSyncInterval(desc->swapchain->presentMode);
        renderer->swapchains[0].present_flags = 0u;

        if (!renderer->swapchains[0].sync_interval &&
            renderer->tearing_supported)
        {
            renderer->swapchains[0].present_flags |= DXGI_PRESENT_ALLOW_TEARING;
        }

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        renderer->swapchains[0].window = (HWND)desc->swapchain->nativeHandle;
        VGPU_ASSERT(IsWindow(renderer->swapchains[0].window));

        if (renderer->swapchains[0].width == 0 ||
            renderer->swapchains[0].height == 0)
        {
            RECT rect;
            GetClientRect(renderer->swapchains[0].window, &rect);
            renderer->swapchains[0].width = rect.right - rect.left;
            renderer->swapchains[0].height = rect.bottom - rect.top;
        }
#else
        renderer->swapchains[0].window = (IUnknown*)desc->swapchain->native_handle;
#endif

        vgpu_d3d11_init_or_resize_swapchain(
            renderer,
            &renderer->swapchains[0],
            renderer->swapchains[0].width, renderer->swapchains[0].height,
            desc->swapchain->fullscreen);
    }

    return true;
}

static void d3d11_destroy(VGPUDevice device)
{
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)device->renderer;

    if (renderer->d3d_device)
    {
        /* Destroy swap chains.*/
        for (uint32_t i = 0; i < _VGPU_MAX_SWAPCHAINS; i++)
        {
            if (!renderer->swapchains[i].handle)
                continue;

            vgpu_d3d11_destroy_swapchain(renderer, &renderer->swapchains[i]);
        }

        SAFE_RELEASE(renderer->d3d_context);
        SAFE_RELEASE(renderer->d3d_annotation);

#if !defined(NDEBUG)
        ULONG ref_count = ID3D11Device1_Release(renderer->d3d_device);
        if (ref_count > 0)
        {
            vgpu_log_error_format("Direct3D11: There are %d unreleased references left on the device", ref_count);

            ID3D11Debug* d3d_debug = nullptr;
            if (SUCCEEDED(ID3D11Device1_QueryInterface(renderer->d3d_device, &vgpu_IID_ID3D11Debug, (void**)&d3d_debug)))
            {
                ID3D11Debug_ReportLiveDeviceObjects(d3d_debug, D3D11_RLDO_SUMMARY | D3D11_RLDO_IGNORE_INTERNAL);
                ID3D11Debug_Release(d3d_debug);
            }
        }
#else
        d3d12.device->Release();
#endif
    }

    SAFE_RELEASE(renderer->dxgi_factory);

#ifdef _DEBUG
    IDXGIDebug* dxgi_debug;
    if (SUCCEEDED(vgpuDXGIGetDebugInterface1(0, &vgpu_IID_IDXGIDebug, (void**)&dxgi_debug)))
    {
        IDXGIDebug_ReportLiveObjects(dxgi_debug, vgpu_DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL);
    }
    SAFE_RELEASE(dxgi_debug);
#endif

    VGPU_FREE(renderer);
    VGPU_FREE(device);
}

static VGPUBackendType d3d11_getBackend(void) {
    return VGPUBackendType_D3D11;
}

static VGPUFeatures d3d11_getFeatures(VGPURenderer* driverData)
{
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driverData;
    return renderer->features;
}

static VGPULimits d3d11_getLimits(VGPURenderer* driverData)
{
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driverData;
    return renderer->limits;
}

static VGPURenderPass d3d11_get_default_render_pass(VGPURenderer* driverData) {
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driverData;
    return renderer->swapchains[0].renderPass;
}

static void d3d11_begin_frame(VGPURenderer* driverData) {
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driverData;
}

static void d3d11_end_frame(VGPURenderer* driverData) {
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driverData;
    HRESULT hr = S_OK;

    for (uint32_t i = 0; i < _VGPU_MAX_SWAPCHAINS; i++)
    {
        if (!renderer->swapchains[i].handle)
            continue;

        hr = IDXGISwapChain1_Present(
            renderer->swapchains[i].handle,
            renderer->swapchains[i].sync_interval,
            renderer->swapchains[i].present_flags);

        if (hr == DXGI_ERROR_DEVICE_REMOVED
            || hr == DXGI_ERROR_DEVICE_HUNG
            || hr == DXGI_ERROR_DEVICE_RESET
            || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR
            || hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
        {
            //HandleDeviceLost();
            return;
        }
    }
}

/* Buffer */
static VGPUBuffer d3d11_bufferCreate(VGPURenderer* driverData, const VGPUBufferDescriptor* descriptor)
{
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driverData;

    D3D11_BUFFER_DESC d3d11_desc;
    memset(&d3d11_desc, 0, sizeof(d3d11_desc));
    d3d11_desc.ByteWidth = (UINT)descriptor->size;
    d3d11_desc.Usage = D3D11_USAGE_DEFAULT;
    d3d11_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    d3d11_desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA* initialDataPtr = 0;
    D3D11_SUBRESOURCE_DATA initData;
    if (descriptor->content != nullptr) {
        initData.pSysMem = descriptor->content;
        initData.SysMemPitch = 0;
        initData.SysMemSlicePitch = 0;
        initialDataPtr = &initData;
    }

    ID3D11Buffer* handle;
    HRESULT hr = ID3D11Device1_CreateBuffer(renderer->d3d_device, &d3d11_desc, initialDataPtr, &handle);
    if (FAILED(hr)) {
        return nullptr;
    }

    VGPUBufferD3D11* result = _VGPU_ALLOC_HANDLE(VGPUBufferD3D11);
    result->handle = handle;
    return (VGPUBuffer)result;
}

static void d3d11_bufferDestroy(VGPURenderer* driver_data, VGPUBuffer handle) {
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driver_data;
    VGPUBufferD3D11* buffer = (VGPUBufferD3D11*)handle;
    ID3D11Buffer_Release(buffer->handle);
    VGPU_FREE(buffer);
}

/* Texture */
static VGPUTexture d3d11_create_texture(VGPURenderer* driverData, const vgpu_texture_desc* desc)
{
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driverData;
    VGPUTextureD3D11* texture = _VGPU_ALLOC_HANDLE(VGPUTextureD3D11);
    if (desc->external_handle != nullptr)
    {
        texture->handle.resource = (ID3D11Resource*)desc->external_handle;
        ID3D11Resource_AddRef(texture->handle.resource);
    }
    else
    {
        HRESULT hr = S_OK;

        DXGI_FORMAT dxgi_format;

        // If depth and either ua or sr, set to typeless
        if (vgpu_is_depth_format(desc->format) &&
            (desc->usage & VGPU_TEXTURE_USAGE_SAMPLED | VGPU_TEXTURE_USAGE_STORAGE) != VGPU_TEXTURE_USAGE_NONE)
        {
            dxgi_format = _vgpu_d3d_get_typeless_format(desc->format);
        }
        else
        {
            dxgi_format = _vgpu_d3d_get_format(desc->format);
        }

        if (desc->type == VGPU_TEXTURE_TYPE_3D)
        {
        }
        else
        {
            const uint32_t multiplier = (desc->type == VGPU_TEXTURE_TYPE_CUBE) ? 6 : 1;

            const D3D11_TEXTURE2D_DESC d3d11_desc = {
                .Width = desc->width,
                .Height = desc->height,
                .MipLevels = desc->mip_levels,
                .ArraySize = desc->layers * multiplier,
                .Format = dxgi_format,
                .SampleDesc = {
                    .Count = desc->sample_count,
                    .Quality = desc->sample_count > 0 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0
                },
                .Usage = D3D11_USAGE_DEFAULT,
                .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                .CPUAccessFlags = 0,
                .MiscFlags = (desc->type == VGPU_TEXTURE_TYPE_CUBE) ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0
            };
            hr = ID3D11Device1_CreateTexture2D(renderer->d3d_device, &d3d11_desc, NULL, &texture->handle.tex2d);
        }

        if (FAILED(hr)) {
            VGPU_FREE(texture);
            return nullptr;
        }
    }

    memcpy(&(texture->desc), desc, sizeof(*desc));
    return (VGPUTexture)texture;
}

static void d3d11_destroy_texture(VGPURenderer* driverData, VGPUTexture handle)
{
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driverData;
    VGPUTextureD3D11* texture = (VGPUTextureD3D11*)handle;
    ID3D11Resource_Release(texture->handle.resource);
    VGPU_FREE(texture);
}

static vgpu_texture_desc d3d11_query_texture_desc(VGPUTexture handle)
{
    VGPUTextureD3D11* texture = (VGPUTextureD3D11*)handle;
    return texture->desc;
}

/* Sampler */
D3D11_FILTER_TYPE getD3D11FilterType(VGPUFilterMode filter)
{
    switch (filter)
    {
    case VGPUFilterMode_Nearest:
        return D3D11_FILTER_TYPE_POINT;
    case VGPUFilterMode_Linear:
        return D3D11_FILTER_TYPE_LINEAR;
    default:
        VGPU_UNREACHABLE();
        return (D3D11_FILTER_TYPE)-1;
    }
}

D3D11_FILTER getD3D11Filter(VGPUFilterMode minFilter, VGPUFilterMode magFilter, VGPUFilterMode mipFilter, bool isComparison, bool isAnisotropic)
{
    D3D11_FILTER filter;
    D3D11_FILTER_REDUCTION_TYPE reduction = isComparison ? D3D11_FILTER_REDUCTION_TYPE_COMPARISON : D3D11_FILTER_REDUCTION_TYPE_STANDARD;

    if (isAnisotropic)
    {
        filter = D3D11_ENCODE_ANISOTROPIC_FILTER(reduction);
    }
    else
    {
        D3D11_FILTER_TYPE dxMin = getD3D11FilterType(minFilter);
        D3D11_FILTER_TYPE dxMag = getD3D11FilterType(magFilter);
        D3D11_FILTER_TYPE dxMip = getD3D11FilterType(mipFilter);
        filter = D3D11_ENCODE_BASIC_FILTER(dxMin, dxMag, dxMip, reduction);
    }

    return filter;
};


static D3D11_TEXTURE_ADDRESS_MODE getD3D11AddressMode(VGPUAddressMode mode)
{
    switch (mode)
    {
    case VGPUAddressMode_Repeat:
        return D3D11_TEXTURE_ADDRESS_WRAP;

    case VGPUAddressMode_MirrorRepeat:
        return D3D11_TEXTURE_ADDRESS_MIRROR;

    case VGPUAddressMode_ClampToBorderColor:
        return D3D11_TEXTURE_ADDRESS_BORDER;

    default:
    case VGPUAddressMode_ClampToEdge:
        return D3D11_TEXTURE_ADDRESS_CLAMP;
    }
}

static vgpu_sampler d3d11_samplerCreate(VGPURenderer* driverData, const VGPUSamplerDescriptor* descriptor)
{
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driverData;

    const bool isComparison =
        (descriptor->compare != VGPUCompareFunction_Undefined) &&
        (descriptor->compare != VGPUCompareFunction_Never);

    D3D11_SAMPLER_DESC sampler_desc = {
        .Filter = getD3D11Filter(descriptor->minFilter, descriptor->magFilter, descriptor->mipmapFilter, isComparison, descriptor->maxAnisotropy > 1),
        .AddressU = getD3D11AddressMode(descriptor->addressModeU),
        .AddressV = getD3D11AddressMode(descriptor->addressModeV),
        .AddressW = getD3D11AddressMode(descriptor->addressModeW),
        .MipLODBias = 0.0f,
        .MaxAnisotropy = descriptor->maxAnisotropy,
        .ComparisonFunc = getD3D11Comparison(descriptor->compare, D3D11_COMPARISON_NEVER),
        .MinLOD = descriptor->lodMinClamp,
        .MaxLOD = descriptor->lodMaxClamp == 0.0f ? 3.402823466e+38F : descriptor->lodMaxClamp,
    };

    switch (descriptor->border_color) {
    case VGPUBorderColor_TransparentBlack:
        sampler_desc.BorderColor[0] = 0.0f;
        sampler_desc.BorderColor[1] = 0.0f;
        sampler_desc.BorderColor[2] = 0.0f;
        sampler_desc.BorderColor[3] = 0.0f;
        break;
    case VGPUBorderColor_OpaqueBlack:
        sampler_desc.BorderColor[0] = 0.0f;
        sampler_desc.BorderColor[1] = 0.0f;
        sampler_desc.BorderColor[2] = 0.0f;
        sampler_desc.BorderColor[3] = 1.0f;
        break;
    case VGPUBorderColor_OpaqueWhite:
        sampler_desc.BorderColor[0] = 1.0f;
        sampler_desc.BorderColor[1] = 1.0f;
        sampler_desc.BorderColor[2] = 1.0f;
        sampler_desc.BorderColor[3] = 1.0f;
        break;
    }

    ID3D11SamplerState* handle;
    HRESULT hr = ID3D11Device1_CreateSamplerState(renderer->d3d_device, &sampler_desc, &handle);
    if (FAILED(hr)) {
        return nullptr;
    }

    VGPUSamplerD3D11* result = _VGPU_ALLOC_HANDLE(VGPUSamplerD3D11);
    result->handle = handle;
    return (vgpu_sampler)result;
}

static void d3d11_samplerDestroy(VGPURenderer* driverData, vgpu_sampler handle)
{
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driverData;
    VGPUSamplerD3D11* sampler = (VGPUSamplerD3D11*)handle;
    ID3D11SamplerState_Release(sampler->handle);
    VGPU_FREE(sampler);
}

/* RenderPass */
static VGPURenderPass d3d11_renderPassCreate(VGPURenderer* driverData, const VGPURenderPassDescriptor* desc)
{
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driverData;
    VGPURenderPassD3D11* renderPass = _VGPU_ALLOC_HANDLE(VGPURenderPassD3D11);
    renderPass->width = UINT32_MAX;
    renderPass->height = UINT32_MAX;

    for (uint32_t i = 0; i < VGPU_MAX_COLOR_ATTACHMENTS; i++) {
        if (!desc->colorAttachments[i].texture) {
            continue;
        }

        uint32_t mip_level = desc->colorAttachments[i].mipLevel;
        VGPUTextureD3D11* d3d11_texture = (VGPUTextureD3D11*)desc->colorAttachments[i].texture;
        renderPass->width = _vgpu_min(renderPass->width, _vgpu_max(1u, d3d11_texture->desc.width >> mip_level));
        renderPass->height = _vgpu_min(renderPass->height, _vgpu_max(1u, d3d11_texture->desc.height >> mip_level));

        HRESULT hr = ID3D11Device1_CreateRenderTargetView(
            renderer->d3d_device,
            d3d11_texture->handle.resource,
            nullptr,
            &renderPass->color_rtvs[renderPass->num_color_rtvs]);
        VHR(hr);
        memcpy(&renderPass->clearColors[i].r, &desc->colorAttachments[i].clearColor, sizeof(VGPUColor));
        renderPass->num_color_rtvs++;
    }

    return (VGPURenderPass)renderPass;
}

static void d3d11_renderPassDestroy(VGPURenderer* driverData, VGPURenderPass handle)
{
    _VGPU_UNUSED(driverData);
    VGPURenderPassD3D11* render_pass = (VGPURenderPassD3D11*)handle;
    for (uint32_t i = 0; i < render_pass->num_color_rtvs; i++) {
        ID3D11RenderTargetView_Release(render_pass->color_rtvs[i]);
    }

    if (render_pass->dsv) {
        ID3D11DepthStencilView_Release(render_pass->dsv);
    }

    VGPU_FREE(render_pass);
}

static void d3d11_renderPassGetExtent(VGPURenderer* driverData, VGPURenderPass handle, uint32_t* width, uint32_t* height)
{
    _VGPU_UNUSED(driverData);
    VGPURenderPassD3D11* render_pass = (VGPURenderPassD3D11*)handle;
    if (width) {
        *width = render_pass->width;
    }

    if (height) {
        *height = render_pass->height;
    }
}

void d3d11_renderPassSetClearColors(VGPURenderer* driverData, VGPURenderPass handle, uint32_t firstAttachment, uint32_t count, const VGPUColor* pColors)
{
    VGPURenderPassD3D11* renderPass = (VGPURenderPassD3D11*)handle;
    for (uint32_t i = firstAttachment; i < count; i++)
    {
        memcpy(&renderPass->clearColors[i].r, &pColors[i].r, sizeof(VGPUColor));
    }
}

static void d3d11_cmdBeginRenderPass(VGPURenderer* driverData, VGPURenderPass handle)
{
    VGPURendererD3D11* renderer = (VGPURendererD3D11*)driverData;
    VGPURenderPassD3D11* render_pass = (VGPURenderPassD3D11*)handle;

    ID3D11DeviceContext1_OMSetRenderTargets(
        renderer->d3d_context,
        render_pass->num_color_rtvs, render_pass->color_rtvs,
        render_pass->dsv);

    /* Apply viewport and scissor rect to the render pass */
    const D3D11_VIEWPORT viewport = {
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = (FLOAT)render_pass->width,
        .Height = (FLOAT)render_pass->height,
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f
    };

    const D3D11_RECT scissor_rect = {
        .left = 0,
        .top = 0,
        .right = (LONG)render_pass->width,
        .bottom = (LONG)render_pass->height,
    };

    ID3D11DeviceContext_RSSetViewports(renderer->d3d_context, 1, &viewport);
    ID3D11DeviceContext_RSSetScissorRects(renderer->d3d_context, 1, &scissor_rect);

    /* perform load action */
    for (uint32_t i = 0; i < render_pass->num_color_rtvs; i++)
    {
        ID3D11DeviceContext_ClearRenderTargetView(
            renderer->d3d_context, render_pass->color_rtvs[i],
            &render_pass->clearColors[i].r);
    }
}

static void d3d11_cmdEndRenderPass(VGPURenderer* driverData) {

}

bool vgpu_d3d11_supported(void) {
    if (d3d11.available_initialized) {
        return d3d11.available;
    }
    d3d11.available_initialized = true;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) 
    d3d11.dxgi_handle = LoadLibraryW(L"dxgi.dll");
    if (d3d11.dxgi_handle == nullptr) {
        return false;
    }

    d3d11.CreateDXGIFactory2 = (PFN_CREATE_DXGI_FACTORY2)GetProcAddress(d3d11.dxgi_handle, "CreateDXGIFactory2");
    if (d3d11.CreateDXGIFactory2 == nullptr)
    {
        d3d11.CreateDXGIFactory1 = (PFN_CREATE_DXGI_FACTORY1)GetProcAddress(d3d11.dxgi_handle, "CreateDXGIFactory1");
        if (d3d11.CreateDXGIFactory1 == nullptr)
        {
            return false;
        }

        return false;
    }
    else
    {
        d3d11.can_use_new_features = true;
        d3d11.DXGIGetDebugInterface1 = (PFN_GET_DXGI_DEBUG_INTERFACE1)GetProcAddress(d3d11.dxgi_handle, "DXGIGetDebugInterface1");
    }

    d3d11.d3d11_handle = LoadLibraryW(L"d3d11.dll");
    if (d3d11.d3d11_handle == nullptr) {
        return false;
    }

    d3d11.D3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(d3d11.d3d11_handle, "D3D11CreateDevice");
    if (d3d11.D3D11CreateDevice == nullptr) {
        return false;
    }

#endif

    return true;
}

VGPUDevice d3d11_create_device(void) {
    VGPUDevice device;
    VGPURendererD3D11* renderer;

    device = (VGPUDeviceImpl*)VGPU_MALLOC(sizeof(VGPUDeviceImpl));
    ASSIGN_DRIVER(d3d11);

    /* Init the d3d11 renderer */
    renderer = _VGPU_ALLOC_HANDLE(VGPURendererD3D11);

    /* Reference vgpu_device and renderer together. */
    renderer->gpu_device = device;
    device->renderer = (VGPURenderer*)renderer;

    return device;
}
