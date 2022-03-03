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

struct gfxVulkanRenderer
{
    VkInstance instance;
};

static void vulkan_destroyDevice(gfxDevice device)
{
    gfxVulkanRenderer* renderer = (gfxVulkanRenderer*)device->driverData;

    delete renderer;
    VGPU_FREE(device);
}

static gfxDevice vulkanCreateDevice(void)
{
    VkResult result = volkInitialize();
    if (result != VK_SUCCESS)
    {
        return nullptr;
    }

    gfxVulkanRenderer* renderer = new gfxVulkanRenderer();
    gfxDevice_T* device = (gfxDevice_T*)VGPU_MALLOC(sizeof(gfxDevice_T));
    ASSIGN_DRIVER(vulkan);

    device->driverData = (gfxRenderer*)renderer;
    return device;
}

gfxDriver vulkan_driver = {
    VGFX_API_VULKAN,
    vulkanCreateDevice
};

#endif /* VGFX_VULKAN_DRIVER */
