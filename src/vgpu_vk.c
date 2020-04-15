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

#if defined(_WIN32) || defined(_WIN64)
#   define   VK_SURFACE_EXT             "VK_KHR_win32_surface"
#   define   VK_CREATE_SURFACE_FN        vkCreateWin32SurfaceKHR
#   define   VK_CREATE_SURFACE_FN_TYPE   PFN_vkCreateWin32SurfaceKHR
#   define   VK_USE_PLATFORM_WIN32_KHR
#   define NOMINMAX
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#elif defined(__ANDROID__)
#   define   VK_SURFACE_EXT             "VK_KHR_android_surface"
#   define   VK_CREATE_SURFACE_FN        vkCreateAndroidSurfaceKHR
#   define   VK_CREATE_SURFACE_FN_TYPE   PFN_vkCreateAndroidSurfaceKHR
#   define   VK_USE_PLATFORM_ANDROID_KHR
#else
#   include <xcb/xcb.h>
#   include <dlfcn.h>
#   include <X11/Xlib-xcb.h>
#   define   VK_SURFACE_EXT             "VK_KHR_xcb_surface"
#   define   VK_CREATE_SURFACE_FN        vkCreateXcbSurfaceKHR
#   define   VK_CREATE_SURFACE_FN_TYPE   PFN_vkCreateXcbSurfaceKHR
#   define   VK_USE_PLATFORM_XCB_KHR
#endif

#ifndef VULKAN_H_
#define VULKAN_H_ 1
#endif
#define VKBIND_IMPLEMENTATION
#include "vk/vkbind.h"
#include "vk/vk_mem_alloc.h"

#define _VK_GPU_MAX_PHYSICAL_DEVICES (32u)
#define VK_CHECK(res) do { VkResult r = (res); VGPU_CHECK(r >= 0, vkGetErrorString(r)); } while (0)

#if !defined(NDEBUG) || defined(DEBUG) || defined(_DEBUG)
#   define VULKAN_DEBUG
#endif

typedef struct {
    bool swapchain;
    bool maintenance_1;
    bool maintenance_2;
    bool maintenance_3;
    bool get_memory_requirements2;
    bool dedicated_allocation;
    bool image_format_list;
    bool debug_marker;
} vk_physical_device_features;

typedef struct VGPUVkQueueFamilyIndices {
    uint32_t graphics_queue_family;
    uint32_t compute_queue_family;
    uint32_t copy_queue_family;
} VGPUVkQueueFamilyIndices;



#define _VGPU_VK_MAX_SWAPCHAINS (16u)
typedef struct VGPUSwapchainVk {
    VkSurfaceKHR surface;
    VkSwapchainKHR handle;

    uint32_t preferredImageCount;
    uint32_t width;
    uint32_t height;
    VkPresentModeKHR presentMode;
    VGPUTextureFormat color_format;

    uint32_t imageCount;
    VGPUTexture backbufferTextures[4];
    VGPUTexture depthStencilTexture;
    VkRenderPass vkRenderPass;
    VGPURenderPass renderPass;
} VGPUSwapchainVk;

typedef struct VGPUTextureVk {
    VkFormat format;
    VkImage handle;
    VkImageView view;
    VmaAllocation memory;
    bool external;
    VGPUExtent3D size;
} VGPUTextureVk;

typedef struct VGPUVkRenderer {
    /* Associated vgpu_device */
    VGPUDevice gpu_device;

    VkPhysicalDevice physical_device;
    VGPUVkQueueFamilyIndices queue_families;

    bool api_version_12;
    vk_physical_device_features device_features;

    vgpu_features features;
    vgpu_limits limits;

    VkbAPI api;

    VkDevice device;
    struct vk_device_table* table;
    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue copy_queue;
    VmaAllocator memory_allocator;

    VGPUSwapchainVk swapchains[_VGPU_VK_MAX_SWAPCHAINS];
} VGPUVkRenderer;

static struct {
    bool available_initialized;
    bool available;
    VkbAPI api;

    uint32_t apiVersion;
    bool debug_utils;
    bool headless;
    bool surface_capabilities2;
    bool physical_device_properties2;
    bool external_memory_capabilities;
    bool external_semaphore_capabilities;
    bool full_screen_exclusive;
    VkInstance instance;

    VkDebugUtilsMessengerEXT debug_utils_messenger;
    VkDebugReportCallbackEXT debug_report_callback;

    uint32_t physical_device_count;
    VkPhysicalDevice physical_devices[_VK_GPU_MAX_PHYSICAL_DEVICES];

    /* Number of device created. */
    uint32_t deviceCount;
} vk;

#if defined(VULKAN_DEBUG)
VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    // Log debug messge
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        vgpu_log_format(VGPU_LOG_LEVEL_WARN, "%u - %s: %s", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        vgpu_log_format(VGPU_LOG_LEVEL_ERROR, "%u - %s: %s", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
    }

    return VK_FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT type,
    uint64_t object,
    size_t location,
    int32_t message_code,
    const char* layer_prefix,
    const char* message,
    void* user_data)
{
    _VGPU_UNUSED(type);
    _VGPU_UNUSED(object);
    _VGPU_UNUSED(location);
    _VGPU_UNUSED(message_code);
    _VGPU_UNUSED(user_data);
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        vgpu_log_format(VGPU_LOG_LEVEL_ERROR, "%s: %s", layer_prefix, message);
    }
    else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        vgpu_log_format(VGPU_LOG_LEVEL_WARN, "%s: %s", layer_prefix, message);
    }
    else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
        vgpu_log_format(VGPU_LOG_LEVEL_WARN, "%s: %s", layer_prefix, message);
    }
    else
    {
        vgpu_log_format(VGPU_LOG_LEVEL_INFO, "%s: %s", layer_prefix, message);
    }
    return VK_FALSE;
}
#endif

static const char* vkGetErrorString(VkResult result) {
    switch (result) {
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "Out of CPU memory";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "Out of GPU memory";
    case VK_ERROR_MEMORY_MAP_FAILED: return "Could not map memory";
    case VK_ERROR_DEVICE_LOST: return "Lost connection to GPU";
    case VK_ERROR_TOO_MANY_OBJECTS: return "Too many objects";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "Unsupported format";
    default: return NULL;
    }
}

static vk_physical_device_features vgpuVkQueryDeviceExtensionSupport(VkPhysicalDevice physical_device)
{
    uint32_t ext_count = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_device, NULL, &ext_count, nullptr));

    VkExtensionProperties* available_extensions = VGPU_ALLOCA(VkExtensionProperties, ext_count);
    assert(available_extensions);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &ext_count, available_extensions));

    vk_physical_device_features result;
    memset(&result, 0, sizeof(result));
    for (uint32_t i = 0; i < ext_count; ++i) {
        if (strcmp(available_extensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            result.swapchain = true;
        }
        else if (strcmp(available_extensions[i].extensionName, "VK_KHR_maintenance1") == 0) {
            result.maintenance_1 = true;
        }
        else if (strcmp(available_extensions[i].extensionName, VK_KHR_MAINTENANCE2_EXTENSION_NAME) == 0) {
            result.maintenance_2 = true;
        }
        else if (strcmp(available_extensions[i].extensionName, VK_KHR_MAINTENANCE3_EXTENSION_NAME) == 0) {
            result.maintenance_3 = true;
        }
        else if (strcmp(available_extensions[i].extensionName, "VK_KHR_get_memory_requirements2") == 0) {
            result.get_memory_requirements2 = true;
        }
        else if (strcmp(available_extensions[i].extensionName, "VK_KHR_dedicated_allocation") == 0) {
            result.dedicated_allocation = true;
        }
        else if (strcmp(available_extensions[i].extensionName, "VK_KHR_image_format_list") == 0) {
            result.image_format_list = true;
        }
        else if (strcmp(available_extensions[i].extensionName, "VK_EXT_debug_marker") == 0) {
            result.debug_marker = true;
        }
        else if (strcmp(available_extensions[i].extensionName, "VK_EXT_full_screen_exclusive") == 0) {
            vk.full_screen_exclusive = true;
        }
    }

    return result;
}

static bool vgpuQueryPresentationSupport(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    return vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, queueFamilyIndex);
#elif defined(__ANDROID__)
    return true;
#else
    /* TODO: */
    return true;
#endif
}

static VGPUVkQueueFamilyIndices vgpuVkQueryQueueFamilies(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
    uint32_t queue_count = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, NULL);
    VkQueueFamilyProperties* queue_families = VGPU_ALLOCA(VkQueueFamilyProperties, queue_count);
    assert(queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, queue_families);

    VGPUVkQueueFamilyIndices result;
    result.graphics_queue_family = VK_QUEUE_FAMILY_IGNORED;
    result.compute_queue_family = VK_QUEUE_FAMILY_IGNORED;
    result.copy_queue_family = VK_QUEUE_FAMILY_IGNORED;

    for (uint32_t i = 0; i < queue_count; i++)
    {
        VkBool32 present_support = VK_TRUE;
        if (surface != VK_NULL_HANDLE) {
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
        }
        else {
            present_support = vgpuQueryPresentationSupport(physical_device, i);
        }

        static const VkQueueFlags required = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT;
        if (present_support && ((queue_families[i].queueFlags & required) == required))
        {
            result.graphics_queue_family = i;
            break;
        }
    }

    /* Dedicated compute queue. */
    for (uint32_t i = 0; i < queue_count; i++)
    {
        static const VkQueueFlags required = VK_QUEUE_COMPUTE_BIT;
        if (i != result.graphics_queue_family &&
            (queue_families[i].queueFlags & required) == required)
        {
            result.compute_queue_family = i;
            break;
        }
    }

    /* Dedicated transfer queue. */
    for (uint32_t i = 0; i < queue_count; i++)
    {
        static const VkQueueFlags required = VK_QUEUE_TRANSFER_BIT;
        if (i != result.graphics_queue_family &&
            i != result.compute_queue_family &&
            (queue_families[i].queueFlags & required) == required)
        {
            result.copy_queue_family = i;
            break;
        }
    }

    if (result.copy_queue_family == VK_QUEUE_FAMILY_IGNORED)
    {
        for (uint32_t i = 0; i < queue_count; i++)
        {
            static const VkQueueFlags required = VK_QUEUE_TRANSFER_BIT;
            if (i != result.graphics_queue_family &&
                (queue_families[i].queueFlags & required) == required)
            {
                result.copy_queue_family = i;
                break;
            }
        }
    }

    return result;
}

static bool vgpuVkIsDeviceSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, bool headless)
{
    VkPhysicalDeviceProperties gpuProps;
    vkGetPhysicalDeviceProperties(physicalDevice, &gpuProps);

    /* We run on vulkan 1.1 or higher. */
    if (gpuProps.apiVersion < VK_API_VERSION_1_1)
    {
        return false;
    }

    VGPUVkQueueFamilyIndices indices = vgpuVkQueryQueueFamilies(physicalDevice, surface);

    if (indices.graphics_queue_family == VK_QUEUE_FAMILY_IGNORED)
        return false;

    vk_physical_device_features features = vgpuVkQueryDeviceExtensionSupport(physicalDevice);
    if (!headless && !features.swapchain) {
        return false;
    }

    /* We required maintenance_1 to support viewport flipping to match DX style. */
    if (!features.maintenance_1) {
        return false;
    }

    return true;
}

static bool vk_create_surface(void* native_handle, VkSurfaceKHR* pSurface) {
    VkResult result = VK_SUCCESS;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    HWND hWnd = (HWND)native_handle;

    const VkWin32SurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .hinstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
        .hwnd = hWnd
    };
#endif
    result = vk.api.VK_CREATE_SURFACE_FN(vk.instance, &surface_info, NULL, pSurface);
    if (result != VK_SUCCESS) {
        vgpu_log_error("Failed to create surface");
        return false;
    }

    return true;
}

static VkImageView vgpuVkCreateImageView(
    VGPURenderer* driverData,
    VkImage image,
    VkImageViewType  image_type,
    VkFormat         image_format,
    uint32_t         nmips,
    uint32_t         nlayers);

/* Swapchain */
#define _VGPU_VK_MAX_SURFACE_FORMATS (32u)
#define _VGPU_VK_MAX_PRESENT_MODES (16u)

static VkPresentModeKHR vgpuVkGetPresentMode(VGPUPresentMode value) {
    switch (value)
    {
    case VGPUPresentMode_Mailbox:
        return VK_PRESENT_MODE_MAILBOX_KHR;

    case VGPUPresentMode_Immediate:
        return VK_PRESENT_MODE_IMMEDIATE_KHR;

    case VGPUPresentMode_Fifo:
    default:
        return VK_PRESENT_MODE_FIFO_KHR;
    }
}

typedef struct VGPUVkSurfaceCaps {
    bool success;
    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t formatCount;
    uint32_t presentModeCount;
    VkSurfaceFormatKHR formats[_VGPU_VK_MAX_SURFACE_FORMATS];
    VkPresentModeKHR present_modes[_VGPU_VK_MAX_PRESENT_MODES];
} VGPUVkSurfaceCaps;

static VGPUVkSurfaceCaps vgpuVkQuerySwapchainSupport(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
    VGPUVkSurfaceCaps caps;
    memset(&caps, 0, sizeof(VGPUVkSurfaceCaps));
    caps.formatCount = _VGPU_VK_MAX_SURFACE_FORMATS;
    caps.presentModeCount = _VGPU_VK_MAX_PRESENT_MODES;

    const VkPhysicalDeviceSurfaceInfo2KHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        .pNext = NULL,
        .surface = surface
    };

    if (vk.surface_capabilities2)
    {
        VkSurfaceCapabilities2KHR surface_caps2 = {
            .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
            .pNext = NULL
        };

        if (vk.api.vkGetPhysicalDeviceSurfaceCapabilities2KHR(physical_device, &surface_info, &surface_caps2) != VK_SUCCESS)
        {
            return caps;
        }
        caps.capabilities = surface_caps2.surfaceCapabilities;

        if (vk.api.vkGetPhysicalDeviceSurfaceFormats2KHR(physical_device, &surface_info, &caps.formatCount, NULL) != VK_SUCCESS)
        {
            return caps;
        }

        VkSurfaceFormat2KHR* formats2 = VGPU_ALLOCA(VkSurfaceFormat2KHR, caps.formatCount);
        VGPU_ASSERT(formats2);

        for (uint32_t i = 0; i < caps.formatCount; ++i)
        {
            memset(&formats2[i], 0, sizeof(VkSurfaceFormat2KHR));
            formats2[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
        }

        if (vk.api.vkGetPhysicalDeviceSurfaceFormats2KHR(physical_device, &surface_info, &caps.formatCount, formats2) != VK_SUCCESS)
        {
            return caps;
        }

        for (uint32_t i = 0; i < caps.formatCount; ++i)
        {
            caps.formats[i] = formats2[i].surfaceFormat;
        }
    }
    else
    {
        if (vk.api.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps.capabilities) != VK_SUCCESS)
        {
            return caps;
        }

        if (vk.api.vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &caps.formatCount, caps.formats) != VK_SUCCESS)
        {
            return caps;
        }
    }

#ifdef _WIN32
    if (vk.surface_capabilities2 && vk.full_screen_exclusive)
    {
        if (vk.api.vkGetPhysicalDeviceSurfacePresentModes2EXT(physical_device, &surface_info, &caps.presentModeCount, caps.present_modes) != VK_SUCCESS)
        {
            return caps;
        }
    }
    else
#endif
    {
        if (vk.api.vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &caps.presentModeCount, caps.present_modes) != VK_SUCCESS)
        {
            return caps;
        }
    }

    caps.success = true;
    return caps;
}

static void vgpuVkSwapchainDestroy(VGPURenderer* driverData, VGPUSwapchainVk* swapchain);

static bool vgpuVkSwapchainInit(VGPURenderer* driverData, VGPUSwapchainVk* swapchain)
{
    VGPUVkRenderer* renderer = (VGPUVkRenderer*)driverData;

    VGPUVkSurfaceCaps surface_caps = vgpuVkQuerySwapchainSupport(renderer->physical_device, swapchain->surface);

    VkSwapchainKHR oldSwapchain = swapchain->handle;

    /* Detect image count. */
    uint32_t image_count = swapchain->preferredImageCount;
    if (image_count == 0)
    {
        image_count = surface_caps.capabilities.minImageCount + 1;
        if ((surface_caps.capabilities.maxImageCount > 0) &&
            (image_count > surface_caps.capabilities.maxImageCount))
        {
            image_count = surface_caps.capabilities.maxImageCount;
        }
    }
    else
    {
        if (surface_caps.capabilities.maxImageCount != 0)
            image_count = _vgpu_min(image_count, surface_caps.capabilities.maxImageCount);
        image_count = _vgpu_max(image_count, surface_caps.capabilities.minImageCount);
    }

    /* Extent */
    VkExtent2D swapchain_size = { swapchain->width, swapchain->height };
    if (swapchain_size.width < 1 || swapchain_size.height < 1)
    {
        swapchain_size = surface_caps.capabilities.currentExtent;
    }
    else
    {
        swapchain_size.width = _vgpu_max(swapchain_size.width, surface_caps.capabilities.minImageExtent.width);
        swapchain_size.width = _vgpu_min(swapchain_size.width, surface_caps.capabilities.maxImageExtent.width);
        swapchain_size.height = _vgpu_max(swapchain_size.height, surface_caps.capabilities.minImageExtent.height);
        swapchain_size.height = _vgpu_min(swapchain_size.height, surface_caps.capabilities.maxImageExtent.height);
    }

    /* Surface format. */
    VkSurfaceFormatKHR format;
    if (surface_caps.formatCount == 1 &&
        surface_caps.formats[0].format == VK_FORMAT_UNDEFINED)
    {
        format = surface_caps.formats[0];
        format.format = VK_FORMAT_B8G8R8A8_UNORM;
    }
    else
    {
        if (surface_caps.formatCount == 0)
        {
            vgpu_log_error("Vulkan: Surface has no formats.");
            return false;
        }

        const bool srgb = false;
        bool found = false;
        for (uint32_t i = 0; i < surface_caps.formatCount; i++)
        {
            if (srgb)
            {
                if (surface_caps.formats[i].format == VK_FORMAT_R8G8B8A8_SRGB ||
                    surface_caps.formats[i].format == VK_FORMAT_B8G8R8A8_SRGB ||
                    surface_caps.formats[i].format == VK_FORMAT_A8B8G8R8_SRGB_PACK32)
                {
                    format = surface_caps.formats[i];
                    found = true;
                    break;
                }
            }
            else
            {
                if (surface_caps.formats[i].format == VK_FORMAT_R8G8B8A8_UNORM ||
                    surface_caps.formats[i].format == VK_FORMAT_B8G8R8A8_UNORM ||
                    surface_caps.formats[i].format == VK_FORMAT_A8B8G8R8_UNORM_PACK32)
                {
                    format = surface_caps.formats[i];
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            format = surface_caps.formats[0];
        }
    }

    VGPUTextureUsage textureUsage = VGPUTextureUsage_OutputAttachment;

    // Enable transfer destination on swap chain images if supported
    VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Enable transfer source on swap chain images if supported
    if (surface_caps.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        image_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        textureUsage |= VGPUTextureUsage_CopySrc;
    }

    // Enable transfer destination on swap chain images if supported
    if (surface_caps.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        image_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        textureUsage |= VGPUTextureUsage_CopyDst;
    }

    VkSurfaceTransformFlagBitsKHR pre_transform;
    if ((surface_caps.capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0)
        pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    else
        pre_transform = surface_caps.capabilities.currentTransform;

    VkCompositeAlphaFlagBitsKHR composite_mode = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (surface_caps.capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        composite_mode = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    if (surface_caps.capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        composite_mode = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (surface_caps.capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
        composite_mode = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    if (surface_caps.capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
        composite_mode = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;

    bool present_mode_found = false;
    for (uint32_t i = 0; i < surface_caps.presentModeCount; i++) {
        if (surface_caps.present_modes[i] == swapchain->presentMode) {
            present_mode_found = true;
            break;
        }
    }

    if (!present_mode_found) {
        swapchain->presentMode = VK_PRESENT_MODE_FIFO_KHR;
    }

    /* We use same family for graphics and present so no sharing is necessary. */
    const VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .surface = swapchain->surface,
        .minImageCount = image_count,
        .imageFormat = format.format,
        .imageColorSpace = format.colorSpace,
        .imageExtent = swapchain_size,
        .imageArrayLayers = 1,
        .imageUsage = image_usage,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .preTransform = pre_transform,
        .compositeAlpha = composite_mode,
        .presentMode = swapchain->presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = oldSwapchain
    };

    VkResult result = renderer->api.vkCreateSwapchainKHR(
        renderer->device,
        &create_info,
        NULL,
        &swapchain->handle);
    if (result != VK_SUCCESS) {
        vgpuVkSwapchainDestroy(driverData, swapchain);
        return false;
    }

    if (oldSwapchain != VK_NULL_HANDLE)
    {
        renderer->api.vkDestroySwapchainKHR(renderer->device, oldSwapchain, NULL);
    }
    else
    {
        /* Create swapchain render pass */

        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        uint32_t attachmentCount = 1;
        VkAttachmentDescription attachments[4];
        VkAttachmentReference references[3];
        references[0] = (VkAttachmentReference){ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        // Color attachment
        attachments[0].flags = 0u;
        attachments[0].format = format.format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        if (depthFormat != VK_FORMAT_UNDEFINED)
        {
            // Depth attachment
            attachments[1].format = depthFormat;
            attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
            attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachmentCount++;
        }

        VkSubpassDescription subpass = {
            .colorAttachmentCount = 1,
            .pColorAttachments = references,
            .pDepthStencilAttachment = nullptr // info->depth.texture ? &references[canvas->colorAttachmentCount] : NULL
        };

        // Subpass dependencies for layout transitions
        VkSubpassDependency dependencies[2];

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        const VkRenderPassCreateInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = attachmentCount,
            .pAttachments = attachments,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 2,
            .pDependencies = dependencies
        };

        VK_CHECK(renderer->api.vkCreateRenderPass(renderer->device, &renderPassInfo, nullptr, &swapchain->vkRenderPass));
    }

    // Obtain swapchain images.
    result = renderer->api.vkGetSwapchainImagesKHR(renderer->device, swapchain->handle, &swapchain->imageCount, NULL);
    if (result != VK_SUCCESS) {
        vgpuVkSwapchainDestroy(driverData, swapchain);
        return false;
    }

    VkImage* swapChainImages = VGPU_ALLOCA(VkImage, swapchain->imageCount);
    result = renderer->api.vkGetSwapchainImagesKHR(renderer->device, swapchain->handle, &swapchain->imageCount, swapChainImages);
    if (result != VK_SUCCESS) {
        vgpuVkSwapchainDestroy(driverData, swapchain);
        return false;
    }

    const VGPUTextureDescriptor textureDesc = {
        .dimension = VGPUTextureDimension_2D,
        .usage = textureUsage,
        .size = {
            .width = swapchain_size.width,
            .height = swapchain_size.height,
            .depth = create_info.imageArrayLayers
        },
        .format = VGPUTextureFormat_BGRA8Unorm,
        .mipLevelCount = 1u,
        .sampleCount = 1u
    };

    for (uint32_t i = 0; i < swapchain->imageCount; i++)
    {
        swapchain->backbufferTextures[i] = vgpuTextureCreateExternal(&textureDesc, swapChainImages[i]);

        ((VGPUTextureVk*)swapchain->backbufferTextures[i])->view = vgpuVkCreateImageView(driverData, swapChainImages[i], VK_IMAGE_VIEW_TYPE_2D, format.format, 1, 1);
    }

    swapchain->depthStencilTexture = nullptr;

    /*const vgpu_render_pass_desc pass_desc = {
        .color_attachments[0].texture = swapchain->backbufferTexture
    };

    swapchain->render_pass = vgpu_render_pass_create(&pass_desc);*/
    return true;
}

static void vgpuVkSwapchainDestroy(VGPURenderer* driverData, VGPUSwapchainVk* swapchain)
{
    VGPUVkRenderer* renderer = (VGPUVkRenderer*)driverData;
    if (swapchain->depthStencilTexture) {
        vgpuTextureDestroy(swapchain->depthStencilTexture);
    }

    for (uint32_t i = 0; i < swapchain->imageCount; i++)
    {
        vgpuTextureDestroy(swapchain->backbufferTextures[i]);
    }

    if (swapchain->vkRenderPass != VK_NULL_HANDLE)
    {
        renderer->api.vkDestroyRenderPass(renderer->device, swapchain->vkRenderPass, NULL);
        swapchain->vkRenderPass = VK_NULL_HANDLE;
    }
    //vgpu_render_pass_destroy(swapchain->renderPass);

    if (swapchain->handle != VK_NULL_HANDLE)
    {
        vk.api.vkDestroySwapchainKHR(renderer->device, swapchain->handle, NULL);
        swapchain->handle = VK_NULL_HANDLE;
    }

    if (swapchain->surface != VK_NULL_HANDLE)
    {
        vk.api.vkDestroySurfaceKHR(vk.instance, swapchain->surface, NULL);
        swapchain->surface = VK_NULL_HANDLE;
    }
}

static bool vk_init(VGPUDevice device, const char* app_name, const VGpuDeviceDescriptor* desc)
{
    if (!vgpu_vk_supported()) {
        return false;
    }

    /* Setup instance only once. */
    if (!vk.instance) {
        bool validation = false;
#if defined(VULKAN_DEBUG)
        if (desc->flags & VGPU_CONFIG_FLAGS_VALIDATION) {
            validation = true;
        }
#endif

        uint32_t instance_extension_count;
        VK_CHECK(vk.api.vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, NULL));

        VkExtensionProperties* available_instance_extensions = VGPU_ALLOCA(VkExtensionProperties, instance_extension_count);
        VK_CHECK(vk.api.vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, available_instance_extensions));

        uint32_t enabled_ext_count = 0;
        char* enabled_exts[16];

        for (uint32_t i = 0; i < instance_extension_count; ++i)
        {
            if (strcmp(available_instance_extensions[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
                vk.debug_utils = true;
                enabled_exts[enabled_ext_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
            }
            else if (strcmp(available_instance_extensions[i].extensionName, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME) == 0)
            {
                vk.headless = true;
            }
            else if (strcmp(available_instance_extensions[i].extensionName, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME) == 0)
            {
                vk.surface_capabilities2 = true;
            }
            else if (strcmp(available_instance_extensions[i].extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0)
            {
                vk.physical_device_properties2 = true;
                enabled_exts[enabled_ext_count++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
            }
            else if (strcmp(available_instance_extensions[i].extensionName, VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME) == 0)
            {
                vk.external_memory_capabilities = true;
                enabled_exts[enabled_ext_count++] = VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME;
            }
            else if (strcmp(available_instance_extensions[i].extensionName, VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME) == 0)
            {
                vk.external_semaphore_capabilities = true;
                enabled_exts[enabled_ext_count++] = VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME;
            }
        }

        if (desc->swapchain == nullptr)
        {
            if (!vk.headless) {

            }
            else
            {
                enabled_exts[enabled_ext_count++] = VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME;
            }
        }
        else
        {
            enabled_exts[enabled_ext_count++] = VK_KHR_SURFACE_EXTENSION_NAME;

#if defined(_WIN32)
            enabled_exts[enabled_ext_count++] = "VK_KHR_win32_surface";
#endif

            if (vk.surface_capabilities2) {
                enabled_exts[enabled_ext_count++] = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
            }
        }

        uint32_t enabled_instance_layers_count = 0;
        const char* enabled_instance_layers[8];

#if defined(VULKAN_DEBUG)
        if (validation) {
            uint32_t instance_layer_count;
            VK_CHECK(vk.api.vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr));

            VkLayerProperties* supported_validation_layers = VGPU_ALLOCA(VkLayerProperties, instance_layer_count);
            assert(supported_validation_layers);
            VK_CHECK(vk.api.vkEnumerateInstanceLayerProperties(&instance_layer_count, supported_validation_layers));

            // Search for VK_LAYER_KHRONOS_validation first.
            bool found = false;
            for (uint32_t i = 0; i < instance_layer_count; ++i) {
                if (strcmp(supported_validation_layers[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                    enabled_instance_layers[enabled_instance_layers_count++] = "VK_LAYER_KHRONOS_validation";
                    found = true;
                    break;
                }
            }

            // Fallback to VK_LAYER_LUNARG_standard_validation.
            if (!found) {
                for (uint32_t i = 0; i < instance_layer_count; ++i) {
                    if (strcmp(supported_validation_layers[i].layerName, "VK_LAYER_LUNARG_standard_validation") == 0) {
                        enabled_instance_layers[enabled_instance_layers_count++] = "VK_LAYER_LUNARG_standard_validation";
                    }
                }
            }
        }
#endif

        /* We require version 1.1 or higher */
        if (!vk.api.vkEnumerateInstanceVersion) {
            return false;
        }

        vk.apiVersion = 0;
        if (vk.api.vkEnumerateInstanceVersion(&vk.apiVersion) != VK_SUCCESS)
        {
            vk.apiVersion = VK_API_VERSION_1_1;
        }

        if (vk.apiVersion < VK_API_VERSION_1_1) {
            return false;
        }

        VkInstanceCreateInfo instance_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &(VkApplicationInfo) {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = app_name,
                .apiVersion = vk.apiVersion
            },
            .enabledLayerCount = enabled_instance_layers_count,
            .ppEnabledLayerNames = enabled_instance_layers,
            .enabledExtensionCount = enabled_ext_count,
            .ppEnabledExtensionNames = enabled_exts
        };

#if defined(VULKAN_DEBUG)
        VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        VkDebugReportCallbackCreateInfoEXT debug_report_create_info = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT };
        if (vk.debug_utils)
        {
            debug_utils_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
            debug_utils_create_info.pfnUserCallback = debug_utils_messenger_callback;

            instance_info.pNext = &debug_utils_create_info;
        }
        else
        {
            debug_report_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
            debug_report_create_info.pfnCallback = debug_callback;

            instance_info.pNext = &debug_report_create_info;
        }
#endif

        VkResult result = vk.api.vkCreateInstance(&instance_info, NULL, &vk.instance);
        if (result != VK_SUCCESS) {
            vgpuShutdown();
            return false;
        }

        result = vkbInitInstanceAPI(vk.instance, &vk.api);
        if (result != VK_SUCCESS) {
            vgpu_log_error("Failed to initialize instance API.");
            vgpuShutdown();
            return false;
        }

#if defined(VULKAN_DEBUG)
        if (vk.debug_utils)
        {
            result = vk.api.vkCreateDebugUtilsMessengerEXT(vk.instance, &debug_utils_create_info, NULL, &vk.debug_utils_messenger);
            if (result != VK_SUCCESS)
            {
                vgpu_log_error("Could not create debug utils messenger");
                vgpuShutdown();
                return false;
            }
        }
        else
        {
            result = vk.api.vkCreateDebugReportCallbackEXT(vk.instance, &debug_report_create_info, NULL, &vk.debug_report_callback);
            if (result != VK_SUCCESS) {
                vgpu_log_error("Could not create debug report callback");
                vgpuShutdown();
                return false;
            }
        }
#endif

        /* Enumerate all physical device. */
        vk.physical_device_count = _VK_GPU_MAX_PHYSICAL_DEVICES;
        result = vk.api.vkEnumeratePhysicalDevices(vk.instance, &vk.physical_device_count, vk.physical_devices);
        if (result != VK_SUCCESS) {
            vgpu_log_error("Vulkan: Cannot enumerate physical devices.");
            vgpuShutdown();
            return false;
        }
    }

    const bool headless = desc->swapchain == nullptr;
    VGPUVkRenderer* renderer = (VGPUVkRenderer*)device->renderer;
    renderer->api = vk.api;

    /* Create surface if required. */
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!headless)
    {
        if (!vk_create_surface(desc->swapchain->native_handle, &surface))
        {
            vgpuShutdown();
            return false;
        }
    }

    /* Find best supported physical device. */
    const VGPUAdapterType preferredAdapter = VGPUAdapterType_DiscreteGPU;
    uint32_t best_device_score = 0U;
    uint32_t best_device_index = VK_QUEUE_FAMILY_IGNORED;
    for (uint32_t i = 0; i < vk.physical_device_count; ++i)
    {
        if (!vgpuVkIsDeviceSuitable(vk.physical_devices[i], surface, headless)) {
            continue;
        }

        VkPhysicalDeviceProperties physical_device_props;
        vkGetPhysicalDeviceProperties(vk.physical_devices[i], &physical_device_props);

        uint32_t score = 0u;

        if (physical_device_props.apiVersion >= VK_API_VERSION_1_2) {
            score += 10000u;
        }

        switch (physical_device_props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 100U;
            if (preferredAdapter == VGPUAdapterType_DiscreteGPU) {
                score += 1000u;
            }
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 90U;
            if (preferredAdapter == VGPUAdapterType_IntegratedGPU) {
                score += 1000u;
            }
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 80U;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score += 70U;
            if (preferredAdapter == VGPUAdapterType_CPU) {
                score += 1000u;
            }
            break;
        default: score += 10U;
        }
        if (score > best_device_score) {
            best_device_index = i;
            best_device_score = score;
        }
    }

    if (best_device_index == VK_QUEUE_FAMILY_IGNORED) {
        vgpu_log_error("Vulkan: Cannot find suitable physical device.");
        vgpuShutdown();
        return false;
    }
    renderer->physical_device = vk.physical_devices[best_device_index];
    renderer->queue_families = vgpuVkQueryQueueFamilies(renderer->physical_device, surface);
    renderer->device_features = vgpuVkQueryDeviceExtensionSupport(renderer->physical_device);

    VkPhysicalDeviceProperties gpu_props;
    vkGetPhysicalDeviceProperties(renderer->physical_device, &gpu_props);

    if (gpu_props.apiVersion >= VK_API_VERSION_1_2)
    {
        renderer->api_version_12 = true;
    }

    /* Setup device queue's. */
    uint32_t queue_count;
    vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device, &queue_count, NULL);
    VkQueueFamilyProperties* queue_families = VGPU_ALLOCA(VkQueueFamilyProperties, queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device, &queue_count, queue_families);

    uint32_t universal_queue_index = 1;
    uint32_t graphics_queue_index = 0;
    uint32_t compute_queue_index = 0;
    uint32_t copy_queue_index = 0;

    if (renderer->queue_families.compute_queue_family == VK_QUEUE_FAMILY_IGNORED)
    {
        renderer->queue_families.compute_queue_family = renderer->queue_families.graphics_queue_family;
        compute_queue_index = _vgpu_min(queue_families[renderer->queue_families.graphics_queue_family].queueCount - 1, universal_queue_index);
        universal_queue_index++;
    }

    if (renderer->queue_families.copy_queue_family == VK_QUEUE_FAMILY_IGNORED)
    {
        renderer->queue_families.copy_queue_family = renderer->queue_families.graphics_queue_family;
        copy_queue_index = _vgpu_min(queue_families[renderer->queue_families.graphics_queue_family].queueCount - 1, universal_queue_index);
        universal_queue_index++;
    }
    else if (renderer->queue_families.copy_queue_family == renderer->queue_families.compute_queue_family)
    {
        copy_queue_index = _vgpu_min(queue_families[renderer->queue_families.compute_queue_family].queueCount - 1, 1u);
    }

    static const float graphics_queue_prio = 0.5f;
    static const float compute_queue_prio = 1.0f;
    static const float transfer_queue_prio = 1.0f;
    float prio[3] = { graphics_queue_prio, compute_queue_prio, transfer_queue_prio };

    uint32_t queue_family_count = 0;
    VkDeviceQueueCreateInfo queue_info[3] = { 0 };
    queue_info[queue_family_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[queue_family_count].queueFamilyIndex = renderer->queue_families.graphics_queue_family;
    queue_info[queue_family_count].queueCount = _vgpu_min(universal_queue_index, queue_families[renderer->queue_families.graphics_queue_family].queueCount);
    queue_info[queue_family_count].pQueuePriorities = prio;
    queue_family_count++;

    if (renderer->queue_families.compute_queue_family != renderer->queue_families.graphics_queue_family)
    {
        queue_info[queue_family_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[queue_family_count].queueFamilyIndex = renderer->queue_families.compute_queue_family;
        queue_info[queue_family_count].queueCount = _vgpu_min(renderer->queue_families.copy_queue_family == renderer->queue_families.compute_queue_family ? 2u : 1u,
            queue_families[renderer->queue_families.compute_queue_family].queueCount);
        queue_info[queue_family_count].pQueuePriorities = prio + 1;
        queue_family_count++;
    }

    if (renderer->queue_families.copy_queue_family != renderer->queue_families.graphics_queue_family &&
        renderer->queue_families.copy_queue_family != renderer->queue_families.compute_queue_family)
    {
        queue_info[queue_family_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[queue_family_count].queueFamilyIndex = renderer->queue_families.copy_queue_family;
        queue_info[queue_family_count].queueCount = 1;
        queue_info[queue_family_count].pQueuePriorities = prio + 2;
        queue_family_count++;
    }

    /* Setup device extensions now. */
    uint32_t enabled_device_ext_count = 0;
    char* enabled_device_exts[64];
    enabled_device_exts[enabled_device_ext_count++] = "VK_KHR_maintenance1";

    if (!headless) {
        enabled_device_exts[enabled_device_ext_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    }

    if (renderer->device_features.maintenance_2) {
        enabled_device_exts[enabled_device_ext_count++] = "VK_KHR_maintenance2";
    }

    if (renderer->device_features.maintenance_3) {
        enabled_device_exts[enabled_device_ext_count++] = "VK_KHR_maintenance3";
    }

    if (renderer->device_features.get_memory_requirements2 &&
        renderer->device_features.dedicated_allocation)
    {
        enabled_device_exts[enabled_device_ext_count++] = "VK_KHR_get_memory_requirements2";
        enabled_device_exts[enabled_device_ext_count++] = "VK_KHR_dedicated_allocation";
    }

#ifdef _WIN32
    if (vk.surface_capabilities2 &&
        vk.full_screen_exclusive)
    {
        enabled_device_exts[enabled_device_ext_count++] = "VK_EXT_full_screen_exclusive";
    }
#endif

    VkPhysicalDeviceFeatures2KHR features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR
    };
    vk.api.vkGetPhysicalDeviceFeatures2(renderer->physical_device, &features);

    // Enable device features we might care about.
    {
        VkPhysicalDeviceFeatures enabled_features;
        memset(&enabled_features, 0, sizeof(VkPhysicalDeviceFeatures));
        if (features.features.textureCompressionETC2)
            enabled_features.textureCompressionETC2 = VK_TRUE;
        if (features.features.textureCompressionBC)
            enabled_features.textureCompressionBC = VK_TRUE;
        if (features.features.textureCompressionASTC_LDR)
            enabled_features.textureCompressionASTC_LDR = VK_TRUE;
        if (features.features.fullDrawIndexUint32)
            enabled_features.fullDrawIndexUint32 = VK_TRUE;
        if (features.features.imageCubeArray)
            enabled_features.imageCubeArray = VK_TRUE;
        if (features.features.fillModeNonSolid)
            enabled_features.fillModeNonSolid = VK_TRUE;
        if (features.features.independentBlend)
            enabled_features.independentBlend = VK_TRUE;
    }

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queue_family_count,
        .pQueueCreateInfos = queue_info,
        .enabledExtensionCount = enabled_device_ext_count,
        .ppEnabledExtensionNames = enabled_device_exts
    };

    if (vk.physical_device_properties2)
        device_info.pNext = &features;
    else
        device_info.pEnabledFeatures = &features.features;

    VkResult result = vk.api.vkCreateDevice(renderer->physical_device, &device_info, NULL, &renderer->device);
    if (result != VK_SUCCESS) {
        vgpuShutdown();
        return false;
    }

    result = vkbInitDeviceAPI(renderer->device, &renderer->api);
    if (result != VK_SUCCESS) {
        vgpuShutdown();
        return false;
    }

    renderer->api.vkGetDeviceQueue(renderer->device, renderer->queue_families.graphics_queue_family, graphics_queue_index, &renderer->graphics_queue);
    renderer->api.vkGetDeviceQueue(renderer->device, renderer->queue_families.compute_queue_family, compute_queue_index, &renderer->compute_queue);
    renderer->api.vkGetDeviceQueue(renderer->device, renderer->queue_families.copy_queue_family, copy_queue_index, &renderer->copy_queue);

    /* Create memory allocator. */
    {
        VmaAllocatorCreateFlags allocator_flags = 0;
        if (renderer->device_features.get_memory_requirements2 &&
            renderer->device_features.dedicated_allocation)
        {
            allocator_flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
        }

        const VmaAllocatorCreateInfo allocator_info = {
            .flags = allocator_flags,
            .physicalDevice = renderer->physical_device,
            .device = renderer->device,
            .instance = vk.instance,
            .vulkanApiVersion = vk.apiVersion
        };
        result = vmaCreateAllocator(&allocator_info, &renderer->memory_allocator);
        if (result != VK_SUCCESS) {
            vgpu_log_error("Vulkan: Cannot create memory allocator.");
            vgpuShutdown();
            return false;
        }
    }

    /* Init features and limits. */
    renderer->features.independent_blend = features.features.independentBlend;
    renderer->features.compute_shader = true;
    renderer->features.geometry_shader = features.features.geometryShader;
    renderer->features.tessellation_shader = features.features.tessellationShader;
    renderer->features.multi_viewport = features.features.multiViewport;
    renderer->features.index_uint32 = features.features.fullDrawIndexUint32;
    renderer->features.multi_draw_indirect = features.features.multiDrawIndirect;
    renderer->features.fill_mode_non_solid = features.features.fillModeNonSolid;
    renderer->features.sampler_anisotropy = features.features.samplerAnisotropy;
    renderer->features.texture_compression_BC = features.features.textureCompressionBC;
    renderer->features.texture_compression_PVRTC = false;
    renderer->features.texture_compression_ETC2 = features.features.textureCompressionETC2;
    renderer->features.texture_compression_ASTC = features.features.textureCompressionASTC_LDR;
    //renderer->features.texture_1D = true;
    renderer->features.texture_3D = true;
    renderer->features.texture_2D_array = true;
    renderer->features.texture_cube_array = features.features.imageCubeArray;
    //renderer->features.raytracing = vk.KHR_get_physical_device_properties2
    //    && renderer->device_features.get_memory_requirements2
    //    || HasExtension(VK_NV_RAY_TRACING_EXTENSION_NAME);

    // Limits
    renderer->limits.max_vertex_attributes = gpu_props.limits.maxVertexInputAttributes;
    renderer->limits.max_vertex_bindings = gpu_props.limits.maxVertexInputBindings;
    renderer->limits.max_vertex_attribute_offset = gpu_props.limits.maxVertexInputAttributeOffset;
    renderer->limits.max_vertex_binding_stride = gpu_props.limits.maxVertexInputBindingStride;

    renderer->limits.max_texture_size_1d = gpu_props.limits.maxImageDimension1D;
    renderer->limits.max_texture_size_2d = gpu_props.limits.maxImageDimension2D;
    renderer->limits.max_texture_size_3d = gpu_props.limits.maxImageDimension3D;
    renderer->limits.max_texture_size_cube = gpu_props.limits.maxImageDimensionCube;
    renderer->limits.max_texture_array_layers = gpu_props.limits.maxImageArrayLayers;
    renderer->limits.max_color_attachments = gpu_props.limits.maxColorAttachments;
    renderer->limits.max_uniform_buffer_size = gpu_props.limits.maxUniformBufferRange;
    renderer->limits.min_uniform_buffer_offset_alignment = gpu_props.limits.minUniformBufferOffsetAlignment;
    renderer->limits.max_storage_buffer_size = gpu_props.limits.maxStorageBufferRange;
    renderer->limits.min_storage_buffer_offset_alignment = gpu_props.limits.minStorageBufferOffsetAlignment;
    renderer->limits.max_sampler_anisotropy = (uint32_t)gpu_props.limits.maxSamplerAnisotropy;
    renderer->limits.max_viewports = gpu_props.limits.maxViewports;
    renderer->limits.max_viewport_width = gpu_props.limits.maxViewportDimensions[0];
    renderer->limits.max_viewport_height = gpu_props.limits.maxViewportDimensions[1];
    renderer->limits.max_tessellation_patch_size = gpu_props.limits.maxTessellationPatchSize;
    renderer->limits.point_size_range_min = gpu_props.limits.pointSizeRange[0];
    renderer->limits.point_size_range_max = gpu_props.limits.pointSizeRange[1];
    renderer->limits.line_width_range_min = gpu_props.limits.lineWidthRange[0];
    renderer->limits.line_width_range_max = gpu_props.limits.lineWidthRange[1];
    renderer->limits.max_compute_shared_memory_size = gpu_props.limits.maxComputeSharedMemorySize;
    renderer->limits.max_compute_work_group_count_x = gpu_props.limits.maxComputeWorkGroupCount[0];
    renderer->limits.max_compute_work_group_count_y = gpu_props.limits.maxComputeWorkGroupCount[1];
    renderer->limits.max_compute_work_group_count_z = gpu_props.limits.maxComputeWorkGroupCount[2];
    renderer->limits.max_compute_work_group_invocations = gpu_props.limits.maxComputeWorkGroupInvocations;
    renderer->limits.max_compute_work_group_size_x = gpu_props.limits.maxComputeWorkGroupSize[0];
    renderer->limits.max_compute_work_group_size_y = gpu_props.limits.maxComputeWorkGroupSize[1];
    renderer->limits.max_compute_work_group_size_z = gpu_props.limits.maxComputeWorkGroupSize[2];

    /* Create main context and set it as active. */
    if (surface != VK_NULL_HANDLE)
    {
        renderer->swapchains[0].surface = surface;
        renderer->swapchains[0].width = desc->swapchain->width;
        renderer->swapchains[0].height = desc->swapchain->height;
        renderer->swapchains[0].presentMode = vgpuVkGetPresentMode(desc->swapchain->presentMode);

        if (!vgpuVkSwapchainInit(device->renderer, &renderer->swapchains[0]))
        {
            vgpuShutdown();
            return false;
        }
    }

    /* Increase device being created. */
    vk.deviceCount++;

    return true;
}

static void vk_waitIdle(VGPUDevice device);

static void vk_destroy(VGPUDevice device)
{
    VGPUVkRenderer* renderer = (VGPUVkRenderer*)device->renderer;

    if (renderer->device != VK_NULL_HANDLE) {
        vk_waitIdle(device);
    }

    /* Destroy swap chains.*/
    for (uint32_t i = 0; i < _VGPU_VK_MAX_SWAPCHAINS; i++)
    {
        if (renderer->swapchains[i].handle == VK_NULL_HANDLE)
            continue;

        vgpuVkSwapchainDestroy(device->renderer, &renderer->swapchains[i]);
    }

    if (renderer->memory_allocator != VK_NULL_HANDLE)
    {
        VmaStats stats;
        vmaCalculateStats(renderer->memory_allocator, &stats);

        if (stats.total.usedBytes > 0) {
            vgpu_log_format(VGPU_LOG_LEVEL_ERROR, "Total device memory leaked: %llx bytes.", stats.total.usedBytes);
        }

        vmaDestroyAllocator(renderer->memory_allocator);
    }

    if (renderer->device != VK_NULL_HANDLE) {
        vkDestroyDevice(renderer->device, NULL);
    }

    vk.deviceCount--;

    if (vk.deviceCount == 0)
    {
        if (vk.debug_utils_messenger != VK_NULL_HANDLE)
        {
            vk.api.vkDestroyDebugUtilsMessengerEXT(vk.instance, vk.debug_utils_messenger, NULL);
        }
        else if (vk.debug_report_callback != VK_NULL_HANDLE)
        {
            vk.api.vkDestroyDebugReportCallbackEXT(vk.instance, vk.debug_report_callback, NULL);
        }

        if (vk.instance != VK_NULL_HANDLE)
        {
            vk.api.vkDestroyInstance(vk.instance, NULL);
        }

        vkbUninit();
        vk.instance = VK_NULL_HANDLE;
    }

    VGPU_FREE(renderer);
    VGPU_FREE(device);
}

static void vk_waitIdle(VGPUDevice device) {
    VGPUVkRenderer* renderer = (VGPUVkRenderer*)device->renderer;
    VK_CHECK(renderer->api.vkDeviceWaitIdle(renderer->device));
}

static VGPUBackendType vk_getBackend(void) {
    return VGPUBackendType_Vulkan;
}

static vgpu_features vk_get_features(VGPURenderer* driver_data)
{
    VGPUVkRenderer* renderer = (VGPUVkRenderer*)driver_data;
    return renderer->features;
}

static vgpu_limits vk_get_limits(VGPURenderer* driver_data)
{
    VGPUVkRenderer* renderer = (VGPUVkRenderer*)driver_data;
    return renderer->limits;
}

static VGPURenderPass vk_get_default_render_pass(VGPURenderer* driver_data)
{
    VGPUVkRenderer* renderer = (VGPUVkRenderer*)driver_data;
    return nullptr;
}

static void vk_begin_frame(VGPURenderer* driver_data)
{

}

static void vk_end_frame(VGPURenderer* driver_data)
{

}

/* Texture */
static VkImageView vgpuVkCreateImageView(
    VGPURenderer* driverData,
    VkImage image,
    VkImageViewType  image_type,
    VkFormat         image_format,
    uint32_t         nmips,
    uint32_t         nlayers)
{
    const bool is_depth = image_format == VK_FORMAT_D16_UNORM ||
        image_format == VK_FORMAT_D16_UNORM_S8_UINT ||
        image_format == VK_FORMAT_D24_UNORM_S8_UINT ||
        image_format == VK_FORMAT_D32_SFLOAT ||
        image_format == VK_FORMAT_D32_SFLOAT_S8_UINT;

    const VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = image_type,
        .format = image_format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0u,
            .levelCount = nmips,
            .baseArrayLayer = 0u,
            .layerCount = nlayers
        }
    };

    VGPUVkRenderer* renderer = (VGPUVkRenderer*)driverData;
    VkImageView handle;
    const VkResult result = renderer->api.vkCreateImageView(renderer->device, &createInfo, NULL, &handle);
    if (result != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return handle;
}

static VGPUTexture vk_textureCreate(VGPURenderer* driverData, const VGPUTextureDescriptor* desc, const void* initial_data)
{
    VGPUVkRenderer* renderer = (VGPUVkRenderer*)driverData;
    return nullptr;
}

static VGPUTexture vk_textureCreateExternal(VGPURenderer* driverData, const VGPUTextureDescriptor* descriptor, void* handle)
{
    VGPUVkRenderer* renderer = (VGPUVkRenderer*)driverData;
    VGPUTextureVk* result = _VGPU_ALLOC_HANDLE(VGPUTextureVk);
    result->handle = (VkImage)handle;
    result->external = true;
    result->size = descriptor->size;
    return (VGPUTexture)result;
}

static void vk_textureDestroy(VGPURenderer* driverData, VGPUTexture handle)
{
    VGPUVkRenderer* renderer = (VGPUVkRenderer*)driverData;
    VGPUTextureVk* texture = (VGPUTextureVk*)handle;
    if (!texture->external)
    {
    }

    if (texture->view != VK_NULL_HANDLE)
    {
        vk.api.vkDestroyImageView(renderer->device, texture->view, NULL);
    }

    VGPU_FREE(handle);
}

/* Sampler */
static VGPUSampler vk_samplerCreate(VGPURenderer* driverData, const VGPUSamplerDescriptor* descriptor)
{
    return nullptr;
}

static void vk_samplerDestroy(VGPURenderer* driver_data, VGPUSampler sampler)
{

}

/* RenderPass */
static VGPURenderPass vk_renderPassCreate(VGPURenderer* driverData, const vgpu_render_pass_desc* desc)
{
    VGPUVkRenderer* renderer = (VGPUVkRenderer*)driverData;

    uint32_t attachmentCount = 0;
    VkAttachmentDescription attachments[VGPU_MAX_COLOR_ATTACHMENTS + 1];
    VkAttachmentReference references[VGPU_MAX_COLOR_ATTACHMENTS + 1];
    //VkImageView imageViews[VGPU_MAX_COLOR_ATTACHMENTS + 1];

    bool hasDepthStencil = desc->depth_stencil_attachment.texture != nullptr;
    uint32_t colorAttachmentCount = 0;
    for (uint32_t i = 0; i < VGPU_MAX_COLOR_ATTACHMENTS; i++)
    {
        if (!desc->color_attachments[i].texture) {
            continue;
        }

        VGPUTextureVk* texture = (VGPUTextureVk*)desc->color_attachments[i].texture;

        attachments[attachmentCount] = (VkAttachmentDescription){
            .format = texture->format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

        references[i] = (VkAttachmentReference){ i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        attachmentCount++;
        colorAttachmentCount++;
    }

    VkSubpassDescription subpass = {
        .colorAttachmentCount = colorAttachmentCount,
        .pColorAttachments = references,
        .pDepthStencilAttachment = hasDepthStencil ? &references[colorAttachmentCount] : NULL
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = attachmentCount,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass
    };

    VkRenderPass renderPass;
    if (renderer->api.vkCreateRenderPass(renderer->device, &renderPassInfo, NULL, &renderPass)) {
        return nullptr;
    }

    return nullptr;
}

static void vk_renderPassDestroy(VGPURenderer* driver_data, VGPURenderPass handle)
{

}

static void vk_renderPassGetExtent(VGPURenderer* driver_data, VGPURenderPass handle, uint32_t* width, uint32_t* height)
{

}

/* Commands */
static void vk_cmd_begin_render_pass(VGPURenderer* driver_data, const vgpu_begin_render_pass_desc* begin_pass_desc)
{

}

static void vk_cmd_end_render_pass(VGPURenderer* driver_data)
{

}

bool vgpu_vk_supported(void) {
    if (vk.available_initialized) {
        return vk.available;
    }

    vk.available_initialized = true;

    VkResult result = vkbInit(&vk.api);
    if (result != VK_SUCCESS) {
        vgpu_log_error("Failed to initialize vkbind.");
        return false;
    }

    vk.available = true;
    return true;
}

VGPUDevice vk_create_device(void) {
    VGPUDevice device;
    VGPUVkRenderer* renderer;

    device = (VGPUDeviceImpl*)VGPU_MALLOC(sizeof(VGPUDeviceImpl));
    ASSIGN_DRIVER(vk);

    /* Init the vk renderer */
    renderer = (VGPUVkRenderer*)_VGPU_ALLOC_HANDLE(VGPUVkRenderer);

    /* Reference vgpu_device and renderer together. */
    renderer->gpu_device = device;
    device->renderer = (VGPURenderer*)renderer;

    return device;
}
