// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGFX_D3D12_DRIVER)

#include "vgfx_driver.h"

static gfxDevice d3d12CreateDevice(void)
{
    return nullptr;
}

gfxDriver d3d12_driver = {
    VGFX_API_D3D12,
    d3d12CreateDevice
};

#endif /* VGFX_D3D12_DRIVER */
