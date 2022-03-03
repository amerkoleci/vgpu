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

struct gfxD3D12Renderer
{
    IDXGIFactory4* factory;
};

static void d3d12_destroyDevice(gfxDevice device)
{
    gfxD3D12Renderer* renderer = (gfxD3D12Renderer*)device->driverData;

    delete renderer;
    VGPU_FREE(device);
}

static gfxDevice d3d12CreateDevice(void)
{
    gfxD3D12Renderer* renderer = new gfxD3D12Renderer();
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
