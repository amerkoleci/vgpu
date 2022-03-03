// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGFX_VULKAN_DRIVER)

#include "vgfx_driver.h"
VGFX_DISABLE_WARNINGS()
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <spirv_reflect.h>
VGFX_ENABLE_WARNINGS()

static gfxDevice vulkanCreateDevice(void)
{
    return nullptr;
}

gfxDriver vulkan_driver = {
    VGFX_API_VULKAN,
    vulkanCreateDevice
};

#endif /* VGFX_VULKAN_DRIVER */
