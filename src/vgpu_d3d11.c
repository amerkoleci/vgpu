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
typedef struct vgpu_d3d11_swapchain {
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HWND window;
#else
    IUnknown* window;
#endif

    uint32_t width;
    uint32_t height;

    vgpu_pixel_format color_format;
    IDXGISwapChain1* handle;
    UINT sync_interval;
    UINT present_flags;
    vgpu_texture* backbuffer;
    vgpu_texture* depth_stencil_texture;
    vgpu_render_pass* render_pass;
} vgpu_d3d11_swapchain;

typedef struct d3d11_gpu_texture {
    union
    {
        ID3D11Resource* resource;
        ID3D11Texture1D* tex1d;
        ID3D11Texture2D* tex2d;
        ID3D11Texture3D* tex3d;
    } handle;
    uint32_t width;
    uint32_t height;
} d3d11_gpu_texture;

typedef struct d3d11_gpu_render_pass {
    uint32_t width;
    uint32_t height;
    uint32_t num_color_rtvs;
    ID3D11RenderTargetView* color_rtvs[VGPU_MAX_COLOR_ATTACHMENTS];
    ID3D11DepthStencilView* dsv;
} d3d11_gpu_render_pass;

typedef struct d3d11_renderer {
    /* Associated vgpu_device */
    vgpu_device* gpu_device;

    bool headless;
    bool validation;
    IDXGIFactory2* dxgi_factory;
    bool flip_present_supported;
    bool tearing_supported;

    ID3D11Device1* d3d_device;
    ID3D11DeviceContext1* d3d_context;
    ID3DUserDefinedAnnotation* d3d_annotation;
    D3D_FEATURE_LEVEL feature_level;

    vgpu_features features;
    vgpu_limits limits;

    vgpu_d3d11_swapchain swapchains[_VGPU_MAX_SWAPCHAINS];
} d3d11_renderer;

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

/* Conversion functions */
static vgpu_texture_usage d3d12_gpu_get_texture_usage(UINT bind_flags) {
    vgpu_texture_usage usage = 0;
    if (bind_flags & D3D11_BIND_SHADER_RESOURCE) {
        usage |= VGPU_TEXTURE_USAGE_SAMPLED;
    }
    if (bind_flags & D3D11_BIND_UNORDERED_ACCESS) {
        usage |= VGPU_TEXTURE_USAGE_STORAGE;
    }
    if (bind_flags & D3D11_BIND_RENDER_TARGET ||
        bind_flags & D3D11_BIND_DEPTH_STENCIL) {
        usage |= VGPU_TEXTURE_USAGE_RENDER_TARGET;
    }

    return usage;
}

static void vgpu_d3d11_init_or_resize_swapchain(
    d3d11_renderer* renderer,
    vgpu_d3d11_swapchain* swapchain,
    uint32_t width, uint32_t height, bool fullscreen)
{
    const uint32_t sample_count = 1u;
    const DXGI_FORMAT back_buffer_dxgi_format = _vgpu_d3d_swapchain_pixel_format(swapchain->color_format);

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
        .width = d3d_texture_desc.Width,
        .height = d3d_texture_desc.Height,
        .layers = d3d_texture_desc.ArraySize,
        .mip_levels = d3d_texture_desc.MipLevels,
        .format = VGPU_PIXEL_FORMAT_BGRA8_UNORM,
        .usage = d3d12_gpu_get_texture_usage(d3d_texture_desc.BindFlags),
        .sample_count = (vgpu_sample_count)d3d_texture_desc.SampleDesc.Count
    };
    swapchain->backbuffer = vgpu_texture_create_external(&texture_desc, render_target);
    swapchain->depth_stencil_texture = nullptr;

    const vgpu_render_pass_desc pass_desc = {
        .color_attachments[0].texture = swapchain->backbuffer
    };

    swapchain->render_pass = vgpu_render_pass_create(&pass_desc);
}

static bool d3d11_create_factory(d3d11_renderer* renderer)
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


static void vgpu_d3d11_destroy_swapchain(d3d11_renderer* renderer, vgpu_d3d11_swapchain* swapchain)
{
    if (swapchain->depth_stencil_texture) {
        vgpu_texture_destroy(swapchain->depth_stencil_texture);
    }

    vgpu_texture_destroy(swapchain->backbuffer);
    vgpu_render_pass_destroy(swapchain->render_pass);

    SAFE_RELEASE(swapchain->handle);
}

static bool d3d11_init(vgpu_device* device, const char* app_name, const vgpu_desc* desc)
{
    _VGPU_UNUSED(app_name);

    d3d11_renderer* renderer = (d3d11_renderer*)device->renderer;
    renderer->headless = desc->swapchain == NULL;
    renderer->validation = (desc->flags & VGPU_CONFIG_FLAGS_VALIDATION);
    if (!d3d11_create_factory(renderer)) {
        vgpu_shutdown();
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
            vgpu_shutdown();
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
        renderer->features.independent_blend = (renderer->feature_level >= D3D_FEATURE_LEVEL_10_0);
        renderer->features.compute_shader = (renderer->feature_level >= D3D_FEATURE_LEVEL_10_0);
        renderer->features.geometry_shader = (renderer->feature_level >= D3D_FEATURE_LEVEL_11_0);
        renderer->features.tessellation_shader = (renderer->feature_level >= D3D_FEATURE_LEVEL_11_0);
        renderer->features.multi_viewport = true;
        renderer->features.index_uint32 = true;
        renderer->features.multi_draw_indirect = (renderer->feature_level >= D3D_FEATURE_LEVEL_11_0);
        renderer->features.fill_mode_non_solid = true;
        renderer->features.sampler_anisotropy = true;
        renderer->features.texture_compression_BC = true;
        renderer->features.texture_compression_PVRTC = false;
        renderer->features.texture_compression_ETC2 = false;
        renderer->features.texture_compression_ASTC = false;
        renderer->features.texture_1D = true;
        renderer->features.texture_3D = true;
        renderer->features.texture_2D_array = (renderer->feature_level >= D3D_FEATURE_LEVEL_10_0);
        renderer->features.texture_cube_array = (renderer->feature_level >= D3D_FEATURE_LEVEL_10_1);
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
    }

    /* Create main swap chain if required */
    if (desc->swapchain)
    {
        renderer->swapchains[0].width = desc->swapchain->width;
        renderer->swapchains[0].height = desc->swapchain->height;
        renderer->swapchains[0].sync_interval = 1u;
        renderer->swapchains[0].present_flags = 0u;

        if (!desc->swapchain->vsync) {
            renderer->swapchains[0].sync_interval = 0u;

            if (renderer->tearing_supported) {
                renderer->swapchains[0].present_flags |= DXGI_PRESENT_ALLOW_TEARING;
            }
        }

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        renderer->swapchains[0].window = (HWND)desc->swapchain->native_handle;
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

static void d3d11_destroy(vgpu_device* device)
{
    d3d11_renderer* renderer = (d3d11_renderer*)device->renderer;

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

static vgpu_backend d3d11_get_backend() {
    return VGPU_BACKEND_DIRECT3D11;
}

static vgpu_features d3d11_get_features(vgpu_renderer* driver_data) {
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
    return renderer->features;
}

static vgpu_limits d3d11_get_limits(vgpu_renderer* driver_data) {
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
    return renderer->limits;
}

static vgpu_render_pass* d3d11_get_default_render_pass(vgpu_renderer* driver_data) {
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
    return renderer->swapchains[0].render_pass;
}

static void d3d11_begin_frame(vgpu_renderer* driver_data) {
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
}

static void d3d11_end_frame(vgpu_renderer* driver_data) {
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
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

static vgpu_texture* d3d11_texture_create(vgpu_renderer* driver_data, const vgpu_texture_desc* desc, const void* initial_data) {
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
    d3d11_gpu_texture* result = _VGPU_ALLOC_HANDLE(d3d11_gpu_texture);
    result->width = desc->width;
    result->height = desc->height;
    return (vgpu_texture*)result;
}

static vgpu_texture* d3d11_texture_create_external(vgpu_renderer* driver_data, const vgpu_texture_desc* desc, void* handle) {
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
    d3d11_gpu_texture* result = _VGPU_ALLOC_HANDLE(d3d11_gpu_texture);
    result->handle.resource = (ID3D11Resource*)handle;
    result->width = desc->width;
    result->height = desc->height;
    return (vgpu_texture*)result;
}

static void d3d11_texture_destroy(vgpu_renderer* driver_data, vgpu_texture* texture) {
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
    d3d11_gpu_texture* d3d11_texture = (d3d11_gpu_texture*)texture;

    ID3D11Resource_Release(d3d11_texture->handle.resource);
}

/* Sampler */
static vgpu_sampler* d3d11_sampler_create(vgpu_renderer* driver_data, const vgpu_sampler_desc* desc)
{
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
    return nullptr;
    //d3d11_gpu_sampler* result = _VGPU_ALLOC_HANDLE(d3d11_gpu_sampler);
    //return (vgpu_sampler*)result;
}

static void d3d11_sampler_destroy(vgpu_renderer* driver_data, vgpu_sampler* texture)
{
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
    //d3d11_gpu_sampler* d3d11_texture = (d3d11_gpu_sampler*)texture;
    //d3d11_texture->handle.tex2d->lpVtbl->Release(d3d11_texture->handle.tex2d);
}

/* RenderPass */
static vgpu_render_pass* d3d11_render_pass_create(vgpu_renderer* driver_data, const vgpu_render_pass_desc* desc)
{
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
    d3d11_gpu_render_pass* render_pass = _VGPU_ALLOC_HANDLE(d3d11_gpu_render_pass);
    render_pass->width = UINT32_MAX;
    render_pass->height = UINT32_MAX;

    for (uint32_t i = 0; i < VGPU_MAX_COLOR_ATTACHMENTS; i++) {
        if (!desc->color_attachments[i].texture) {
            continue;
        }

        uint32_t mip_level = desc->color_attachments[i].mip_level;
        d3d11_gpu_texture* d3d11_texture = (d3d11_gpu_texture*)desc->color_attachments[i].texture;
        render_pass->width = _vgpu_min(render_pass->width, _vgpu_max(1u, d3d11_texture->width >> mip_level));
        render_pass->height = _vgpu_min(render_pass->height, _vgpu_max(1u, d3d11_texture->height >> mip_level));

        HRESULT hr = ID3D11Device1_CreateRenderTargetView(
            renderer->d3d_device,
            d3d11_texture->handle.resource,
            nullptr,
            &render_pass->color_rtvs[render_pass->num_color_rtvs]);
        VHR(hr);
        render_pass->num_color_rtvs++;
    }

    return (vgpu_render_pass*)render_pass;
}

static void d3d11_render_pass_destroy(vgpu_renderer* driver_data, vgpu_render_pass* handle)
{
    _VGPU_UNUSED(driver_data);
    d3d11_gpu_render_pass* render_pass = (d3d11_gpu_render_pass*)handle;
    for (uint32_t i = 0; i < render_pass->num_color_rtvs; i++) {
        ID3D11RenderTargetView_Release(render_pass->color_rtvs[i]);
    }

    if (render_pass->dsv) {
        ID3D11DepthStencilView_Release(render_pass->dsv);
    }
}

static void d3d11_render_pass_get_extent(vgpu_renderer* driver_data, vgpu_render_pass* handle, uint32_t* width, uint32_t* height)
{
    d3d11_gpu_render_pass* render_pass = (d3d11_gpu_render_pass*)handle;
    if (width) {
        *width = render_pass->width;
    }

    if (height) {
        *height = render_pass->height;
    }
}

static void d3d11_cmd_begin_render_pass(vgpu_renderer* driver_data, const vgpu_begin_render_pass_desc* begin_pass_desc) {
    d3d11_renderer* renderer = (d3d11_renderer*)driver_data;
    d3d11_gpu_render_pass* render_pass = (d3d11_gpu_render_pass*)begin_pass_desc->render_pass;

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
        ID3D11DeviceContext_ClearRenderTargetView(renderer->d3d_context, render_pass->color_rtvs[i], &begin_pass_desc->color_attachments[i].r);
    }
}

static void d3d11_cmd_end_render_pass(vgpu_renderer* driver_data) {

}

vgpu_device* d3d11_create_device() {
    vgpu_device* device;
    d3d11_renderer* renderer;

    device = (vgpu_device*)VGPU_MALLOC(sizeof(vgpu_device));
    ASSIGN_DRIVER(d3d11);

    /* Init the d3d11 renderer */
    renderer = (d3d11_renderer*)VGPU_MALLOC(sizeof(d3d11_renderer));
    memset(renderer, 0, sizeof(d3d11_renderer));

    /* Reference vgpu_device and renderer together. */
    renderer->gpu_device = device;
    device->renderer = (vgpu_renderer*)renderer;

    return device;
}

bool vgpu_d3d11_supported() {
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
