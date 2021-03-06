// Copyright (c) Amer Koleci.
// Distributed under the MIT license. See the LICENSE file in the project root for more information.

#if defined(VGPU_DRIVER_D3D11)
#define D3D11_NO_HELPERS
#include <d3d11_1.h>
#include "vgpu_d3d_common.h"
#include <stdio.h>

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
    union {
        ID3D11RenderTargetView** rtvArray;
        ID3D11DepthStencilView** dsvArray;
    };
    DXGI_FORMAT dxgi_format;

    VGPUTextureType type;
    uint32_t width;
    uint32_t height;
    uint32_t depthOrArrayLayers;
    VGPUTextureFormat format;
    uint32_t mipLevels;
    uint32_t sample_count;
} D3D11Texture;

typedef struct D3D11Buffer {
    ID3D11Buffer* handle;
} D3D11Buffer;

typedef struct D3D11_ShaderModule {
    union {
        ID3D11DeviceChild* handle;
        ID3D11VertexShader* vertex;
        ID3D11PixelShader* fragment;
        ID3D11ComputeShader* compute;
    };
} D3D11_ShaderModule;

typedef struct D3D11_SwapChain {
    VGPUTextureFormat colorFormat;
    VGPUTextureFormat depthStencilFormat;
    IDXGISwapChain1* handle;
    uint32_t width;
    uint32_t height;
    VGPUTexture backbuffer;
    VGPUTexture depthStencilTexture;
} D3D11_SwapChain;


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

    VGPUDeviceCaps caps;

    bool validation;
    IDXGIFactory2* factory;
    uint32_t factory_caps;

    uint32_t sync_interval;
    uint32_t present_flags;

    ID3D11Device1* device;
    ID3D11DeviceContext1* context;
    ID3DUserDefinedAnnotation* d3d_annotation;
    D3D_FEATURE_LEVEL feature_level;
    bool is_lost;

    VGPUTextureFormat defaultDepthFormat;
    VGPUTextureFormat defaultDepthStencilFormat;

    uint32_t rtvs_count;
    ID3D11RenderTargetView* cur_rtvs[VGPU_MAX_COLOR_ATTACHMENTS];
    ID3D11DepthStencilView* cur_dsv;

    /* Data for unset */
    ID3D11RenderTargetView* zero_rtvs[VGPU_MAX_COLOR_ATTACHMENTS];

    D3D11_SwapChain swapchains[8];
} d3d11;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   define _vgpu_CreateDXGIFactory1 d3d11.dxgi.CreateDXGIFactory1
#   define _vgpu_CreateDXGIFactory2 d3d11.dxgi.CreateDXGIFactory2
#   define _vgpu_D3D11CreateDevice d3d11.D3D11CreateDevice
#else
#   define _vgpu_CreateDXGIFactory1 CreateDXGIFactory1
#   define _vgpu_CreateDXGIFactory2 CreateDXGIFactory2
#   define _vgpu_D3D11CreateDevice D3D11CreateDevice
#endif

/* Device functions */
static bool _vgpu_d3d11_createFactory(void)
{
    SAFE_RELEASE(d3d11.factory);
    HRESULT hr = S_OK;

#if defined(_DEBUG)
    bool debugDXGI = false;

    if (d3d11.validation)
    {
        IDXGIInfoQueue* dxgi_info_queue;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        if (d3d11.dxgi.DXGIGetDebugInterface1 &&
            SUCCEEDED(d3d11.dxgi.DXGIGetDebugInterface1(0, __uuidof(IDXGIInfoQueue), (void**)&dxgi_info_queue)))
#else
        if (SUCCEEDED(DXGIGetDebugInterface1(0, __uuidof(IDXGIInfoQueue), (void**)&dxgi_info_queue)))
#endif
        {
            debugDXGI = true;
            hr = _vgpu_CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, __uuidof(IDXGIFactory2), (void**)&d3d11.factory);
            if (FAILED(hr)) {
                return false;
            }

            VHR(dxgi_info_queue->SetBreakOnSeverity(D3D_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE));
            VHR(dxgi_info_queue->SetBreakOnSeverity(D3D_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE));
            VHR(dxgi_info_queue->SetBreakOnSeverity(D3D_DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, FALSE));

            DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
            {
                80 // IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides.
            };

            DXGI_INFO_QUEUE_FILTER filter;
            memset(&filter, 0, sizeof(filter));
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            dxgi_info_queue->AddStorageFilterEntries(D3D_DXGI_DEBUG_DXGI, &filter);
            dxgi_info_queue->Release();
        }
    }

    if (!debugDXGI)
#endif
    {
        hr = _vgpu_CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&d3d11.factory);
        if (FAILED(hr)) {
            return false;
        }
    }

    d3d11.factory_caps = 0;

    // Disable FLIP if not on a supporting OS
    d3d11.factory_caps |= DXGIFACTORY_CAPS_FLIP_PRESENT;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    {
        IDXGIFactory4* factory4;
        HRESULT hr = d3d11.factory->QueryInterface(__uuidof(IDXGIFactory4), (void**)&factory4);
        if (FAILED(hr))
        {
            d3d11.factory_caps &= DXGIFACTORY_CAPS_FLIP_PRESENT;
        }
        factory4->Release();
    }
#endif

    // Check tearing support.
    {
        BOOL allowTearing = FALSE;
        IDXGIFactory5* factory5;
        HRESULT hr = d3d11.factory->QueryInterface(__uuidof(IDXGIFactory5), (void**)&factory5);
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
            d3d11.factory_caps |= DXGIFACTORY_CAPS_TEARING;
        }
    }

    return true;
}
static IDXGIAdapter1* d3d11_getAdapter(bool lowPower = false)
{
    /* Detect adapter now. */
    IDXGIAdapter1* adapter = NULL;

#if defined(__dxgi1_6_h__) && defined(NTDDI_WIN10_RS4)
    {
        IDXGIFactory6* factory6;
        HRESULT hr = d3d11.factory->QueryInterface(__uuidof(IDXGIFactory6), (void**)&factory6);
        if (SUCCEEDED(hr))
        {
            // By default prefer high performance
            DXGI_GPU_PREFERENCE gpuPreference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
            if (lowPower)
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
        for (uint32_t i = 0; DXGI_ERROR_NOT_FOUND != d3d11.factory->EnumAdapters1(i, &adapter); i++)
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

static void d3d11_createSwapChain(D3D11_SwapChain* swapchain, const vgpu_swapchain_info* info);
static void d3d11_destroySwapChain(D3D11_SwapChain* swapchain);
static void d3d11_presentSwapChain(D3D11_SwapChain* swapchain);


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

static bool d3d11_init(const vgpu_info* info) {
    /* Allocate and zero out the renderer */
    d3d11.validation = (info->flags | VGPUDeviceFlags_Debug) || (info->flags | VGPUDeviceFlags_GPUBasedValidation);

    if (!_vgpu_d3d11_createFactory()) {
        return NULL;
    }

    IDXGIAdapter1* dxgi_adapter = d3d11_getAdapter(false);

    /* Setup present flags. */
    d3d11.sync_interval = info->swapchain.vsync ? 1 : 0;
    d3d11.present_flags = 0;
    if (!d3d11.sync_interval && d3d11.factory_caps & DXGIFACTORY_CAPS_TEARING)
    {
        d3d11.present_flags |= DXGI_PRESENT_ALLOW_TEARING;
    }

    /* Create d3d11 device */
    {
        UINT creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

        if (d3d11.validation && _vgpu_d3d11_sdk_layers_available())
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
                &d3d11.feature_level,
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
                &d3d11.feature_level,
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
        if (SUCCEEDED(temp_d3d_device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3d_debug)))
        {
            ID3D11InfoQueue* d3d_info_queue;
            if (SUCCEEDED(d3d_debug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3d_info_queue)))
            {
#ifdef _DEBUG
                d3d_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                d3d_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
#endif
                D3D11_MESSAGE_ID hide[] =
                {
                    D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
                };
                D3D11_INFO_QUEUE_FILTER filter;
                memset(&filter, 0, sizeof(D3D11_INFO_QUEUE_FILTER));
                filter.DenyList.NumIDs = _countof(hide);
                filter.DenyList.pIDList = hide;
                d3d_info_queue->AddStorageFilterEntries(&filter);
                d3d_info_queue->Release();
            }

            d3d_debug->Release();
        }
#endif

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        {
            IDXGIDevice1* dxgiDevice1;
            if (SUCCEEDED(temp_d3d_device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgiDevice1)))
            {
                /* Default to 3. */
                VHR(dxgiDevice1->SetMaximumFrameLatency(VGPU_MAX_INFLIGHT_FRAMES));
                dxgiDevice1->Release();
            }
        }
#endif

        VHR(temp_d3d_device->QueryInterface(__uuidof(ID3D11Device1), (void**)&d3d11.device));
        VHR(temp_d3d_context->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&d3d11.context));
        VHR(temp_d3d_context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)&d3d11.d3d_annotation));
        SAFE_RELEASE(temp_d3d_context);
        SAFE_RELEASE(temp_d3d_device);
    }

    /* Init caps first. */
    {
        DXGI_ADAPTER_DESC1 adapterDesc;
        VHR(dxgi_adapter->GetDesc1(&adapterDesc));

        /* Log some info */
        vgpu_log_info("VGPU driver: D3D11");
        vgpu_log_info("Direct3D Adapter: VID:%04X, PID:%04X - %ls", adapterDesc.VendorId, adapterDesc.DeviceId, adapterDesc.Description);

        d3d11.caps.backendType = VGPU_BACKEND_TYPE_DIRECT3D11;
        d3d11.caps.vendorId = adapterDesc.VendorId;
        d3d11.caps.adapterId = adapterDesc.DeviceId;
        _vgpu_StringConvert(adapterDesc.Description, d3d11.caps.adapterName);

        // Detect adapter type.
        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            d3d11.caps.adapterType = VGPUAdapterType_CPU;
        }
        else
        {
            d3d11.caps.adapterType = VGPUAdapterType_IntegratedGPU;
        }

        d3d11.caps.features.independentBlend = (d3d11.feature_level >= D3D_FEATURE_LEVEL_10_0);
        d3d11.caps.features.computeShader = (d3d11.feature_level >= D3D_FEATURE_LEVEL_10_0);
        d3d11.caps.features.tessellationShader = (d3d11.feature_level >= D3D_FEATURE_LEVEL_11_0);
        d3d11.caps.features.multiViewport = true;
        d3d11.caps.features.indexUInt32 = true;
        d3d11.caps.features.multiDrawIndirect = (d3d11.feature_level >= D3D_FEATURE_LEVEL_11_0);
        d3d11.caps.features.fillModeNonSolid = true;
        d3d11.caps.features.samplerAnisotropy = true;
        d3d11.caps.features.textureCompressionETC2 = false;
        d3d11.caps.features.textureCompressionASTC_LDR = false;
        d3d11.caps.features.textureCompressionBC = true;
        d3d11.caps.features.textureCubeArray = (d3d11.feature_level >= D3D_FEATURE_LEVEL_10_1);
        d3d11.caps.features.raytracing = false;

        // Limits
        d3d11.caps.limits.maxVertexAttributes = VGPU_MAX_VERTEX_ATTRIBUTES;
        d3d11.caps.limits.maxVertexBindings = VGPU_MAX_VERTEX_ATTRIBUTES;
        d3d11.caps.limits.maxVertexAttributeOffset = VGPU_MAX_VERTEX_ATTRIBUTE_OFFSET;
        d3d11.caps.limits.maxVertexBindingStride = VGPU_MAX_VERTEX_BUFFER_STRIDE;

        d3d11.caps.limits.maxTextureDimension2D = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        d3d11.caps.limits.maxTextureDimension3D = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        d3d11.caps.limits.maxTextureDimensionCube = D3D11_REQ_TEXTURECUBE_DIMENSION;
        d3d11.caps.limits.maxTextureArrayLayers = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        d3d11.caps.limits.maxColorAttachments = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
        d3d11.caps.limits.maxUniformBufferRange = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
        d3d11.caps.limits.maxStorageBufferRange = UINT32_MAX;
        d3d11.caps.limits.minUniformBufferOffsetAlignment = 256u;
        d3d11.caps.limits.minStorageBufferOffsetAlignment = 32u;
        d3d11.caps.limits.maxSamplerAnisotropy = D3D11_MAX_MAXANISOTROPY;
        d3d11.caps.limits.maxViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        d3d11.caps.limits.maxViewportWidth = D3D11_VIEWPORT_BOUNDS_MAX;
        d3d11.caps.limits.maxViewportHeight = D3D11_VIEWPORT_BOUNDS_MAX;
        d3d11.caps.limits.maxTessellationPatchSize = D3D11_IA_PATCH_MAX_CONTROL_POINT_COUNT;
        d3d11.caps.limits.maxComputeSharedMemorySize = D3D11_CS_THREAD_LOCAL_TEMP_REGISTER_POOL;
        d3d11.caps.limits.maxComputeWorkGroupCountX = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        d3d11.caps.limits.maxComputeWorkGroupCountY = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        d3d11.caps.limits.maxComputeWorkGroupCountZ = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        d3d11.caps.limits.maxComputeWorkGroupInvocations = D3D11_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;
        d3d11.caps.limits.maxComputeWorkGroupSizeX = D3D11_CS_THREAD_GROUP_MAX_X;
        d3d11.caps.limits.maxComputeWorkGroupSizeY = D3D11_CS_THREAD_GROUP_MAX_Y;
        d3d11.caps.limits.maxComputeWorkGroupSizeZ = D3D11_CS_THREAD_GROUP_MAX_Z;

        /* see: https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_format_support */
        UINT dxgiFormatCaps = 0;
        for (uint32_t format = (VGPU_TEXTURE_FORMAT_UNDEFINED + 1); format < VGPUTextureFormat_Count; format++)
        {
            DXGI_FORMAT dxgiFormat = _vgpu_to_dxgi_format((VGPUTextureFormat)format);
            if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
                continue;

            HRESULT hr = d3d11.device->CheckFormatSupport(dxgiFormat, &dxgiFormatCaps);
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
        VHR(d3d11.device->CheckFormatSupport(_vgpu_to_dxgi_format(VGPUTextureFormat_Depth32Float), &dxgiFormatCaps));
        if (dxgiFormatCaps & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)
        {
            d3d11.defaultDepthFormat = VGPUTextureFormat_Depth32Float;
        }
        else
        {
            VHR(d3d11.device->CheckFormatSupport(_vgpu_to_dxgi_format(VGPUTextureFormat_Depth16Unorm), &dxgiFormatCaps));
            if (dxgiFormatCaps & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)
            {
                d3d11.defaultDepthFormat = VGPUTextureFormat_Depth16Unorm;
            }
        }

        // Detect default depth stencil format.
        VHR(d3d11.device->CheckFormatSupport(_vgpu_to_dxgi_format(VGPUTextureFormat_Depth24UnormStencil8), &dxgiFormatCaps));
        if (dxgiFormatCaps & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)
        {
            d3d11.defaultDepthStencilFormat = VGPUTextureFormat_Depth24UnormStencil8;
        }
        else
        {
            VHR(d3d11.device->CheckFormatSupport(_vgpu_to_dxgi_format(VGPUTextureFormat_Depth32FloatStencil8), &dxgiFormatCaps));
            if (dxgiFormatCaps & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)
            {
                d3d11.defaultDepthStencilFormat = VGPUTextureFormat_Depth32FloatStencil8;
            }
        }
    }

    /* Release adapter */
    dxgi_adapter->Release();

    if (info->swapchain.window_handle)
    {
        d3d11_createSwapChain(&d3d11.swapchains[0], &info->swapchain);
    }

    return true;
}

static void d3d11_shutdown(void) {
    for (uint32_t i = 0; i < _vgpu_count_of(d3d11.swapchains); i++)
    {
        if (!d3d11.swapchains[i].handle)
            continue;

        d3d11_destroySwapChain(&d3d11.swapchains[i]);
    }

    SAFE_RELEASE(d3d11.context);
    SAFE_RELEASE(d3d11.d3d_annotation);

#if !defined(NDEBUG)
    ULONG refCount = d3d11.device->Release();
    if (refCount > 0)
    {
        vgpu_log_error("Direct3D11: There are %d unreleased references left on the device", refCount);

        ID3D11Debug* d3d11_debug = NULL;
        if (SUCCEEDED(d3d11.device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3d11_debug)))
        {
            d3d11_debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
            d3d11_debug->Release();
        }
    }
#else
    d3d11.device->Release();
#endif
    d3d11.device = nullptr;

    SAFE_RELEASE(d3d11.factory);

#ifdef _DEBUG
    IDXGIDebug1* dxgi_debug1;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    if (d3d11.dxgi.DXGIGetDebugInterface1 &&
        SUCCEEDED(d3d11.dxgi.DXGIGetDebugInterface1(0, __uuidof(IDXGIDebug1), (void**)&dxgi_debug1)))
#else
    if (SUCCEEDED(DXGIGetDebugInterface1(0, __uuidof(IDXGIDebug1), (void**)&dxgi_debug1)))
#endif
    {
        dxgi_debug1->ReportLiveObjects(D3D_DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        dxgi_debug1->Release();
    }
#endif
}

static void d3d11_get_caps(VGPUDeviceCaps* caps)
{
    *caps = d3d11.caps;
}

static VGPUTextureFormat d3d11_getDefaultDepthFormat(void)
{
    return d3d11.defaultDepthFormat;
}

static VGPUTextureFormat d3d11_getDefaultDepthStencilFormat(void)
{
    return d3d11.defaultDepthStencilFormat;
}

static vgpu_bool d3d11_frame_begin(void) {
    return true;
}

static void d3d11_frame_end(void) {
    for (uint32_t i = 0; i < _vgpu_count_of(d3d11.swapchains); i++) {
        if (!d3d11.swapchains[i].handle)
            continue;

        d3d11_presentSwapChain(&d3d11.swapchains[i]);
    }

    if (!d3d11.is_lost)
    {
        // Output information is cached on the DXGI Factory. If it is stale we need to create a new factory.
        if (!d3d11.factory->IsCurrent())
        {
            _vgpu_d3d11_createFactory();
        }
    }
}

static VGPUTexture d3d11_getBackbufferTexture(uint32_t swapchain) {
    return d3d11.swapchains[swapchain].backbuffer;
}


/* Swapchain */
static void d3d11_swapchain_update(D3D11_SwapChain* swapchain)
{
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc;
    VHR(swapchain->handle->GetDesc1(&swapchain_desc));
    swapchain->width = swapchain_desc.Width;
    swapchain->height = swapchain_desc.Height;

    // Get backbuffer
    {
        ID3D11Texture2D* backbuffer;
        VHR(swapchain->handle->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer));

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
        swapchain->backbuffer = vgpuCreateTexture(&textureDesc);

        backbuffer->Release();
    }
}

static void d3d11_createSwapChain(D3D11_SwapChain* swapchain, const vgpu_swapchain_info* info)
{
    swapchain->handle = vgpu_d3d_create_swapchain(
        d3d11.factory,
        d3d11.factory_caps,
        d3d11.device,
        info->window_handle,
        info->width, info->height,
        info->colorFormat,
        VGPU_NUM_INFLIGHT_FRAMES,
        !info->fullscreen
    );

    swapchain->colorFormat = info->colorFormat;

    d3d11_swapchain_update(swapchain);
}

/* Texture */
static ID3D11RenderTargetView* _vgpu_d3d11_createRTV(ID3D11Resource* resource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t arraySlice)
{
    D3D11_RESOURCE_DIMENSION dimension;
    resource->GetType(&dimension);

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = format;

    switch (dimension)
    {
    case D3D11_RESOURCE_DIMENSION_BUFFER:
        break;

    case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
    {
        D3D11_TEXTURE1D_DESC desc = {};
        ((ID3D11Texture1D*)resource)->GetDesc(&desc);

        if (desc.ArraySize > 1)
        {
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
            rtvDesc.Texture1DArray.MipSlice = mipSlice;
            if (arraySlice != -1)
            {
                rtvDesc.Texture1DArray.ArraySize = 1;
                rtvDesc.Texture1DArray.FirstArraySlice = arraySlice;
            }
            else
            {
                rtvDesc.Texture1DArray.ArraySize = desc.ArraySize;
            }
        }
        else
        {
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
            rtvDesc.Texture1D.MipSlice = mipSlice;
        }
    }

    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
    {
        D3D11_TEXTURE2D_DESC desc = {};
        ((ID3D11Texture2D*)resource)->GetDesc(&desc);

        if (desc.SampleDesc.Count > 1)
        {
            if (desc.ArraySize > 1)
            {
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
                if (arraySlice != -1)
                {
                    rtvDesc.Texture2DMSArray.ArraySize = 1;
                    rtvDesc.Texture2DMSArray.FirstArraySlice = arraySlice;
                }
                else
                {
                    rtvDesc.Texture2DMSArray.ArraySize = desc.ArraySize;
                }
            }
            else
            {
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
            }
        }
        else
        {
            if (desc.ArraySize > 1)
            {
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = mipSlice;
                if (arraySlice != -1)
                {
                    rtvDesc.Texture2DArray.FirstArraySlice = arraySlice;
                    rtvDesc.Texture2DArray.ArraySize = 1;
                }
                else
                {
                    rtvDesc.Texture2DArray.ArraySize = desc.ArraySize;
                }
            }
            else
            {
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Texture2D.MipSlice = mipSlice;
            }
        }
        break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
    {
        D3D11_TEXTURE3D_DESC desc = {};
        ((ID3D11Texture3D*)resource)->GetDesc(&desc);

        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
        rtvDesc.Texture3D.MipSlice = mipSlice;
        if (arraySlice != -1)
        {
            rtvDesc.Texture3D.WSize = 1;
            rtvDesc.Texture3D.FirstWSlice = arraySlice;
        }
        else
        {
            rtvDesc.Texture3D.WSize = desc.Depth;
        }
        break;
    }

    default:
        VGPU_UNREACHABLE();
        return nullptr;
    };

    ID3D11RenderTargetView* view;
    HRESULT hr = d3d11.device->CreateRenderTargetView(resource, &rtvDesc, &view);
    if (FAILED(hr))
    {
        vgpu_log_error("Direct3D11: Failed to create RTV");
        return nullptr;
    }

    return view;
}

static ID3D11DepthStencilView* _vgpu_d3d11_createDSV(ID3D11Resource* resource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t arraySlice)
{
    D3D11_RESOURCE_DIMENSION dimension;
    resource->GetType(&dimension);

    D3D11_DEPTH_STENCIL_VIEW_DESC  dsvDesc = {};
    dsvDesc.Format = format;

    switch (dimension)
    {
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
    {
        D3D11_TEXTURE1D_DESC desc = {};
        ((ID3D11Texture1D*)resource)->GetDesc(&desc);

        if (desc.ArraySize > 1)
        {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
            dsvDesc.Texture1DArray.MipSlice = mipSlice;
            if (arraySlice != -1)
            {
                dsvDesc.Texture1DArray.ArraySize = 1;
                dsvDesc.Texture1DArray.FirstArraySlice = arraySlice;
            }
            else
            {
                dsvDesc.Texture1DArray.ArraySize = desc.ArraySize;
            }
        }
        else
        {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
            dsvDesc.Texture1D.MipSlice = mipSlice;
        }
    }

    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
    {
        D3D11_TEXTURE2D_DESC desc = {};
        ((ID3D11Texture2D*)resource)->GetDesc(&desc);

        if (desc.SampleDesc.Count > 1)
        {
            if (desc.ArraySize > 1)
            {
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
                if (arraySlice != -1)
                {
                    dsvDesc.Texture2DMSArray.ArraySize = 1;
                    dsvDesc.Texture2DMSArray.FirstArraySlice = arraySlice;
                }
                else
                {
                    dsvDesc.Texture2DMSArray.ArraySize = desc.ArraySize;
                }
            }
            else
            {
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
            }
        }
        else
        {
            if (desc.ArraySize > 1)
            {
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                dsvDesc.Texture2DArray.MipSlice = mipSlice;
                if (arraySlice != -1)
                {
                    dsvDesc.Texture2DArray.FirstArraySlice = arraySlice;
                    dsvDesc.Texture2DArray.ArraySize = 1;
                }
                else
                {
                    dsvDesc.Texture2DArray.ArraySize = desc.ArraySize;
                }
            }
            else
            {
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                dsvDesc.Texture2D.MipSlice = mipSlice;
            }
        }
        break;
    }

    case D3D11_RESOURCE_DIMENSION_BUFFER:
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        vgpu_log_error("Cannot create DepthStencilView for 3D texture");
        VGPU_UNREACHABLE();
        return nullptr;

    default:
        VGPU_UNREACHABLE();
        return nullptr;
    };

    ID3D11DepthStencilView* view;
    HRESULT hr = d3d11.device->CreateDepthStencilView(resource, &dsvDesc, &view);
    if (FAILED(hr))
    {
        vgpu_log_error("Direct3D11: Failed to create DSV");
        return nullptr;
    }

    return view;
}

static VGPUTexture d3d11_createTexture(const VGPUTextureDescription* desc)
{
    D3D11Texture* texture = VGPU_ALLOC_HANDLE(D3D11Texture);
    texture->type = desc->type;
    texture->width = desc->width;
    texture->height = desc->height;
    texture->depthOrArrayLayers = desc->depth;
    texture->format = desc->format;
    texture->mipLevels = desc->mipLevelCount;
    texture->sample_count = desc->sampleCount;

    texture->dxgi_format = _vgpu_d3d_format_with_usage(desc->format, desc->usage);

    HRESULT hr = S_OK;
    if (desc->externalHandle)
    {
        texture->handle = (ID3D11Resource*)desc->externalHandle;
        texture->handle->AddRef();
    }
    else
    {
        D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
        UINT bindFlags = 0;
        UINT CPUAccessFlags = 0;
        UINT miscFlags = 0;
        UINT arraySizeMultiplier = 1;
        if (desc->type == VGPU_TEXTURE_TYPE_CUBE) {
            arraySizeMultiplier = 6;
            miscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
        }

        if (desc->usage & VGPUTextureUsage_Sampled)
        {
            bindFlags |= D3D11_BIND_SHADER_RESOURCE;
        }

        if (desc->usage & VGPUTextureUsage_Storage)
        {
            bindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        }

        if (desc->usage & VGPUTextureUsage_RenderTarget)
        {
            if (vgpu_is_depth_stencil_format(desc->format))
            {
                bindFlags |= D3D11_BIND_DEPTH_STENCIL;
            }
            else
            {
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
            D3D11_TEXTURE2D_DESC d3d11_desc = {};
            d3d11_desc.Width = desc->width;
            d3d11_desc.Height = desc->height;
            d3d11_desc.MipLevels = desc->mipLevelCount;
            d3d11_desc.ArraySize = desc->depth * arraySizeMultiplier;
            d3d11_desc.Format = texture->dxgi_format;
            d3d11_desc.SampleDesc.Count = 1;
            d3d11_desc.SampleDesc.Quality = 0;
            d3d11_desc.Usage = usage;
            d3d11_desc.BindFlags = bindFlags;
            d3d11_desc.CPUAccessFlags = CPUAccessFlags;
            d3d11_desc.MiscFlags = miscFlags;

            hr = d3d11.device->CreateTexture2D(
                &d3d11_desc,
                NULL,
                (ID3D11Texture2D**)&texture->handle
            );
        }
        else if (desc->type == VGPU_TEXTURE_TYPE_3D) {
            D3D11_TEXTURE3D_DESC d3d11_desc = {};
            d3d11_desc.Width = desc->width;
            d3d11_desc.Height = desc->height;
            d3d11_desc.Depth = desc->depth;
            d3d11_desc.MipLevels = desc->mipLevelCount;
            d3d11_desc.Format = texture->dxgi_format;
            d3d11_desc.Usage = usage;
            d3d11_desc.BindFlags = bindFlags;
            d3d11_desc.CPUAccessFlags = CPUAccessFlags;
            d3d11_desc.MiscFlags = miscFlags;

            hr = d3d11.device->CreateTexture3D(
                &d3d11_desc,
                NULL,
                (ID3D11Texture3D**)&texture->handle
            );
        }
    }

    if (desc->usage & VGPUTextureUsage_RenderTarget)
    {
        if (vgpu_is_depth_stencil_format(desc->format))
        {
            texture->dsv = _vgpu_d3d11_createDSV(texture->handle, texture->dxgi_format, 0, -1);
        }
        else
        {
            texture->rtv = _vgpu_d3d11_createRTV(texture->handle, texture->dxgi_format, 0, -1);
        }
    }

    return (VGPUTexture)texture;
}

static void d3d11_destroyTexture(VGPUTexture handle)
{
    D3D11Texture* texture = (D3D11Texture*)handle;
    SAFE_RELEASE(texture->handle);
    SAFE_RELEASE(texture->rtv);
    VGPU_FREE(texture);
}

/* Buffer */
static VGPUBuffer d3d11_createBuffer(const VGPUBufferDescriptor* descriptor)
{
    /* Verify resource limits first. */
    static constexpr uint64_t c_maxBytes = D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_A_TERM * 1024u * 1024u;
    static_assert(c_maxBytes <= UINT32_MAX, "Exceeded integer limits");

    uint64_t sizeInBytes = descriptor->size;

    if (sizeInBytes > c_maxBytes)
    {
        //vgpu_log_error("Direct3D11: Resource size too large for DirectX 11 (size {})", sizeInBytes);
        return nullptr;
    }

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(sizeInBytes);
    if (descriptor->usage & VGPUBufferUsage_Uniform)
    {
        // This cannot be combined with nothing else.
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    }
    else
    {
        if (descriptor->usage & VGPUBufferUsage_Dynamic)
        {
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        }
        else if (descriptor->usage & VGPUBufferUsage_Staging)
        {
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
        }
        else
        {
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.CPUAccessFlags = 0;
        }

        if (descriptor->usage & VGPUBufferUsage_Vertex)
            desc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
        if (descriptor->usage & VGPUBufferUsage_Index)
            desc.BindFlags |= D3D11_BIND_INDEX_BUFFER;

        if (descriptor->usage & VGPUBufferUsage_Storage)
        {
            desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
            desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        }

        if (descriptor->usage & VGPUBufferUsage_Indirect)
        {
            desc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
        }
    }

    desc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA* initialDataPtr = nullptr;
    D3D11_SUBRESOURCE_DATA initialResourceData = {};
    if (descriptor->content != nullptr)
    {
        initialResourceData.pSysMem = descriptor->content;
        initialDataPtr = &initialResourceData;
    }

    ID3D11Buffer* handle;
    HRESULT hr = d3d11.device->CreateBuffer(&desc, initialDataPtr, &handle);
    if (FAILED(hr))
    {
        vgpu_log_error("Direct3D11: Failed to create buffer");
        return nullptr;
    }

    D3D11Buffer* buffer = VGPU_ALLOC_HANDLE(D3D11Buffer);
    buffer->handle = handle;
    return (VGPUBuffer)buffer;
}

static void d3d11_destroyBuffer(VGPUBuffer handle)
{
    D3D11Buffer* buffer = (D3D11Buffer*)handle;
    SAFE_RELEASE(buffer->handle);
    VGPU_FREE(buffer);
}

/* Sampler */
static VGPUSampler d3d11_createSampler(const VGPUSamplerDescriptor* desc)
{
    return nullptr;
}

static void d3d11_destroySampler(VGPUSampler handle)
{

}

/* Shader */
static VGPUShaderModule d3d11_createShaderModule(const VGPUShaderModuleDescriptor* desc)
{
    D3D11_ShaderModule* shader = VGPU_ALLOC_HANDLE(D3D11_ShaderModule);

    HRESULT hr = S_OK;
    switch (desc->stage)
    {
    case VGPUShaderStage_Vertex:
        hr = d3d11.device->CreateVertexShader(desc->code, desc->size, nullptr, &shader->vertex);
        break;

    case VGPUShaderStage_Fragment:
        hr = d3d11.device->CreatePixelShader(desc->code, desc->size, nullptr, &shader->fragment);
        break;

    default:
        VGPU_UNREACHABLE();
        break;
    }

    if (FAILED(hr))
    {
        vgpu_log_error("Direct3D11: Failed to create shader module");
        VGPU_FREE(shader);
        return nullptr;
    }

    return (VGPUShaderModule)shader;
}

static void d3d11_destroyShaderModule(VGPUShaderModule handle)
{
    D3D11_ShaderModule* shader = (D3D11_ShaderModule*)handle;
    SAFE_RELEASE(shader->handle);
    VGPU_FREE(shader);
}

/* SwapChain */
static void d3d11_destroySwapChain(D3D11_SwapChain* swapchain)
{
    vgpuDestroyTexture(swapchain->backbuffer);
    if (swapchain->depthStencilTexture) {
        vgpuDestroyTexture(swapchain->depthStencilTexture);
    }
    SAFE_RELEASE(swapchain->handle);
}

static void d3d11_presentSwapChain(D3D11_SwapChain* swapchain)
{
    HRESULT hr = swapchain->handle->Present(d3d11.sync_interval, d3d11.present_flags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED
        || hr == DXGI_ERROR_DEVICE_HUNG
        || hr == DXGI_ERROR_DEVICE_RESET
        || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR
        || hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
    {
        d3d11.is_lost = true;
    }
}

/* Commands */
static void d3d11_cmdBeginRenderPass(const VGPURenderPassDescriptor* desc)
{
    d3d11.rtvs_count = 0;
    for (uint32_t i = 0; i < VGPU_MAX_COLOR_ATTACHMENTS; i++)
    {
        if (!desc->colorAttachments[i].texture)
            continue;

        D3D11Texture* texture = (D3D11Texture*)desc->colorAttachments[i].texture;

        if (desc->colorAttachments[i].loadOp == VGPULoadOp_Clear)
        {
            d3d11.context->ClearRenderTargetView(texture->rtv, &desc->colorAttachments[i].clearColor.r);
        }

        d3d11.cur_rtvs[d3d11.rtvs_count++] = texture->rtv;
    }

    if (desc->depthStencilAttachment.texture)
    {
        D3D11Texture* texture = (D3D11Texture*)desc->depthStencilAttachment.texture;
        d3d11.cur_dsv = texture->dsv;

        const bool hasDepth = vgpu_is_depth_format(texture->format);
        const bool hasStencil = vgpu_is_stencil_format(texture->format);

        UINT clearFlags = 0;
        if (hasDepth && desc->depthStencilAttachment.depthLoadOp == VGPULoadOp_Clear)
        {
            clearFlags |= D3D11_CLEAR_DEPTH;
        }

        if (hasStencil && desc->depthStencilAttachment.stencilLoadOp == VGPULoadOp_Clear)
        {
            clearFlags |= D3D11_CLEAR_STENCIL;
        }

        d3d11.context->ClearDepthStencilView(
            texture->dsv,
            clearFlags,
            desc->depthStencilAttachment.clearDepth,
            static_cast<uint8_t>(desc->depthStencilAttachment.clearStencil)
        );
    }

    d3d11.context->OMSetRenderTargets(d3d11.rtvs_count, d3d11.cur_rtvs, d3d11.cur_dsv);
}

static void d3d11_cmdEndRenderPass(void)
{
    d3d11.context->OMSetRenderTargets(VGPU_MAX_COLOR_ATTACHMENTS, d3d11.zero_rtvs, nullptr);

    for (uint32_t i = 0; i < d3d11.rtvs_count; i++) {
        d3d11.cur_rtvs[i] = nullptr;
    }

    d3d11.rtvs_count = 0;
    d3d11.cur_dsv = nullptr;
}

/* Driver functions */
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

static VGPUDevice* d3d11_createDevice(VGPUDeviceFlags flags) {
    VGPUDevice* device = new VGPUDevice();
    //ASSIGN_DRIVER(d3d11);
    return device;
}

VGPU_Driver D3D11_Driver = {
    VGPU_BACKEND_TYPE_DIRECT3D11,
    d3d11_is_supported,
    d3d11_createDevice
};

#endif /* defined(VGPU_DRIVER_D3D11)  */
