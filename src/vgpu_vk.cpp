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

#include "vgpu/vgpu.h"
#include <assert.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#   include <malloc.h>
#   undef    alloca
#   define   alloca _malloca
#   define   freea  _freea
#else
#   include <alloca.h>
#endif

#include <stdio.h> /* vsnprintf */

#ifndef VGPU_ASSERT
#   define VGPU_ASSERT(c) assert(c)
#endif

#ifndef VGPU_ALLOC
#   define VGPU_ALLOC(type) ((type*) malloc(sizeof(type)))
#   define VGPU_ALLOCN(type, n) ((type*) malloc(sizeof(type) * n))
#   define VGPU_FREE(ptr) free(ptr)
#   define VGPU_ALLOC_HANDLE(type) ((type) calloc(1, sizeof(type##_T)))
#endif

#if defined( __clang__ )
#   define VGPU_UNREACHABLE() __builtin_unreachable()
#   define VGPU_THREADLOCAL _Thread_local
#elif defined(__GNUC__)
#   define VGPU_UNREACHABLE() __builtin_unreachable()
#   define VGPU_THREADLOCAL __thread
#elif defined(_MSC_VER)
#   define VGPU_UNREACHABLE() __assume(false)
#   define VGPU_THREADLOCAL __declspec(thread)
#else
#   define VGPU_UNREACHABLE()((void)0)
#   define VGPU_THREADLOCAL
#endif

#define _vgpu_min(a,b) ((a<b)?a:b)
#define _vgpu_max(a,b) ((a>b)?a:b)
#define _vgpu_clamp(v,v0,v1) ((v<v0)?(v0):((v>v1)?(v1):(v)))

#if defined(_WIN32) || defined(_WIN64)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   define VK_SURFACE_EXT               "VK_KHR_win32_surface"
#   define VK_CREATE_SURFACE_FN         vkCreateWin32SurfaceKHR
#   define VK_USE_PLATFORM_WIN32_KHR    1
#elif defined(__ANDROID__)
#   define VK_SURFACE_EXT               "VK_KHR_android_surface"
#   define VK_CREATE_SURFACE_FN         vkCreateAndroidSurfaceKHR
#   define VK_USE_PLATFORM_ANDROID_KHR  1
#elif defined(__linux__)
#   include <xcb/xcb.h>
#   include <dlfcn.h>
#   include <X11/Xlib-xcb.h>
#   define VK_SURFACE_EXT               "VK_KHR_xcb_surface"
#   define VK_CREATE_SURFACE_FN         vkCreateXcbSurfaceKHR
#   define VK_USE_PLATFORM_XCB_KHR      1
#endif

#ifndef VMA_STATIC_VULKAN_FUNCTION
#   define VMA_STATIC_VULKAN_FUNCTION 0
#endif
#include "volk.h"
#include <vk_mem_alloc.h>
//#include <spirv-cross/spirv_hlsl.hpp>

#if !defined(VGPU_DEBUG) && !defined(NDEBUG)
#   define VGPU_DEBUG 1
#endif

/* Vulkan only handles (ATM) */
VGPU_DEFINE_HANDLE(VgpuPhysicalDevice);
VGPU_DEFINE_HANDLE(VgpuSwapchain);
VGPU_DEFINE_HANDLE(VgpuCommandQueue);
#define VGPU_MAX_COMMAND_BUFFER 16u

/* Handle declaration */
typedef struct VgpuPhysicalDevice_T {
    VkPhysicalDevice                    vk_handle;
    VkPhysicalDeviceProperties          properties;
    VkPhysicalDeviceMemoryProperties    memoryProperties;
    VkPhysicalDeviceFeatures            features;
    uint32_t                            queueFamilyCount;
    VkQueueFamilyProperties*            queueFamilyProperties;
    uint32_t                            extensionsCount;
    VkExtensionProperties*              extensions;
} VgpuPhysicalDevice_T;

typedef struct VgpuCommandQueue_T {
    VkQueue             vk_queue;
    uint32_t            vk_queueFamilyIndex;
    VkCommandPool       vk_handle;
    uint32_t            commandBuffersCount;
    VgpuCommandBuffer   commandBuffers[VGPU_MAX_COMMAND_BUFFER];
} VgpuCommandQueue_T;

typedef struct VgpuCommandBuffer_T {
    VgpuCommandQueue    queue;
    VkCommandBuffer     vk_handle;
    VkSemaphore         vk_semaphore;
    bool                recording;
} VgpuCommandBuffer_T;

typedef struct VgpuSwapchain_T {
    VkSwapchainKHR      vk_handle;
    VkRenderPass        renderPass;
    uint32_t            width;
    uint32_t            height;
    uint32_t            imageIndex;  // index of currently acquired image.
    uint32_t            imageCount;
    VkImage*            images;
    VkImageView*        imageViews;
    VkSemaphore*        imageSemaphores;
    //VgpuTexture*      textures;       /* TODO: Use VgpuTexture */
    VkFramebuffer*      framebuffers;   /* TODO: Convert to VgpuFramebuffer */
    VkFormat            depthStencilFormat;
} VgpuSwapchain_T;

typedef struct VgpuTexture_T {
    VkImage     image;
    VkFormat    format;
} VgpuTexture_T;

typedef struct VgpuSampler_T {
    VkSampler   vk_handle;
} VgpuSampler_T;

typedef struct VgpuFramebuffer_T {
    uint32_t    width;
    uint32_t    height;
} VgpuFramebuffer_T;

typedef struct VgpuShaderModule_T {
    VkShaderModule  vk_handle;
} VgpuShaderModule_T;

// Proxy log callback
#define VGPU_MAX_LOG_MESSAGE 4096
static vgpu_log_fn s_logCallback = nullptr;

static void vgpu_internal_log(VgpuLogLevel level, const char* context, const char* message)
{
    if (s_logCallback) {
        s_logCallback(level, context, message);
    }
}

static void vgpu_internal_log(VgpuLogLevel level, const char* context, const char* format, ...)
{
    char logMessage[VGPU_MAX_LOG_MESSAGE];
    va_list args;
    va_start(args, format);
    vsnprintf(logMessage, VGPU_MAX_LOG_MESSAGE, format, args);
    if (s_logCallback) {
        s_logCallback(level, context, logMessage);
    }
    va_end(args);
}

/* Global handles. */
struct VgpuDeviceFeaturesVk
{
    bool supportsPhysicalDeviceProperties2 = false;
    bool supportsExternal = false;
    bool supportsDedicated = false;
    bool supportsImageFormatList = false;
    bool supportsDebugMarker = false;
    bool supportsDebugUtils = false;
    bool supportsMirrorClampToEdge = false;
    bool supportsGoogleDisplayTiming = false;
    VkPhysicalDeviceSubgroupProperties subgroupProperties = {};
    VkPhysicalDevice8BitStorageFeaturesKHR storage8bitFeatures = {};
    VkPhysicalDevice16BitStorageFeaturesKHR storage_16bitFeatures = {};
    VkPhysicalDeviceFloat16Int8FeaturesKHR float16Int8Features = {};
    VkPhysicalDeviceFeatures enabledFeatures = {};
};

struct VgpuVkFrameData {
    uint32_t                    submittedCmdBuffersCount;
    VkCommandBuffer             submittedCmdBuffers[VGPU_MAX_COMMAND_BUFFER];
    VkSemaphore                 waitSemaphores[VGPU_MAX_COMMAND_BUFFER];
    VkFence                     fence;
};

struct {
    VgpuBool32                  initialized = VGPU_FALSE;
    VgpuBool32                  available = VGPU_FALSE;

    /* Extensions */
    bool                        KHR_get_physical_device_properties2;    /* VK_KHR_get_physical_device_properties2 */
    bool                        KHR_external_memory_capabilities;       /* VK_KHR_external_memory_capabilities */
    bool                        KHR_external_semaphore_capabilities;    /* VK_KHR_external_semaphore_capabilities */

    bool                        EXT_debug_report;       /* VK_EXT_debug_report */
    bool                        EXT_debug_utils;        /* VK_EXT_debug_utils */

    bool                        KHR_surface;            /* VK_KHR_surface */

    /* Layers */
    bool                        VK_LAYER_LUNARG_standard_validation;
    bool                        VK_LAYER_RENDERDOC_Capture;

    VkInstance                  instance = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT    debugCallback = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT    debugMessenger = VK_NULL_HANDLE;
    VgpuPhysicalDevice          physicalDevices[VGPU_MAX_PHYSICAL_DEVICES];
    VgpuPhysicalDevice          physicalDevice = VK_NULL_HANDLE;
    VkDevice                    device = VK_NULL_HANDLE;

    VgpuDeviceFeaturesVk        features = {};

    uint32_t                    graphicsQueueFamily = VK_QUEUE_FAMILY_IGNORED;
    uint32_t                    computeQueueFamily = VK_QUEUE_FAMILY_IGNORED;
    uint32_t                    transferQueueFamily = VK_QUEUE_FAMILY_IGNORED;

    VkQueue                     graphicsQueue = VK_NULL_HANDLE;
    VkQueue                     computeQueue = VK_NULL_HANDLE;
    VkQueue                     transferQueue = VK_NULL_HANDLE;


    VmaAllocator                memoryAllocator = VK_NULL_HANDLE;
    VgpuCommandQueue            graphicsCommandQueue = VK_NULL_HANDLE;
    VgpuCommandQueue            computeCommandQueue = VK_NULL_HANDLE;
    VgpuCommandQueue            copyCommandQueue = VK_NULL_HANDLE;
} _vk;

struct VgpuVkContext {
    uint32_t                    maxInflightFrames;
    uint32_t                    frameIndex;
    VgpuVkFrameData*            frameData = nullptr;
    VkSurfaceKHR                surface = VK_NULL_HANDLE;
    VgpuSwapchain               swapchain = VK_NULL_HANDLE;
};

static bool s_initialized = false;
static VgpuVkContext *s_current_context = NULL;

/* Conversion functions */
static const char* vgpuVkVulkanResultString(VkResult result)
{
    switch (result)
    {
    case VK_SUCCESS:
        return "Success";
    case VK_NOT_READY:
        return "A fence or query has not yet completed";
    case VK_TIMEOUT:
        return "A wait operation has not completed in the specified time";
    case VK_EVENT_SET:
        return "An event is signaled";
    case VK_EVENT_RESET:
        return "An event is unsignaled";
    case VK_INCOMPLETE:
        return "A return array was too small for the result";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "A host memory allocation has failed";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "A device memory allocation has failed";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "Initialization of an object could not be completed for implementation-specific reasons";
    case VK_ERROR_DEVICE_LOST:
        return "The logical or physical device has been lost";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "Mapping of a memory object has failed";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "A requested layer is not present or could not be loaded";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "A requested extension is not supported";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "A requested feature is not supported";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "The requested version of Vulkan is not supported by the driver or is otherwise incompatible";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "Too many objects of the type have already been created";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "A requested format is not supported on this device";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "A surface is no longer available";
    case VK_SUBOPTIMAL_KHR:
        return "A swapchain no longer matches the surface properties exactly, but can still be used";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "A surface has changed in such a way that it is no longer compatible with the swapchain";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return "The display used by a swapchain does not use the same presentable image layout";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "The requested window is already connected to a VkSurfaceKHR, or to some other non-Vulkan API";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "A validation layer found an error";
    default:
        return "ERROR: UNKNOWN VULKAN ERROR";
    }
}

// Helper utility converts Vulkan API failures into exceptions.
static void vgpuVkLogIfFailed(VkResult result)
{
    if (result < VK_SUCCESS)
    {
        vgpu_internal_log(
            VGPU_LOG_LEVEL_ERROR,
            "Vulkan"
            "Fatal vulkan result is \"%s\" in %u at line %u",
            vgpuVkVulkanResultString(result),
            __FILE__,
            __LINE__);
    }
}

static VgpuResult vgpuVkConvertResult(VkResult value)
{
    switch (value)
    {
    case VK_SUCCESS:                        return VGPU_SUCCESS;
    case VK_NOT_READY:                      return VGPU_NOT_READY;
    case VK_TIMEOUT:                        return VGPU_TIMEOUT;
    case VK_INCOMPLETE:                     return VGPU_INCOMPLETE;
    case VK_ERROR_OUT_OF_HOST_MEMORY:       return VGPU_ERROR_OUT_OF_HOST_MEMORY;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:     return VGPU_ERROR_OUT_OF_DEVICE_MEMORY;
    case VK_ERROR_INITIALIZATION_FAILED:    return VGPU_ERROR_INITIALIZATION_FAILED;
    case VK_ERROR_DEVICE_LOST:              return VGPU_ERROR_DEVICE_LOST;
    case VK_ERROR_TOO_MANY_OBJECTS:         return VGPU_ERROR_TOO_MANY_OBJECTS;

    default:
        if (value < VK_SUCCESS) {
            return VGPU_ERROR_GENERIC;
        }

        return VGPU_SUCCESS;
    }
}

static VgpuPixelFormat vgpuGetPixelFormat(VkFormat format, VgpuBool32& srgb) {
    srgb = VGPU_FALSE;
    switch (format) {
    case VK_FORMAT_B8G8R8A8_UNORM:
        return VGPU_PIXEL_FORMAT_BGRA8_UNORM;

    case VK_FORMAT_B8G8R8A8_SRGB:
        srgb = VGPU_TRUE;
        return VGPU_PIXEL_FORMAT_BGRA8_UNORM;

    default:
        return VGPU_PIXEL_FORMAT_UNDEFINED;
    }
}

static VkCompareOp vgpuVkGetCompareOp(VgpuCompareOp op) {
    switch (op)
    {
    case VGPU_COMPARE_OP_NEVER:         return VK_COMPARE_OP_NEVER;
    case VGPU_COMPARE_OP_LESS:          return VK_COMPARE_OP_LESS;
    case VGPU_COMPARE_OP_EQUAL:         return VK_COMPARE_OP_EQUAL;
    case VGPU_COMPARE_OP_LESS_EQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
    case VGPU_COMPARE_OP_GREATER:       return VK_COMPARE_OP_GREATER;
    case VGPU_COMPARE_OP_NOT_EQUAL:     return VK_COMPARE_OP_NOT_EQUAL;
    case VGPU_COMPARE_OP_GREATER_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case VGPU_COMPARE_OP_ALWAYS:        return VK_COMPARE_OP_ALWAYS;
    default:
        VGPU_UNREACHABLE();
        return VK_COMPARE_OP_NEVER;
    }
}

static VkFilter vgpuVkGetFilter(VgpuSamplerMinMagFilter filter) {
    switch (filter)
    {
    case VGPU_SAMPLER_MIN_MAG_FILTER_NEAREST:   return VK_FILTER_NEAREST;
    case VGPU_SAMPLER_MIN_MAG_FILTER_LINEAR:    return VK_FILTER_LINEAR;
    default:
        VGPU_UNREACHABLE();
        return VK_FILTER_NEAREST;
    }
}

static VkSamplerMipmapMode vgpuVkGetMipMapFilter(VgpuSamplerMipFilter filter) {
    switch (filter)
    {
    case VGPU_SAMPLER_MIP_FILTER_NEAREST:   return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case VGPU_SAMPLER_MIP_FILTER_LINEAR:    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default:
        VGPU_UNREACHABLE();
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }
}

static VkSamplerAddressMode vgpuVkGetAddressMode(VgpuSamplerAddressMode mode, bool supportsMirrorClampToEdge) {

    switch (mode)
    {
    case VGPU_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    case VGPU_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
        return supportsMirrorClampToEdge ? VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    case VGPU_SAMPLER_ADDRESS_MODE_REPEAT:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;

    case VGPU_SAMPLER_ADDRESS_MODE_MIRROR_REPEAT:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    case VGPU_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER_COLOR:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    default:
        VGPU_UNREACHABLE();
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
}

static VkBorderColor vgpuVkGetBorderColor(VgpuSamplerBorderColor color) {
    switch (color)
    {
    case VK_SAMPLER_BORDER_COLOR_TRANSPARENT_BLACK:
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

    case VK_SAMPLER_BORDER_COLOR_OPAQUE_BLACK:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    case VK_SAMPLER_BORDER_COLOR_OPAQUE_WHITE:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    default:
        VGPU_UNREACHABLE();
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vgpuVkMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageType,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void *pUserData)
{
    //GPUDeviceVk* device = static_cast<GPUDeviceVk*>(pUserData);

    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        {
            vgpu_internal_log(VGPU_LOG_LEVEL_ERROR, "vulkan", "Validation Error: %s", pCallbackData->pMessage);
            //device->NotifyValidationError(pCallbackData->pMessage);
        }
        else
        {
            vgpu_internal_log(VGPU_LOG_LEVEL_ERROR, "vulkan", "Other Error: %s", pCallbackData->pMessage);
        }

        break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        {
            vgpu_internal_log(VGPU_LOG_LEVEL_WARN, "vulkan", "Validation Warning: %s", pCallbackData->pMessage);
        }
        else
        {
            vgpu_internal_log(VGPU_LOG_LEVEL_WARN, "vulkan", "Other Warning: %s", pCallbackData->pMessage);
        }
        break;

#if 0
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
            ALIMER_LOGINFO("[Vulkan]: Validation Info: {}", pCallbackData->pMessage);
        else
            ALIMER_LOGINFO("[Vulkan]: Other Info: {}", pCallbackData->pMessage);
        break;
#endif

    default:
        return VK_FALSE;
    }

    /*bool log_object_names = false;
    for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
    {
        auto *name = pCallbackData->pObjects[i].pObjectName;
        if (name)
        {
            log_object_names = true;
            break;
        }
    }

    if (log_object_names)
    {
        for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
        {
            auto *name = pCallbackData->pObjects[i].pObjectName;
            ALIMER_LOGINFO("  Object #%u: %s", i, name ? name : "N/A");
        }
    }*/

    return VK_FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vgpuVkDebugCallback(VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT, uint64_t,
    size_t, int32_t messageCode, const char *pLayerPrefix,
    const char *pMessage, void *pUserData)
{
    //GPUDeviceVk* device = static_cast<GPUDeviceVk*>(pUserData);

    // False positives about lack of srcAccessMask/dstAccessMask.
    if (strcmp(pLayerPrefix, "DS") == 0 && messageCode == 10)
    {
        return VK_FALSE;
    }

    // Demote to a warning, it's a false positive almost all the time for Granite.
    if (strcmp(pLayerPrefix, "DS") == 0 && messageCode == 6)
    {
        flags = VK_DEBUG_REPORT_DEBUG_BIT_EXT;
    }

    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        //ALIMER_LOGERROR("[Vulkan]: %s: %s", pLayerPrefix, pMessage);
        //device->NotifyValidationError(pMessage);
    }
    else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        //ALIMER_LOGWARN("[Vulkan]: %s: %s", pLayerPrefix, pMessage);
    }
    else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
        //ALIMER_LOGWARN("[Vulkan]: Performance warning: %s: %s", pLayerPrefix, pMessage);
    }
    else
    {
        //ALIMER_LOGINFO("[Vulkan]: {}: {}", pLayerPrefix, pMessage);
    }

    return VK_FALSE;
}

static VgpuPhysicalDevice vgpuVkAllocateDevice(VkPhysicalDevice physicalDevice)
{
    VgpuPhysicalDevice handle = VGPU_ALLOC_HANDLE(VgpuPhysicalDevice);
    handle->vk_handle = physicalDevice;
    vkGetPhysicalDeviceProperties(physicalDevice, &handle->properties);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &handle->memoryProperties);
    vkGetPhysicalDeviceFeatures(physicalDevice, &handle->features);

    // Queue family properties, used for setting up requested queues upon device creation
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &handle->queueFamilyCount, nullptr);
    VGPU_ASSERT(handle->queueFamilyCount > 0);
    handle->queueFamilyProperties = (VkQueueFamilyProperties*)calloc(handle->queueFamilyCount, sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &handle->queueFamilyCount, handle->queueFamilyProperties);

    // Get list of supported extensions
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &handle->extensionsCount, nullptr);
    if (handle->extensionsCount > 0)
    {
        handle->extensions = (VkExtensionProperties*)calloc(handle->extensionsCount, sizeof(VkExtensionProperties));
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &handle->extensionsCount, handle->extensions);
    }

    return handle;
}

static void vgpuVkFreeDevice(VgpuPhysicalDevice handle)
{
    VGPU_ASSERT(handle);
    free(handle->queueFamilyProperties);
    free(handle->extensions);
}

static bool vgpuVkIsExtensionSupported(VgpuPhysicalDevice handle, const char* extension)
{
    VGPU_ASSERT(handle);

    for (uint32_t i = 0; i < handle->extensionsCount; ++i)
    {
        if (strcmp(handle->extensions[i].extensionName, extension) == 0)
        {
            return true;
        }
    }

    return false;
}

// Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
static void vgpuVkTransitionImageLayout(
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout,
    VkImageSubresourceRange subresourceRange,
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
    // Create an image barrier object
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (oldImageLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        // Image layout is undefined (or does not matter)
        // Only valid as initial layout
        // No flags required, listed only for completeness
        imageMemoryBarrier.srcAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        // Image is preinitialized
        // Only valid as initial layout for linear images, preserves memory contents
        // Make sure host writes have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image is a color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image is a depth/stencil attachment
        // Make sure any writes to the depth/stencil buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image is a transfer source 
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image is a transfer destination
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image is read by a shader
        // Make sure any shader reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Target layouts (new)
    // Destination access mask controls the dependency for the new image layout
    switch (newImageLayout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image will be used as a transfer destination
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image will be used as a transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image will be used as a color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image layout will be used as a depth/stencil attachment
        // Make sure any writes to depth/stencil buffer have been finished
        imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image will be read in a shader (sampler, input attachment)
        // Make sure any writes to the image have been finished
        if (imageMemoryBarrier.srcAccessMask == 0)
        {
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Put barrier inside setup command buffer
    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);
}

// Uses a fixed sub resource layout with first mip level and layer
static void vgpuVkTransitionImageLayout(
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkImageAspectFlags aspectMask,
    VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout,
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = aspectMask;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;
    vgpuVkTransitionImageLayout(commandBuffer, image, oldImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask);
}

static void vgpuVkClearImageWithColor(
    VkCommandBuffer commandBuffer,
    VkImage image,
    VkImageSubresourceRange range,
    VkImageAspectFlags aspect,
    VkImageLayout sourceLayout,
    VkImageLayout destLayout,
    VkClearColorValue *clearValue)
{
    // Transition to destination layout.
    vgpuVkTransitionImageLayout(commandBuffer, image, aspect, sourceLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Clear the image
    range.aspectMask = aspect;
    vkCmdClearColorImage(
        commandBuffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        clearValue,
        1,
        &range);

    // Transition back to source layout.
    vgpuVkTransitionImageLayout(commandBuffer, image, aspect, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, destLayout);
}

static void vgpuVkCreateSemaphore(VkDevice device, VkSemaphore* pSemaphore)
{
    VkSemaphoreCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;

    vgpuVkLogIfFailed(
        vkCreateSemaphore(device, &createInfo, NULL, pSemaphore)
    );
}

static void vgpuVkCreateFence(VkDevice device, bool signaled, VkFence* pFence)
{
    VkFenceCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u;
    vgpuVkLogIfFailed(
        vkCreateFence(device, &createInfo, NULL, pFence)
    );
}

VkResult vgpuVkCreateSurface(const VgpuSwapchainDescriptor*  descriptor, VkSurfaceKHR* pSurface);
VgpuResult vgpuCreateSwapchain(const VgpuSwapchainDescriptor* descriptor, VkSwapchainKHR oldSwapchain, VkSurfaceKHR surface, VgpuSwapchain* pSwapchain);
void vgpuDestroySwapchain(VgpuSwapchain swapchain);
VgpuResult vgpuVkFlushCommandBuffer(VgpuCommandBuffer commandBuffer);

VgpuCommandQueue vgpuVkGetCommandQueue(VgpuCommandQueueType type) {
    switch (type)
    {
    case VGPU_COMMAND_QUEUE_TYPE_GRAPHICS:
        return _vk.graphicsCommandQueue;

    case VGPU_COMMAND_QUEUE_TYPE_COMPUTE:
        return _vk.computeCommandQueue;

    case VGPU_COMMAND_QUEUE_TYPE_COPY:
        return _vk.copyCommandQueue;

    default:
        assert(false && "Invalid command queue type.");
    }

    return nullptr;
}

VkResult vgpuVkCreateInstance(const char* applicationName, bool validation, bool headless)
{
    VkApplicationInfo appInfo;
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = applicationName;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "vgpu";
    appInfo.engineVersion = VK_MAKE_VERSION(VGPU_VERSION_MAJOR, VGPU_VERSION_MINOR, 0);
    appInfo.apiVersion = volkGetInstanceVersion();

    if (appInfo.apiVersion >= VK_API_VERSION_1_1)
    {
        appInfo.apiVersion = VK_API_VERSION_1_1;
    }

    uint32_t instanceLayersCount = 0;
    uint32_t instanceExtensionsCount = 0;
    const char* instanceLayers[10];
    const char* instanceExtensions[20];

    if (_vk.KHR_get_physical_device_properties2)
    {
        instanceExtensions[instanceExtensionsCount++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
    }

    if (_vk.KHR_get_physical_device_properties2
        && _vk.KHR_external_memory_capabilities
        && _vk.KHR_external_semaphore_capabilities)
    {
        instanceExtensions[instanceExtensionsCount++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
        instanceExtensions[instanceExtensionsCount++] = VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME;
        instanceExtensions[instanceExtensionsCount++] = VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME;
    }

    if (_vk.EXT_debug_utils)
    {
        instanceExtensions[instanceExtensionsCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }

#ifdef VGPU_DEBUG
    if (validation)
    {
        if (!_vk.EXT_debug_utils && _vk.EXT_debug_report)
        {
            instanceExtensions[instanceExtensionsCount++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
        }

        bool force_no_validation = false;
        /*if (getenv("VGPU_VULKAN_NO_VALIDATION"))
        {
            force_no_validation = true;
        }*/

        if (!force_no_validation && _vk.VK_LAYER_LUNARG_standard_validation)
        {
            instanceLayers[instanceLayersCount++] = "VK_LAYER_LUNARG_standard_validation";
        }
    }
#endif

    if (!headless
        && _vk.KHR_surface)
    {
        instanceExtensions[instanceExtensionsCount++] = "VK_KHR_surface";
        instanceExtensions[instanceExtensionsCount++] = VK_SURFACE_EXT;
    }

    VkInstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = nullptr;
    instanceCreateInfo.flags = 0;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledLayerCount = instanceLayersCount;
    instanceCreateInfo.ppEnabledLayerNames = instanceLayers;
    instanceCreateInfo.enabledExtensionCount = instanceExtensionsCount;
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions;
    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &_vk.instance);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    volkLoadInstance(_vk.instance);

    if (validation)
    {
        if (_vk.EXT_debug_utils)
        {
            VkDebugUtilsMessengerCreateInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
            info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            info.pfnUserCallback = vgpuVkMessengerCallback;
            info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
            info.pUserData = nullptr;
            vkCreateDebugUtilsMessengerEXT(_vk.instance, &info, nullptr, &_vk.debugMessenger);
        }
        else if (_vk.EXT_debug_report)
        {
            VkDebugReportCallbackCreateInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT };
            info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
                VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            info.pfnCallback = vgpuVkDebugCallback;
            info.pUserData = nullptr;
            vkCreateDebugReportCallbackEXT(_vk.instance, &info, nullptr, &_vk.debugCallback);
        }
    }

    return result;
}

VkResult vgpuVkDetectPhysicalDevice(VgpuDevicePreference preference)
{
    uint32_t gpuCount = 0;
    vkEnumeratePhysicalDevices(_vk.instance, &gpuCount, nullptr);
    if (gpuCount == 0)
    {
        //ALIMER_LOGCRITICAL("Vulkan: No physical device detected");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPhysicalDevice* physicalDevices = (VkPhysicalDevice*)alloca(gpuCount * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(_vk.instance, &gpuCount, physicalDevices);
    uint32_t bestDeviceScore = 0U;
    uint32_t bestDeviceIndex = ~0u;
    for (uint32_t i = 0; i < gpuCount; ++i)
    {
        uint32_t score = 0U;
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);
        switch (properties.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 100U;
            if (preference == VGPU_DEVICE_PREFERENCE_DISCRETE) {
                score += 1000U;
            }
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 90U;
            if (preference == VGPU_DEVICE_PREFERENCE_INTEGRATED) {
                score += 1000U;
            }
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 80U;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score += 70U;
            break;
        default: score += 10U;
        }

        if (score > bestDeviceScore) {
            bestDeviceIndex = i;
            bestDeviceScore = score;
        }
    }

    if (bestDeviceIndex == ~0u)
    {
        //ALIMER_LOGCRITICAL("Vulkan: No physical device supported.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    _vk.physicalDevice = vgpuVkAllocateDevice(physicalDevices[bestDeviceIndex]);
    return VK_SUCCESS;
}

VkResult vgpuVkCreateDevice(VkSurfaceKHR surface)
{
    VkResult result = VK_SUCCESS;

    for (uint32_t i = 0; i < _vk.physicalDevice->queueFamilyCount; i++)
    {
        VkBool32 supported = surface == VK_NULL_HANDLE;
        if (surface != VK_NULL_HANDLE)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(_vk.physicalDevice->vk_handle, i, surface, &supported);
        }

        static const VkQueueFlags required = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT;
        if (supported
            && ((_vk.physicalDevice->queueFamilyProperties[i].queueFlags & required) == required))
        {
            _vk.graphicsQueueFamily = i;
            break;
        }
    }

    for (uint32_t i = 0; i < _vk.physicalDevice->queueFamilyCount; i++)
    {
        static const VkQueueFlags required = VK_QUEUE_COMPUTE_BIT;
        if (i != _vk.graphicsQueueFamily
            && (_vk.physicalDevice->queueFamilyProperties[i].queueFlags & required) == required)
        {
            _vk.computeQueueFamily = i;
            break;
        }
    }

    for (uint32_t i = 0; i < _vk.physicalDevice->queueFamilyCount; i++)
    {
        static const VkQueueFlags required = VK_QUEUE_TRANSFER_BIT;
        if (i != _vk.graphicsQueueFamily
            && i != _vk.computeQueueFamily
            && (_vk.physicalDevice->queueFamilyProperties[i].queueFlags & required) == required)
        {
            _vk.transferQueueFamily = i;
            break;
        }
    }

    if (_vk.transferQueueFamily == VK_QUEUE_FAMILY_IGNORED)
    {
        for (uint32_t i = 0; i < _vk.physicalDevice->queueFamilyCount; i++)
        {
            static const VkQueueFlags required = VK_QUEUE_TRANSFER_BIT;
            if (i != _vk.graphicsQueueFamily
                && (_vk.physicalDevice->queueFamilyProperties[i].queueFlags & required) == required)
            {
                _vk.transferQueueFamily = i;
                break;
            }
        }
    }

    if (_vk.graphicsQueueFamily == VK_QUEUE_FAMILY_IGNORED)
    {
        //ALIMER_LOGERROR("Vulkan: Invalid graphics queue family");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t universalQueueIndex = 1;
    uint32_t graphicsQueueIndex = 0;
    uint32_t computeQueueIndex = 0;
    uint32_t transferQueueIndex = 0;

    if (_vk.computeQueueFamily == VK_QUEUE_FAMILY_IGNORED)
    {
        _vk.computeQueueFamily = _vk.graphicsQueueFamily;
        computeQueueIndex = _vgpu_min(_vk.physicalDevice->queueFamilyProperties[_vk.graphicsQueueFamily].queueCount - 1, universalQueueIndex);
        universalQueueIndex++;
    }

    if (_vk.transferQueueFamily == VK_QUEUE_FAMILY_IGNORED)
    {
        _vk.transferQueueFamily = _vk.graphicsQueueFamily;
        transferQueueIndex = _vgpu_min(_vk.physicalDevice->queueFamilyProperties[_vk.graphicsQueueFamily].queueCount - 1, universalQueueIndex);
        universalQueueIndex++;
    }
    else if (_vk.transferQueueFamily == _vk.computeQueueFamily)
    {
        transferQueueIndex = _vgpu_min(_vk.physicalDevice->queueFamilyProperties[_vk.computeQueueFamily].queueCount - 1, 1u);
    }

    static const float graphicsQueuePriority = 0.5f;
    static const float computeQueuePriority = 1.0f;
    static const float transferQueuePriority = 1.0f;
    float queuePriorities[3] = { graphicsQueuePriority, computeQueuePriority, transferQueuePriority };

    uint32_t queueFamilyCount = 0;
    VkDeviceQueueCreateInfo queueCreateInfo[3] = {};

    VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfo;

    queueCreateInfo[queueFamilyCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo[queueFamilyCount].queueFamilyIndex = _vk.graphicsQueueFamily;
    queueCreateInfo[queueFamilyCount].queueCount = _vgpu_min(universalQueueIndex, _vk.physicalDevice->queueFamilyProperties[_vk.graphicsQueueFamily].queueCount);
    queueCreateInfo[queueFamilyCount].pQueuePriorities = queuePriorities;
    queueFamilyCount++;

    if (_vk.computeQueueFamily != _vk.graphicsQueueFamily)
    {
        queueCreateInfo[queueFamilyCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo[queueFamilyCount].queueFamilyIndex = _vk.computeQueueFamily;
        queueCreateInfo[queueFamilyCount].queueCount = _vgpu_min(_vk.transferQueueFamily == _vk.computeQueueFamily ? 2u : 1u,
            _vk.physicalDevice->queueFamilyProperties[_vk.computeQueueFamily].queueCount);
        queueCreateInfo[queueFamilyCount].pQueuePriorities = queuePriorities + 1;
        queueFamilyCount++;
    }

    if (_vk.transferQueueFamily != _vk.graphicsQueueFamily
        && _vk.transferQueueFamily != _vk.computeQueueFamily)
    {
        queueCreateInfo[queueFamilyCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo[queueFamilyCount].queueFamilyIndex = _vk.transferQueueFamily;
        queueCreateInfo[queueFamilyCount].queueCount = 1;
        queueCreateInfo[queueFamilyCount].pQueuePriorities = queuePriorities + 2;
        queueFamilyCount++;
    }

    uint32_t enabledExtensionsCount = 0;
    const char* enabledExtensions[20];
    if (surface != VK_NULL_HANDLE)
    {
        enabledExtensions[enabledExtensionsCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    }

    // Enable VK_KHR_maintenance1 to support nevagive viewport and match DirectX coordinate system.
    if (vgpuVkIsExtensionSupported(_vk.physicalDevice, VK_KHR_MAINTENANCE1_EXTENSION_NAME))
    {
        enabledExtensions[enabledExtensionsCount++] = VK_KHR_MAINTENANCE1_EXTENSION_NAME;
    }

    if (vgpuVkIsExtensionSupported(_vk.physicalDevice, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME)
        && vgpuVkIsExtensionSupported(_vk.physicalDevice, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME))
    {
        _vk.features.supportsDedicated = true;
        enabledExtensions[enabledExtensionsCount++] = VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
        enabledExtensions[enabledExtensionsCount++] = VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME;
    }

    if (vgpuVkIsExtensionSupported(_vk.physicalDevice, VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME))
    {
        _vk.features.supportsImageFormatList = true;
        enabledExtensions[enabledExtensionsCount++] = VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME;
    }

    if (vgpuVkIsExtensionSupported(_vk.physicalDevice, VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME))
    {
        _vk.features.supportsGoogleDisplayTiming = true;
        enabledExtensions[enabledExtensionsCount++] = VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME;
    }

    if (_vk.EXT_debug_utils)
    {
        _vk.features.supportsDebugUtils = true;
    }

    if (vgpuVkIsExtensionSupported(_vk.physicalDevice, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
    {
        _vk.features.supportsDebugMarker = true;
        enabledExtensions[enabledExtensionsCount++] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
    }

    if (vgpuVkIsExtensionSupported(_vk.physicalDevice, VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME))
    {
        _vk.features.supportsMirrorClampToEdge = true;
        enabledExtensions[enabledExtensionsCount++] = VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME;
    }

    VkPhysicalDeviceFeatures2KHR features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
    if (_vk.KHR_get_physical_device_properties2)
    {
        vkGetPhysicalDeviceFeatures2KHR(_vk.physicalDevice->vk_handle, &features);
    }
    else
    {
        vkGetPhysicalDeviceFeatures(_vk.physicalDevice->vk_handle, &features.features);
    }

    // Enable device features we might care about.
    {
        VkPhysicalDeviceFeatures enabledFeatures = {};
        if (features.features.samplerAnisotropy) {
            enabledFeatures.samplerAnisotropy = VK_TRUE;
        }
        if (features.features.textureCompressionETC2) {
            enabledFeatures.textureCompressionETC2 = VK_TRUE;
        }
        if (features.features.textureCompressionBC) {
            enabledFeatures.textureCompressionBC = VK_TRUE;
        }
        if (features.features.textureCompressionASTC_LDR) {
            enabledFeatures.textureCompressionASTC_LDR = VK_TRUE;
        }
        if (features.features.fullDrawIndexUint32) {
            enabledFeatures.fullDrawIndexUint32 = VK_TRUE;
        }
        if (features.features.imageCubeArray) {
            enabledFeatures.imageCubeArray = VK_TRUE;
        }
        if (features.features.fillModeNonSolid) {
            enabledFeatures.fillModeNonSolid = VK_TRUE;
        }
        if (features.features.independentBlend) {
            enabledFeatures.independentBlend = VK_TRUE;
        }
        if (features.features.sampleRateShading) {
            enabledFeatures.sampleRateShading = VK_TRUE;
        }
        if (features.features.fragmentStoresAndAtomics) {
            enabledFeatures.fragmentStoresAndAtomics = VK_TRUE;
        }
        if (features.features.shaderStorageImageExtendedFormats) {
            enabledFeatures.shaderStorageImageExtendedFormats = VK_TRUE;
        }
        if (features.features.shaderStorageImageMultisample) {
            enabledFeatures.shaderStorageImageMultisample = VK_TRUE;
        }
        if (features.features.largePoints) {
            enabledFeatures.largePoints = VK_TRUE;
        }

        features.features = enabledFeatures;
    }

    if (_vk.KHR_get_physical_device_properties2) {
        deviceCreateInfo.pNext = &features;
    }
    else {
        deviceCreateInfo.pEnabledFeatures = &features.features;
    }

    deviceCreateInfo.queueCreateInfoCount = queueFamilyCount;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.enabledExtensionCount = enabledExtensionsCount;
    deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions;

    result = vkCreateDevice(_vk.physicalDevice->vk_handle, &deviceCreateInfo, nullptr, &_vk.device);
    if (result != VK_SUCCESS)
    {
        //ALIMER_LOGERROR("Vulkan: Failed to create device");
        return result;
    }

    volkLoadDevice(_vk.device);
    vkGetDeviceQueue(_vk.device, _vk.graphicsQueueFamily, graphicsQueueIndex, &_vk.graphicsQueue);
    vkGetDeviceQueue(_vk.device, _vk.computeQueueFamily, computeQueueIndex, &_vk.computeQueue);
    vkGetDeviceQueue(_vk.device, _vk.transferQueueFamily, transferQueueIndex, &_vk.transferQueue);

    // Set up VMA.
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
    vulkanFunctions.vkFreeMemory = vkFreeMemory;
    vulkanFunctions.vkMapMemory = vkMapMemory;
    vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
    vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
    vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
    vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
    vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
    vulkanFunctions.vkCreateImage = vkCreateImage;
    vulkanFunctions.vkDestroyImage = vkDestroyImage;
    vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
#if VMA_DEDICATED_ALLOCATION
    vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
    vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
#endif

    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.physicalDevice = _vk.physicalDevice->vk_handle;
    allocatorCreateInfo.device = _vk.device;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
    result = vmaCreateAllocator(&allocatorCreateInfo, &_vk.memoryAllocator);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    return result;
}

VkResult vgpuVkCreateSurface(const VgpuSwapchainDescriptor*  descriptor, VkSurfaceKHR* pSurface)
{
    VkResult result = VK_SUCCESS;
    if (descriptor == nullptr)
    {
        return result;
    }

#if defined(_WIN32) || defined(_WIN64)
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo;
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext = nullptr;
    surfaceCreateInfo.flags = 0;
    if (descriptor->nativeDisplay == 0) {
        surfaceCreateInfo.hinstance = GetModuleHandleW(nullptr);
    }
    else {
        surfaceCreateInfo.hinstance = (HMODULE)descriptor->nativeDisplay;
    }

    surfaceCreateInfo.hwnd = (HWND)descriptor->nativeHandle;
#elif defined(__ANDROID__)
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo;
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext = nullptr;
    surfaceCreateInfo.flags = 0;
    surfaceCreateInfo.window = (ANativeWindow*)descriptor->nativeHandle;
#elif defined(__linux__)
    VkXcbSurfaceCreateInfoKHR surfaceCreateInfo;
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext = nullptr;
    surfaceCreateInfo.flags = 0;

    if (descriptor->nativeDisplay == 0) {
        static xcb_connection_t* XCB_CONNECTION = nullptr;
        if (XCB_CONNECTION == nullptr) {
            int screenIndex = 0;
            xcb_screen_t* screen = nullptr;
            xcb_connection_t* connection = xcb_connect(NULL, &screenIndex);
            const xcb_setup_t *setup = xcb_get_setup(connection);
            for (xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
                screen >= 0 && it.rem;
                xcb_screen_next(&it)) {
                if (screenIndex-- == 0) {
                    screen = it.data;
                }
            }
            assert(screen);
            XCB_CONNECTION = connection;
        }

        surfaceCreateInfo.connection = XCB_CONNECTION;
    }
    else {
        surfaceCreateInfo.connection = (xcb_connection_t*)descriptor->nativeDisplay;
    }

    surfaceCreateInfo.window = (xcb_window_t)descriptor->nativeHandle;
#elif defined(VK_USE_PLATFORM_IOS_MVK)
    VkIOSSurfaceCreateInfoMVK surfaceCreateInfo;
    memset(&surfaceCreateInfo, 0, sizeof(surfaceCreateInfo));
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
    surfaceCreateInfo.pView = descriptor->nativeHandle;
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
    VkMacOSSurfaceCreateInfoMVK surfaceCreateInfo;
    memset(&surfaceCreateInfo, 0, sizeof(surfaceCreateInfo));
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    surfaceCreateInfo.pView = descriptor->nativeHandle;
#endif

    result = VK_CREATE_SURFACE_FN(_vk.instance, &surfaceCreateInfo, nullptr, pSurface);
    return result;
}

VgpuResult vgpuCreateSwapchain(const VgpuSwapchainDescriptor* descriptor, VkSwapchainKHR oldSwapchain, VkSurfaceKHR surface, VgpuSwapchain* pSwapchain)
{
    VkSurfaceCapabilitiesKHR surfaceCaps;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_vk.physicalDevice->vk_handle, surface, &surfaceCaps) != VK_SUCCESS)
    {
        return VGPU_ERROR_INITIALIZATION_FAILED;
    }

    // No surface, should sleep and retry maybe?.
    if (surfaceCaps.maxImageExtent.width == 0 &&
        surfaceCaps.maxImageExtent.height == 0)
    {
        return VGPU_ERROR_INITIALIZATION_FAILED;
    }

    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(_vk.physicalDevice->vk_handle, _vk.graphicsQueueFamily, surface, &supported);
    if (!supported)
    {
        return VGPU_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(_vk.physicalDevice->vk_handle, surface, &formatCount, nullptr);
    VkSurfaceFormatKHR *formats = (VkSurfaceFormatKHR*)alloca(sizeof(VkSurfaceFormatKHR) * formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(_vk.physicalDevice->vk_handle, surface, &formatCount, formats);

    VkSurfaceFormatKHR format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
    if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
    {
        format = formats[0];
        format.format = VK_FORMAT_B8G8R8A8_UNORM;
    }
    else
    {
        if (formatCount == 0)
        {
            //ALIMER_LOGERROR("Surface has no formats.");
            return VGPU_ERROR_INITIALIZATION_FAILED;
        }

        bool found = false;
        for (uint32_t i = 0; i < formatCount; i++)
        {
            if (formats[i].format == VK_FORMAT_R8G8B8A8_SRGB ||
                formats[i].format == VK_FORMAT_B8G8R8A8_SRGB ||
                formats[i].format == VK_FORMAT_A8B8G8R8_SRGB_PACK32)
            {
                format = formats[i];
                found = true;
            }
        }

        if (!found)
        {
            format = formats[0];
        }
    }

    VkExtent2D swapchainSize;
    if (surfaceCaps.currentExtent.width == ~0u)
    {
        swapchainSize.width = descriptor->width;
        swapchainSize.height = descriptor->height;
    }
    else
    {
        swapchainSize.width = _vgpu_max(_vgpu_min(descriptor->width, surfaceCaps.maxImageExtent.width), surfaceCaps.minImageExtent.width);
        swapchainSize.height = _vgpu_max(_vgpu_min(descriptor->height, surfaceCaps.maxImageExtent.height), surfaceCaps.minImageExtent.height);
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(_vk.physicalDevice->vk_handle, surface, &presentModeCount, nullptr);
    VkPresentModeKHR *presentModes = (VkPresentModeKHR*)alloca(sizeof(VkPresentModeKHR) * presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(_vk.physicalDevice->vk_handle, surface, &presentModeCount, presentModes);

    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!descriptor->vsync)
    {
        for (uint32_t i = 0; i < presentModeCount; i++)
        {
            if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR
                || presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                swapchainPresentMode = presentModes[i];
                break;
            }
        }
    }

    uint32_t desiredNumberOfSwapchainImages = descriptor->tripleBuffer ? 3 : surfaceCaps.minImageCount + 1;
    if (desiredNumberOfSwapchainImages < surfaceCaps.minImageCount) {
        desiredNumberOfSwapchainImages = surfaceCaps.minImageCount;
    }

    if ((surfaceCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfaceCaps.maxImageCount)) {
        desiredNumberOfSwapchainImages = surfaceCaps.maxImageCount;
    }

    //ALIMER_LOGINFO("Targeting {} swapchain images.", desiredNumberOfSwapchainImages);

    VkSurfaceTransformFlagBitsKHR preTransform;
    if (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    else
        preTransform = surfaceCaps.currentTransform;

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    if (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
        compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    if (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
        compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.surface = surface;
    createInfo.minImageCount = desiredNumberOfSwapchainImages;
    createInfo.imageFormat = format.format;
    createInfo.imageColorSpace = format.colorSpace;
    createInfo.imageExtent.width = swapchainSize.width;
    createInfo.imageExtent.height = swapchainSize.height;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
    createInfo.preTransform = preTransform;
    createInfo.compositeAlpha = compositeAlpha;
    createInfo.presentMode = swapchainPresentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VgpuTextureUsage textureUsage = VGPU_TEXTURE_USAGE_RENDER_TARGET;

    // Enable transfer source on swap chain images if supported.
    if (surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        textureUsage = (VgpuTextureUsage)(textureUsage | VGPU_TEXTURE_USAGE_TRANSFER_SRC);
    }

    // Enable transfer destination on swap chain images if supported
    if (surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        textureUsage = (VgpuTextureUsage)(textureUsage | VGPU_TEXTURE_USAGE_TRANSFER_DEST);
    }

    VkSwapchainKHR swapchain;
    VkResult result = vkCreateSwapchainKHR(_vk.device, &createInfo, nullptr, &swapchain);
    if (result != VK_SUCCESS)
    {
        //ALIMER_LOGCRITICAL("Vulkan: Failed to create swapchain, error: {}", vkGetVulkanResultString(result));
        return vgpuVkConvertResult(result);
    }

    VgpuSwapchain handle = VGPU_ALLOC_HANDLE(VgpuSwapchain);
    handle->vk_handle = swapchain;
    handle->depthStencilFormat = VK_FORMAT_UNDEFINED;

    if (descriptor->depthStencil) {
        VkFormat depthFormats[] = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM };
        for (auto& format : depthFormats) {
            VkFormatProperties formatProps;
            vkGetPhysicalDeviceFormatProperties(_vk.physicalDevice->vk_handle, format, &formatProps);
            if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                handle->depthStencilFormat = format;
                break;
            }
        }
    }
    if (oldSwapchain != VK_NULL_HANDLE)
    {
        for (uint32_t i = 0u; i < handle->imageCount; i++)
        {
            vkDestroyImageView(_vk.device, handle->imageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(_vk.device, oldSwapchain, nullptr);
    }


    /* Create swap chain render pass */
    VkAttachmentDescription attachments[4u] = {};
    uint32_t attachmentCount = 0u;
    VkAttachmentReference colorReference;
    VkAttachmentReference resolveReference;
    VkAttachmentReference depthReference;

    VkSubpassDescription subpassDescription;
    subpassDescription.flags = 0u;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;

    /* Optional depth reference */
    if (descriptor->samples > 1u) {
        colorReference.attachment = 0;
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Multisampled attachment that we render to
        attachments[attachmentCount].format = format.format;
        attachments[attachmentCount].samples = (VkSampleCountFlagBits)descriptor->samples;
        attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ++attachmentCount;

        // This is the frame buffer attachment to where the multisampled image
        // will be resolved to and which will be presented to the swapchain
        attachments[attachmentCount].format = format.format;
        attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        ++attachmentCount;

        // Resolve attachment reference for the color attachment
        resolveReference.attachment = 1;
        resolveReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        subpassDescription.pResolveAttachments = &resolveReference;

        if (handle->depthStencilFormat != VK_FORMAT_UNDEFINED) {
            // Multisampled depth attachment we render to
            attachments[attachmentCount].format = handle->depthStencilFormat;
            attachments[attachmentCount].samples = (VkSampleCountFlagBits)descriptor->samples;
            attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            ++attachmentCount;

            // Depth resolve attachment
            attachments[attachmentCount].format = handle->depthStencilFormat;
            attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
            attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            ++attachmentCount;

            depthReference = {};
            depthReference.attachment = 2;
            depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            subpassDescription.pDepthStencilAttachment = &depthReference;
        }
    }
    else {
        colorReference.attachment = 0;
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Color attachment
        attachments[attachmentCount].format = format.format;
        attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        ++attachmentCount;

        // Depth attachment
        if (handle->depthStencilFormat != VK_FORMAT_UNDEFINED) {
            attachments[attachmentCount].format = handle->depthStencilFormat;
            attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
            attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            ++attachmentCount;

            depthReference.attachment = 1;
            depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            subpassDescription.pDepthStencilAttachment = &depthReference;
        }
    }

    // Subpass dependencies for layout transitions
    VkSubpassDependency dependencies[2u];

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

    VkRenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = nullptr;
    renderPassCreateInfo.flags = 0u;
    renderPassCreateInfo.attachmentCount = attachmentCount;
    renderPassCreateInfo.pAttachments = attachments;
    renderPassCreateInfo.subpassCount = 1u;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = 2u;
    renderPassCreateInfo.pDependencies = dependencies;
    result = vkCreateRenderPass(_vk.device, &renderPassCreateInfo, nullptr, &handle->renderPass);

    vkGetSwapchainImagesKHR(_vk.device, swapchain, &handle->imageCount, nullptr);
    handle->images = VGPU_ALLOCN(VkImage, handle->imageCount);
    handle->imageSemaphores = VGPU_ALLOCN(VkSemaphore, handle->imageCount);
    handle->imageViews = VGPU_ALLOCN(VkImageView, handle->imageCount);
    //handle->textures = VGPU_ALLOCN(VgpuTexture, handle->imageCount);
    //handle->framebuffers = VGPU_ALLOCN(VgpuFramebuffer, handle->imageCount);
    handle->framebuffers = VGPU_ALLOCN(VkFramebuffer, handle->imageCount);
    vkGetSwapchainImagesKHR(_vk.device, swapchain, &handle->imageCount, handle->images);

    /* Setup texture descriptor */
    VgpuTextureDescriptor textureDescriptor;
    memset(&textureDescriptor, 0, sizeof(textureDescriptor));
    textureDescriptor.type = VGPU_TEXTURE_TYPE_2D;
    textureDescriptor.width = swapchainSize.width;
    textureDescriptor.height = swapchainSize.height;
    textureDescriptor.depthOrArraySize = 1;
    textureDescriptor.mipLevels = 1;
    textureDescriptor.usage = textureUsage;
    textureDescriptor.samples = AGPU_SAMPLE_COUNT1;
    textureDescriptor.sRGB = false;

    /* Convert pixel format to vgpu */
    textureDescriptor.format = vgpuGetPixelFormat(createInfo.imageFormat, textureDescriptor.sRGB);

    /* Setup framebuffer descriptor */
    //VgpuFramebufferDescriptor framebufferDescriptor;
    //memset(&framebufferDescriptor, 0, sizeof(framebufferDescriptor));

    /* Clear or transition to default image layout */
    const bool canClear = createInfo.imageUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VgpuCommandBufferDescriptor commandBufferDescriptor = {};
    commandBufferDescriptor.type = VGPU_COMMAND_QUEUE_TYPE_GRAPHICS;
    VgpuCommandBuffer clearImageCmdBuffer = vgpuCreateCommandBuffer(&commandBufferDescriptor);
    vgpuBeginCommandBuffer(clearImageCmdBuffer);
    for (uint32_t i = 0u; i < handle->imageCount; ++i)
    {
        // Clear with default value if supported.
        if (canClear)
        {
            // Clear images with default color.
            VkClearColorValue clearColor = {};
            clearColor.float32[3] = 1.0f;
            //clearColor.float32[0] = 1.0f;

            VkImageSubresourceRange clearRange = {};
            clearRange.layerCount = 1;
            clearRange.levelCount = 1;

            // Clear with default color.
            vgpuVkClearImageWithColor(
                clearImageCmdBuffer->vk_handle,
                handle->images[i],
                clearRange,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                &clearColor);
        }
        else
        {
            // Transition image to present layout.
            vgpuVkTransitionImageLayout(
                clearImageCmdBuffer->vk_handle,
                handle->images[i],
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            );
        }

        // Create image views from swap chain
        VkImageViewCreateInfo colorAttachmentView = {};
        colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorAttachmentView.pNext = NULL;
        colorAttachmentView.format = createInfo.imageFormat;
        colorAttachmentView.components = {
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A
        };
        colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentView.subresourceRange.baseMipLevel = 0;
        colorAttachmentView.subresourceRange.levelCount = 1;
        colorAttachmentView.subresourceRange.baseArrayLayer = 0;
        colorAttachmentView.subresourceRange.layerCount = 1;
        colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentView.flags = 0;
        colorAttachmentView.image = handle->images[i];

        vgpuVkLogIfFailed(
            vkCreateImageView(_vk.device, &colorAttachmentView, nullptr, &handle->imageViews[i])
        );

        // Create semaphores to be signaled when a swapchain image becomes available.
        vgpuVkCreateSemaphore(_vk.device, &handle->imageSemaphores[i]);

        // Create backend texture.
        //handle->textures[i] = vgpuCreateExternalTexture(&textureDescriptor, handle->images[i]);

        // Create framebuffer.
        //framebufferDescriptor.colorAttachments[0].texture = handle->textures[i];
        //handle->framebuffers[i] = vgpuCreateFramebuffer(&framebufferDescriptor);

        VkImageView attachments[4];
        attachments[0] = handle->imageViews[i];

        VkFramebufferCreateInfo framebufferCreateInfo = {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = handle->renderPass;
        // TODO: Correctly handle multisampling and depth texture
        framebufferCreateInfo.attachmentCount = 1u;
        framebufferCreateInfo.pAttachments = attachments;
        framebufferCreateInfo.width = swapchainSize.width;
        framebufferCreateInfo.height = swapchainSize.height;
        framebufferCreateInfo.layers = 1;

        vgpuVkLogIfFailed(
            vkCreateFramebuffer(_vk.device, &framebufferCreateInfo, nullptr, &handle->framebuffers[i])
        );
    }

    vgpuEndCommandBuffer(clearImageCmdBuffer);
    vgpuVkFlushCommandBuffer(clearImageCmdBuffer);

    handle->width = swapchainSize.width;
    handle->height = swapchainSize.height;
    handle->imageIndex = 0;
    *pSwapchain = handle;
    return VGPU_SUCCESS;
}

void vgpuDestroySwapchain(VgpuSwapchain swapchain)
{
    VGPU_ASSERT(swapchain);
    for (uint32_t i = 0u; swapchain->imageSemaphores != NULL && i < swapchain->imageCount; ++i)
    {
        if (swapchain->imageSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(_vk.device, swapchain->imageSemaphores[i], NULL);
        }
    }

    for (uint32_t i = 0u;
        swapchain->imageViews != NULL && i < swapchain->imageCount; ++i) {
        vkDestroyImageView(_vk.device, swapchain->imageViews[i], NULL);
    }

    for (uint32_t i = 0u;
        swapchain->framebuffers && i < swapchain->imageCount; ++i) {
        vkDestroyFramebuffer(_vk.device, swapchain->framebuffers[i], NULL);
    }

    if (swapchain->renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(_vk.device, swapchain->renderPass, NULL);
    }

    if (swapchain->vk_handle != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(_vk.device, swapchain->vk_handle, NULL);
    }

    VGPU_FREE(swapchain->imageSemaphores);
    VGPU_FREE(swapchain->images);
    //VGPU_FREE(swapchain->textures);
    VGPU_FREE(swapchain->framebuffers);
    VGPU_FREE(swapchain);
}

VgpuResult vgpuVkCreateCommandQueue(VkQueue queue, uint32_t queueFamilyIndex, VgpuCommandQueue* pCommandQueue)
{
    VkCommandPoolCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool vk_handle;
    VkResult result = vkCreateCommandPool(_vk.device, &createInfo, nullptr, &vk_handle);
    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    VgpuCommandQueue handle = VGPU_ALLOC_HANDLE(VgpuCommandQueue);
    handle->vk_queue = queue;
    handle->vk_queueFamilyIndex = queueFamilyIndex;
    handle->vk_handle = vk_handle;
    *pCommandQueue = handle;
    return VGPU_SUCCESS;
}

void vgpuVkDestoryCommandQueue(VgpuCommandQueue queue)
{
    VGPU_ASSERT(queue);

    if (queue->vk_handle != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(_vk.device, queue->vk_handle, nullptr);
    }

    queue->vk_queue = VK_NULL_HANDLE;
    queue->vk_queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    VGPU_FREE(queue);
}

VgpuResult vgpuVkFlushCommandBuffer(VgpuCommandBuffer commandBuffer)
{
    VGPU_ASSERT(commandBuffer);
    VkResult result = VK_SUCCESS;

    VkSubmitInfo submitInfo;
    memset(&submitInfo, 0, sizeof(submitInfo));
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer->vk_handle;

    // Create fence to ensure that the command buffer has finished executing
    VkFence fence = VK_NULL_HANDLE;
    vgpuVkCreateFence(_vk.device, false, &fence);

    // Submit to the queue
    result = vkQueueSubmit(commandBuffer->queue->vk_queue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    // Wait for the fence to signal that command buffer has finished executing.
    result = vkWaitForFences(_vk.device, 1, &fence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    vkDestroyFence(_vk.device, fence, nullptr);
    vgpuDestroyCommandBuffer(commandBuffer);

    return VGPU_SUCCESS;
}

void vgpuVkInitializeFeatures()
{
    /*_features.SetBackend(GraphicsBackend::Vulkan);
    _features.SetVendorId(_physicalDeviceProperties.vendorID);
    // Find vendor
    switch (_physicalDeviceProperties.vendorID)
    {
    case 0x13B5:
        _features.SetVendor(GpuVendor::ARM);
        break;
    case 0x10DE:
        _features.SetVendor(GpuVendor::NVIDIA);
        break;
    case 0x1002:
    case 0x1022:
        _features.SetVendor(GpuVendor::AMD);
        break;
    case 0x163C:
    case 0x8086:
        _features.SetVendor(GpuVendor::INTEL);
        break;
    default:
        _features.SetVendor(GpuVendor::Unknown);
        break;
    }

    _features.SetDeviceId(_physicalDeviceProperties.deviceID);
    _features.SetDeviceName(_physicalDeviceProperties.deviceName);
    _features.SetMultithreading(true);
    _features.SetMaxColorAttachments(_physicalDeviceProperties.limits.maxColorAttachments);
    _features.SetMaxBindGroups(_physicalDeviceProperties.limits.maxBoundDescriptorSets);
    _features.SetMinUniformBufferOffsetAlignment(static_cast<uint32_t>(_physicalDeviceProperties.limits.minUniformBufferOffsetAlignment));*/
}

/* Buffer */
VgpuResult vgpuCreateBuffer(const VgpuBufferDescriptor* descriptor, const void* pInitData, VgpuBuffer* pBuffer) {
    VGPU_ASSERT(descriptor);
    VGPU_ASSERT(pBuffer);
    *pBuffer = nullptr;
    return VGPU_ERROR_GENERIC;
}

VgpuResult vgpuCreateExternalBuffer(const VgpuBufferDescriptor* descriptor, void* handle, VgpuBuffer* pBuffer) {
    VGPU_ASSERT(descriptor);
    VGPU_ASSERT(pBuffer);
    *pBuffer = nullptr;
    return VGPU_ERROR_GENERIC;
}

void vgpuDestroyBuffer(VgpuBuffer buffer) {
    VGPU_ASSERT(buffer);
}

/* Texture */
VgpuResult vgpuCreateTexture(const VgpuTextureDescriptor* descriptor, const void* pInitData, VgpuTexture* pTexture) {
    VGPU_ASSERT(descriptor);
    VGPU_ASSERT(pTexture);

    *pTexture = nullptr;
    return VGPU_ERROR_GENERIC;
}

VgpuResult vgpuCreateExternalTexture(const VgpuTextureDescriptor* descriptor, void* handle, VgpuTexture* pTexture) {
    VGPU_ASSERT(descriptor);
    VGPU_ASSERT(pTexture);

    *pTexture = VGPU_ALLOC_HANDLE(VgpuTexture);
    (*pTexture)->image = (VkImage)handle;
    return VGPU_SUCCESS;
}

void vgpuDestroyTexture(VgpuTexture texture) {
    VGPU_ASSERT(texture);
}

VgpuFramebuffer vgpuCreateFramebuffer(const VgpuFramebufferDescriptor* descriptor) {
    VGPU_ASSERT(descriptor);
    VgpuFramebuffer framebuffer = VGPU_ALLOC_HANDLE(VgpuFramebuffer);
    for (uint32_t i = 0u; i < VGPU_MAX_COLOR_ATTACHMENTS; ++i) {
        if (descriptor->colorAttachments[i].texture == nullptr)
        {
            continue;
        }
    }
    return nullptr;
}

void vgpuDestroyFramebuffer(VgpuFramebuffer framebuffer) {
    VGPU_ASSERT(framebuffer);
    VGPU_FREE(framebuffer);
}

VgpuShaderModule vgpuCreateShaderModule(const VgpuShaderModuleDescriptor* descriptor) {
    VGPU_ASSERT(descriptor);
    VgpuShaderModule shaderModule = VGPU_ALLOC_HANDLE(VgpuShaderModule);
    VkShaderModuleCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = 0u;
    createInfo.codeSize = (size_t)descriptor->codeSize;
    createInfo.pCode = (uint32_t*)descriptor->pCode;
    VkResult result = vkCreateShaderModule(_vk.device, &createInfo, nullptr, &shaderModule->vk_handle);
    if (result != VK_SUCCESS)
    {
        vgpuDestroyShaderModule(shaderModule);
        return nullptr;
    }

    return shaderModule;
}

void vgpuDestroyShaderModule(VgpuShaderModule shaderModule) {
    VGPU_ASSERT(shaderModule);
    if (shaderModule->vk_handle != VK_NULL_HANDLE) {
        vkDestroyShaderModule(_vk.device, shaderModule->vk_handle, nullptr);
    }
    VGPU_FREE(shaderModule);
}

/* Shader */
VgpuShader vgpuCreateShader(const VgpuShaderDescriptor* descriptor) {
    VGPU_ASSERT(descriptor);
    return nullptr;
}

void vgpuDestroyShader(VgpuShader shader) {
    VGPU_ASSERT(shader);
    VGPU_FREE(shader);
}

/* Pipeline */
VgpuPipeline vgpuCreateRenderPipeline(const VgpuRenderPipelineDescriptor* descriptor) {
    return nullptr;
}

VgpuPipeline vgpuCreateComputePipeline(const VgpuComputePipelineDescriptor* descriptor) {
    return nullptr;
}

void vgpuDestroyPipeline(VgpuPipeline pipeline) {
    VGPU_ASSERT(pipeline);
    VGPU_FREE(pipeline);
}

VgpuBool32 vgpuIsVkSupported(VgpuBool32 headless)
{
    if (_vk.initialized) {
        return _vk.available;
    }

    _vk.initialized = VGPU_TRUE;
    if (volkInitialize() != VK_SUCCESS)
    {
        return VGPU_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t i, count, layerCount;
    VkExtensionProperties* queriedExtensions;
    VkLayerProperties* queriedLayers;

    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    if (result < VK_SUCCESS)
    {
        return result;
    }

    queriedExtensions = (VkExtensionProperties*)calloc(count, sizeof(VkExtensionProperties));
    result = vkEnumerateInstanceExtensionProperties(nullptr, &count, queriedExtensions);
    if (result < VK_SUCCESS)
    {
        free(queriedExtensions);
        return result;
    }

    result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    if (result < VK_SUCCESS)
    {
        return result;
    }

    queriedLayers = (VkLayerProperties*)calloc(layerCount, sizeof(VkLayerProperties));
    result = vkEnumerateInstanceLayerProperties(&layerCount, queriedLayers);
    if (result < VK_SUCCESS)
    {
        free(queriedLayers);
        return result;
    }

    // Initialize extensions.
    bool surface = false;
    bool platform_surface = false;
    for (i = 0; i < count; i++)
    {
        if (strcmp(queriedExtensions[i].extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0)
        {
            _vk.KHR_get_physical_device_properties2 = true;
        }
        else if (strcmp(queriedExtensions[i].extensionName, VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME) == 0)
        {
            _vk.KHR_external_memory_capabilities = true;
        }
        else if (strcmp(queriedExtensions[i].extensionName, VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME) == 0)
        {
            _vk.KHR_external_semaphore_capabilities = true;
        }
        else if (strcmp(queriedExtensions[i].extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0)
        {
            _vk.EXT_debug_report = true;
        }
        else if (strcmp(queriedExtensions[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
        {
            _vk.EXT_debug_utils = true;
        }
        else if (strcmp(queriedExtensions[i].extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0)
        {
            surface = true;
        }
        else if (strcmp(queriedExtensions[i].extensionName, VK_SURFACE_EXT) == 0)
        {
            platform_surface = true;
        }
    }

    _vk.KHR_surface = surface && platform_surface;

    // Initialize layers.
    for (i = 0; i < layerCount; i++)
    {
        if (strcmp(queriedLayers[i].layerName, "VK_LAYER_LUNARG_standard_validation") == 0)
        {
            _vk.VK_LAYER_LUNARG_standard_validation = true;
        }
        else if (strcmp(queriedLayers[i].layerName, "VK_LAYER_RENDERDOC_Capture") == 0)
        {
            _vk.VK_LAYER_RENDERDOC_Capture = true;
        }
    }

    // Free allocated stuff
    free(queriedExtensions);
    free(queriedLayers);
    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    _vk.available = true;
    return true;
}

VgpuBackend vgpuGetBackend() {
    return VGPU_BACKEND_VULKAN;
}

VgpuResult vgpuInitialize(const char* applicationName, const VgpuDescriptor* descriptor) {
    if (s_initialized) {
        return VGPU_ALREADY_INITIALIZED;
    }

    const bool headless = descriptor->swapchain == nullptr;
    if (!vgpuIsVkSupported(headless))
    {
        return VGPU_ERROR_INITIALIZATION_FAILED;
    }

    VgpuVkContext* context = VGPU_ALLOC(VgpuVkContext);
    VkResult result = vgpuVkCreateInstance(applicationName, descriptor->validation, headless);
    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    result = vgpuVkDetectPhysicalDevice(descriptor->devicePreference);
    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    if (!headless)
    {
        result = vgpuVkCreateSurface(descriptor->swapchain, &context->surface);
        if (result != VK_SUCCESS)
        {
            return vgpuVkConvertResult(result);
        }
    }

    result = vgpuVkCreateDevice(context->surface);
    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    // Create command queue's.
    vgpuVkCreateCommandQueue(_vk.graphicsQueue, _vk.graphicsQueueFamily, &_vk.graphicsCommandQueue);
    vgpuVkCreateCommandQueue(_vk.computeQueue, _vk.computeQueueFamily, &_vk.computeCommandQueue);
    vgpuVkCreateCommandQueue(_vk.transferQueue, _vk.transferQueueFamily, &_vk.copyCommandQueue);

    vgpuVkInitializeFeatures();

    if (!headless)
    {
        vgpuCreateSwapchain(
            descriptor->swapchain,
            /*s_current_context->swapchain != nullptr ? s_current_context->swapchain->vk_handle : */VK_NULL_HANDLE,
            context->surface,
            &context->swapchain);
    }

    // Create per frame resources
    context->frameIndex = 0u;
    context->maxInflightFrames = headless ? 3u : context->swapchain->imageCount;
    context->frameData = VGPU_ALLOCN(VgpuVkFrameData, context->maxInflightFrames);
    for (uint32_t i = 0u; i < context->maxInflightFrames; i++)
    {
        context->frameData[i].submittedCmdBuffersCount = 0;
        vgpuVkCreateFence(_vk.device, false, &context->frameData[i].fence);
    }

    s_current_context = context;
    s_logCallback = descriptor->logCallback;
    s_initialized = true;
    return VGPU_SUCCESS;
}

void vgpuShutdown()
{
    if (!s_initialized) {
        return;
    }

    vkDeviceWaitIdle(_vk.device);

    for (uint32_t i = 0u; i < s_current_context->maxInflightFrames; i++)
    {
        vkDestroyFence(_vk.device, s_current_context->frameData[i].fence, NULL);
    }

    // Delete swap chain if created.
    if (s_current_context->swapchain != nullptr)
    {
        vgpuDestroySwapchain(s_current_context->swapchain);
        s_current_context->swapchain = nullptr;
    }

    vgpuVkDestoryCommandQueue(_vk.graphicsCommandQueue);
    vgpuVkDestoryCommandQueue(_vk.computeCommandQueue);
    vgpuVkDestoryCommandQueue(_vk.copyCommandQueue);

    if (_vk.device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(_vk.device, nullptr);
        _vk.device = VK_NULL_HANDLE;
    }

    if (_vk.memoryAllocator != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator(_vk.memoryAllocator);
        _vk.memoryAllocator = VK_NULL_HANDLE;
    }

    if (_vk.debugCallback != VK_NULL_HANDLE)
    {
        vkDestroyDebugReportCallbackEXT(_vk.instance, _vk.debugCallback, nullptr);
        _vk.debugCallback = VK_NULL_HANDLE;
    }

    if (_vk.debugMessenger != VK_NULL_HANDLE)
    {
        vkDestroyDebugUtilsMessengerEXT(_vk.instance, _vk.debugMessenger, nullptr);
        _vk.debugMessenger = VK_NULL_HANDLE;
    }

    vkDestroyInstance(_vk.instance, nullptr);
    vgpuVkFreeDevice(_vk.physicalDevice);
    s_initialized = false;
}

VgpuResult vgpuBeginFrame() {
    VkResult result = VK_SUCCESS;

    uint32_t frameIndex = s_current_context->frameIndex;
    if (s_current_context->swapchain != nullptr)
    {
        result = vkAcquireNextImageKHR(
            _vk.device,
            s_current_context->swapchain->vk_handle,
            UINT64_MAX,
            s_current_context->swapchain->imageSemaphores[frameIndex],
            VK_NULL_HANDLE,
            &s_current_context->swapchain->imageIndex);

        if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
        {
            //_mainSwapChain->Resize();
        }
    }

    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }
    else
    {
        s_current_context->frameData[frameIndex].submittedCmdBuffersCount = 0;
        return VGPU_SUCCESS;
    }
}

VgpuResult vgpuEndFrame() {
    VkResult result = VK_SUCCESS;

    uint32_t frameIndex = s_current_context->frameIndex;
    const VgpuVkFrameData *frameData = &s_current_context->frameData[frameIndex];

    // Submit the pending command buffers.
    const VkPipelineStageFlags colorAttachmentStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    uint32_t waitSemaphoreCount = 0u;
    VkSemaphore* waitSemaphores = NULL;
    const VkPipelineStageFlags *waitStageMasks = NULL;

    if (s_current_context->swapchain != nullptr) {
        waitSemaphoreCount++;
        waitSemaphores = &s_current_context->swapchain->imageSemaphores[frameIndex];
        waitStageMasks = &colorAttachmentStage;
    }

    VkSubmitInfo submitInfo;
    memset(&submitInfo, 0, sizeof(submitInfo));
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = waitSemaphoreCount;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStageMasks;
    submitInfo.commandBufferCount = frameData->submittedCmdBuffersCount;
    submitInfo.pCommandBuffers = frameData->submittedCmdBuffers;
    submitInfo.signalSemaphoreCount = frameData->submittedCmdBuffersCount;
    submitInfo.pSignalSemaphores = frameData->waitSemaphores;

    // Submit to the queue
    result = vkQueueSubmit(_vk.graphicsCommandQueue->vk_queue, 1, &submitInfo, frameData->fence);
    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    if (s_current_context->swapchain != nullptr) {
        VkResult presentResult = VK_SUCCESS;
        VkPresentInfoKHR presentInfo;

        memset(&presentInfo, 0, sizeof(presentInfo));
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = NULL;
        presentInfo.waitSemaphoreCount = frameData->submittedCmdBuffersCount;
        presentInfo.pWaitSemaphores = frameData->waitSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &s_current_context->swapchain->vk_handle;
        presentInfo.pImageIndices = &s_current_context->swapchain->imageIndex;
        presentInfo.pResults = &presentResult;
        result = vkQueuePresentKHR(_vk.graphicsQueue, &presentInfo);
        if (result != VK_SUCCESS)
        {
            return vgpuVkConvertResult(presentResult);
        }
    }

    // Wait for frame fence.
    result = vkWaitForFences(_vk.device, 1, &frameData->fence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }
    result = vkResetFences(_vk.device, 1, &frameData->fence);
    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    // Advance frame index.
    s_current_context->frameIndex = (s_current_context->frameIndex + 1u) % s_current_context->maxInflightFrames;

    // TODO: Delete all pending/deferred resources
    // TODO: Free all command buffers (not submitted).

    return vgpuVkConvertResult(result);
}

VgpuResult vgpuWaitIdle() {
    VkResult result = vkDeviceWaitIdle(_vk.device);
    if (result < VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    return VGPU_SUCCESS;
}

bool vgpuVkImageFormatIsSupported(VkFormat format, VkFormatFeatureFlags required, VkImageTiling tiling)
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(_vk.physicalDevice->vk_handle, format, &props);
    VkFormatFeatureFlags flags = tiling == VK_IMAGE_TILING_OPTIMAL ? props.optimalTilingFeatures : props.linearTilingFeatures;
    return (flags & required) == required;
}

VgpuFramebuffer vgpuGetCurrentFramebuffer() {
    return NULL;
}

VgpuPixelFormat vgpuGetDefaultDepthFormat() {

    if (vgpuVkImageFormatIsSupported(VK_FORMAT_D32_SFLOAT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_TILING_OPTIMAL)) {
        return VGPU_PIXEL_FORMAT_D32_FLOAT;
    }

    if (vgpuVkImageFormatIsSupported(VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_TILING_OPTIMAL)) {
        return VGPU_PIXEL_FORMAT_D24_UNORM_S8_UINT;
    }

    if (vgpuVkImageFormatIsSupported(VK_FORMAT_D16_UNORM, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_TILING_OPTIMAL)) {
        return VGPU_PIXEL_FORMAT_D16_UNORM;
    }

    return VGPU_PIXEL_FORMAT_UNDEFINED;
}

VgpuPixelFormat vgpuGetDefaultDepthStencilFormat() {
    if (vgpuVkImageFormatIsSupported(VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_TILING_OPTIMAL)) {
        return VGPU_PIXEL_FORMAT_D24_UNORM_S8_UINT;
    }

    if (vgpuVkImageFormatIsSupported(VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_TILING_OPTIMAL)) {
        return VGPU_PIXEL_FORMAT_D32_FLOAT_S8_UINT;
    }

    return VGPU_PIXEL_FORMAT_UNDEFINED;
}

bool vgpuQueryFeature(VgpuFeature feature) {
    VGPU_ASSERT(s_initialized);

    switch (feature)
    {
    case VGPU_FEATURE_INSTANCING:
        return true;

    case VGPU_FEATURE_ALPHA_TO_COVERAGE:
        return true;

    case VGPU_FEATURE_BLEND_INDEPENDENT:
        return _vk.physicalDevice->features.independentBlend;

    case VGPU_FEATURE_COMPUTE_SHADER:
        return true;

    case VGPU_FEATURE_GEOMETRY_SHADER:
        return _vk.physicalDevice->features.geometryShader;

    case VGPU_FEATURE_TESSELLATION_SHADER:
        return _vk.physicalDevice->features.tessellationShader;

    case VGPU_FEATURE_MULTI_VIEWPORT:
        return _vk.physicalDevice->features.multiViewport;

    case VGPU_FEATURE_INDEX_UINT32:
        return _vk.physicalDevice->features.fullDrawIndexUint32;

    case VGPU_FEATURE_DRAW_INDIRECT:
        return _vk.physicalDevice->features.multiDrawIndirect;

    case VGPU_FEATURE_FILL_MODE_NON_SOLID:
        return _vk.physicalDevice->features.fillModeNonSolid;

    case VGPU_FEATURE_SAMPLER_ANISOTROPY:
        return _vk.physicalDevice->features.samplerAnisotropy;

    case VGPU_FEATURE_TEXTURE_COMPRESSION_BC:
        return _vk.physicalDevice->features.textureCompressionBC;

    case VGPU_FEATURE_TEXTURE_COMPRESSION_PVRTC:
        return false;

    case VGPU_FEATURE_TEXTURE_COMPRESSION_ETC2:
        return _vk.physicalDevice->features.textureCompressionETC2;

    case VGPU_FEATURE_TEXTURE_COMPRESSION_ATC:
    case VGPU_FEATURE_TEXTURE_COMPRESSION_ASTC:
        return _vk.physicalDevice->features.textureCompressionASTC_LDR;

    case VGPU_FEATURE_TEXTURE_1D:
        return true;

    case VGPU_FEATURE_TEXTURE_3D:
        return true;

    case VGPU_FEATURE_TEXTURE_2D_ARRAY:
        return true;

    case VGPU_FEATURE_TEXTURE_CUBE_ARRAY:
        return _vk.physicalDevice->features.imageCubeArray;

    default:
        return false;
    }
}

void vgpuQueryLimits(VgpuLimits* pLimits) {
    VGPU_ASSERT(s_initialized);
    VGPU_ASSERT(pLimits);

    VgpuLimits limits;
    limits.maxTextureDimension1D = _vk.physicalDevice->properties.limits.maxImageDimension1D;
    limits.maxTextureDimension2D = _vk.physicalDevice->properties.limits.maxImageDimension2D;
    limits.maxTextureDimension3D = _vk.physicalDevice->properties.limits.maxImageDimension3D;
    limits.maxTextureDimensionCube = _vk.physicalDevice->properties.limits.maxImageDimensionCube;
    limits.maxTextureArrayLayers = _vk.physicalDevice->properties.limits.maxImageArrayLayers;
    *pLimits = limits;
}

VgpuBool32 vgpuQueryPixelFormatSupport() {
    VGPU_ASSERT(s_initialized);

    VkFormat vkFormat = VK_FORMAT_UNDEFINED;
    VkImageType vkImageType = VK_IMAGE_TYPE_2D;
    VkImageTiling vkTiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags vkImageUsage = VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageFormatProperties properties;
    VkResult result = vkGetPhysicalDeviceImageFormatProperties(
        _vk.physicalDevice->vk_handle,
        vkFormat,
        vkImageType,
        vkTiling,
        vkImageUsage,
        0,
        &properties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
        return VGPU_FALSE;
    }

    return VGPU_TRUE;
}

/* Sampler */
VgpuResult vgpuCreateSampler(const VgpuSamplerDescriptor* descriptor, VgpuSampler* pSampler) {
    VGPU_ASSERT(descriptor);
    VkSamplerCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.magFilter = vgpuVkGetFilter(descriptor->magFilter);
    createInfo.minFilter = vgpuVkGetFilter(descriptor->minFilter);
    createInfo.mipmapMode = vgpuVkGetMipMapFilter(descriptor->mipmapFilter);
    createInfo.addressModeU = vgpuVkGetAddressMode(descriptor->addressModeU, _vk.features.supportsMirrorClampToEdge);
    createInfo.addressModeV = vgpuVkGetAddressMode(descriptor->addressModeV, _vk.features.supportsMirrorClampToEdge);
    createInfo.addressModeW = vgpuVkGetAddressMode(descriptor->addressModeW, _vk.features.supportsMirrorClampToEdge);
    createInfo.mipLodBias = 0.0f;
    createInfo.anisotropyEnable = descriptor->maxAnisotropy > 1u;
    createInfo.maxAnisotropy = _vgpu_clamp((float)descriptor->maxAnisotropy, 1.0f, _vk.physicalDevice->properties.limits.maxSamplerAnisotropy);
    createInfo.compareEnable = descriptor->compareOp != VGPU_COMPARE_OP_NEVER;
    createInfo.compareOp = vgpuVkGetCompareOp(descriptor->compareOp);
    createInfo.minLod = descriptor->lodMinClamp;
    createInfo.maxLod = descriptor->lodMaxClamp;
    createInfo.borderColor = vgpuVkGetBorderColor(descriptor->borderColor);
    createInfo.unnormalizedCoordinates = VK_FALSE;

    VkSampler vk_handle;
    VkResult result = vkCreateSampler(_vk.device, &createInfo, nullptr, &vk_handle);
    if (result != VK_SUCCESS) {
        return vgpuVkConvertResult(result);
    }

    *pSampler = VGPU_ALLOC_HANDLE(VgpuSampler);
    (*pSampler)->vk_handle = vk_handle;
    return VGPU_SUCCESS;;
}

void vgpuDestroySampler(VgpuSampler sampler) {
    // TODO: Deferred release.
    vkDestroySampler(_vk.device, sampler->vk_handle, nullptr);
}

/* CommandBuffer */
VgpuCommandBuffer vgpuCreateCommandBuffer(const VgpuCommandBufferDescriptor* descriptor) {
    VGPU_ASSERT(descriptor);

    VGPU_ASSERT(descriptor);

    VgpuCommandQueue commandQueue = vgpuVkGetCommandQueue(descriptor->type);
    /*if (commandQueue->freeCommandBuffersCount > 0)
    {
        handle = queue->freeCommandBuffers[queue->freeCommandBuffersCount - 1];
        queue->freeCommandBuffersCount--;
        vkResetCommandBuffer(handle->vk_handle, 0);
        *pCommandBuffer = handle;
        return VGPU_SUCCESS;
    }*/

    VgpuCommandBuffer handle = VGPU_ALLOC_HANDLE(VgpuCommandBuffer);
    handle->queue = commandQueue;
    handle->vk_handle = VK_NULL_HANDLE;
    handle->recording = false;
    return handle;
}

void vgpuDestroyCommandBuffer(VgpuCommandBuffer commandBuffer) {
    VGPU_ASSERT(commandBuffer);

    if (commandBuffer->vk_handle != VK_NULL_HANDLE)
    {
        // TODO: Should we recycle the command buffer for later reuse?.
        //commandBuffer->queue->freeCommandBuffers[commandBuffer->queue->freeCommandBuffersCount++] = commandBuffer;
        vkFreeCommandBuffers(_vk.device, commandBuffer->queue->vk_handle, 1, &commandBuffer->vk_handle);
    }

    if (commandBuffer->vk_semaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(_vk.device, commandBuffer->vk_semaphore, nullptr);
    }

    VGPU_FREE(commandBuffer);
}

VgpuResult vgpuBeginCommandBuffer(VgpuCommandBuffer commandBuffer) {
    VGPU_ASSERT(commandBuffer);

    // Verify we're not recording.
    if (commandBuffer->recording) {
        return VGPU_ERROR_COMMAND_BUFFER_ALREADY_RECORDING;
    }

    VkResult result = VK_SUCCESS;
    if (commandBuffer->vk_handle == VK_NULL_HANDLE)
    {
        VkCommandBufferAllocateInfo info;
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.pNext = nullptr;
        info.commandPool = commandBuffer->queue->vk_handle;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1u;

        result = vkAllocateCommandBuffers(_vk.device, &info, &commandBuffer->vk_handle);
    }
    else
    {
        result = vkResetCommandBuffer(commandBuffer->vk_handle, 0);
    }


    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    result = vkBeginCommandBuffer(commandBuffer->vk_handle, &beginInfo);
    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    commandBuffer->recording = true;
    return VGPU_SUCCESS;
}

VgpuResult vgpuEndCommandBuffer(VgpuCommandBuffer commandBuffer) {
    VGPU_ASSERT(commandBuffer);
    if (!commandBuffer->recording) {
        return VGPU_ERROR_COMMAND_BUFFER_NOT_RECORDING;
    }

    VkResult result = vkEndCommandBuffer(commandBuffer->vk_handle);
    if (result != VK_SUCCESS)
    {
        return vgpuVkConvertResult(result);
    }

    commandBuffer->recording = false;
    return VGPU_SUCCESS;
}

VgpuResult vgpuSubmitCommandBuffers(uint32_t count, VgpuCommandBuffer *pBuffers) {
    VGPU_ASSERT(count > 0);
    VGPU_ASSERT(pBuffers);

    uint32_t frameIndex = s_current_context->frameIndex;
    VgpuVkFrameData *frameData = &s_current_context->frameData[frameIndex];
    for (uint32_t i = 0u; i < count; ++i) {
        if (pBuffers[i]->recording) {
            return VGPU_ERROR_COMMAND_BUFFER_NOT_RECORDING;
        }

        frameData->submittedCmdBuffers[frameData->submittedCmdBuffersCount] = pBuffers[i]->vk_handle;

        // Create semaphore for command buffer
        if (pBuffers[i]->vk_semaphore == VK_NULL_HANDLE) {
            vgpuVkCreateSemaphore(_vk.device, &pBuffers[i]->vk_semaphore);
        }

        frameData->waitSemaphores[frameData->submittedCmdBuffersCount] = pBuffers[i]->vk_semaphore;
        ++frameData->submittedCmdBuffersCount;
    }

    return VGPU_SUCCESS;
}


void vgpuCmdBeginDefaultRenderPass(VgpuCommandBuffer commandBuffer, VgpuColor clearColor, float clearDepth, uint8_t clearStencil) {
    VGPU_ASSERT(commandBuffer);

    if (s_current_context->swapchain == nullptr) {
        return;
    }
    uint32_t frameIndex = s_current_context->frameIndex;
    VkClearValue clearValues[2u];
    clearValues[0].color = { { clearColor.r, clearColor.g, clearColor.b, clearColor.a } };
    clearValues[1].depthStencil = { clearDepth, clearStencil };

    VkRenderPassBeginInfo beginInfo;
    memset(&beginInfo, 0, sizeof(beginInfo));
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = s_current_context->swapchain->renderPass;
    beginInfo.framebuffer = s_current_context->swapchain->framebuffers[frameIndex];
    beginInfo.renderArea.offset.x = 0;
    beginInfo.renderArea.offset.y = 0;
    beginInfo.renderArea.extent.width = s_current_context->swapchain->width;
    beginInfo.renderArea.extent.height = s_current_context->swapchain->height;
    beginInfo.clearValueCount = 2u;
    beginInfo.pClearValues = clearValues;
    vkCmdBeginRenderPass(commandBuffer->vk_handle, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void vgpuCmdBeginRenderPass(VgpuCommandBuffer commandBuffer, VgpuFramebuffer framebuffer) {
    VGPU_ASSERT(commandBuffer);
    VGPU_ASSERT(framebuffer);

    VkRenderPassBeginInfo beginInfo;
    memset(&beginInfo, 0, sizeof(beginInfo));
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    //beginInfo.renderPass = _currentRenderPass->GetVkRenderPass();
    //beginInfo.framebuffer = _currentFramebuffer->GetVkFramebuffer();
    beginInfo.renderArea.offset.x = 0;
    beginInfo.renderArea.offset.y = 0;
    beginInfo.renderArea.extent.width = framebuffer->width;
    beginInfo.renderArea.extent.height = framebuffer->height;
    //bassBeginInfo.clearValueCount = 0;
    //bassBeginInfo.pClearValues = nullptr;
    vkCmdBeginRenderPass(commandBuffer->vk_handle, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void vgpuCmdEndRenderPass(VgpuCommandBuffer commandBuffer) {
    VGPU_ASSERT(commandBuffer);
    vkCmdEndRenderPass(commandBuffer->vk_handle);
}

void vgpuCmdSetShader(VgpuCommandBuffer commandBuffer, VgpuShader shader) {
    VGPU_ASSERT(commandBuffer);
}

void vgpuCmdSetVertexBuffer(VgpuCommandBuffer commandBuffer, uint32_t binding, VgpuBuffer buffer, uint64_t offset, VgpuVertexInputRate inputRate) {
    VGPU_ASSERT(commandBuffer);
}

void vgpuCmdSetIndexBuffer(VgpuCommandBuffer commandBuffer, VgpuBuffer buffer, uint64_t offset, VgpuIndexType indexType) {
    VGPU_ASSERT(commandBuffer);
}

void vgpuCmdSetViewport(VgpuCommandBuffer commandBuffer, VgpuViewport viewport) {
    VGPU_ASSERT(commandBuffer);
}

void vgpuCmdSetViewports(VgpuCommandBuffer commandBuffer, uint32_t viewportCount, const VgpuViewport* pViewports) {
    VGPU_ASSERT(commandBuffer);
}

void vgpuCmdSetScissor(VgpuCommandBuffer commandBuffer, VgpuRect2D scissor) {
    VGPU_ASSERT(commandBuffer);
}

void vgpuCmdSetScissors(VgpuCommandBuffer commandBuffer, uint32_t scissorCount, const VgpuRect2D* pScissors) {
    VGPU_ASSERT(commandBuffer);
}

void vgpuCmdSetPrimitiveTopology(VgpuCommandBuffer commandBuffer, VgpuPrimitiveTopology topology) {
    VGPU_ASSERT(commandBuffer);
}

void vgpuCmdDraw(VgpuCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t firstVertex) {
    VGPU_ASSERT(commandBuffer);
}

void vgpuCmdDrawIndexed(VgpuCommandBuffer commandBuffer, uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    VGPU_ASSERT(commandBuffer);
}
