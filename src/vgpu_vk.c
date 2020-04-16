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
#include "stb_ds.h"

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
    VGPUColor       colorClearValue;
    uint32_t imageIndex;
    uint32_t imageCount;
    VGPUTexture backbufferTextures[4];
    VGPUTexture depthStencilTexture;
    VGPURenderPass renderPasses[4];
} VGPUSwapchainVk;

typedef struct {
    VkBuffer handle;
    VmaAllocation memory;
} VGPUBufferVk;

typedef struct {
    VkFormat format;
    VkImage handle;
    VkImageView view;
    VmaAllocation memory;
    bool external;
    VGPUTextureLayout layout;
    VGPUExtent3D size;
} VGPUTextureVk;

typedef struct {
    VkSampler handle;
} VGPUSamplerVk;

typedef struct {
    VkRenderPass renderPass;
    VkFramebuffer framebuffer;
    VkRect2D renderArea;
    uint32_t attachmentCount;
    VGPUTexture textures[VGPU_MAX_COLOR_ATTACHMENTS + 1];
    VkClearValue clears[VGPU_MAX_COLOR_ATTACHMENTS + 1];
} VGPURenderPassVk;

typedef struct {
    VkObjectType type;
    void* handle1;
    void* handle2;
} VGPUVkObjectRef;

typedef struct {
    VGPUVkObjectRef* data;
    size_t capacity;
    size_t length;
} VGPUVkFreeList;

typedef struct {
    uint32_t index;
    VkFence fence;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderCompleteSemaphore;
    VkCommandBuffer commandBuffer;
    VGPUVkFreeList freeList;
} VGPUVkFrame;

typedef struct
{
    uint32_t colorFormatsCount;
    VkFormat colorFormats[VGPU_MAX_COLOR_ATTACHMENTS];
    VGPULoadOp loadOperations[VGPU_MAX_COLOR_ATTACHMENTS];
    VkFormat depthStencilFormat;
} RenderPassHash;

typedef struct
{
    RenderPassHash key;
    VkRenderPass value;
} RenderPassHashMap;

typedef struct VGPURendererVk {
    /* Associated vgpu_device */
    VGPUDevice gpu_device;

    VkPhysicalDevice physical_device;
    VGPUVkQueueFamilyIndices queue_families;

    bool api_version_12;
    vk_physical_device_features device_features;

    VGPUFeatures features;
    vgpu_limits limits;

    VkbAPI api;

    VkDevice device;
    struct vk_device_table* table;
    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue copy_queue;
    VmaAllocator allocator;
    VkCommandPool commandPool;

    VGPUVkFrame frames[3];
    VGPUVkFrame* frame;
    uint32_t max_inflight_frames;

    VGPUSwapchainVk swapchains[_VGPU_VK_MAX_SWAPCHAINS];

    RenderPassHashMap* renderPassHashMap;
} VGPURendererVk;

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

/* Conversion functions */
static inline VkFormat GetVkFormat(VGPUTextureFormat format)
{
    static VkFormat formats[VGPUTextureFormat_Count] = {
        VK_FORMAT_UNDEFINED,
        // 8-bit pixel formats
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8_SNORM,
        VK_FORMAT_R8_UINT,
        VK_FORMAT_R8_SINT,
        // 16-bit pixel formats
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16_SNORM,
        VK_FORMAT_R16_UINT,
        VK_FORMAT_R16_SINT,
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8_SNORM,
        VK_FORMAT_R8G8_UINT,
        VK_FORMAT_R8G8_SINT,

        // Packed 16-bit pixel formats
        //VK_FORMAT_B5G6R5_UNORM_PACK16,
        //VK_FORMAT_B4G4R4A4_UNORM_PACK16,

        // 32-bit pixel formats
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R32_SINT,
        VK_FORMAT_R32_SFLOAT,
        //VK_FORMAT_R16G16_UNORM,
        //VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_R16G16_UINT,
        VK_FORMAT_R16G16_SINT,
        VK_FORMAT_R16G16_SFLOAT,

        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_FORMAT_R8G8B8A8_SINT,

        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8A8_SRGB,

        // Packed 32-Bit Pixel formats
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,

        // 64-Bit Pixel Formats
        VK_FORMAT_R32G32_UINT,
        VK_FORMAT_R32G32_SINT,
        VK_FORMAT_R32G32_SFLOAT,
        //VK_FORMAT_R16G16B16A16_UNORM,
        //VK_FORMAT_R16G16B16A16_SNORM,
        VK_FORMAT_R16G16B16A16_UINT,
        VK_FORMAT_R16G16B16A16_SINT,
        VK_FORMAT_R16G16B16A16_SFLOAT,

        // 128-Bit Pixel Formats
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R32G32B32A32_SINT,
        VK_FORMAT_R32G32B32A32_SFLOAT,

        // Depth-stencil formats
        //VK_FORMAT_D16_UNORM,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT, /* Dawn maps to VK_FORMAT_D32_SFLOAT */
        VK_FORMAT_D32_SFLOAT_S8_UINT,

        // Compressed BC formats
        VK_FORMAT_BC1_RGB_UNORM_BLOCK,
        VK_FORMAT_BC1_RGB_SRGB_BLOCK,
        VK_FORMAT_BC2_UNORM_BLOCK,
        VK_FORMAT_BC2_SRGB_BLOCK,
        VK_FORMAT_BC3_UNORM_BLOCK,
        VK_FORMAT_BC3_SRGB_BLOCK,
        VK_FORMAT_BC4_UNORM_BLOCK,
        VK_FORMAT_BC4_SNORM_BLOCK,
        VK_FORMAT_BC5_UNORM_BLOCK,
        VK_FORMAT_BC5_SNORM_BLOCK,
        VK_FORMAT_BC6H_UFLOAT_BLOCK,
        VK_FORMAT_BC6H_SFLOAT_BLOCK,
        VK_FORMAT_BC7_UNORM_BLOCK,
        VK_FORMAT_BC7_SRGB_BLOCK,
    };

    return formats[format];
}

static inline VkImageAspectFlags GetVkAspectMask(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_UNDEFINED:
        return 0;

    case VK_FORMAT_S8_UINT:
        return VK_IMAGE_ASPECT_STENCIL_BIT;

    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;

    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        return VK_IMAGE_ASPECT_DEPTH_BIT;

    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

static VkCompareOp getVkCompareOp(VGPUCompareFunction function, VkCompareOp defaultValue)
{
    switch (function)
    {
    case VGPUCompareFunction_Never:
        return VK_COMPARE_OP_NEVER;
    case VGPUCompareFunction_Less:
        return VK_COMPARE_OP_LESS;
    case VGPUCompareFunction_LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case VGPUCompareFunction_Greater:
        return VK_COMPARE_OP_GREATER;
    case VGPUCompareFunction_GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case VGPUCompareFunction_Equal:
        return VK_COMPARE_OP_EQUAL;
    case VGPUCompareFunction_NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case VGPUCompareFunction_Always:
        return VK_COMPARE_OP_ALWAYS;

    default:
        return defaultValue;
    }
}

/* Helper functions .*/
static void vgpuVkDeferredDestroy(VGPURendererVk* renderer, void* handle1, void* handle2, VkObjectType type)
{
    VGPUVkFreeList* freelist = &renderer->frame->freeList;

    if (freelist->length >= freelist->capacity) {
        freelist->capacity = freelist->capacity ? (freelist->capacity * 2) : 1;
        freelist->data = realloc(freelist->data, freelist->capacity * sizeof(*freelist->data));
        VGPU_CHECK(freelist->data, "Out of memory");
    }

    freelist->data[freelist->length++] = (VGPUVkObjectRef){ type, handle1, handle2 };
}

static void vgpuVkProcessDeferredDestroy(VGPURendererVk* renderer, VGPUVkFrame* frame)
{
    for (size_t i = 0; i < frame->freeList.length; i++)
    {
        VGPUVkObjectRef* ref = &frame->freeList.data[i];
        switch (ref->type)
        {
        case VK_OBJECT_TYPE_BUFFER:
            vmaDestroyBuffer(renderer->allocator, (VkBuffer)ref->handle1, (VmaAllocation)ref->handle2);
            break;

        case VK_OBJECT_TYPE_IMAGE:
            vmaDestroyImage(renderer->allocator, (VkImage)ref->handle1, (VmaAllocation)ref->handle2);
            break;

        case VK_OBJECT_TYPE_IMAGE_VIEW:
            renderer->api.vkDestroyImageView(renderer->device, (VkImageView)ref->handle1, NULL);
            break;

        case VK_OBJECT_TYPE_SAMPLER:
            renderer->api.vkDestroySampler(renderer->device, (VkSampler)ref->handle1, NULL);
            break;

        case VK_OBJECT_TYPE_RENDER_PASS:
            renderer->api.vkDestroyRenderPass(renderer->device, (VkRenderPass)ref->handle1, NULL);
            break;

        case VK_OBJECT_TYPE_FRAMEBUFFER:
            renderer->api.vkDestroyFramebuffer(renderer->device, (VkFramebuffer)ref->handle1, NULL);
            break;

        case VK_OBJECT_TYPE_PIPELINE:
            renderer->api.vkDestroyPipeline(renderer->device, (VkPipeline)ref->handle1, NULL);
            break;
        default:
            VGPU_UNREACHABLE();
            break;
        }
    }

    frame->freeList.length = 0;
}

/* Barriers */
static VkAccessFlagBits vgpuVkGetAccessMask(VGPUTextureLayout state, VkImageAspectFlags aspectMask)
{
    switch (state)
    {
    case VGPUTextureLayout_Undefined:
    case VGPUTextureLayout_General:
    case VGPUTextureLayout_Present:
        //case VGPUTextureLayout_PreInitialized:
        return (VkAccessFlagBits)0;
    case VGPUTextureLayout_RenderTarget:
        if (aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
        {
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        }

        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    case VGPUTextureLayout_ShaderRead:
        return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

    case VGPUTextureLayout_ShaderWrite:
        return VK_ACCESS_SHADER_WRITE_BIT;

        /*case VGPUTextureLayout_ResolveDest:
        case VGPUTextureLayout_CopyDest:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VGPUTextureLayout_ResolveSource:
        case VGPUTextureLayout_CopySource:
            return VK_ACCESS_TRANSFER_READ_BIT;*/

    default:
        VGPU_UNREACHABLE();
        return (VkAccessFlagBits)-1;
    }
}

VkImageLayout vgpuVkGetImageLayout(VGPUTextureLayout layout, VkImageAspectFlags aspectMask)
{
    switch (layout)
    {
    case VGPUTextureLayout_Undefined:
        return VK_IMAGE_LAYOUT_UNDEFINED;

    case VGPUTextureLayout_General:
        return VK_IMAGE_LAYOUT_GENERAL;

    case VGPUTextureLayout_RenderTarget:
        if (aspectMask == VK_IMAGE_ASPECT_COLOR_BIT) {
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        //case VGPUTextureLayout_DepthStencilReadOnly:
        //    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    case VGPUTextureLayout_ShaderRead:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    case VGPUTextureLayout_ShaderWrite:
        return VK_IMAGE_LAYOUT_GENERAL;

    case VGPUTextureLayout_Present:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    default:
        _VGPU_UNREACHABLE();
        return (VkImageLayout)-1;
    }
}

static VkPipelineStageFlags vgpuVkGetShaderStageMask(VGPUTextureLayout layout, VkImageAspectFlags aspectMask, bool src)
{
    switch (layout)
    {
    case VGPUTextureLayout_Undefined:
    case VGPUTextureLayout_General:
        assert(src);
        return src ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : (VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    case VGPUTextureLayout_ShaderRead:
    case VGPUTextureLayout_ShaderWrite:
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; // #OPTME Assume the worst
    case VGPUTextureLayout_RenderTarget:
        if (aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
        {
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }

        return src ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        /*case Resource::State::IndirectArg:
            return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        case VGPUTextureLayout_CopyDest:
        case VGPUTextureLayout_CopySource:
        case VGPUTextureLayout_ResolveDest:
        case VGPUTextureLayout_ResolveSource:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;*/
    case VGPUTextureLayout_Present:
        return src ? (VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    default:
        _VGPU_UNREACHABLE();
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

static void vgpuVkTextureBarrier(
    VGPURenderer* driverData,
    VkCommandBuffer commandBuffer,
    VGPUTexture handle,
    VGPUTextureLayout newState)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    VGPUTextureVk* texture = (VGPUTextureVk*)handle;

    if (texture->layout == newState) {
        return;
    }

    const VkImageAspectFlags aspectMask = GetVkAspectMask(texture->format);

    // Create an image barrier object
    const VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = vgpuVkGetAccessMask(texture->layout, aspectMask),
        .dstAccessMask = vgpuVkGetAccessMask(newState, aspectMask),
        .oldLayout = vgpuVkGetImageLayout(texture->layout, aspectMask),
        .newLayout = vgpuVkGetImageLayout(newState, aspectMask),
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture->handle,
        .subresourceRange = {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    renderer->api.vkCmdPipelineBarrier(
        commandBuffer,
        vgpuVkGetShaderStageMask(texture->layout, aspectMask, true),
        vgpuVkGetShaderStageMask(newState, aspectMask, false),
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    texture->layout = newState;
}

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
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;

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

    VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

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

    VGPURenderPassDescriptor passDesc;
    memset(&passDesc, 0, sizeof(VGPURenderPassDescriptor));

    for (uint32_t i = 0; i < swapchain->imageCount; i++)
    {
        swapchain->backbufferTextures[i] = vgpuTextureCreateExternal(&textureDesc, swapChainImages[i]);
        passDesc.colorAttachments[0].texture = swapchain->backbufferTextures[i];
        passDesc.colorAttachments[0].loadOp = VGPULoadOp_Clear;
        passDesc.colorAttachments[0].clearColor = swapchain->colorClearValue;
        swapchain->renderPasses[i] = vgpuRenderPassCreate(&passDesc);
    }

    swapchain->depthStencilTexture = nullptr;

    return true;
}

static void vgpuVkSwapchainDestroy(VGPURenderer* driverData, VGPUSwapchainVk* swapchain)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    if (swapchain->depthStencilTexture) {
        vgpuTextureDestroy(swapchain->depthStencilTexture);
    }

    for (uint32_t i = 0; i < swapchain->imageCount; i++)
    {
        vgpuTextureDestroy(swapchain->backbufferTextures[i]);
        vgpuRenderPassDestroy(swapchain->renderPasses[i]);
    }

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
    VGPURendererVk* renderer = (VGPURendererVk*)device->renderer;
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

    /* Init hash maps */
    hmdefault(renderer->renderPassHashMap, NULL);

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
        result = vmaCreateAllocator(&allocator_info, &renderer->allocator);
        if (result != VK_SUCCESS) {
            vgpu_log_error("Vulkan: Cannot create memory allocator.");
            vgpuShutdown();
            return false;
        }
    }

    /* Init features and limits. */
    renderer->features.independentBlend = features.features.independentBlend;
    renderer->features.computeShader = true;
    renderer->features.geometryShader = features.features.geometryShader;
    renderer->features.tessellationShader = features.features.tessellationShader;
    renderer->features.multiViewport = features.features.multiViewport;
    renderer->features.indexUint32 = features.features.fullDrawIndexUint32;
    renderer->features.multiDrawIndirect = features.features.multiDrawIndirect;
    renderer->features.fillModeNonSolid = features.features.fillModeNonSolid;
    renderer->features.samplerAnisotropy = features.features.samplerAnisotropy;
    renderer->features.textureCompressionETC2 = features.features.textureCompressionETC2;
    renderer->features.textureCompressionASTC_LDR = features.features.textureCompressionASTC_LDR;
    renderer->features.textureCompressionBC = features.features.textureCompressionBC;
    renderer->features.textureCubeArray = features.features.imageCubeArray;
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
        renderer->swapchains[0].colorClearValue = desc->swapchain->colorClearValue;
        renderer->swapchains[0].presentMode = vgpuVkGetPresentMode(desc->swapchain->presentMode);

        if (!vgpuVkSwapchainInit(device->renderer, &renderer->swapchains[0]))
        {
            vgpuShutdown();
            return false;
        }
    }

    {
        const VkCommandPoolCreateInfo commandPoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = renderer->queue_families.graphics_queue_family
        };

        if (renderer->api.vkCreateCommandPool(renderer->device, &commandPoolInfo, NULL, &renderer->commandPool) != VK_SUCCESS)
        {
            vgpuShutdown();
            return false;
        }
    }

    renderer->max_inflight_frames = 2u;
    {
        // Frame state
        renderer->frame = &renderer->frames[0];

        const VkCommandBufferAllocateInfo commandBufferInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = renderer->commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        const VkSemaphoreCreateInfo semaphoreInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0
        };

        const VkFenceCreateInfo fenceInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        for (uint32_t i = 0; i < renderer->max_inflight_frames; i++)
        {
            renderer->frames[i].index = i;

            if (renderer->api.vkAllocateCommandBuffers(renderer->device, &commandBufferInfo, &renderer->frames[i].commandBuffer) != VK_SUCCESS)
            {
                vgpuShutdown();
                return false;
            }

            if (renderer->api.vkCreateFence(renderer->device, &fenceInfo, NULL, &renderer->frames[i].fence) != VK_SUCCESS)
            {
                vgpuShutdown();
                return false;
            }

            if (renderer->api.vkCreateSemaphore(renderer->device, &semaphoreInfo, NULL, &renderer->frames[i].imageAvailableSemaphore) != VK_SUCCESS)
            {
                vgpuShutdown();
                return false;
            }

            if (renderer->api.vkCreateSemaphore(renderer->device, &semaphoreInfo, NULL, &renderer->frames[i].renderCompleteSemaphore) != VK_SUCCESS)
            {
                vgpuShutdown();
                return false;
            }
        }
    }

    /* Increase device being created. */
    vk.deviceCount++;

    return true;
}

static void vk_waitIdle(VGPUDevice device);

static void vk_destroy(VGPUDevice device)
{
    VGPURendererVk* renderer = (VGPURendererVk*)device->renderer;

    if (renderer->device != VK_NULL_HANDLE) {
        VK_CHECK(renderer->api.vkDeviceWaitIdle(renderer->device));
    }

    /* Destroy swap chains.*/
    for (uint32_t i = 0; i < _VGPU_VK_MAX_SWAPCHAINS; i++)
    {
        if (renderer->swapchains[i].handle == VK_NULL_HANDLE)
            continue;

        vgpuVkSwapchainDestroy(device->renderer, &renderer->swapchains[i]);
    }

    /* Destroy hashed objects */
    {
        for (uint32_t i = 0; i < hmlenu(renderer->renderPassHashMap); i++)
        {
            renderer->api.vkDestroyRenderPass(
                renderer->device,
                renderer->renderPassHashMap[i].value,
                NULL
            );
        }
    }

    /* Destroy frame data. */
    for (uint32_t i = 0; i < renderer->max_inflight_frames; i++)
    {
        VGPUVkFrame* frame = &renderer->frames[i];

        vgpuVkProcessDeferredDestroy(renderer, frame);
        free(frame->freeList.data);

        if (frame->fence) {
            renderer->api.vkDestroyFence(renderer->device, frame->fence, NULL);
        }

        if (frame->imageAvailableSemaphore) {
            renderer->api.vkDestroySemaphore(renderer->device, frame->imageAvailableSemaphore, NULL);
        }

        if (frame->renderCompleteSemaphore) {
            renderer->api.vkDestroySemaphore(renderer->device, frame->renderCompleteSemaphore, NULL);
        }

        if (frame->commandBuffer) {
            renderer->api.vkFreeCommandBuffers(renderer->device, renderer->commandPool, 1, &frame->commandBuffer);
        }
    }

    if (renderer->commandPool) {
        renderer->api.vkDestroyCommandPool(renderer->device, renderer->commandPool, NULL);
    }

    if (renderer->allocator != VK_NULL_HANDLE)
    {
        VmaStats stats;
        vmaCalculateStats(renderer->allocator, &stats);

        if (stats.total.usedBytes > 0) {
            vgpu_log_format(VGPU_LOG_LEVEL_ERROR, "Total device memory leaked: %llx bytes.", stats.total.usedBytes);
        }

        vmaDestroyAllocator(renderer->allocator);
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
    VGPURendererVk* renderer = (VGPURendererVk*)device->renderer;
    VK_CHECK(renderer->api.vkDeviceWaitIdle(renderer->device));
}

static VGPUBackendType vk_getBackend(void) {
    return VGPUBackendType_Vulkan;
}

static VGPUFeatures vk_getFeatures(VGPURenderer* driverData)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    return renderer->features;
}

static vgpu_limits vk_getLimits(VGPURenderer* driverData)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    return renderer->limits;
}

static VGPURenderPass vk_get_default_render_pass(VGPURenderer* driverData)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    uint32_t imageIndex = renderer->swapchains[0].imageIndex;
    return renderer->swapchains[0].renderPasses[imageIndex];
}

static void vk_begin_frame(VGPURenderer* driverData)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    VK_CHECK(renderer->api.vkWaitForFences(renderer->device, 1, &renderer->frame->fence, VK_FALSE, ~0ull));
    VK_CHECK(renderer->api.vkResetFences(renderer->device, 1, &renderer->frame->fence));
    vgpuVkProcessDeferredDestroy(renderer, renderer->frame);

    VkResult result = renderer->api.vkAcquireNextImageKHR(
        renderer->device,
        renderer->swapchains[0].handle,
        UINT64_MAX,
        renderer->frame->imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &renderer->swapchains[0].imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR ||
        result == VK_SUBOPTIMAL_KHR)
    {
    }
    else
    {
        VK_CHECK(result);
    }

    const VkCommandBufferBeginInfo beginfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(renderer->api.vkBeginCommandBuffer(renderer->frame->commandBuffer, &beginfo));
}

static void vk_end_frame(VGPURenderer* driverData)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;

    uint32_t imageIndex = renderer->swapchains[0].imageIndex;
    vgpuVkTextureBarrier(driverData,
        renderer->frame->commandBuffer,
        renderer->swapchains[0].backbufferTextures[imageIndex],
        VGPUTextureLayout_Present
    );

    VK_CHECK(renderer->api.vkEndCommandBuffer(renderer->frame->commandBuffer));


    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1u,
        .pWaitSemaphores = &renderer->frame->imageAvailableSemaphore,
        .pWaitDstStageMask = &waitDstStageMask,
        .pCommandBuffers = &renderer->frame->commandBuffer,
        .commandBufferCount = 1u,
        .signalSemaphoreCount = 1u,
        .pSignalSemaphores = &renderer->frame->renderCompleteSemaphore
    };

    VK_CHECK(vkQueueSubmit(renderer->graphics_queue, 1, &submitInfo, renderer->frame->fence));

    /* Present swap chains. */
    const VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1u,
        .pWaitSemaphores = &renderer->frame->renderCompleteSemaphore,
        .swapchainCount = 1u,
        .pSwapchains = &renderer->swapchains[0].handle,
        .pImageIndices = &renderer->swapchains[0].imageIndex,
        .pResults = NULL
    };

    VkResult result = renderer->api.vkQueuePresentKHR(renderer->graphics_queue, &presentInfo);
    if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR)))
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            //windowResize();
            return;
        }
        else {
            VK_CHECK(result);
        }
    }

    /* Advance to next frame. */
    renderer->frame = &renderer->frames[(renderer->frame->index + 1) % renderer->max_inflight_frames];
}



/* Buffer */
static VGPUBuffer vk_bufferCreate(VGPURenderer* driverData, const VGPUBufferDescriptor* descriptor)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    VGPUBufferVk* result = _VGPU_ALLOC_HANDLE(VGPUBufferVk);
    return (VGPUBuffer)result;
}

static void vk_bufferDestroy(VGPURenderer* driverData, VGPUBuffer handle)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    VGPUBufferVk* buffer = (VGPUBufferVk*)handle;
    vgpuVkDeferredDestroy(renderer, buffer->handle, buffer->memory, VK_OBJECT_TYPE_BUFFER);
    VGPU_FREE(buffer);
}

/* Texture */
static VGPUTexture vk_textureCreate(VGPURenderer* driverData, const VGPUTextureDescriptor* descriptor, const void* initial_data)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    VGPUTextureVk* result = _VGPU_ALLOC_HANDLE(VGPUTextureVk);
    result->format = GetVkFormat(descriptor->format);
    result->layout = VGPUTextureLayout_Undefined;
    return nullptr;
}

static VGPUTexture vk_textureCreateExternal(VGPURenderer* driverData, const VGPUTextureDescriptor* descriptor, void* handle)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    VGPUTextureVk* result = _VGPU_ALLOC_HANDLE(VGPUTextureVk);
    result->format = GetVkFormat(descriptor->format);
    result->handle = (VkImage)handle;
    result->external = true;
    result->layout = VGPUTextureLayout_Undefined;
    result->size = descriptor->size;
    return (VGPUTexture)result;
}

static void vk_textureDestroy(VGPURenderer* driverData, VGPUTexture handle)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
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
static inline VkFilter getVkFilter(VGPUFilterMode filter)
{
    switch (filter)
    {
    case VGPUFilterMode_Nearest:
        return VK_FILTER_NEAREST;
    case VGPUFilterMode_Linear:
        return VK_FILTER_LINEAR;
    default:
        _VGPU_UNREACHABLE();
        return VK_FILTER_NEAREST;
    }
}

static inline VkSamplerMipmapMode getVkMipMapFilterMode(VGPUFilterMode filter)
{
    switch (filter)
    {
    case VGPUFilterMode_Nearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case VGPUFilterMode_Linear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default:
        _VGPU_UNREACHABLE();
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }
}

VkSamplerAddressMode getVkAddressMode(VGPUAddressMode mode)
{
    switch (mode)
    {
    case VGPUAddressMode_Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case VGPUAddressMode_MirrorRepeat:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case VGPUAddressMode_ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case VGPUAddressMode_ClampToBorderColor:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        //case VGPUAddressMode_MirrorClampToEdge:
         //   return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    default:
        _VGPU_UNREACHABLE();
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

static inline VkBorderColor getVkBorderColor(VGPUBorderColor value)
{
    switch (value)
    {
    case VGPUBorderColor_TransparentBlack:
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    case VGPUBorderColor_OpaqueBlack:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    case VGPUBorderColor_OpaqueWhite:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    default:
        _VGPU_UNREACHABLE();
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    }
}

static VGPUSampler vk_samplerCreate(VGPURenderer* driverData, const VGPUSamplerDescriptor* descriptor)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    const VkBool32 compareEnable =
        (descriptor->compare != VGPUCompareFunction_Undefined) &&
        (descriptor->compare != VGPUCompareFunction_Never);

    const VkSamplerCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .magFilter = getVkFilter(descriptor->magFilter),
        .minFilter = getVkFilter(descriptor->minFilter),
        .mipmapMode = getVkMipMapFilterMode(descriptor->mipmapFilter),
        .addressModeU = getVkAddressMode(descriptor->addressModeU),
        .addressModeV = getVkAddressMode(descriptor->addressModeV),
        .addressModeW = getVkAddressMode(descriptor->addressModeW),
        .mipLodBias = 0.0f,
        .anisotropyEnable = descriptor->maxAnisotropy > 0,
        .maxAnisotropy = (float)descriptor->maxAnisotropy,
        .compareEnable = compareEnable,
        .compareOp = getVkCompareOp(descriptor->compare, VK_COMPARE_OP_NEVER),
        .minLod = descriptor->lodMinClamp,
        .minLod = descriptor->lodMaxClamp == 0.0f ? 3.402823466e+38F : descriptor->lodMaxClamp,
        .borderColor = getVkBorderColor(descriptor->borderColor),
        .unnormalizedCoordinates = VK_FALSE
    };

    VkSampler handle;
    if (renderer->api.vkCreateSampler(renderer->device, &createInfo, NULL, &handle) != VK_SUCCESS)
    {
        return nullptr;
    }

    VGPUSamplerVk* result = _VGPU_ALLOC_HANDLE(VGPUSamplerVk);
    result->handle = handle;
    return (VGPUSampler)result;
}

static void vk_samplerDestroy(VGPURenderer* driverData, VGPUSampler handle)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    VGPUSamplerVk* sampler = (VGPUSamplerVk*)handle;
    vgpuVkDeferredDestroy(renderer, sampler->handle, NULL, VK_OBJECT_TYPE_SAMPLER);
    VGPU_FREE(sampler);
}

/* RenderPass */
static RenderPassHash vk_GetRenderPassHash(const VGPURenderPassDescriptor* descriptor)
{
    RenderPassHash hash = {
        .colorFormatsCount = 0,
        .depthStencilFormat = VK_FORMAT_UNDEFINED
    };

    for (uint32_t i = 0; i < VGPU_MAX_COLOR_ATTACHMENTS; i++)
    {
        if (!descriptor->colorAttachments[i].texture) {
            continue;
        }

        VGPUTextureVk* texture = (VGPUTextureVk*)descriptor->colorAttachments[i].texture;

        hash.colorFormats[hash.colorFormatsCount] = texture->format;
        hash.loadOperations[hash.colorFormatsCount] = descriptor->colorAttachments[i].loadOp;
        hash.colorFormatsCount++;
    }


    if (descriptor->depthStencilAttachment.texture)
    {
        VGPUTextureVk* texture = (VGPUTextureVk*)descriptor->depthStencilAttachment.texture;
        hash.depthStencilFormat = texture->format;
    }

    return hash;
}

static VkRenderPass vk_GetRenderPass(VGPURenderer* driverData, const VGPURenderPassDescriptor* descriptor)
{
    static const VkAttachmentLoadOp loadOps[] = {
        [VGPULoadOp_DontCare] = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        [VGPULoadOp_Load] = VK_ATTACHMENT_LOAD_OP_LOAD,
        [VGPULoadOp_Clear] = VK_ATTACHMENT_LOAD_OP_CLEAR,
    };

    VGPURendererVk* renderer = (VGPURendererVk*)driverData;

    RenderPassHash hash = vk_GetRenderPassHash(descriptor);

    /* Lookup hash first */
    if (stbds_hmgeti(renderer->renderPassHashMap, hash) != -1)
    {
        return stbds_hmget(renderer->renderPassHashMap, hash);
    }

    uint32_t attachmentCount = hash.colorFormatsCount;
    VkAttachmentDescription attachments[VGPU_MAX_COLOR_ATTACHMENTS + 1];
    VkAttachmentReference references[VGPU_MAX_COLOR_ATTACHMENTS + 1];

    for (uint32_t i = 0; i < hash.colorFormatsCount; i++)
    {
        attachments[i] = (VkAttachmentDescription){
            .format = hash.colorFormats[i],
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = loadOps[hash.loadOperations[i]],
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

        references[i] = (VkAttachmentReference){ i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    }

    if (hash.depthStencilFormat != VK_FORMAT_UNDEFINED)
    {
        uint32_t i = attachmentCount++;
        attachments[i] = (VkAttachmentDescription){
            .format = hash.depthStencilFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        references[i] = (VkAttachmentReference){ i, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    }

    VkSubpassDescription subpass = {
        .flags = 0u,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0u,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = hash.colorFormatsCount,
        .pColorAttachments = references,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = (hash.depthStencilFormat != VK_FORMAT_UNDEFINED) ? &references[hash.colorFormatsCount] : NULL,
        .preserveAttachmentCount = 0u,
        .pPreserveAttachments = nullptr
    };

    const VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .attachmentCount = attachmentCount,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0u,
        .pDependencies = nullptr
    };

    VkRenderPass renderPass;
    if (renderer->api.vkCreateRenderPass(renderer->device, &renderPassInfo, NULL, &renderPass) != VK_SUCCESS)
    {
        return nullptr;
    }

    hmput(renderer->renderPassHashMap, hash, renderPass);
    return renderPass;
}

static VGPURenderPass vk_renderPassCreate(VGPURenderer* driverData, const VGPURenderPassDescriptor* descriptor)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    VGPURenderPassVk* renderPass = _VGPU_ALLOC_HANDLE(VGPURenderPassVk);
    renderPass->renderPass = vk_GetRenderPass(driverData, descriptor);

    renderPass->attachmentCount = 0;
    VkImageView attachments[VGPU_MAX_COLOR_ATTACHMENTS + 1];

    renderPass->renderArea.offset = (VkOffset2D){ 0, 0 };
    renderPass->renderArea.extent.width = UINT32_MAX;
    renderPass->renderArea.extent.height = UINT32_MAX;

    for (uint32_t i = 0; i < VGPU_MAX_COLOR_ATTACHMENTS; i++)
    {
        if (!descriptor->colorAttachments[i].texture) {
            continue;
        }

        VGPUTextureVk* texture = (VGPUTextureVk*)descriptor->colorAttachments[i].texture;

        uint32_t mipLevel = descriptor->colorAttachments[i].mipLevel;
        renderPass->renderArea.extent.width = _vgpu_min(renderPass->renderArea.extent.width, _vgpu_max(1u, texture->size.width >> mipLevel));
        renderPass->renderArea.extent.height = _vgpu_min(renderPass->renderArea.extent.height, _vgpu_max(1u, texture->size.height >> mipLevel));

        VkImageViewCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = texture->handle,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = texture->format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = GetVkAspectMask(texture->format),
                .baseMipLevel = mipLevel,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = descriptor->colorAttachments[i].slice,
                .layerCount = VK_REMAINING_ARRAY_LAYERS
            }
        };

        VK_CHECK(renderer->api.vkCreateImageView(renderer->device, &createInfo, NULL, &texture->view));

        attachments[renderPass->attachmentCount] = texture->view;
        renderPass->textures[i] = descriptor->colorAttachments[i].texture;
        memcpy(renderPass->clears[i].color.float32, &descriptor->colorAttachments[i].clearColor, 4 * sizeof(float));
        renderPass->attachmentCount++;
    }

    VkFramebufferCreateInfo framebufferInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .renderPass = renderPass->renderPass,
        .attachmentCount = renderPass->attachmentCount,
        .pAttachments = attachments,
        .width = renderPass->renderArea.extent.width,
        .height = renderPass->renderArea.extent.height,
        .layers = 1
    };

    if (renderer->api.vkCreateFramebuffer(renderer->device, &framebufferInfo, NULL, &renderPass->framebuffer) != VK_SUCCESS)
    {
        VGPU_FREE(renderPass);
        return nullptr;
    }

    return (VGPURenderPass)renderPass;
}

static void vk_renderPassDestroy(VGPURenderer* driverData, VGPURenderPass handle)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    VGPURenderPassVk* renderPass = (VGPURenderPassVk*)handle;
    vgpuVkDeferredDestroy(renderer, renderPass->framebuffer, nullptr, VK_OBJECT_TYPE_FRAMEBUFFER);
    VGPU_FREE(renderPass);
}

static void vk_renderPassGetExtent(VGPURenderer* driver_data, VGPURenderPass handle, uint32_t* width, uint32_t* height)
{
    VGPURenderPassVk* renderPass = (VGPURenderPassVk*)handle;
    if (width) {
        *width = renderPass->renderArea.extent.width;
    }

    if (height) {
        *height = renderPass->renderArea.extent.height;
    }
}

void vk_renderPassSetClearColors(VGPURenderer* driverData, VGPURenderPass handle, uint32_t firstAttachment, uint32_t count, const VGPUColor* pColors)
{
    VGPURenderPassVk* renderPass = (VGPURenderPassVk*)handle;
    for (uint32_t i = firstAttachment; i < count; i++)
    {
        memcpy(renderPass->clears[i].color.float32, &pColors[i].r, sizeof(VGPUColor));
    }
}

/* Commands */
static void vk_cmdBeginRenderPass(VGPURenderer* driverData, VGPURenderPass handle)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    VGPURenderPassVk* renderPass = (VGPURenderPassVk*)handle;

    for (uint32_t i = 0; i < renderPass->attachmentCount; i++) {
        vgpuVkTextureBarrier(driverData,
            renderer->frame->commandBuffer,
            renderPass->textures[i],
            VGPUTextureLayout_RenderTarget
        );
    }

    const VkRenderPassBeginInfo beginfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass->renderPass,
        .framebuffer = renderPass->framebuffer,
        .renderArea = renderPass->renderArea,
        .clearValueCount = renderPass->attachmentCount,
        .pClearValues = renderPass->clears
    };

    renderer->api.vkCmdBeginRenderPass(renderer->frame->commandBuffer, &beginfo, VK_SUBPASS_CONTENTS_INLINE);
}

static void vk_cmdEndRenderPass(VGPURenderer* driverData)
{
    VGPURendererVk* renderer = (VGPURendererVk*)driverData;
    renderer->api.vkCmdEndRenderPass(renderer->frame->commandBuffer);
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
    VGPURendererVk* renderer;

    device = (VGPUDeviceImpl*)VGPU_MALLOC(sizeof(VGPUDeviceImpl));
    ASSIGN_DRIVER(vk);

    /* Init the vk renderer */
    renderer = (VGPURendererVk*)_VGPU_ALLOC_HANDLE(VGPURendererVk);

    /* Reference vgpu_device and renderer together. */
    renderer->gpu_device = device;
    device->renderer = (VGPURenderer*)renderer;

    return device;
}
