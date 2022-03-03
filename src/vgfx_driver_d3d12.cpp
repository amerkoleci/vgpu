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

struct gfxD3D12Renderer
{
    IDXGIFactory4* factory;
    bool tearingSupported;
    ID3D12Device5* device;
};

static void d3d12_destroyDevice(gfxDevice device)
{
    gfxD3D12Renderer* renderer = (gfxD3D12Renderer*)device->driverData;

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
                debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
                debugDevice->Release();
            }

        }
#else
        (void)refCount; // avoid warning
#endif
        renderer->device = nullptr;
    }

    SafeRelease(renderer->factory);

#if defined(_DEBUG) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    {
        IDXGIDebug1* dxgiDebug;
        if (vgfxDXGIGetDebugInterface1 != nullptr
            && SUCCEEDED(vgfxDXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            dxgiDebug->ReportLiveObjects(VGFX_DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
            dxgiDebug->Release();
        }
    }
#endif

    delete renderer;
    VGFX_FREE(device);
}

static bool d3d12_isSupported()
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

    IDXGIFactory4* dxgiFactory = nullptr;
    if (FAILED(vgfxCreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory))))
    {
        return false;
    }

    bool foundCompatibleDevice = true;
    IDXGIAdapter1* dxgiAdapter = nullptr;
    for (uint32_t i = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(i, &dxgiAdapter); ++i)
    {
        DXGI_ADAPTER_DESC1 adapterDesc;
        dxgiAdapter->GetDesc1(&adapterDesc);

        // Don't select the Basic Render Driver adapter.
        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            dxgiAdapter->Release();
            continue;
        }

        // Check to see if the adapter supports Direct3D 12,
        // but don't create the actual device.
        if (SUCCEEDED(vgfxD3D12CreateDevice(dxgiAdapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
        {
            foundCompatibleDevice = true;
            break;
        }
    }

    SafeRelease(dxgiAdapter);
    SafeRelease(dxgiFactory);

    if (foundCompatibleDevice)
    {
        available = true;
        return true;
    }

    return false;
}

static gfxDevice d3d12CreateDevice(const VGFXDeviceInfo* info)
{
    if (!d3d12_isSupported())
    {
        return nullptr;
    }

    gfxD3D12Renderer* renderer = new gfxD3D12Renderer();

    DWORD dxgiFactoryFlags = 0;
    if (info->validationMode != VGFX_VALIDATION_MODE_DISABLED)
    {
        ID3D12Debug* debugController;
        if (SUCCEEDED(vgfxD3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            if (info->validationMode == VGFX_VALIDATION_MODE_GPU)
            {
                ID3D12Debug1* debugController1;
                if (SUCCEEDED(debugController->QueryInterface(&debugController1)))
                {
                    debugController1->SetEnableGPUBasedValidation(TRUE);
                    debugController1->SetEnableSynchronizedCommandQueueValidation(TRUE);
                    debugController1->Release();
                }

                ID3D12Debug2* debugController2;
                if (SUCCEEDED(debugController->QueryInterface(&debugController2)))
                {
                    debugController2->SetGPUBasedValidationFlags(D3D12_GPU_BASED_VALIDATION_FLAGS_NONE);
                    debugController2->Release();
                }
            }

            debugController->Release();
        }
        else
        {
            OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
        }

#ifdef _DEBUG
        IDXGIInfoQueue* dxgiInfoQueue;
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
            dxgiInfoQueue->Release();
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

        IDXGIFactory5* dxgiFactory5 = nullptr;
        HRESULT hr = renderer->factory->QueryInterface(&dxgiFactory5);
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
        SafeRelease(dxgiFactory5);
    }

    {
        IDXGIFactory6* dxgiFactory6 = nullptr;
        const bool queryByPreference = SUCCEEDED(renderer->factory->QueryInterface(&dxgiFactory6));
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

        IDXGIAdapter1* dxgiAdapter = nullptr;
        for (uint32_t i = 0; NextAdapter(i, &dxgiAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 adapterDesc;
            dxgiAdapter->GetDesc1(&adapterDesc);

            // Don't select the Basic Render Driver adapter.
            if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                dxgiAdapter->Release();
                continue;
            }

            for (auto& featurelevel : s_featureLevels)
            {
                if (SUCCEEDED(vgfxD3D12CreateDevice(dxgiAdapter, featurelevel, IID_PPV_ARGS(&renderer->device))))
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

        // Init capabilities.
        DXGI_ADAPTER_DESC1 adapterDesc;
        dxgiAdapter->GetDesc1(&adapterDesc);

        SafeRelease(dxgiAdapter);
    }

    gfxDevice_T* device = (gfxDevice_T*)VGPU_MALLOC(sizeof(gfxDevice_T));
    ASSIGN_DRIVER(d3d12);

    device->driverData = (gfxRenderer*)renderer;
    return device;
}

gfxDriver d3d12_driver = {
    VGFX_API_D3D12,
    d3d12CreateDevice
};

#endif /* VGFX_D3D12_DRIVER */
