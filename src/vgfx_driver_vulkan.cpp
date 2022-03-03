// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGFX_VULKAN_DRIVER)

#include "vgfx_driver.h"

static gfxDevice vulkanCreateDevice(void)
{
    return nullptr;
}

gfxDriver vulkan_driver = {
    VGFX_API_VULKAN,
    vulkanCreateDevice
};

#endif /* VGFX_VULKAN_DRIVER */
