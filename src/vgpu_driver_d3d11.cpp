// Copyright � Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGPU_D3D11_DRIVER)

#include "vgpu_driver_d3d.h"
#define D3D11_NO_HELPERS
#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <sstream>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

namespace
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    using PFN_CREATE_DXGI_FACTORY2 = decltype(&CreateDXGIFactory2);
    static PFN_CREATE_DXGI_FACTORY2 vgfxCreateDXGIFactory2 = nullptr;

    static PFN_D3D11_CREATE_DEVICE vgfxD3D11CreateDevice = nullptr;

#if defined(_DEBUG)
    using PFN_DXGI_GET_DEBUG_INTERFACE1 = decltype(&DXGIGetDebugInterface1);
    static PFN_DXGI_GET_DEBUG_INTERFACE1 vgfxDXGIGetDebugInterface1 = nullptr;
#endif
#endif

    static constexpr D3D_FEATURE_LEVEL s_featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    // Check for SDK Layer support.
    inline bool SdkLayersAvailable() noexcept
    {
        HRESULT hr = vgfxD3D11CreateDevice(
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

    // Declare custom object of "WKPDID_D3DDebugObjectName" as defined in <d3dcommon.h> to avoid linking with "dxguid.lib"
    static const GUID g_WKPDID_D3DDebugObjectName = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87,0x42,0xac,0xb0,0xbf,0x85,0xc2,0x00 } };

    inline void D3D11SetName(ID3D11DeviceChild* obj, const char* name)
    {
        if (obj)
        {
            if (name != nullptr)
            {
                obj->SetPrivateData(g_WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
            }
            else
            {
                obj->SetPrivateData(g_WKPDID_D3DDebugObjectName, 0, nullptr);
            }
        }
    }
}

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define vgfxCreateDXGIFactory2 CreateDXGIFactory2
#define vgfxD3D11CreateDevice D3D11CreateDevice
#endif


struct VGFXD3D11Buffer
{
    ID3D11Buffer* handle = nullptr;
};

struct VGFXD3D11Texture
{
    ID3D11Resource* handle = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN;
    std::unordered_map<size_t, ID3D11RenderTargetView*> rtvCache;
    std::unordered_map<size_t, ID3D11DepthStencilView*> dsvCache;
};

struct VGFXD3D11SwapChain
{
    IDXGISwapChain1* handle = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    bool vsync = false;
    VGFXTexture backbufferTexture;
};

struct VGFXD3D11Renderer
{
    ComPtr<IDXGIFactory2> factory;
    bool tearingSupported = false;

    uint32_t vendorID;
    uint32_t deviceID;
    std::string adapterName;
    std::string driverDescription;
    VGFXAdapterType adapterType;

    ID3D11Device1* device = nullptr;
    ID3D11DeviceContext1* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel{};

    // Current frame present swap chains
    std::vector<VGFXD3D11SwapChain*> swapChains;
};

static ID3D11RenderTargetView* d3d11_GetRTV(
    VGFXD3D11Renderer* renderer,
    VGFXD3D11Texture* texture,
    DXGI_FORMAT format,
    uint32_t mipLevel,
    uint32_t slice)
{
    if (format == DXGI_FORMAT_UNKNOWN)
    {
        format = texture->viewFormat;
    }

    size_t hash = 0;
    hash_combine(hash, format);
    hash_combine(hash, mipLevel);
    hash_combine(hash, slice);

    auto it = texture->rtvCache.find(hash);
    if (it == texture->rtvCache.end())
    {
        D3D11_RESOURCE_DIMENSION type;
        texture->handle->GetType(&type);

        D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
        viewDesc.Format = format;

        switch (type)
        {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            {
                D3D11_TEXTURE1D_DESC desc1d;
                ((ID3D11Texture1D*)texture->handle)->GetDesc(&desc1d);

                if (desc1d.ArraySize > 1)
                {
                    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
                    viewDesc.Texture1DArray.MipSlice = mipLevel;
                    viewDesc.Texture1DArray.FirstArraySlice = slice;
                    viewDesc.Texture1DArray.ArraySize = 1;
                }
                else
                {
                    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1D.MipSlice = mipLevel;
                }
            }
            break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            {
                D3D11_TEXTURE2D_DESC desc2d;
                ((ID3D11Texture2D*)texture->handle)->GetDesc(&desc2d);

                if (desc2d.ArraySize > 1)
                {
                    if (desc2d.SampleDesc.Count > 1)
                    {
                        viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
                        viewDesc.Texture2DMSArray.FirstArraySlice = slice;
                        viewDesc.Texture2DMSArray.ArraySize = 1;
                    }
                    else
                    {
                        viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                        viewDesc.Texture2DArray.MipSlice = mipLevel;
                        viewDesc.Texture2DArray.FirstArraySlice = slice;
                        viewDesc.Texture2DArray.ArraySize = 1;
                    }
                }
                else
                {
                    if (desc2d.SampleDesc.Count > 1)
                    {
                        viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
                    }
                    else
                    {
                        viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                        viewDesc.Texture2D.MipSlice = mipLevel;
                    }
                }
            }
            break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
            {
                D3D11_TEXTURE3D_DESC desc3d;
                ((ID3D11Texture3D*)texture->handle)->GetDesc(&desc3d);

                viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.MipSlice = mipLevel;
                viewDesc.Texture3D.FirstWSlice = slice;
                viewDesc.Texture3D.WSize = -1;
                break;
            }
            break;

            default:
                vgfxLogError("D3D11: Invalid texture dimension");
                return nullptr;
        }

        ID3D11RenderTargetView* newView = nullptr;
        const HRESULT hr = renderer->device->CreateRenderTargetView(texture->handle, &viewDesc, &newView);
        if (FAILED(hr))
        {
            vgfxLogError("D3D11: Failed to create RenderTargetView");
            return nullptr;
        }

        texture->rtvCache[hash] = newView;
        return newView;
    }

    return it->second;
}

static ID3D11DepthStencilView* d3d11_GetDSV(
    VGFXD3D11Renderer* renderer,
    VGFXD3D11Texture* texture,
    DXGI_FORMAT format,
    uint32_t mipLevel,
    uint32_t slice)
{
    if (format == DXGI_FORMAT_UNKNOWN)
    {
        format = texture->viewFormat;
    }

    size_t hash = 0;
    hash_combine(hash, mipLevel);
    hash_combine(hash, slice);
    hash_combine(hash, format);

    auto it = texture->dsvCache.find(hash);
    if (it == texture->dsvCache.end())
    {
        D3D11_RESOURCE_DIMENSION type;
        texture->handle->GetType(&type);

        D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
        viewDesc.Format = format;

        switch (type)
        {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            {
                D3D11_TEXTURE1D_DESC desc1d;
                ((ID3D11Texture1D*)texture->handle)->GetDesc(&desc1d);

                if (desc1d.ArraySize > 1)
                {
                    viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
                    viewDesc.Texture1DArray.MipSlice = mipLevel;
                    viewDesc.Texture1DArray.FirstArraySlice = slice;
                    viewDesc.Texture1DArray.ArraySize = 1;
                }
                else
                {
                    viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1D.MipSlice = mipLevel;
                }
            }
            break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            {
                D3D11_TEXTURE2D_DESC desc2d;
                ((ID3D11Texture2D*)texture->handle)->GetDesc(&desc2d);

                if (desc2d.ArraySize > 1)
                {
                    if (desc2d.SampleDesc.Count > 1)
                    {
                        viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
                        viewDesc.Texture2DMSArray.FirstArraySlice = slice;
                        viewDesc.Texture2DMSArray.ArraySize = 1;
                    }
                    else
                    {
                        viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                        viewDesc.Texture2DArray.MipSlice = mipLevel;
                        viewDesc.Texture2DArray.FirstArraySlice = slice;
                        viewDesc.Texture2DArray.ArraySize = 1;
                    }
                }
                else
                {
                    if (desc2d.SampleDesc.Count > 1)
                    {
                        viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
                    }
                    else
                    {
                        viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                        viewDesc.Texture2D.MipSlice = mipLevel;
                    }
                }
            }
            break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                vgfxLogError("D3D11: Cannot create 3D texture DSV");
                return nullptr;

            default:
                vgfxLogError("D3D11: Invalid texture dimension");
                return nullptr;
        }

        ID3D11DepthStencilView* newView = nullptr;
        const HRESULT hr = renderer->device->CreateDepthStencilView(texture->handle, &viewDesc, &newView);
        if (FAILED(hr))
        {
            vgfxLogError("D3D11: Failed to create DepthStencilView");
            return nullptr;
        }

        texture->dsvCache[hash] = newView;
        return newView;
    }

    return it->second;
}

static void d3d11_destroyDevice(VGFXDevice device)
{
    VGFXD3D11Renderer* renderer = (VGFXD3D11Renderer*)device->driverData;

    SafeRelease(renderer->context);

    if (renderer->device)
    {
        const ULONG refCount = renderer->device->Release();

#if defined(_DEBUG)
        if (refCount)
        {
            //DebugString("There are %d unreleased references left on the D3D device!", ref_count);

            ComPtr<ID3D11Debug> d3d11Debug;
            if (SUCCEEDED(renderer->device->QueryInterface(d3d11Debug.GetAddressOf())))
            {
                d3d11Debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
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

static void d3d11_frame(VGFXRenderer* driverData)
{
    HRESULT hr = S_OK;
    VGFXD3D11Renderer* renderer = (VGFXD3D11Renderer*)driverData;

    // Present acquired SwapChains
    for (size_t i = 0, count = renderer->swapChains.size(); i < count && SUCCEEDED(hr); ++i)
    {
        VGFXD3D11SwapChain* swapChain = renderer->swapChains[i];

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
            return;
        }

    }
}

static void d3d11_waitIdle(VGFXRenderer* driverData)
{
    VGFXD3D11Renderer* renderer = (VGFXD3D11Renderer*)driverData;
    renderer->context->Flush();
}

static bool d3d11_hasFeature(VGFXRenderer* driverData, VGPUFeature feature) {
    VGFXD3D11Renderer* renderer = (VGFXD3D11Renderer*)driverData;
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

static void d3d11_getAdapterProperties(VGFXRenderer* driverData, VGPUAdapterProperties* properties)
{
    VGFXD3D11Renderer* renderer = (VGFXD3D11Renderer*)driverData;

    properties->vendorID = renderer->vendorID;
    properties->deviceID = renderer->deviceID;
    properties->name = renderer->adapterName.c_str();
    properties->driverDescription = renderer->driverDescription.c_str();
    properties->adapterType = renderer->adapterType;
    properties->backendType = VGFXBackendType_D3D11;
}

static void d3d11_getLimits(VGFXRenderer* driverData, VGPULimits* limits)
{
    VGFXD3D11Renderer* renderer = (VGFXD3D11Renderer*)driverData;

    D3D11_FEATURE_DATA_D3D11_OPTIONS featureData = {};
    HRESULT hr = renderer->device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &featureData, sizeof(featureData));
    if (FAILED(hr))
    {
    }

    uint32_t maxCBVsPerStage = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
    uint32_t maxSRVsPerStage = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
    uint32_t maxUAVsPerStage = D3D11_1_UAV_SLOT_COUNT;
    uint32_t maxSamplersPerStage = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;

    limits->maxTextureDimension1D = D3D11_REQ_TEXTURE1D_U_DIMENSION;
    limits->maxTextureDimension2D = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    limits->maxTextureDimension3D = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
    limits->maxTextureArrayLayers = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    limits->maxBindGroups = 0;
    limits->maxDynamicUniformBuffersPerPipelineLayout = 0;
    limits->maxDynamicStorageBuffersPerPipelineLayout = 0;
    limits->maxSampledTexturesPerShaderStage = maxSRVsPerStage;
    limits->maxSamplersPerShaderStage = maxSamplersPerStage;
    limits->maxStorageBuffersPerShaderStage = maxUAVsPerStage - maxUAVsPerStage / 2;
    limits->maxStorageTexturesPerShaderStage = maxUAVsPerStage / 2;
    limits->maxUniformBuffersPerShaderStage = maxCBVsPerStage;
    limits->maxUniformBufferBindingSize = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
    // D3D12 has no documented limit on the size of a storage buffer binding.
    limits->maxStorageBufferBindingSize = 4294967295;
    limits->minUniformBufferOffsetAlignment = 256;
    limits->minStorageBufferOffsetAlignment = 32;
    limits->maxVertexBuffers = 16;
    limits->maxVertexAttributes = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
    limits->maxVertexBufferArrayStride = 2048u;
    limits->maxInterStageShaderComponents = D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENTS_COMPONENTS;

    // https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-compute-shaders
    // Thread Group Shared Memory is limited to 16Kb on downlevel hardware. This is less than
    // the 32Kb that is available to Direct3D 11 hardware. D3D12 is also 32kb.
    limits->maxComputeWorkgroupStorageSize = 32768;

    // https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-attributes-numthreads
    limits->maxComputeInvocationsPerWorkGroup = D3D11_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;
    limits->maxComputeWorkGroupSizeX = D3D11_CS_THREAD_GROUP_MAX_X;
    limits->maxComputeWorkGroupSizeY = D3D11_CS_THREAD_GROUP_MAX_X;
    limits->maxComputeWorkGroupSizeZ = D3D11_CS_THREAD_GROUP_MAX_X;

    // https://docs.maxComputeWorkgroupSizeXmicrosoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_dispatch_arguments
    limits->maxComputeWorkGroupsPerDimension = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
}

/* Buffer */
static VGFXBuffer d3d11_createBuffer(VGFXRenderer* driverData, const VGFXBufferDesc* desc, const void* pInitialData)
{
    VGFXD3D11Renderer* renderer = (VGFXD3D11Renderer*)driverData;

    D3D11_BUFFER_DESC d3d_desc;
    d3d_desc.ByteWidth = (uint32_t)desc->size;
    d3d_desc.Usage = D3D11_USAGE_DEFAULT;
    d3d_desc.BindFlags = 0;
    d3d_desc.CPUAccessFlags = 0;
    d3d_desc.MiscFlags = 0;
    d3d_desc.StructureByteStride = 0;

    if (desc->usage & VGFXBufferUsage_Uniform)
    {
        d3d_desc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
        d3d_desc.Usage = D3D11_USAGE_DYNAMIC;
        d3d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    }
    else
    {
        if (desc->usage & VGFXBufferUsage_Vertex)
        {
            d3d_desc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
        }

        if (desc->usage & VGFXBufferUsage_Index)
        {
            d3d_desc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
        }

        // TODO: understand D3D11_RESOURCE_MISC_BUFFER_STRUCTURED usage
        if (desc->usage & VGFXBufferUsage_ShaderRead)
        {
            d3d_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            d3d_desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        }
        if (desc->usage & VGFXBufferUsage_ShaderWrite)
        {
            d3d_desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
            d3d_desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        }

        if (desc->usage & VGFXBufferUsage_Indirect)
        {
            d3d_desc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
        }
    }

    // If a resource array was provided for the resource, create the resource pre-populated
    D3D11_SUBRESOURCE_DATA initialData;
    D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
    if (pInitialData != nullptr)
    {
        initialData.pSysMem = pInitialData;
        initialData.SysMemPitch = (UINT)desc->size;
        initialData.SysMemSlicePitch = 0;
        pInitData = &initialData;
    }

    VGFXD3D11Buffer* buffer = new VGFXD3D11Buffer();

    HRESULT hr = renderer->device->CreateBuffer(&d3d_desc, pInitData, &buffer->handle);

    if (FAILED(hr))
    {
        vgfxLogError("D3D11: Failed to create buffer");
        delete buffer;
        return nullptr;
    }


    if (desc->label)
    {
        D3D11SetName(buffer->handle, desc->label);
    }

    return (VGFXBuffer)buffer;
}

static void d3d11_destroyBuffer(VGFXRenderer* driverData, VGFXBuffer resource)
{
    VGFXD3D11Buffer* d3dBuffer = (VGFXD3D11Buffer*)resource;
    SafeRelease(d3dBuffer->handle);
    delete d3dBuffer;
}

/* Texture */
static VGFXTexture d3d11_createTexture(VGFXRenderer* driverData, const VGFXTextureDesc* desc)
{
    VGFXD3D11Renderer* renderer = (VGFXD3D11Renderer*)driverData;

    D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
    UINT bindFlags = 0;
    UINT cpuAccessFlags = 0u;
    DXGI_FORMAT format = ToDXGIFormat(desc->format);

    if (desc->usage & VGFXTextureUsage_ShaderRead)
    {
        bindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }

    if (desc->usage & VGFXTextureUsage_ShaderWrite)
    {
        bindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    if (desc->usage & VGFXTextureUsage_RenderTarget)
    {
        if (vgfxIsDepthStencilFormat(desc->format))
        {
            bindFlags |= D3D11_BIND_DEPTH_STENCIL;
        }
        else
        {
            bindFlags |= D3D11_BIND_RENDER_TARGET;
        }
    }

    // If shader read/write and depth format, set to typeless
    if (vgfxIsDepthFormat(desc->format) && desc->usage & (VGFXTextureUsage_ShaderRead | VGFXTextureUsage_ShaderWrite))
    {
        format = GetTypelessFormatFromDepthFormat(desc->format);
    }

    VGFXD3D11Texture* texture = new VGFXD3D11Texture();
    texture->width = desc->width;
    texture->height = desc->height;
    texture->format = format;
    texture->viewFormat = ToDXGIFormat(desc->format);

    HRESULT hr = E_FAIL;
    if (desc->type == VGFXTextureType3D)
    {
        D3D11_TEXTURE3D_DESC d3d_desc = {};
        d3d_desc.Width = desc->width;
        d3d_desc.Height = desc->height;
        d3d_desc.Depth = desc->depthOrArraySize;
        d3d_desc.MipLevels = desc->mipLevelCount;
        d3d_desc.Format = format;
        d3d_desc.Usage = usage;
        d3d_desc.BindFlags = bindFlags;
        d3d_desc.CPUAccessFlags = cpuAccessFlags;

        hr = renderer->device->CreateTexture3D(&d3d_desc, nullptr, (ID3D11Texture3D**)&texture->handle);
    }
    else
    {
        D3D11_TEXTURE2D_DESC d3d_desc = {};
        d3d_desc.Width = desc->width;
        d3d_desc.Height = desc->height;
        d3d_desc.MipLevels = desc->mipLevelCount;
        d3d_desc.ArraySize = desc->depthOrArraySize;
        d3d_desc.Format = format;
        d3d_desc.SampleDesc.Count = desc->sampleCount;
        d3d_desc.Usage = usage;
        d3d_desc.BindFlags = bindFlags;
        d3d_desc.CPUAccessFlags = cpuAccessFlags;

        if (desc->width == desc->height && (desc->depthOrArraySize % 6 == 0))
        {
            d3d_desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        }

        hr = renderer->device->CreateTexture2D(&d3d_desc, nullptr, (ID3D11Texture2D**)&texture->handle);
    }

    if (FAILED(hr))
    {
        vgfxLogError("D3D11: Failed to create texture");
        delete texture;
        return nullptr;
    }

    if (desc->label)
    {
        D3D11SetName(texture->handle, desc->label);
    }

    return (VGFXTexture)texture;
}

static void d3d11_destroyTexture(VGFXRenderer*, VGFXTexture texture)
{
    VGFXD3D11Texture* d3d11Texture = (VGFXD3D11Texture*)texture;
    for (auto& it : d3d11Texture->rtvCache)
    {
        SafeRelease(it.second);
    }
    d3d11Texture->rtvCache.clear();
    for (auto& it : d3d11Texture->dsvCache)
    {
        SafeRelease(it.second);
    }
    d3d11Texture->dsvCache.clear();
    SafeRelease(d3d11Texture->handle);
    delete d3d11Texture;
}

/* SwapChain */
static void d3d11_updateSwapChain(VGFXD3D11Renderer* renderer, VGFXD3D11SwapChain* swapChain)
{
    HRESULT hr = S_OK;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    hr = swapChain->handle->GetDesc1(&swapChainDesc);
    VGFX_ASSERT(SUCCEEDED(hr));

    swapChain->width = swapChainDesc.Width;
    swapChain->height = swapChainDesc.Height;

    VGFXD3D11Texture* texture = new VGFXD3D11Texture();
    texture->width = swapChainDesc.Width;
    texture->height = swapChainDesc.Height;
    // TODO: Handle srgb
    texture->format = swapChainDesc.Format;
    texture->viewFormat = swapChainDesc.Format;

    hr = swapChain->handle->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&texture->handle);
    VGFX_ASSERT(SUCCEEDED(hr));
    swapChain->backbufferTexture = (VGFXTexture)texture;
}

static VGFXSwapChain d3d11_createSwapChain(VGFXRenderer* driverData, VGFXSurface surface, const VGFXSwapChainDesc* info)
{
    VGFXD3D11Renderer* renderer = (VGFXD3D11Renderer*)driverData;

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

    IDXGISwapChain1* handle = nullptr;
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = TRUE;

    // Create a swap chain for the window.
    HRESULT hr = renderer->factory->CreateSwapChainForHwnd(
        renderer->device,
        surface->window,
        &swapChainDesc,
        &fsSwapChainDesc,
        nullptr,
        &handle
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
        renderer->device,
        surface->window,
        &swapChainDesc,
        nullptr,
        &handle
    );
#endif

    VGFXD3D11SwapChain* swapChain = new VGFXD3D11SwapChain();
    swapChain->handle = handle;
    swapChain->vsync = info->presentMode == VGFXPresentMode_Fifo;
    d3d11_updateSwapChain(renderer, swapChain);
    return (VGFXSwapChain)swapChain;
}

static void d3d11_destroySwapChain(VGFXRenderer* driverData, VGFXSwapChain swapChain)
{
    _VGFX_UNUSED(driverData);
    VGFXD3D11SwapChain* d3dSwapChain = (VGFXD3D11SwapChain*)swapChain;

    d3d11_destroyTexture(nullptr, d3dSwapChain->backbufferTexture);
    d3dSwapChain->backbufferTexture = nullptr;
    d3dSwapChain->handle->Release();

    delete d3dSwapChain;
}

static void d3d11_getSwapChainSize(VGFXRenderer*, VGFXSwapChain swapChain, VGFXSize2D* pSize)
{
    VGFXD3D11SwapChain* d3dSwapChain = (VGFXD3D11SwapChain*)swapChain;
    pSize->width = d3dSwapChain->width;
    pSize->height = d3dSwapChain->height;
}

static VGFXTexture d3d11_acquireNextTexture(VGFXRenderer* driverData, VGFXSwapChain swapChain)
{
    VGFXD3D11Renderer* renderer = (VGFXD3D11Renderer*)driverData;
    VGFXD3D11SwapChain* d3dSwapChain = (VGFXD3D11SwapChain*)swapChain;

    renderer->swapChains.push_back(d3dSwapChain);
    return d3dSwapChain->backbufferTexture;
}

static void d3d11_beginRenderPass(VGFXRenderer* driverData, const VGFXRenderPassDesc* info)
{
    VGFXD3D11Renderer* renderer = (VGFXD3D11Renderer*)driverData;

    uint32_t width = UINT32_MAX;
    uint32_t height = UINT32_MAX;
    uint32_t numRTVs = 0;
    ID3D11RenderTargetView* RTVs[VGFX_MAX_COLOR_ATTACHMENTS] = {};
    ID3D11DepthStencilView* DSV = nullptr;

    for (uint32_t i = 0; i < info->colorAttachmentCount; ++i)
    {
        const VGFXRenderPassColorAttachment* attachment = &info->colorAttachments[i];
        VGFXD3D11Texture* texture = (VGFXD3D11Texture*)attachment->texture;
        const uint32_t level = attachment->level;
        const uint32_t slice = attachment->slice;

        RTVs[i] = d3d11_GetRTV(renderer, texture, DXGI_FORMAT_UNKNOWN, level, slice);

        switch (attachment->loadOp)
        {
            case VGFXLoadOp_Clear:
                renderer->context->ClearRenderTargetView(RTVs[i], &attachment->clearColor.r);
                break;

            default:
                break;
        }

        width = std::min(width, std::max(1U, texture->width >> level));
        height = std::min(height, std::max(1U, texture->height >> level));
        numRTVs++;
    }

    if (info->depthStencilAttachment)
    {
        const VGFXRenderPassDepthStencilAttachment* attachment = info->depthStencilAttachment;
        VGFXD3D11Texture* texture = (VGFXD3D11Texture*)attachment->texture;
        const uint32_t level = attachment->level;
        const uint32_t slice = attachment->slice;

        DSV = d3d11_GetDSV(renderer, texture, DXGI_FORMAT_UNKNOWN, level, slice);

        UINT clearFlags = {};
        float clearDepth = 0.0f;
        uint8_t clearStencil = 0u;

        if (attachment->depthLoadOp == VGFXLoadOp_Clear)
        {
            clearFlags |= D3D11_CLEAR_DEPTH;
            clearDepth = attachment->clearDepth;
        }

        if (attachment->stencilLoadOp == VGFXLoadOp_Clear)
        {
            clearFlags |= D3D11_CLEAR_STENCIL;
            clearStencil = attachment->clearStencil;
        }

        if (clearFlags)
        {
            renderer->context->ClearDepthStencilView(DSV, clearFlags, clearDepth, clearStencil);
        }

        width = std::min(width, std::max(1U, texture->width >> level));
        height = std::min(height, std::max(1U, texture->height >> level));
    }

    renderer->context->OMSetRenderTargets(numRTVs, RTVs, DSV);

    // Set the viewport and scissor.
    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    D3D11_RECT scissorRect = { 0, 0, static_cast<long>(width), static_cast<long>(height) };
    renderer->context->RSSetViewports(1, &viewport);
    renderer->context->RSSetScissorRects(1, &scissorRect);
}

static void d3d11_endRenderPass(VGFXRenderer* driverData)
{
    VGFXD3D11Renderer* renderer = (VGFXD3D11Renderer*)driverData;
}

static bool d3d11_isSupported(void)
{
    static bool available_initialized = false;
    static bool available = false;

    if (available_initialized) {
        return available;
    }

    available_initialized = true;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HMODULE dxgiDLL = LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE d3d11DLL = LoadLibraryExW(L"d3d11.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (dxgiDLL == nullptr ||
        d3d11DLL == nullptr)
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

    vgfxD3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(d3d11DLL, "D3D11CreateDevice");
    if (!vgfxD3D11CreateDevice)
    {
        return false;
    }
#endif

    HRESULT hr = vgfxD3D11CreateDevice(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        s_featureLevels,
        _countof(s_featureLevels),
        D3D11_SDK_VERSION,
        NULL,
        NULL,
        NULL
    );

    if (FAILED(hr))
    {
        // D3D11.1 not available
        hr = vgfxD3D11CreateDevice(
            NULL,
            D3D_DRIVER_TYPE_HARDWARE,
            NULL,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            &s_featureLevels[1],
            _countof(s_featureLevels) - 1,
            D3D11_SDK_VERSION,
            NULL,
            NULL,
            NULL
        );
    }

    if (FAILED(hr))
    {
        return false;
    }

    available = true;
    return true;
}

static VGFXDevice d3d11_createDevice(const VGFXDeviceDesc* info)
{
    VGFX_ASSERT(info);

    VGFXD3D11Renderer* renderer = new VGFXD3D11Renderer();

    DWORD dxgiFactoryFlags = 0;
    if (info->validationMode != VGFXValidationMode_Disabled)
    {
#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(vgfxDXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
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

    if (FAILED(vgfxCreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&renderer->factory))))
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

    ComPtr<IDXGIFactory6> dxgiFactory6;
    const bool queryByPreference = SUCCEEDED(renderer->factory.As(&dxgiFactory6));
    auto NextAdapter = [&](uint32_t index, IDXGIAdapter1** ppAdapter)
    {
        if (queryByPreference)
            return dxgiFactory6->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(ppAdapter));
        else
            return renderer->factory->EnumAdapters1(index, ppAdapter);
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

        break;
    }

    VGFX_ASSERT(dxgiAdapter != nullptr);
    if (dxgiAdapter == nullptr)
    {
        vgfxLogError("DXGI: No capable adapter found!");
        delete renderer;
        return nullptr;
    }

    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (info->validationMode != VGFXValidationMode_Disabled)
    {
        if (SdkLayersAvailable())
        {
            // If the project is in a debug build, enable debugging via SDK Layers with this flag.
            creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
        }
        else
        {
            OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
        }
    }

    // Create the Direct3D 11 API device object and a corresponding context.
    ComPtr<ID3D11Device> tempDevice;
    ComPtr<ID3D11DeviceContext> context;

    HRESULT hr = E_FAIL;

    // Create a WARP device instead of a hardware device if no adapter found.
    if (dxgiAdapter == nullptr)
    {
        // If the initialization fails, fall back to the WARP device.
        // For more information on WARP, see:
        // http://go.microsoft.com/fwlink/?LinkId=286690
        hr = vgfxD3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            creationFlags,
            s_featureLevels,
            _countof(s_featureLevels),
            D3D11_SDK_VERSION,
            tempDevice.GetAddressOf(),
            &renderer->featureLevel,
            context.GetAddressOf()
        );
    }
    else
    {
        hr = vgfxD3D11CreateDevice(
            dxgiAdapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            creationFlags,
            s_featureLevels,
            _countof(s_featureLevels),
            D3D11_SDK_VERSION,
            tempDevice.GetAddressOf(),
            &renderer->featureLevel,
            context.GetAddressOf()
        );

        if (FAILED(hr))
        {
            // D3D11.1 not available
            hr = vgfxD3D11CreateDevice(
                NULL,
                D3D_DRIVER_TYPE_HARDWARE,
                NULL,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                &s_featureLevels[1],
                _countof(s_featureLevels) - 1,
                D3D11_SDK_VERSION,
                tempDevice.GetAddressOf(),
                &renderer->featureLevel,
                context.GetAddressOf()
            );
        }
    }

    if (FAILED(hr))
    {
        delete renderer;
        return nullptr;
    }

    if (info->validationMode != VGFXValidationMode_Disabled)
    {
        ComPtr<ID3D11Debug> d3dDebug;
        if (SUCCEEDED(tempDevice.As(&d3dDebug)))
        {
            ComPtr<ID3D11InfoQueue> d3dInfoQueue;
            if (SUCCEEDED(d3dDebug.As(&d3dInfoQueue)))
            {
#ifdef _DEBUG
                d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
                d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
#endif
                D3D11_MESSAGE_ID hide[] =
                {
                    D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
                };
                D3D11_INFO_QUEUE_FILTER filter = {};
                filter.DenyList.NumIDs = _countof(hide);
                filter.DenyList.pIDList = hide;
                d3dInfoQueue->AddStorageFilterEntries(&filter);
            }
        }
    }

    if (FAILED(tempDevice->QueryInterface(&renderer->device)))
    {
        delete renderer;
        return nullptr;
    }

    if (FAILED(context->QueryInterface(&renderer->context)))
    {
        delete renderer;
        return nullptr;
    }

    //ThrowIfFailed(context.As(&m_d3dAnnotation));

    if (info->label)
    {
        renderer->device->SetPrivateData(g_WKPDID_D3DDebugObjectName, (UINT)strlen(info->label), info->label);
    }

    // Init capabilities.
    vgfxLogInfo("vgfx driver: D3D11");
    if (dxgiAdapter != nullptr)
    {
        DXGI_ADAPTER_DESC1 adapterDesc;
        dxgiAdapter->GetDesc1(&adapterDesc);

        renderer->vendorID = adapterDesc.VendorId;
        renderer->deviceID = adapterDesc.DeviceId;
        renderer->adapterName = WCharToUTF8(adapterDesc.Description);

        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            renderer->adapterType = VGFXAdapterType_CPU;
        }
        else {
            renderer->adapterType = /*(arch.UMA == TRUE) ? VGFXAdapterType_IntegratedGPU :*/ VGFXAdapterType_DiscreteGPU;
        }

        // Convert the adapter's D3D12 driver version to a readable string like "24.21.13.9793".
        LARGE_INTEGER umdVersion;
        if (dxgiAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umdVersion) != DXGI_ERROR_UNSUPPORTED)
        {
            uint64_t encodedVersion = umdVersion.QuadPart;
            std::ostringstream o;
            o << "D3D11 driver version ";
            uint16_t driverVersion[4] = {};

            for (size_t i = 0; i < 4; ++i) {
                driverVersion[i] = (encodedVersion >> (48 - 16 * i)) & 0xFFFF;
                o << driverVersion[i] << ".";
            }

            renderer->driverDescription = o.str();
        }

        vgfxLogInfo("D3D11 Adapter: %S", adapterDesc.Description);
    }
    else
    {
        renderer->adapterName = "WARP";
        renderer->adapterType = VGFXAdapterType_CPU;
        vgfxLogInfo("D3D11 Adapter: WARP");
    }

    VGFXDevice_T* device = (VGFXDevice_T*)VGFX_MALLOC(sizeof(VGFXDevice_T));
    ASSIGN_DRIVER(d3d11);

    device->driverData = (VGFXRenderer*)renderer;
    return device;
}

VGFXDriver d3d11_driver = {
    VGFXBackendType_D3D11,
    d3d11_isSupported,
    d3d11_createDevice
};

#endif /* VGPU_D3D11_DRIVER */