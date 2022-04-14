// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGPU_VULKAN_DRIVER)

#include "vgpu_driver.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#   define NOMINMAX
#   define NODRAWTEXT
#   define NOGDI
#   define NOBITMAP
#   define NOMCX
#   define NOSERVICE
#   define NOHELP
#   define WIN32_LEAN_AND_MEAN
#endif

VGFX_DISABLE_WARNINGS()
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <spirv_reflect.h>
VGFX_ENABLE_WARNINGS()
#include <vector>
#include <deque>
#include <mutex>
#include <unordered_map>

#if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#undef Success
#undef None
#undef Always
#undef Bool
#endif

namespace
{
    inline const char* ToString(VkResult result)
    {
        switch (result)
        {
#define STR(r)   \
	case VK_##r: \
		return #r
            STR(NOT_READY);
            STR(TIMEOUT);
            STR(EVENT_SET);
            STR(EVENT_RESET);
            STR(INCOMPLETE);
            STR(ERROR_OUT_OF_HOST_MEMORY);
            STR(ERROR_OUT_OF_DEVICE_MEMORY);
            STR(ERROR_INITIALIZATION_FAILED);
            STR(ERROR_DEVICE_LOST);
            STR(ERROR_MEMORY_MAP_FAILED);
            STR(ERROR_LAYER_NOT_PRESENT);
            STR(ERROR_EXTENSION_NOT_PRESENT);
            STR(ERROR_FEATURE_NOT_PRESENT);
            STR(ERROR_INCOMPATIBLE_DRIVER);
            STR(ERROR_TOO_MANY_OBJECTS);
            STR(ERROR_FORMAT_NOT_SUPPORTED);
            STR(ERROR_SURFACE_LOST_KHR);
            STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
            STR(SUBOPTIMAL_KHR);
            STR(ERROR_OUT_OF_DATE_KHR);
            STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
            STR(ERROR_VALIDATION_FAILED_EXT);
            STR(ERROR_INVALID_SHADER_NV);
#undef STR
            default:
                return "UNKNOWN_ERROR";
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        _VGFX_UNUSED(pUserData);

        const char* messageTypeStr = "General";

        if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
            messageTypeStr = "Validation";
        else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
            messageTypeStr = "Performance";

        // Log debug messge
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            vgfxLogWarn("Vulkan - %s: %s", messageTypeStr, pCallbackData->pMessage);
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            vgfxLogError("Vulkan - %s: %s", messageTypeStr, pCallbackData->pMessage);
        }

        return VK_FALSE;
    }

    bool ValidateLayers(const std::vector<const char*>& required, const std::vector<VkLayerProperties>& available)
    {
        for (auto layer : required)
        {
            bool found = false;
            for (auto& available_layer : available)
            {
                if (strcmp(available_layer.layerName, layer) == 0)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                vgfxLogWarn("Validation Layer '%s' not found", layer);
                return false;
            }
        }

        return true;
    }

    std::vector<const char*> GetOptimalValidationLayers(const std::vector<VkLayerProperties>& supported_instance_layers)
    {
        std::vector<std::vector<const char*>> validation_layer_priority_list =
        {
            // The preferred validation layer is "VK_LAYER_KHRONOS_validation"
            {"VK_LAYER_KHRONOS_validation"},

            // Otherwise we fallback to using the LunarG meta layer
            {"VK_LAYER_LUNARG_standard_validation"},

            // Otherwise we attempt to enable the individual layers that compose the LunarG meta layer since it doesn't exist
            {
                "VK_LAYER_GOOGLE_threading",
                "VK_LAYER_LUNARG_parameter_validation",
                "VK_LAYER_LUNARG_object_tracker",
                "VK_LAYER_LUNARG_core_validation",
                "VK_LAYER_GOOGLE_unique_objects",
            },

            // Otherwise as a last resort we fallback to attempting to enable the LunarG core layer
            {"VK_LAYER_LUNARG_core_validation"}
        };

        for (auto& validation_layers : validation_layer_priority_list)
        {
            if (ValidateLayers(validation_layers, supported_instance_layers))
            {
                return validation_layers;
            }

            vgfxLogWarn("Couldn't enable validation layers (see log for error) - falling back");
        }

        // Else return nothing
        return {};
    }

    static VkBool32 vulkan_queryPresentationSupport(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex)
    {
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
        // All Android queues surfaces support present.
        return true;
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
        return vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, queueFamilyIndex);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
        // TODO: vkGetPhysicalDeviceXcbPresentationSupportKHR
        return true;
#else
        return true;
#endif
    }

    struct PhysicalDeviceExtensions
    {
        bool swapchain;
        bool depth_clip_enable;
        bool driver_properties;
        bool memory_budget;
        bool performance_query;
        bool host_query_reset;
        bool deferred_host_operations;
        bool renderPass2;
        bool accelerationStructure;
        bool raytracingPipeline;
        bool rayQuery;
        bool fragment_shading_rate;
        bool NV_mesh_shader;
        bool EXT_conditional_rendering;
        bool win32_full_screen_exclusive;
        bool vertex_attribute_divisor;
        bool extended_dynamic_state;
        bool vertex_input_dynamic_state;
        bool extended_dynamic_state2;
        bool dynamic_rendering;
    };

    inline PhysicalDeviceExtensions QueryPhysicalDeviceExtensions(VkPhysicalDevice physicalDevice)
    {
        uint32_t count = 0;
        VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr);
        if (result != VK_SUCCESS)
            return {};

        std::vector<VkExtensionProperties> vk_extensions(count);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, vk_extensions.data());

        PhysicalDeviceExtensions extensions{};

        for (uint32_t i = 0; i < count; ++i)
        {
            if (strcmp(vk_extensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                extensions.swapchain = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME) == 0) {
                extensions.depth_clip_enable = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, "VK_KHR_driver_properties") == 0) {
                extensions.driver_properties = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0) {
                extensions.memory_budget = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME) == 0) {
                extensions.performance_query = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME) == 0) {
                extensions.host_query_reset = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) == 0) {
                extensions.deferred_host_operations = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME) == 0) {
                extensions.renderPass2 = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) {
                extensions.accelerationStructure = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) {
                extensions.raytracingPipeline = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_KHR_RAY_QUERY_EXTENSION_NAME) == 0) {
                extensions.rayQuery = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) == 0) {
                extensions.fragment_shading_rate = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_NV_MESH_SHADER_EXTENSION_NAME) == 0) {
                extensions.NV_mesh_shader = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME) == 0) {
                extensions.EXT_conditional_rendering = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME) == 0) {
                extensions.vertex_attribute_divisor = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME) == 0) {
                extensions.extended_dynamic_state = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME) == 0) {
                extensions.vertex_input_dynamic_state = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME) == 0) {
                extensions.extended_dynamic_state2 = true;
            }
            else if (strcmp(vk_extensions[i].extensionName, "VK_KHR_dynamic_rendering") == 0) {
                extensions.dynamic_rendering = true;
            }
#if defined(VK_USE_PLATFORM_WIN32_KHR)
            else if (strcmp(vk_extensions[i].extensionName, VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME) == 0) {
                extensions.win32_full_screen_exclusive = true;
            }
#endif
        }

        VkPhysicalDeviceProperties gpuProps;
        vkGetPhysicalDeviceProperties(physicalDevice, &gpuProps);

        // Core 1.2
        if (gpuProps.apiVersion >= VK_API_VERSION_1_2)
        {
            extensions.driver_properties = true;
            extensions.renderPass2 = true;
        }

        // Core 1.3
        if (gpuProps.apiVersion >= VK_API_VERSION_1_3)
        {
        }

        return extensions;
    }

    constexpr VkFormat ToVk(VGFXTextureFormat format)
    {
        switch (format)
        {
            // 8-bit formats
            case VGFXTextureFormat_R8UInt:      return VK_FORMAT_R8_UINT;
            case VGFXTextureFormat_R8SInt:      return VK_FORMAT_R8_SINT;
            case VGFXTextureFormat_R8UNorm:     return VK_FORMAT_R8_UNORM;
            case VGFXTextureFormat_R8SNorm:     return VK_FORMAT_R8_SNORM;
                // 16-bit formats
            case VGFXTextureFormat_R16UInt:         return VK_FORMAT_R16_UINT;
            case VGFXTextureFormat_R16SInt:         return VK_FORMAT_R16_SINT;
            case VGFXTextureFormat_R16UNorm:        return VK_FORMAT_R16_UNORM;
            case VGFXTextureFormat_R16SNorm:        return VK_FORMAT_R16_SNORM;
            case VGFXTextureFormat_R16Float:        return VK_FORMAT_R16_SFLOAT;
            case VGFXTextureFormat_RG8UInt:         return VK_FORMAT_R8G8_UINT;
            case VGFXTextureFormat_RG8SInt:         return VK_FORMAT_R8G8_SINT;
            case VGFXTextureFormat_RG8UNorm:        return VK_FORMAT_R8G8_UNORM;
            case VGFXTextureFormat_RG8SNorm:        return VK_FORMAT_R8G8_SNORM;
                // Packed 16-Bit Pixel Formats
            case VGFXTextureFormat_BGRA4UNorm:       return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
            case VGFXTextureFormat_B5G6R5UNorm:      return VK_FORMAT_B5G6R5_UNORM_PACK16;
            case VGFXTextureFormat_B5G5R5A1UNorm:    return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
                // 32-bit formats
            case VGFXTextureFormat_R32UInt:          return VK_FORMAT_R32_UINT;
            case VGFXTextureFormat_R32SInt:          return VK_FORMAT_R32_SINT;
            case VGFXTextureFormat_R32Float:         return VK_FORMAT_R32_SFLOAT;
            case VGFXTextureFormat_RG16UInt:         return VK_FORMAT_R16G16_UINT;
            case VGFXTextureFormat_RG16SInt:         return VK_FORMAT_R16G16_SINT;
            case VGFXTextureFormat_RG16UNorm:        return VK_FORMAT_R16G16_UNORM;
            case VGFXTextureFormat_RG16SNorm:        return VK_FORMAT_R16G16_SNORM;
            case VGFXTextureFormat_RG16Float:        return VK_FORMAT_R16G16_SFLOAT;
            case VGFXTextureFormat_RGBA8UInt:        return VK_FORMAT_R8G8B8A8_UINT;
            case VGFXTextureFormat_BGRA8UNorm:       return VK_FORMAT_B8G8R8A8_UNORM;
            case VGFXTextureFormat_RGBA8UNorm:       return VK_FORMAT_R8G8B8A8_UNORM;
            case VGFXTextureFormat_RGBA8UNormSrgb:   return VK_FORMAT_R8G8B8A8_SRGB;
            case VGFXTextureFormat_RGBA8SNorm:       return VK_FORMAT_R8G8B8A8_SNORM;
            case VGFXTextureFormat_RGBA8SInt:        return VK_FORMAT_R8G8B8A8_SINT;
            case VGFXTextureFormat_BGRA8UNormSrgb:   return VK_FORMAT_B8G8R8A8_SRGB;
                // Packed 32-Bit formats
            case VGFXTextureFormat_RGB10A2UNorm:     return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            case VGFXTextureFormat_RG11B10Float:     return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
            case VGFXTextureFormat_RGB9E5Float:      return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
                // 64-Bit formats
            case VGFXTextureFormat_RG32UInt:         return VK_FORMAT_R32G32_UINT;
            case VGFXTextureFormat_RG32SInt:         return VK_FORMAT_R32G32_SINT;
            case VGFXTextureFormat_RG32Float:        return VK_FORMAT_R32G32_SFLOAT;
            case VGFXTextureFormat_RGBA16UInt:       return VK_FORMAT_R16G16B16A16_UINT;
            case VGFXTextureFormat_RGBA16SInt:       return VK_FORMAT_R16G16B16A16_SINT;
            case VGFXTextureFormat_RGBA16UNorm:      return VK_FORMAT_R16G16B16A16_UNORM;
            case VGFXTextureFormat_RGBA16SNorm:      return VK_FORMAT_R16G16B16A16_SNORM;
            case VGFXTextureFormat_RGBA16Float:      return VK_FORMAT_R16G16B16A16_SFLOAT;
                // 128-Bit formats
            case VGFXTextureFormat_RGBA32UInt:       return VK_FORMAT_R32G32B32A32_UINT;
            case VGFXTextureFormat_RGBA32SInt:       return VK_FORMAT_R32G32B32A32_SINT;
            case VGFXTextureFormat_RGBA32Float:      return VK_FORMAT_R32G32B32A32_SFLOAT;
                // Depth-stencil formats
            case VGFXTextureFormat_Depth16UNorm:     return VK_FORMAT_D16_UNORM;
            case VGFXTextureFormat_Depth32Float:     return VK_FORMAT_D32_SFLOAT;
            case VGFXTextureFormat_Depth24UNormStencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
            case VGFXTextureFormat_Depth32FloatStencil8: return VK_FORMAT_D32_SFLOAT_S8_UINT;
                // Compressed BC formats
            case VGFXTextureFormat_BC1UNorm:         return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            case VGFXTextureFormat_BC1UNormSrgb:     return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
            case VGFXTextureFormat_BC2UNorm:         return VK_FORMAT_BC2_UNORM_BLOCK;
            case VGFXTextureFormat_BC2UNormSrgb:     return VK_FORMAT_BC2_SRGB_BLOCK;
            case VGFXTextureFormat_BC3UNorm:         return VK_FORMAT_BC3_UNORM_BLOCK;
            case VGFXTextureFormat_BC3UNormSrgb:     return VK_FORMAT_BC3_SRGB_BLOCK;
            case VGFXTextureFormat_BC4SNorm:            return VK_FORMAT_BC4_SNORM_BLOCK;
            case VGFXTextureFormat_BC4UNorm:            return VK_FORMAT_BC4_UNORM_BLOCK;
            case VGFXTextureFormat_BC5SNorm:           return VK_FORMAT_BC5_SNORM_BLOCK;
            case VGFXTextureFormat_BC5UNorm:           return VK_FORMAT_BC5_UNORM_BLOCK;
            case VGFXTextureFormat_BC6HUFloat:        return VK_FORMAT_BC6H_UFLOAT_BLOCK;
            case VGFXTextureFormat_BC6HSFloat:         return VK_FORMAT_BC6H_SFLOAT_BLOCK;
            case VGFXTextureFormat_BC7UNorm:         return VK_FORMAT_BC7_UNORM_BLOCK;
            case VGFXTextureFormat_BC7UNormSrgb:     return VK_FORMAT_BC7_SRGB_BLOCK;
                // EAC/ETC compressed formats
            case VGFXTextureFormat_ETC2RGB8UNorm:            return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
            case VGFXTextureFormat_ETC2RGB8UNormSrgb:        return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
            case VGFXTextureFormat_ETC2RGB8A1UNorm:          return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
            case VGFXTextureFormat_ETC2RGB8A1UNormSrgb:      return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
            case VGFXTextureFormat_ETC2RGBA8UNorm:           return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
            case VGFXTextureFormat_ETC2RGBA8UNormSrgb:       return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
            case VGFXTextureFormat_EACR11UNorm:              return VK_FORMAT_EAC_R11_UNORM_BLOCK;
            case VGFXTextureFormat_EACR11SNorm:              return VK_FORMAT_EAC_R11_SNORM_BLOCK;
            case VGFXTextureFormat_EACRG11UNorm:             return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
            case VGFXTextureFormat_EACRG11SNorm:             return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
                // ASTC compressed formats
            case VGFXTextureFormat_ASTC4x4UNorm:
                return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC4x4UNormSrgb:
                return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC5x4UNorm:
                return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC5x4UNormSrgb:
                return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC5x5UNorm:
                return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC5x5UNormSrgb:
                return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC6x5UNorm:
                return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC6x5UNormSrgb:
                return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC6x6UNorm:
                return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC6x6UNormSrgb:
                return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC8x5UNorm:
                return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC8x5UNormSrgb:
                return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC8x6UNorm:
                return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC8x6UNormSrgb:
                return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC8x8UNorm:
                return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC8x8UNormSrgb:
                return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC10x5UNorm:
                return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC10x5UNormSrgb:
                return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC10x6UNorm:
                return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC10x6UNormSrgb:
                return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC10x8UNorm:
                return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC10x8UNormSrgb:
                return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC10x10UNorm:
                return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC10x10UNormSrgb:
                return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC12x10UNorm:
                return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC12x10UNormSrgb:
                return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
            case VGFXTextureFormat_ASTC12x12UNorm:
                return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
            case VGFXTextureFormat_ASTC12x12UNormSrgb:
                return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;

            default:
                return VK_FORMAT_UNDEFINED;
        }
    }

    constexpr VkAttachmentLoadOp ToVk(VGFXLoadOp op)
    {
        switch (op)
        {
            case VGFXLoadOp_DontCare:
                return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            case VGFXLoadOp_Load:
                return VK_ATTACHMENT_LOAD_OP_LOAD;
            case VGFXLoadOp_Clear:
                return VK_ATTACHMENT_LOAD_OP_CLEAR;

            default:
                return VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
        }
    }

    constexpr VkAttachmentStoreOp ToVk(VGFXStoreOp op)
    {
        switch (op)
        {
            case VGFXStoreOp_DontCare:
                return VK_ATTACHMENT_STORE_OP_DONT_CARE;
            case VGFXStoreOp_Store:
                return VK_ATTACHMENT_STORE_OP_STORE;

            default:
                return VK_ATTACHMENT_STORE_OP_MAX_ENUM;
        }
    }

    constexpr VkImageAspectFlags GetImageAspectFlags(VkFormat format)
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
}

/// Helper macro to test the result of Vulkan calls which can return an error.
#define VK_CHECK(x) \
	do \
	{ \
		VkResult err = x; \
		if (err) \
		{ \
			vgfxLogError("Detected Vulkan error: %s", ToString(err)); \
		} \
	} while (0)

#define VK_LOG_ERROR(result, message) vgfxLogError("Vulkan: %s, error: %s", message, ToString(result));

struct VGFXVulkanBuffer
{
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation  allocation = nullptr;
};

struct VGFXVulkanTexture
{
    VkImage handle = VK_NULL_HANDLE;
    VmaAllocation  allocation = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    bool isSwapChain = false;
    std::unordered_map<size_t, VkImageView> viewCache;
};

struct VGFXVulkanSwapChain
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    bool vsync = false;
    VGFXTextureFormat colorFormat;
    bool allowHDR = true;
    uint32_t imageIndex;
    std::vector<VGFXTexture> backbufferTextures;

    VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
    VkSemaphore releaseSemaphore = VK_NULL_HANDLE;
};

struct VGFXVulkanRenderer
{
#if defined(VK_USE_PLATFORM_XCB_KHR)
    struct {
        void* handle;
        decltype(&::XGetXCBConnection) GetXCBConnection;
    } x11xcb;
#endif

    bool debugUtils;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugUtilsMessenger;

    PhysicalDeviceExtensions supportedExtensions;
    VkPhysicalDeviceDriverProperties driverProperties;
    VkPhysicalDeviceProperties2 properties2 = {};
    VkPhysicalDeviceVulkan11Properties properties_1_1 = {};
    VkPhysicalDeviceVulkan12Properties properties_1_2 = {};
    VkPhysicalDeviceSamplerFilterMinmaxProperties sampler_minmax_properties = {};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties = {};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR raytracing_properties = {};
    VkPhysicalDeviceFragmentShadingRatePropertiesKHR fragment_shading_rate_properties = {};
    VkPhysicalDeviceMeshShaderPropertiesNV mesh_shader_properties = {};

    VkPhysicalDeviceFeatures2 features2 = {};
    VkPhysicalDeviceVulkan11Features features_1_1 = {};
    VkPhysicalDeviceVulkan12Features features_1_2 = {};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features = {};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracing_features = {};
    VkPhysicalDeviceRayQueryFeaturesKHR raytracing_query_features = {};
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragment_shading_rate_features = {};
    VkPhysicalDeviceMeshShaderFeaturesNV mesh_shader_features = {};
    VkPhysicalDeviceConditionalRenderingFeaturesEXT conditional_rendering_features = {};
    VkPhysicalDeviceDepthClipEnableFeaturesEXT depth_clip_enable_features = {};
    VkPhysicalDevicePerformanceQueryFeaturesKHR perf_counter_features = {};
    VkPhysicalDeviceHostQueryResetFeatures host_query_reset_features = {};
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT vertexDivisorFeatures = {};
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures = {};
    VkPhysicalDeviceExtendedDynamicState2FeaturesEXT extendedDynamicState2Features = {};
    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertexInputDynamicStateFeatures = {};
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {};
    bool dynamicRendering{ false }; // Beta
    VkDeviceSize minAllocationAlignment{ 0 };
    std::string driverDescription;

    VkPhysicalDevice physicalDevice;
    uint32_t graphicsQueueFamily = VK_QUEUE_FAMILY_IGNORED;
    uint32_t computeQueueFamily = VK_QUEUE_FAMILY_IGNORED;
    uint32_t copyQueueFamily = VK_QUEUE_FAMILY_IGNORED;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkQueue copyQueue = VK_NULL_HANDLE;

    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;

    uint32_t frameIndex = 0;
    uint64_t frameCount = 0;

    struct FrameResources
    {
        VkFence fence = VK_NULL_HANDLE;
        VkCommandPool initCommandPool = VK_NULL_HANDLE;
        VkCommandBuffer initCommandBuffer = VK_NULL_HANDLE;

        VkCommandPool frameCommandPool = VK_NULL_HANDLE;
        VkCommandBuffer frameCommandBuffer = VK_NULL_HANDLE;
    };
    FrameResources frames[VGFX_MAX_INFLIGHT_FRAMES];

    VkCommandBuffer GetCommandBuffer() const {
        return frames[frameIndex].frameCommandBuffer;
    }

    std::mutex initLocker;
    bool submitInits = false;

    std::vector<VGFXVulkanSwapChain*> swapChains;
    std::vector<VGFXVulkanTexture*> swapChainTextures;

    VkBuffer		nullBuffer = VK_NULL_HANDLE;
    VmaAllocation	nullBufferAllocation = VK_NULL_HANDLE;
    VkBufferView	nullBufferView = VK_NULL_HANDLE;
    VkSampler		nullSampler = VK_NULL_HANDLE;
    VmaAllocation	nullImageAllocation1D = VK_NULL_HANDLE;
    VmaAllocation	nullImageAllocation2D = VK_NULL_HANDLE;
    VmaAllocation	nullImageAllocation3D = VK_NULL_HANDLE;
    VkImage			nullImage1D = VK_NULL_HANDLE;
    VkImage			nullImage2D = VK_NULL_HANDLE;
    VkImage			nullImage3D = VK_NULL_HANDLE;
    VkImageView		nullImageView1D = VK_NULL_HANDLE;
    VkImageView		nullImageView1DArray = VK_NULL_HANDLE;
    VkImageView		nullImageView2D = VK_NULL_HANDLE;
    VkImageView		nullImageView2DArray = VK_NULL_HANDLE;
    VkImageView		nullImageViewCube = VK_NULL_HANDLE;
    VkImageView		nullImageViewCubeArray = VK_NULL_HANDLE;
    VkImageView		nullImageView3D = VK_NULL_HANDLE;

    std::unordered_map<size_t, VkRenderPass> renderPassCache;
    std::unordered_map<size_t, VkFramebuffer> framebufferCache;

    // Deletion queue objects
    std::mutex destroyMutex;
    std::deque<std::pair<std::pair<VkBuffer, VmaAllocation>, uint64_t>> destroyedBuffers;
    std::deque<std::pair<std::pair<VkImage, VmaAllocation>, uint64_t>> destroyedImagesQueue;
    std::deque<std::pair<VkImageView, uint64_t>> destroyedImageViews;
    std::deque<std::pair<VkSampler, uint64_t>> destroyedSamplers;
    std::deque<std::pair<VkShaderModule, uint64_t>> destroyedShaderModules;
    std::deque<std::pair<VkPipeline, uint64_t>> destroyedPipelines;
    std::deque<std::pair<VkDescriptorPool, uint64_t>> destroyedDescriptorPools;
    std::deque<std::pair<VkQueryPool, uint64_t>> destroyedQueryPools;
};

static void vulkan_SetObjectName(VGFXVulkanRenderer* renderer, VkObjectType type, uint64_t handle, const char* name)
{
    if (!renderer->debugUtils)
    {
        return;
    }

    VkDebugUtilsObjectNameInfoEXT info;
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.pNext = nullptr;
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = name;
    VK_CHECK(vkSetDebugUtilsObjectNameEXT(renderer->device, &info));
}

static VkSurfaceKHR vulkan_createSurface(VGFXVulkanRenderer* renderer, void* windowHandle)
{
    VkResult result = VK_SUCCESS;
    VkSurfaceKHR vk_surface = VK_NULL_HANDLE;

    VkInstance instance = renderer->instance;

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    VkAndroidSurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.window = surface->window;
    result = vkCreateAndroidSurfaceKHR(instance, &createInfo, NULL, &vk_surface);
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandle(nullptr);
    createInfo.hwnd = (HWND)windowHandle;
    result = vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &vk_surface);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    VkMetalSurfaceCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    createInfo.pLayer = (const CAMetalLayer*)windowHandle;
    result = vkCreateMetalSurfaceEXT(instance, &createInfo, nullptr, &vk_surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    VkXcbSurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    //createInfo.connection = renderer->GetXCBConnection(static_cast<Display*>(surface->display));
    createInfo.window = (xcb_window_t)windowHandle;
    result = vkCreateXcbSurfaceKHR(instance, &createInfo, nullptr, &vk_surface);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    VkXlibSurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    //createInfo.dpy = (Display*)surface->display;
    createInfo.window = (Window)windowHandle;
    result = vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &vk_surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    VkWaylandSurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    createInfo.display = display;
    createInfo.surface = (wl_surface*)windowHandle;
    result = vkCreateWaylandSurfaceKHR(instance, &createInfo, nullptr, &vk_surface);
#elif defined(VK_USE_PLATFORM_DISPLAY_KHR)
#elif defined(VK_USE_PLATFORM_HEADLESS_EXT)
    VkHeadlessSurfaceCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
    result = vkCreateHeadlessSurfaceEXT(instance, &createInfo, nullptr, &vk_surface);
#endif

    if (result != VK_SUCCESS)
    {
        VK_LOG_ERROR(result, "Failed to create surface");
    }

    return vk_surface;
}

static VkImageView vulkan_GetView(VGFXVulkanRenderer* renderer, VGFXVulkanTexture* texture, uint32_t baseMipLevel, uint32_t levelCount, uint32_t baseArrayLayer, uint32_t layerCount)
{
    size_t hash = 0;
    hash_combine(hash, baseMipLevel);
    hash_combine(hash, levelCount);
    hash_combine(hash, baseArrayLayer);
    hash_combine(hash, layerCount);

    auto it = texture->viewCache.find(hash);
    if (it == texture->viewCache.end())
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = texture->handle;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = texture->format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = GetImageAspectFlags(viewInfo.format);
        viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
        viewInfo.subresourceRange.levelCount = levelCount;
        viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView newView;
        const VkResult result = vkCreateImageView(renderer->device, &viewInfo, nullptr, &newView);
        if (result != VK_SUCCESS)
        {
            VK_LOG_ERROR(result, "Failed to create ImageView");
            return VK_NULL_HANDLE;
        }

        texture->viewCache[hash] = newView;
        return newView;
    }

    return it->second;
}

static VkImageView vulkan_GetRTV(VGFXVulkanRenderer* renderer, VGFXVulkanTexture* texture, uint32_t level, uint32_t slice)
{
    return vulkan_GetView(renderer, texture, level, 1, slice, 1);
}

static VkRenderPass vulkan_RequestRenderPass(VGFXVulkanRenderer* renderer, const VGFXRenderPassDesc* info)
{
    size_t hash = 0;
    hash_combine(hash, info->colorAttachmentCount);

    for (uint32_t i = 0; i < info->colorAttachmentCount; ++i)
    {
        VGFXVulkanTexture* texture = (VGFXVulkanTexture*)info->colorAttachments[i].texture;

        hash_combine(hash, texture->format);
        hash_combine(hash, info->colorAttachments[i].loadOp);
        hash_combine(hash, info->colorAttachments[i].storeOp);
        if (texture->isSwapChain)
        {
            hash_combine(hash, VK_IMAGE_LAYOUT_UNDEFINED);
            hash_combine(hash, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        }
        else
        {
            hash_combine(hash, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            hash_combine(hash, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
    }

    auto it = renderer->renderPassCache.find(hash);
    if (it == renderer->renderPassCache.end())
    {
        // Not found -> create new
        VkAttachmentDescription2  attachmentDescriptions[VGFX_MAX_COLOR_ATTACHMENTS * 2 + 1] = {};
        VkAttachmentReference2 colorAttachmentRefs[VGFX_MAX_COLOR_ATTACHMENTS] = {};
        VkAttachmentReference2 resolveAttachmentRefs[VGFX_MAX_COLOR_ATTACHMENTS] = {};
        VkAttachmentReference2 shadingRateAttachmentRef = {};
        VkAttachmentReference2 depthStencilAttachmentRef = {};

        const VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT; // VulkanSampleCount(key->sampleCount);

        VkSubpassDescription2 subpass = {};
        subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        for (uint32_t i = 0; i < info->colorAttachmentCount; ++i)
        {
            VGFXVulkanTexture* texture = (VGFXVulkanTexture*)info->colorAttachments[i].texture;

            auto& attachmentRef = colorAttachmentRefs[subpass.colorAttachmentCount];
            auto& attachmentDesc = attachmentDescriptions[subpass.colorAttachmentCount];

            attachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
            attachmentRef.attachment = subpass.colorAttachmentCount;
            attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentRef.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            attachmentDesc.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
            attachmentDesc.flags = 0;
            attachmentDesc.format = texture->format;
            attachmentDesc.samples = sampleCount;
            attachmentDesc.loadOp = ToVk(info->colorAttachments[i].loadOp);
            attachmentDesc.storeOp = ToVk(info->colorAttachments[i].storeOp);
            attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            if (texture->isSwapChain)
            {
                attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            }
            else
            {
                attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }

            subpass.colorAttachmentCount++;
        }

        const bool hasDepthStencil = info->depthStencilAttachment != nullptr;

        uint32_t attachmentCount = subpass.colorAttachmentCount;
        VkAttachmentReference2* depthStencilAttachment = nullptr;
        if (hasDepthStencil)
        {
            const VGFXRenderPassDepthStencilAttachment* attachment = info->depthStencilAttachment;
            VGFXVulkanTexture* texture = (VGFXVulkanTexture*)attachment->texture;

            auto& attachmentDesc = attachmentDescriptions[subpass.colorAttachmentCount];
            depthStencilAttachment = &depthStencilAttachmentRef;

            depthStencilAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
            depthStencilAttachmentRef.attachment = subpass.colorAttachmentCount;
            depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthStencilAttachmentRef.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            attachmentDesc.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
            attachmentDesc.flags = 0;
            attachmentDesc.format = texture->format;
            attachmentDesc.samples = sampleCount;
            attachmentDesc.loadOp = ToVk(attachment->depthLoadOp);
            attachmentDesc.storeOp = ToVk(attachment->depthStoreOp);
            attachmentDesc.stencilLoadOp = ToVk(attachment->stencilLoadOp);
            attachmentDesc.stencilStoreOp = ToVk(attachment->stencilStoreOp);
            attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            ++attachmentCount;
        }

        subpass.pColorAttachments = colorAttachmentRefs;
        //subpassDesc.pResolveAttachments = resolveTargetAttachmentRefs;
        subpass.pDepthStencilAttachment = depthStencilAttachment;

        // Use subpass dependencies for layout transitions
        VkSubpassDependency2 dependencies[2] = {};
        dependencies[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        dependencies[1].pNext = nullptr;
        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        if (hasDepthStencil)
        {
            dependencies[0].dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependencies[0].dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            dependencies[1].srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependencies[1].srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        // Finally, create the renderpass.
        VkRenderPassCreateInfo2 createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
        createInfo.attachmentCount = attachmentCount;
        createInfo.pAttachments = attachmentDescriptions;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 2u;
        createInfo.pDependencies = dependencies;

        VkRenderPass renderPass = VK_NULL_HANDLE;
        VK_CHECK(vkCreateRenderPass2(renderer->device, &createInfo, nullptr, &renderPass));

        renderer->renderPassCache[hash] = renderPass;

        return renderPass;
    }

    return it->second;
}

static VkFramebuffer vulkan_RequestFramebuffer(
    VGFXVulkanRenderer* renderer,
    VkRenderPass renderPass,
    const VGFXRenderPassDesc* info,
    uint32_t* width, uint32_t* height)
{
    *width = UINT32_MAX;
    *height = UINT32_MAX;
    uint32_t layers = 1u;

    size_t hash = 0;
    hash_combine(hash, renderPass);

    uint32_t attachmentCount = 0;
    VkImageView attachments[VGFX_MAX_COLOR_ATTACHMENTS + 1];

    for (uint32_t i = 0; i < info->colorAttachmentCount; ++i)
    {
        const VGFXRenderPassColorAttachment* attachment = &info->colorAttachments[i];

        VGFXVulkanTexture* texture = (VGFXVulkanTexture*)attachment->texture;
        const uint32_t level = attachment->level;
        const uint32_t slice = attachment->slice;

        attachments[attachmentCount] = vulkan_GetRTV(renderer, texture, level, slice);
        hash_combine(hash, attachments[attachmentCount]);
        attachmentCount++;

        *width = std::min(*width, std::max(1U, texture->width >> level));
        *height = std::min(*height, std::max(1U, texture->height >> level));
    }

    hash_combine(hash, attachmentCount);

    auto it = renderer->framebufferCache.find(hash);
    if (it == renderer->framebufferCache.end())
    {
        VkFramebufferCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.renderPass = renderPass;
        createInfo.attachmentCount = attachmentCount;
        createInfo.pAttachments = attachments;
        createInfo.width = *width;
        createInfo.height = *height;
        createInfo.layers = layers;

        VkFramebuffer framebuffer;
        VK_CHECK(vkCreateFramebuffer(renderer->device, &createInfo, nullptr, &framebuffer));
        renderer->framebufferCache[hash] = framebuffer;

        return framebuffer;
    }

    return it->second;
}

static void vulkan_ProcessDeletionQueue(VGFXVulkanRenderer* renderer)
{
    renderer->destroyMutex.lock();

    while (!renderer->destroyedBuffers.empty())
    {
        if (renderer->destroyedBuffers.front().second + VGFX_MAX_INFLIGHT_FRAMES < renderer->frameCount)
        {
            auto item = renderer->destroyedBuffers.front();
            renderer->destroyedBuffers.pop_front();
            vmaDestroyBuffer(renderer->allocator, item.first.first, item.first.second);
        }
        else
        {
            break;
        }
    }

    while (!renderer->destroyedImagesQueue.empty())
    {
        if (renderer->destroyedImagesQueue.front().second + VGFX_MAX_INFLIGHT_FRAMES < renderer->frameCount)
        {
            auto item = renderer->destroyedImagesQueue.front();
            renderer->destroyedImagesQueue.pop_front();
            vmaDestroyImage(renderer->allocator, item.first.first, item.first.second);
        }
        else
        {
            break;
        }
    }

    while (!renderer->destroyedImageViews.empty())
    {
        if (renderer->destroyedImageViews.front().second + VGFX_MAX_INFLIGHT_FRAMES < renderer->frameCount)
        {
            auto item = renderer->destroyedImageViews.front();
            renderer->destroyedImageViews.pop_front();
            vkDestroyImageView(renderer->device, item.first, nullptr);
        }
        else
        {
            break;
        }
    }

    while (!renderer->destroyedSamplers.empty())
    {
        if (renderer->destroyedSamplers.front().second + VGFX_MAX_INFLIGHT_FRAMES < renderer->frameCount)
        {
            auto item = renderer->destroyedSamplers.front();
            renderer->destroyedSamplers.pop_front();
            vkDestroySampler(renderer->device, item.first, nullptr);
        }
        else
        {
            break;
        }
    }

    while (!renderer->destroyedPipelines.empty())
    {
        if (renderer->destroyedPipelines.front().second + VGFX_MAX_INFLIGHT_FRAMES < renderer->frameCount)
        {
            auto item = renderer->destroyedPipelines.front();
            renderer->destroyedPipelines.pop_front();
            vkDestroyPipeline(renderer->device, item.first, nullptr);
        }
        else
        {
            break;
        }
    }

    while (!renderer->destroyedDescriptorPools.empty())
    {
        if (renderer->destroyedDescriptorPools.front().second + VGFX_MAX_INFLIGHT_FRAMES < renderer->frameCount)
        {
            auto item = renderer->destroyedDescriptorPools.front();
            renderer->destroyedDescriptorPools.pop_front();
            vkDestroyDescriptorPool(renderer->device, item.first, nullptr);
        }
        else
        {
            break;
        }
    }

    while (!renderer->destroyedQueryPools.empty())
    {
        if (renderer->destroyedQueryPools.front().second + VGFX_MAX_INFLIGHT_FRAMES < renderer->frameCount)
        {
            auto item = renderer->destroyedQueryPools.front();
            renderer->destroyedQueryPools.pop_front();
            vkDestroyQueryPool(renderer->device, item.first, nullptr);
        }
        else
        {
            break;
        }
    }
    renderer->destroyMutex.unlock();
}

static void vulkan_destroyDevice(VGPUDevice device)
{
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)device->driverData;
    VK_CHECK(vkDeviceWaitIdle(renderer->device));

    for (auto& frame : renderer->frames)
    {
        vkDestroyFence(renderer->device, frame.fence, nullptr);
        vkDestroyCommandPool(renderer->device, frame.initCommandPool, nullptr);
        vkDestroyCommandPool(renderer->device, frame.frameCommandPool, nullptr);
    }

    // Release caches
    for (auto& it : renderer->framebufferCache)
    {
        vkDestroyFramebuffer(renderer->device, it.second, nullptr);
    }
    renderer->framebufferCache.clear();

    for (auto& it : renderer->renderPassCache)
    {
        vkDestroyRenderPass(renderer->device, it.second, nullptr);
    }
    renderer->renderPassCache.clear();

    renderer->frameCount = UINT64_MAX;
    vulkan_ProcessDeletionQueue(renderer);
    renderer->frameCount = 0;

    vmaDestroyBuffer(renderer->allocator, renderer->nullBuffer, renderer->nullBufferAllocation);
    vkDestroyBufferView(renderer->device, renderer->nullBufferView, nullptr);
    vmaDestroyImage(renderer->allocator, renderer->nullImage1D, renderer->nullImageAllocation1D);
    vmaDestroyImage(renderer->allocator, renderer->nullImage2D, renderer->nullImageAllocation2D);
    vmaDestroyImage(renderer->allocator, renderer->nullImage3D, renderer->nullImageAllocation3D);
    vkDestroyImageView(renderer->device, renderer->nullImageView1D, nullptr);
    vkDestroyImageView(renderer->device, renderer->nullImageView1DArray, nullptr);
    vkDestroyImageView(renderer->device, renderer->nullImageView2D, nullptr);
    vkDestroyImageView(renderer->device, renderer->nullImageView2DArray, nullptr);
    vkDestroyImageView(renderer->device, renderer->nullImageViewCube, nullptr);
    vkDestroyImageView(renderer->device, renderer->nullImageViewCubeArray, nullptr);
    vkDestroyImageView(renderer->device, renderer->nullImageView3D, nullptr);
    vkDestroySampler(renderer->device, renderer->nullSampler, nullptr);

    if (renderer->allocator != VK_NULL_HANDLE)
    {
        //VmaStats stats;
        //vmaCalculateStats(renderer->allocator, &stats);

        //if (stats.total.usedBytes > 0)
        //{
        //    vgfxLogError("Total device memory leaked: {} bytes.", stats.total.usedBytes);
        //}

        vmaDestroyAllocator(renderer->allocator);
        renderer->allocator = VK_NULL_HANDLE;
    }

    if (renderer->device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(renderer->device, nullptr);
        renderer->device = VK_NULL_HANDLE;
    }

    if (renderer->debugUtilsMessenger != VK_NULL_HANDLE)
    {
        vkDestroyDebugUtilsMessengerEXT(renderer->instance, renderer->debugUtilsMessenger, nullptr);
        renderer->debugUtilsMessenger = VK_NULL_HANDLE;
    }

    if (renderer->instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(renderer->instance, nullptr);
        renderer->instance = VK_NULL_HANDLE;
    }

#if defined(VK_USE_PLATFORM_XCB_KHR)
    if (renderer->x11xcb.handle)
    {
        dlclose(renderer->x11xcb.handle);
        renderer->x11xcb.handle = NULL;
    }
#endif

    delete renderer;
    VGFX_FREE(device);
}

static uint64_t vulkan_frame(VGFXRenderer* driverData)
{
    VkResult result = VK_SUCCESS;
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;

    // Collect present swapchains
    std::vector<VkSemaphore> waitSemaphores;
    std::vector<VkPipelineStageFlags> waitStages;
    std::vector<VkSemaphore> signalSemaphores;
    std::vector<VkSwapchainKHR> presentSwapChains;
    std::vector<uint32_t> swapChainImageIndices;

    for (size_t i = 0, count = renderer->swapChains.size(); i < count; ++i)
    {
        VGFXVulkanSwapChain* swapChain = renderer->swapChains[i];

        waitSemaphores.push_back(swapChain->acquireSemaphore);
        waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        signalSemaphores.push_back(swapChain->releaseSemaphore);

        presentSwapChains.push_back(swapChain->handle);
        swapChainImageIndices.push_back(swapChain->imageIndex);
    }

    renderer->initLocker.lock();

    // Submit current frame.
    {
        auto& frame = renderer->frames[renderer->frameIndex];

        // Transitions first.
        std::vector<VkCommandBuffer> submitCommandBuffers;
        if (renderer->submitInits)
        {
            result = vkEndCommandBuffer(frame.initCommandBuffer);
            VGFX_ASSERT(result == VK_SUCCESS);
            submitCommandBuffers.push_back(frame.initCommandBuffer);
        }

        result = vkEndCommandBuffer(frame.frameCommandBuffer);
        VGFX_ASSERT(result == VK_SUCCESS);
        submitCommandBuffers.push_back(frame.frameCommandBuffer);

        // Submit now
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages.data();
        submitInfo.commandBufferCount = (uint32_t)submitCommandBuffers.size();
        submitInfo.pCommandBuffers = submitCommandBuffers.data();
        submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
        submitInfo.pSignalSemaphores = signalSemaphores.data();

        result = vkQueueSubmit(renderer->graphicsQueue, 1, &submitInfo, frame.fence);
        VGFX_ASSERT(result == VK_SUCCESS);

        if (!presentSwapChains.empty())
        {
            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = (uint32_t)signalSemaphores.size();
            presentInfo.pWaitSemaphores = signalSemaphores.data();
            presentInfo.swapchainCount = (uint32_t)presentSwapChains.size();
            presentInfo.pSwapchains = presentSwapChains.data();
            presentInfo.pImageIndices = swapChainImageIndices.data();
            result = vkQueuePresentKHR(renderer->graphicsQueue, &presentInfo);
            if (result != VK_SUCCESS)
            {
                // Handle outdated error in present:
                if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    // TODO
                }
                else
                {
                    VGFX_ASSERT(false);
                }
            }
        }
    }

    renderer->swapChains.clear();

    renderer->frameCount++;
    renderer->frameIndex = renderer->frameCount % VGFX_MAX_INFLIGHT_FRAMES;

    // Begin new frame
    {
        auto& frame = renderer->frames[renderer->frameIndex];

        // Initiate stalling CPU when GPU is not yet finished with next frame.
        if (renderer->frameCount >= VGFX_MAX_INFLIGHT_FRAMES)
        {
            result = vkWaitForFences(renderer->device, 1, &frame.fence, VK_TRUE, 0xFFFFFFFFFFFFFFFF);
            VGFX_ASSERT(result == VK_SUCCESS);

            result = vkResetFences(renderer->device, 1, &frame.fence);
            VGFX_ASSERT(result == VK_SUCCESS);
        }


        // Safe delete deferred destroys
        vulkan_ProcessDeletionQueue(renderer);

        // Restart transition command buffers first.
        result = vkResetCommandPool(renderer->device, frame.initCommandPool, 0);
        VGFX_ASSERT(result == VK_SUCCESS);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = vkBeginCommandBuffer(frame.initCommandBuffer, &beginInfo);
        VGFX_ASSERT(result == VK_SUCCESS);

        // Restart frame command buffers now.
        result = vkResetCommandPool(renderer->device, frame.frameCommandPool, 0);
        VGFX_ASSERT(result == VK_SUCCESS);
        result = vkBeginCommandBuffer(frame.frameCommandBuffer, &beginInfo);
        VGFX_ASSERT(result == VK_SUCCESS);
    }

    renderer->submitInits = false;
    renderer->initLocker.unlock();

    // Return current frame
    return renderer->frameCount - 1;
}

static void vulkan_waitIdle(VGFXRenderer* driverData) {
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;
    VK_CHECK(vkDeviceWaitIdle(renderer->device));
}

static bool vulkan_hasFeature(VGFXRenderer* driverData, VGPUFeature feature) {
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;
    switch (feature)
    {
        case VGPU_FEATURE_COMPUTE:
            return true;

        case VGPU_FEATURE_INDEPENDENT_BLEND:
            return renderer->features2.features.independentBlend == VK_TRUE;

        case VGPU_FEATURE_TEXTURE_CUBE_ARRAY:
            return renderer->features2.features.imageCubeArray == VK_TRUE;

        case VGPU_FEATURE_TEXTURE_COMPRESSION_BC:
            return renderer->features2.features.textureCompressionBC;

        case VGPU_FEATURE_TEXTURE_COMPRESSION_ETC2:
            return renderer->features2.features.textureCompressionETC2;

        case VGPU_FEATURE_TEXTURE_COMPRESSION_ASTC:
            return renderer->features2.features.textureCompressionASTC_LDR;

        default:
            return false;
    }
}

static void vulkan_getAdapterProperties(VGFXRenderer* driverData, VGPUAdapterProperties* properties)
{
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;

    properties->vendorID = renderer->properties2.properties.vendorID;
    properties->deviceID = renderer->properties2.properties.deviceID;
    properties->name = renderer->properties2.properties.deviceName;
    properties->driverDescription = renderer->driverDescription.c_str();

    switch (renderer->properties2.properties.deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            properties->adapterType = VGPU_ADAPTER_TYPE_INTEGRATED_GPU;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            properties->adapterType = VGPU_ADAPTER_TYPE_DISCRETE_GPU;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            properties->adapterType = VGPU_ADAPTER_TYPE_VIRTUAL_GPU;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            properties->adapterType = VGPU_ADAPTER_TYPE_CPU;
            break;
        default:
            properties->adapterType = VGPU_ADAPTER_TYPE_OTHER;
            break;
    }

    properties->backendType = VGPU_BACKEND_TYPE_VULKAN;
}

static void vulkan_getLimits(VGFXRenderer* driverData, VGPULimits* limits)
{
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;

#define SET_LIMIT_FROM_VULKAN(vulkanName, name) limits->name = renderer->properties2.properties.limits.vulkanName
    SET_LIMIT_FROM_VULKAN(maxImageDimension1D, maxTextureDimension1D);
    SET_LIMIT_FROM_VULKAN(maxImageDimension2D, maxTextureDimension2D);
    SET_LIMIT_FROM_VULKAN(maxImageArrayLayers, maxTextureArrayLayers);
    SET_LIMIT_FROM_VULKAN(maxBoundDescriptorSets, maxBindGroups);
    SET_LIMIT_FROM_VULKAN(maxDescriptorSetUniformBuffersDynamic, maxDynamicUniformBuffersPerPipelineLayout);
    SET_LIMIT_FROM_VULKAN(maxDescriptorSetStorageBuffersDynamic, maxDynamicStorageBuffersPerPipelineLayout);
    SET_LIMIT_FROM_VULKAN(maxPerStageDescriptorSampledImages, maxSampledTexturesPerShaderStage);
    SET_LIMIT_FROM_VULKAN(maxPerStageDescriptorSamplers, maxSamplersPerShaderStage);
    SET_LIMIT_FROM_VULKAN(maxPerStageDescriptorStorageBuffers, maxStorageBuffersPerShaderStage);
    SET_LIMIT_FROM_VULKAN(maxPerStageDescriptorStorageImages, maxStorageTexturesPerShaderStage);
    SET_LIMIT_FROM_VULKAN(maxPerStageDescriptorUniformBuffers, maxUniformBuffersPerShaderStage);

    limits->maxUniformBufferBindingSize = renderer->properties2.properties.limits.maxUniformBufferRange;
    limits->maxStorageBufferBindingSize = renderer->properties2.properties.limits.maxStorageBufferRange;
    limits->minUniformBufferOffsetAlignment = (uint32_t)renderer->properties2.properties.limits.minUniformBufferOffsetAlignment;
    limits->minUniformBufferOffsetAlignment = (uint32_t)renderer->properties2.properties.limits.minUniformBufferOffsetAlignment;
    SET_LIMIT_FROM_VULKAN(maxVertexInputBindings, maxVertexBuffers);
    SET_LIMIT_FROM_VULKAN(maxVertexInputAttributes, maxVertexAttributes);

    limits->maxVertexBufferArrayStride = std::min(
        renderer->properties2.properties.limits.maxVertexInputBindingStride,
        renderer->properties2.properties.limits.maxVertexInputAttributeOffset + 1);

    limits->maxInterStageShaderComponents = std::min(
        renderer->properties2.properties.limits.maxVertexOutputComponents,
        renderer->properties2.properties.limits.maxFragmentInputComponents);

    SET_LIMIT_FROM_VULKAN(maxComputeSharedMemorySize, maxComputeWorkgroupStorageSize);
    SET_LIMIT_FROM_VULKAN(maxComputeWorkGroupInvocations, maxComputeInvocationsPerWorkGroup);
    SET_LIMIT_FROM_VULKAN(maxComputeWorkGroupSize[0], maxComputeWorkGroupSizeX);
    SET_LIMIT_FROM_VULKAN(maxComputeWorkGroupSize[1], maxComputeWorkGroupSizeY);
    SET_LIMIT_FROM_VULKAN(maxComputeWorkGroupSize[2], maxComputeWorkGroupSizeZ);
    SET_LIMIT_FROM_VULKAN(maxComputeWorkGroupSize[2], maxComputeWorkGroupsPerDimension);

    limits->maxComputeWorkGroupsPerDimension = std::min(
        std::min(
            renderer->properties2.properties.limits.maxComputeWorkGroupCount[0],
            renderer->properties2.properties.limits.maxComputeWorkGroupCount[1]),
        renderer->properties2.properties.limits.maxComputeWorkGroupCount[2]
    );

#undef SET_LIMIT_FROM_VULKAN
}

/* Buffer */
static VGFXBuffer vulkan_createBuffer(VGFXRenderer* driverData, const VGFXBufferDesc* desc, const void* pInitialData)
{
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc->size;
    bufferInfo.usage = 0;

    if (desc->usage & VGFXBufferUsage_Vertex)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }

    if (desc->usage & VGFXBufferUsage_Index)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }

    if (desc->usage & VGFXBufferUsage_Uniform)
    {
        bufferInfo.size = VmaAlignUp(bufferInfo.size, renderer->properties2.properties.limits.minUniformBufferOffsetAlignment);
        bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    if (desc->usage & VGFXBufferUsage_ShaderRead)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    }

    if (desc->usage & VGFXBufferUsage_ShaderWrite)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    }

    if (desc->usage & VGFXBufferUsage_Indirect)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }

    //if (usage & VGFXBufferUsage_Predication)
    //{
    //    bufferInfo.usage |= VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;
    //}
    //
    //if (usage & VGFXBufferUsage_RayTracing))
    //{
    //    bufferInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    //    bufferInfo.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    //    bufferInfo.usage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    //}

    if (renderer->features_1_2.bufferDeviceAddress == VK_TRUE &&
        desc->usage & (VGFXBufferUsage_Vertex | VGFXBufferUsage_Index/* | VGFXBufferUsage_RayTracing*/))
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    uint32_t sharingIndices[3] = {};
    if (renderer->graphicsQueueFamily != renderer->computeQueueFamily ||
        renderer->graphicsQueueFamily != renderer->copyQueueFamily)
    {
        // For buffers, always just use CONCURRENT access modes,
        // so we don't have to deal with acquire/release barriers in async compute.
        bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;

        sharingIndices[bufferInfo.queueFamilyIndexCount++] = renderer->graphicsQueueFamily;

        if (renderer->graphicsQueueFamily != renderer->computeQueueFamily)
        {
            sharingIndices[bufferInfo.queueFamilyIndexCount++] = renderer->computeQueueFamily;
        }

        if (renderer->graphicsQueueFamily != renderer->copyQueueFamily
            && renderer->computeQueueFamily != renderer->copyQueueFamily)
        {
            sharingIndices[bufferInfo.queueFamilyIndexCount++] = renderer->copyQueueFamily;
        }

        bufferInfo.pQueueFamilyIndices = sharingIndices;
    }
    else
    {
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferInfo.queueFamilyIndexCount = 0;
        bufferInfo.pQueueFamilyIndices = nullptr;
    }

    VmaAllocationCreateInfo memoryInfo = {};
    memoryInfo.usage = VMA_MEMORY_USAGE_AUTO;
    //if (info.heapType == HeapType::Readback)
    //{
    //    bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    //    memoryInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    //}
    //else if (info.heapType == HeapType::Upload)
    //{
    //    bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    //    memoryInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    //}

    VmaAllocationInfo allocationInfo{};
    VGFXVulkanBuffer* buffer = new VGFXVulkanBuffer();
    VkResult result = vmaCreateBuffer(renderer->allocator, &bufferInfo, &memoryInfo,
        &buffer->handle,
        &buffer->allocation,
        &allocationInfo);

    if (result != VK_SUCCESS)
    {
        VK_LOG_ERROR(result, "Failed to create buffer.");
        return nullptr;
    }

    if (desc->label)
    {
        vulkan_SetObjectName(renderer, VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer->handle, desc->label);
    }

    return (VGFXBuffer)buffer;
}

static void vulkan_destroyBuffer(VGFXRenderer* driverData, VGFXBuffer resource)
{
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;
    VGFXVulkanBuffer* buffer = (VGFXVulkanBuffer*)resource;

    renderer->destroyMutex.lock();
    if (buffer->allocation)
    {
        renderer->destroyedBuffers.push_back(std::make_pair(std::make_pair(buffer->handle, buffer->allocation), renderer->frameCount));
    }
    renderer->destroyMutex.unlock();

    delete buffer;
}

/* Texture */
static VGFXTexture vulkan_createTexture(VGFXRenderer* driverData, const VGFXTextureDesc* desc)
{
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;

    VmaAllocationCreateInfo memoryInfo = {};
    memoryInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = nullptr;
    imageInfo.flags = 0;
    imageInfo.format = ToVk(desc->format);
    imageInfo.extent.width = desc->width;
    imageInfo.extent.height = desc->height;
    if (desc->type == VGPU_TEXTURE_TYPE_3D)
    {
        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        imageInfo.extent.depth = desc->depthOrArraySize;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        imageInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    }
    else
    {
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.depth = 1;
        imageInfo.arrayLayers = desc->depthOrArraySize;
        imageInfo.samples = (VkSampleCountFlagBits)desc->sampleCount;

        if (desc->width == desc->height && (desc->depthOrArraySize % 6 == 0))
        {
            imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }
    }
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = desc->mipLevelCount;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (desc->usage & VGFXTextureUsage_ShaderRead)
        imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

    if (desc->usage & VGFXTextureUsage_ShaderWrite)
        imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

    if (desc->usage & VGFXTextureUsage_RenderTarget)
    {
        if (vgfxIsDepthStencilFormat(desc->format))
        {
            imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        else
        {
            imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }

    uint32_t sharingIndices[3] = {};
    if (renderer->graphicsQueueFamily != renderer->computeQueueFamily ||
        renderer->graphicsQueueFamily != renderer->copyQueueFamily)
    {
        // For buffers, always just use CONCURRENT access modes,
        // so we don't have to deal with acquire/release barriers in async compute.
        imageInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;

        sharingIndices[imageInfo.queueFamilyIndexCount++] = renderer->graphicsQueueFamily;

        if (renderer->graphicsQueueFamily != renderer->computeQueueFamily)
        {
            sharingIndices[imageInfo.queueFamilyIndexCount++] = renderer->computeQueueFamily;
        }

        if (renderer->graphicsQueueFamily != renderer->copyQueueFamily
            && renderer->computeQueueFamily != renderer->copyQueueFamily)
        {
            sharingIndices[imageInfo.queueFamilyIndexCount++] = renderer->copyQueueFamily;
        }

        imageInfo.pQueueFamilyIndices = sharingIndices;
    }
    else
    {
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.queueFamilyIndexCount = 0;
        imageInfo.pQueueFamilyIndices = nullptr;
    }

    // TODO: Handle readback texture

    VGFXVulkanTexture* texture = new VGFXVulkanTexture();
    VkResult result = vmaCreateImage(renderer->allocator,
        &imageInfo, &memoryInfo,
        &texture->handle,
        &texture->allocation,
        nullptr);

    if (result != VK_SUCCESS)
    {
        vgfxLogError("Vulkan: Failed to create texture");
        delete texture;
        return nullptr;
    }

    texture->width = imageInfo.extent.width;
    texture->height = imageInfo.extent.height;
    texture->format = imageInfo.format;

    return (VGFXTexture)texture;
}

static void vulkan_destroyTexture(VGFXRenderer* driverData, VGFXTexture texture)
{
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;
    VGFXVulkanTexture* vulkanTexture = (VGFXVulkanTexture*)texture;

    renderer->destroyMutex.lock();
    for (auto& it : vulkanTexture->viewCache)
    {
        renderer->destroyedImageViews.push_back(std::make_pair(it.second, renderer->frameCount));
    }
    vulkanTexture->viewCache.clear();
    if (vulkanTexture->allocation)
    {
        renderer->destroyedImagesQueue.push_back(std::make_pair(std::make_pair(vulkanTexture->handle, vulkanTexture->allocation), renderer->frameCount));
    }
    renderer->destroyMutex.unlock();

    delete vulkanTexture;
}

/* SwapChain */
static void vulkan_updateSwapChain(VGFXVulkanRenderer* renderer, VGFXVulkanSwapChain* swapChain)
{
    VkResult result = VK_SUCCESS;
    VkSurfaceCapabilitiesKHR caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->physicalDevice, swapChain->surface, &caps));

    uint32_t formatCount;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physicalDevice, swapChain->surface, &formatCount, nullptr));

    std::vector<VkSurfaceFormatKHR> swapchainFormats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physicalDevice, swapChain->surface, &formatCount, swapchainFormats.data()));

    uint32_t presentModeCount;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physicalDevice, swapChain->surface, &presentModeCount, nullptr));

    std::vector<VkPresentModeKHR> swapchainPresentModes(presentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physicalDevice, swapChain->surface, &presentModeCount, swapchainPresentModes.data()));

    VkSurfaceFormatKHR surfaceFormat = {};
    surfaceFormat.format = ToVk(swapChain->colorFormat);
    surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    bool valid = false;

    for (const auto& format : swapchainFormats)
    {
        if (!swapChain->allowHDR && format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            continue;

        if (format.format == surfaceFormat.format)
        {
            surfaceFormat = format;
            valid = true;
            break;
        }
    }
    if (!valid)
    {
        surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
        surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }

    if (caps.currentExtent.width != 0xFFFFFFFF && caps.currentExtent.width != 0xFFFFFFFF)
    {
        swapChain->width = caps.currentExtent.width;
        swapChain->height = caps.currentExtent.height;
    }
    else
    {
        swapChain->width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, swapChain->width));
        swapChain->height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, swapChain->height));
    }

    // Determine the number of images
    uint32_t imageCount = caps.minImageCount + 1;
    if ((caps.maxImageCount > 0) && (imageCount > caps.maxImageCount))
    {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = swapChain->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent.width = swapChain->width;
    createInfo.imageExtent.height = swapChain->height;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.preTransform = caps.currentTransform;

    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // The only one that is always supported
    if (!swapChain->vsync)
    {
        // The mailbox/immediate present mode is not necessarily supported:
        for (auto& presentMode : swapchainPresentModes)
        {
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                createInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
            if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
            {
                createInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = swapChain->handle;

    VK_CHECK(vkCreateSwapchainKHR(renderer->device, &createInfo, nullptr, &swapChain->handle));

    if (createInfo.oldSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(renderer->device, createInfo.oldSwapchain, nullptr);
    }

    VK_CHECK(vkGetSwapchainImagesKHR(renderer->device, swapChain->handle, &imageCount, nullptr));
    std::vector<VkImage> swapchainImages(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(renderer->device, swapChain->handle, &imageCount, swapchainImages.data()));

    swapChain->imageIndex = 0;
    swapChain->backbufferTextures.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VGFXVulkanTexture* texture = new VGFXVulkanTexture();
        texture->handle = swapchainImages[i];
        texture->width = createInfo.imageExtent.width;
        texture->height = createInfo.imageExtent.height;
        texture->format = createInfo.imageFormat;
        texture->isSwapChain = true;
        swapChain->backbufferTextures[i] = (VGFXTexture)texture;
    }

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (swapChain->acquireSemaphore == VK_NULL_HANDLE)
    {
        result = vkCreateSemaphore(renderer->device, &semaphoreInfo, nullptr, &swapChain->acquireSemaphore);
        VGFX_ASSERT(result == VK_SUCCESS);
    }

    if (swapChain->releaseSemaphore == VK_NULL_HANDLE)
    {
        result = vkCreateSemaphore(renderer->device, &semaphoreInfo, nullptr, &swapChain->releaseSemaphore);
        VGFX_ASSERT(result == VK_SUCCESS);
    }

    swapChain->width = createInfo.imageExtent.width;
    swapChain->height = createInfo.imageExtent.height;
}

static VGPUSwapChain vulkan_createSwapChain(VGFXRenderer* driverData, void* windowHandle, const VGPUSwapChainDesc* info)
{
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;
    VkSurfaceKHR vk_surface = vulkan_createSurface(renderer, windowHandle);
    if (vk_surface == VK_NULL_HANDLE)
    {
        return nullptr;
    }

    VkBool32 supported = VK_FALSE;
    VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(renderer->physicalDevice, renderer->graphicsQueueFamily, vk_surface, &supported);
    if (result != VK_SUCCESS || !supported)
    {
        return nullptr;
    }

    VGFXVulkanSwapChain* swapChain = new VGFXVulkanSwapChain();
    swapChain->surface = vk_surface;
    swapChain->width = info->width;
    swapChain->height = info->height;
    swapChain->colorFormat = info->format;
    swapChain->vsync = info->presentMode == VGFXPresentMode_Fifo;
    vulkan_updateSwapChain(renderer, swapChain);

    return (VGPUSwapChain)swapChain;
}

static void vulkan_destroySwapChain(VGFXRenderer* driverData, VGPUSwapChain swapChain)
{
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;
    VGFXVulkanSwapChain* vulkanSwapChain = (VGFXVulkanSwapChain*)swapChain;
    //vulkan_destroyTexture(nullptr, d3dSwapChain->backbufferTexture);

    for (size_t i = 0, count = vulkanSwapChain->backbufferTextures.size(); i < count; ++i)
    {
        vulkan_destroyTexture(driverData, vulkanSwapChain->backbufferTextures[i]);
    }

    if (vulkanSwapChain->acquireSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(renderer->device, vulkanSwapChain->acquireSemaphore, nullptr);
        vulkanSwapChain->acquireSemaphore = VK_NULL_HANDLE;
    }

    if (vulkanSwapChain->releaseSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(renderer->device, vulkanSwapChain->releaseSemaphore, nullptr);
        vulkanSwapChain->releaseSemaphore = VK_NULL_HANDLE;
    }

    if (vulkanSwapChain->handle != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(renderer->device, vulkanSwapChain->handle, nullptr);
        vulkanSwapChain->handle = VK_NULL_HANDLE;
    }

    if (vulkanSwapChain->surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(renderer->instance, vulkanSwapChain->surface, nullptr);
        vulkanSwapChain->surface = VK_NULL_HANDLE;
    }

    delete vulkanSwapChain;
}

static void vulkan_getSwapChainSize(VGFXRenderer*, VGPUSwapChain swapChain, VGPUSize2D* pSize)
{
    VGFXVulkanSwapChain* vulkanSwapChain = (VGFXVulkanSwapChain*)swapChain;
    pSize->width = vulkanSwapChain->width;
    pSize->height = vulkanSwapChain->height;
}

static VGFXTexture vulkan_acquireNextTexture(VGFXRenderer* driverData, VGPUSwapChain swapChain)
{
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;
    VGFXVulkanSwapChain* vulkanSwapChain = (VGFXVulkanSwapChain*)swapChain;

    VkResult result = vkAcquireNextImageKHR(renderer->device,
        vulkanSwapChain->handle,
        UINT64_MAX,
        vulkanSwapChain->acquireSemaphore, VK_NULL_HANDLE,
        &vulkanSwapChain->imageIndex);

    if (result != VK_SUCCESS)
    {
        // Handle outdated error in acquire
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // TODO
        }
    }

    renderer->swapChains.push_back(vulkanSwapChain);
    return vulkanSwapChain->backbufferTextures[vulkanSwapChain->imageIndex];
}

/* Commands */
static void insertImageMemoryBarrier(
    VkCommandBuffer         commandBuffer,
    VkImage                 image,
    VkAccessFlags           src_access_mask,
    VkAccessFlags           dst_access_mask,
    VkImageLayout           old_layout,
    VkImageLayout           new_layout,
    VkPipelineStageFlags    src_stage_mask,
    VkPipelineStageFlags    dst_stage_mask,
    VkImageSubresourceRange subresource_range)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.image = image;
    barrier.subresourceRange = subresource_range;

    vkCmdPipelineBarrier(
        commandBuffer,
        src_stage_mask,
        dst_stage_mask,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
}

static VGPUCommandBuffer vulkan_beginCommandBuffer(VGFXRenderer* driverData, const char* label)
{
    return nullptr;
}

static void vulkan_submit(VGFXRenderer* driverData, VGPUCommandBuffer* commandBuffers, uint32_t count)
{
}

static void vulkan_beginRenderPass(VGFXRenderer* driverData, const VGFXRenderPassDesc* desc)
{
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;
    VkCommandBuffer commandBuffer = renderer->GetCommandBuffer();

    if (renderer->dynamicRendering)
    {
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = VK_REMAINING_MIP_LEVELS;
        range.baseArrayLayer = 0;
        range.layerCount = VK_REMAINING_ARRAY_LAYERS;

        VkRenderingInfoKHR renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderingInfo.pNext = VK_NULL_HANDLE;
        renderingInfo.flags = 0u;
        renderingInfo.renderArea.extent = { UINT32_MAX, UINT32_MAX };
        renderingInfo.layerCount = 1;
        renderingInfo.viewMask = 0;

        VkRenderingAttachmentInfo colorAttachments[VGFX_MAX_COLOR_ATTACHMENTS] = {};
        VkRenderingAttachmentInfo depthStencilAttachmentInfo{};
        

        for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i)
        {
            const VGFXRenderPassColorAttachment* attachment = &desc->colorAttachments[i];
            VGFXVulkanTexture* texture = (VGFXVulkanTexture*)attachment->texture;
            const uint32_t level = attachment->level;
            const uint32_t slice = attachment->slice;

            if (texture->isSwapChain)
            {
                insertImageMemoryBarrier(
                    commandBuffer,
                    texture->handle,
                    0,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, range);
            
                renderer->swapChainTextures.push_back(texture);
            }

            renderingInfo.renderArea.extent.width = std::min(renderingInfo.renderArea.extent.width, std::max(1U, texture->width >> level));
            renderingInfo.renderArea.extent.height = std::min(renderingInfo.renderArea.extent.height, std::max(1U, texture->height >> level));

            colorAttachments[renderingInfo.colorAttachmentCount].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachments[renderingInfo.colorAttachmentCount].pNext = nullptr;
            colorAttachments[renderingInfo.colorAttachmentCount].imageView = vulkan_GetRTV(renderer, texture, level, slice);
            colorAttachments[renderingInfo.colorAttachmentCount].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachments[renderingInfo.colorAttachmentCount].resolveMode = VK_RESOLVE_MODE_NONE;
            colorAttachments[renderingInfo.colorAttachmentCount].loadOp = ToVk(attachment->loadOp);
            colorAttachments[renderingInfo.colorAttachmentCount].storeOp = ToVk(attachment->storeOp);

            colorAttachments[renderingInfo.colorAttachmentCount].clearValue.color.float32[0] = attachment->clearColor.r;
            colorAttachments[renderingInfo.colorAttachmentCount].clearValue.color.float32[1] = attachment->clearColor.g;
            colorAttachments[renderingInfo.colorAttachmentCount].clearValue.color.float32[2] = attachment->clearColor.b;
            colorAttachments[renderingInfo.colorAttachmentCount].clearValue.color.float32[3] = attachment->clearColor.a;

            renderingInfo.colorAttachmentCount++;
        }
        renderingInfo.pColorAttachments = colorAttachments;

        if (desc->depthStencilAttachment != nullptr)
        {
            const VGFXRenderPassDepthStencilAttachment* attachment = desc->depthStencilAttachment;
            VGFXVulkanTexture* texture = (VGFXVulkanTexture*)attachment->texture;
            const uint32_t level = attachment->level;
            const uint32_t slice = attachment->slice;

            depthStencilAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            depthStencilAttachmentInfo.pNext = VK_NULL_HANDLE;
            depthStencilAttachmentInfo.imageView = vulkan_GetRTV(renderer, texture, level, slice);
            depthStencilAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthStencilAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
            depthStencilAttachmentInfo.loadOp = ToVk(attachment->depthLoadOp);
            depthStencilAttachmentInfo.storeOp = ToVk(attachment->depthStoreOp);
            depthStencilAttachmentInfo.clearValue.depthStencil.depth = attachment->clearDepth;

            // Barrier
            insertImageMemoryBarrier(
                commandBuffer,
                texture->handle,
                0,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, range);

            renderer->swapChainTextures.push_back(texture);

            renderingInfo.pDepthAttachment = &depthStencilAttachmentInfo;
            renderingInfo.pStencilAttachment = &depthStencilAttachmentInfo;
        }

        vkCmdBeginRenderingKHR(commandBuffer, &renderingInfo);
    }
    else
    {
        uint32_t width = 0;
        uint32_t height = 0;
        VkRenderPass renderPass = vulkan_RequestRenderPass(renderer, desc);
        VkClearValue clearValues[VGFX_MAX_COLOR_ATTACHMENTS + 1];

        for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i)
        {
            VGFXRenderPassColorAttachment attachment = desc->colorAttachments[i];

            clearValues[i].color.float32[0] = attachment.clearColor.r;
            clearValues[i].color.float32[1] = attachment.clearColor.g;
            clearValues[i].color.float32[2] = attachment.clearColor.b;
            clearValues[i].color.float32[3] = attachment.clearColor.a;
        }

        VkRenderPassBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.renderPass = renderPass;
        beginInfo.framebuffer = vulkan_RequestFramebuffer(renderer, renderPass, desc, &width, &height);
        beginInfo.renderArea.offset.x = 0;
        beginInfo.renderArea.offset.y = 0;
        beginInfo.renderArea.extent.width = width;
        beginInfo.renderArea.extent.height = height;
        beginInfo.clearValueCount = 1;
        beginInfo.pClearValues = clearValues;
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
}

static void vulkan_endRenderPass(VGFXRenderer* driverData)
{
    VGFXVulkanRenderer* renderer = (VGFXVulkanRenderer*)driverData;
    VkCommandBuffer commandBuffer = renderer->GetCommandBuffer();

    if (renderer->dynamicRendering)
    {
        vkCmdEndRenderingKHR(commandBuffer);

        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = VK_REMAINING_MIP_LEVELS;
        range.baseArrayLayer = 0;
        range.layerCount = VK_REMAINING_ARRAY_LAYERS;

        for (auto texture : renderer->swapChainTextures)
        {
            insertImageMemoryBarrier(
                commandBuffer,
                texture->handle,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                0,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                range);
        }

        renderer->swapChainTextures.clear();
    }
    else
    {
        vkCmdEndRenderPass(commandBuffer);
    }
}

static bool vulkan_isSupported(void)
{
    static bool available_initialized = false;
    static bool available = false;

    if (available_initialized) {
        return available;
    }

    available_initialized = true;

    VkResult result = volkInitialize();
    if (result != VK_SUCCESS)
    {
        return false;
    }

    available = true;
    return true;
}

static VGPUDevice vulkan_createDevice(const VGPUDeviceDesc* info)
{
    VGFXVulkanRenderer* renderer = new VGFXVulkanRenderer();

#if defined(VK_USE_PLATFORM_XCB_KHR)
#if defined(__CYGWIN__)
    renderer->x11xcb.handle = dlopen("libX11-xcb-1.so", RTLD_LAZY | RTLD_LOCAL);
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    renderer->x11xcb.handle = dlopen("libX11-xcb.so", RTLD_LAZY | RTLD_LOCAL);
#else
    renderer->x11xcb.handle = dlopen("libX11-xcb.so.1", RTLD_LAZY | RTLD_LOCAL);
#endif

    if (renderer->x11xcb.handle)
    {
        renderer->x11xcb.GetXCBConnection = (PFN_XGetXCBConnection)dlsym(renderer->x11xcb.handle, "XGetXCBConnection");
    }
#endif

    uint32_t apiVersion = volkGetInstanceVersion();

    // Enumerate available layers and extensions:
    {
        uint32_t instanceLayerCount;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
        std::vector<VkLayerProperties> availableInstanceLayers(instanceLayerCount);
        VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, availableInstanceLayers.data()));

        uint32_t extensionCount = 0;
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));
        std::vector<VkExtensionProperties> availableInstanceExtensions(extensionCount);
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableInstanceExtensions.data()));

        std::vector<const char*> instanceLayers;
        std::vector<const char*> instanceExtensions;

        for (auto& availableExtension : availableInstanceExtensions)
        {
            if (strcmp(availableExtension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
            {
                renderer->debugUtils = true;
                instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
            else if (strcmp(availableExtension.extensionName, "VK_EXT_swapchain_colorspace") == 0)
            {
                instanceExtensions.push_back("VK_EXT_swapchain_colorspace");
            }

            /* Core 1.1 */
            if (apiVersion < VK_API_VERSION_1_1)
            {
                if (strcmp(availableExtension.extensionName, "VK_KHR_get_physical_device_properties2") == 0)
                {
                    instanceExtensions.push_back("VK_KHR_get_physical_device_properties2");
                }
                else if (strcmp(availableExtension.extensionName, "VK_KHR_external_memory_capabilities") == 0)
                {
                    instanceExtensions.push_back("VK_KHR_external_memory_capabilities");
                }
                else if (strcmp(availableExtension.extensionName, "VK_KHR_external_semaphore_capabilities") == 0)
                {
                    instanceExtensions.push_back("VK_KHR_external_semaphore_capabilities");
                }
            }
        }

        instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

        // Enable surface extensions depending on os
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
        instanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
        instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
        instanceExtensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
        instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        instanceExtensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
        instanceExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_DISPLAY_KHR)
        instanceExtensions.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_HEADLESS_EXT)
        instanceExtensions.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
#else
#   pragma error Platform not supported
#endif

        if (info->validationMode != VGPU_VALIDATION_MODE_DISABLED)
        {
            // Determine the optimal validation layers to enable that are necessary for useful debugging
            std::vector<const char*> optimalValidationLyers = GetOptimalValidationLayers(availableInstanceLayers);
            instanceLayers.insert(instanceLayers.end(), optimalValidationLyers.begin(), optimalValidationLyers.end());
        }

#if defined(_DEBUG)
        bool validationFeatures = false;
        if (info->validationMode == VGPU_VALIDATION_MODE_GPU)
        {
            uint32_t layerInstanceExtensionCount;
            VK_CHECK(vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &layerInstanceExtensionCount, nullptr));
            std::vector<VkExtensionProperties> availableLayerInstanceExtensions(layerInstanceExtensionCount);
            VK_CHECK(vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &layerInstanceExtensionCount, availableLayerInstanceExtensions.data()));

            for (auto& availableExtension : availableLayerInstanceExtensions)
            {
                if (strcmp(availableExtension.extensionName, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) == 0)
                {
                    validationFeatures = true;
                    instanceExtensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
                }
            }
        }
#endif 

        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = info->label;
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "vgpu";
        appInfo.engineVersion = VK_MAKE_VERSION(VGPU_VERSION_MAJOR, VGPU_VERSION_MINOR, VGPU_VERSION_PATCH);
        appInfo.apiVersion = VK_HEADER_VERSION_COMPLETE;

        VkInstanceCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pNext = NULL;
        createInfo.flags = 0;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
        createInfo.ppEnabledLayerNames = instanceLayers.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        createInfo.ppEnabledExtensionNames = instanceExtensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo{};

        if (info->validationMode != VGPU_VALIDATION_MODE_DISABLED && renderer->debugUtils)
        {
            debugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugUtilsCreateInfo.pNext = nullptr;
            debugUtilsCreateInfo.flags = 0;

            debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            if (info->validationMode == VGPU_VALIDATION_MODE_VERBOSE)
            {
                debugUtilsCreateInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
            }

            debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugUtilsCreateInfo.pfnUserCallback = DebugUtilsMessengerCallback;
            createInfo.pNext = &debugUtilsCreateInfo;
        }

#if defined(_DEBUG)
        VkValidationFeaturesEXT validationFeaturesInfo = { VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
        if (validationFeatures)
        {
            static const VkValidationFeatureEnableEXT enable_features[2] = {
                VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
                VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
            };
            validationFeaturesInfo.enabledValidationFeatureCount = 2;
            validationFeaturesInfo.pEnabledValidationFeatures = enable_features;
            validationFeaturesInfo.pNext = createInfo.pNext;
            createInfo.pNext = &validationFeaturesInfo;
        }
#endif

        VkResult result = vkCreateInstance(&createInfo, nullptr, &renderer->instance);
        if (result != VK_SUCCESS)
        {
            VK_LOG_ERROR(result, "Failed to create Vulkan instance.");
            delete renderer;
            return nullptr;
        }

        volkLoadInstanceOnly(renderer->instance);

        if (info->validationMode != VGPU_VALIDATION_MODE_DISABLED && renderer->debugUtils)
        {
            result = vkCreateDebugUtilsMessengerEXT(renderer->instance, &debugUtilsCreateInfo, nullptr, &renderer->debugUtilsMessenger);
            if (result != VK_SUCCESS)
            {
                VK_LOG_ERROR(result, "Could not create debug utils messenger");
            }
        }

#ifdef _DEBUG
        vgfxLogInfo("Created VkInstance with version: %d.%d.%d",
            VK_VERSION_MAJOR(appInfo.apiVersion),
            VK_VERSION_MINOR(appInfo.apiVersion),
            VK_VERSION_PATCH(appInfo.apiVersion)
        );

        if (createInfo.enabledLayerCount)
        {
            vgfxLogInfo("Enabled %d Validation Layers:", createInfo.enabledLayerCount);

            for (uint32_t i = 0; i < createInfo.enabledLayerCount; ++i)
            {
                vgfxLogInfo("	\t%s", createInfo.ppEnabledLayerNames[i]);
            }
        }

        vgfxLogInfo("Enabled %d Instance Extensions:", createInfo.enabledExtensionCount);
        for (uint32_t i = 0; i < createInfo.enabledExtensionCount; ++i)
        {
            vgfxLogInfo("	\t%s", createInfo.ppEnabledExtensionNames[i]);
        }
#endif
    }

    // Enumerate physical device and create logical device.
    {
        uint32_t deviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(renderer->instance, &deviceCount, nullptr));

        if (deviceCount == 0)
        {
            vgfxLogError("Vulkan: Failed to find GPUs with Vulkan support");
            delete renderer;
            return nullptr;
        }

        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(renderer->instance, &deviceCount, physicalDevices.data()));

        std::vector<const char*> enabledExtensions;
        for (const VkPhysicalDevice& candidatePhysicalDevice : physicalDevices)
        {
            bool suitable = true;

            // We require minimum 1.1
            VkPhysicalDeviceProperties gpuProps;
            vkGetPhysicalDeviceProperties(candidatePhysicalDevice, &gpuProps);
            if (gpuProps.apiVersion < VK_API_VERSION_1_1)
            {
                continue;
            }

            PhysicalDeviceExtensions physicalDeviceExt = QueryPhysicalDeviceExtensions(candidatePhysicalDevice);
            suitable = physicalDeviceExt.swapchain && physicalDeviceExt.depth_clip_enable;

            if (!suitable)
            {
                continue;
            }

            renderer->features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            renderer->features_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            renderer->features_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            renderer->features2.pNext = &renderer->features_1_1;
            renderer->features_1_1.pNext = &renderer->features_1_2;
            void** features_chain = &renderer->features_1_2.pNext;
            renderer->acceleration_structure_features = {};
            renderer->raytracing_features = {};
            renderer->raytracing_query_features = {};
            renderer->fragment_shading_rate_features = {};
            renderer->mesh_shader_features = {};

            renderer->driverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
            renderer->properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            renderer->properties_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
            renderer->properties_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
            renderer->properties2.pNext = &renderer->properties_1_1;
            renderer->properties_1_1.pNext = &renderer->properties_1_2;
            void** properties_chain = &renderer->properties_1_2.pNext;
            renderer->sampler_minmax_properties = {};
            renderer->acceleration_structure_properties = {};
            renderer->raytracing_properties = {};
            renderer->fragment_shading_rate_properties = {};
            renderer->mesh_shader_properties = {};

            renderer->sampler_minmax_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES;
            *properties_chain = &renderer->sampler_minmax_properties;
            properties_chain = &renderer->sampler_minmax_properties.pNext;

            enabledExtensions.clear();
            enabledExtensions.push_back("VK_KHR_swapchain");

            if (physicalDeviceExt.driver_properties)
            {
                enabledExtensions.push_back("VK_KHR_driver_properties");
                *properties_chain = &renderer->driverProperties;
                properties_chain = &renderer->driverProperties.pNext;
            }

            // For performance queries, we also use host query reset since queryPool resets cannot live in the same command buffer as beginQuery
            if (physicalDeviceExt.performance_query &&
                physicalDeviceExt.host_query_reset)
            {
                renderer->perf_counter_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR;
                enabledExtensions.push_back(VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME);
                *features_chain = &renderer->perf_counter_features;
                features_chain = &renderer->perf_counter_features.pNext;

                renderer->host_query_reset_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;
                enabledExtensions.push_back(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME);
                *features_chain = &renderer->host_query_reset_features;
                features_chain = &renderer->host_query_reset_features.pNext;
            }

            if (physicalDeviceExt.memory_budget)
            {
                enabledExtensions.push_back("VK_EXT_memory_budget");
            }

            if (physicalDeviceExt.depth_clip_enable)
            {
                renderer->depth_clip_enable_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
                enabledExtensions.push_back("VK_EXT_depth_clip_enable");
                *features_chain = &renderer->depth_clip_enable_features;
                features_chain = &renderer->depth_clip_enable_features.pNext;
            }

            // Core 1.2.
            {
                // Required by VK_KHR_spirv_1_4
                //enabledExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

                // Required for VK_KHR_ray_tracing_pipeline
                //enabledExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

                //enabledExtensions.push_back(VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME);

                // Required by VK_KHR_acceleration_structure
                //enabledExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

                // Required by VK_KHR_acceleration_structure
                //enabledExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
            }

            if (physicalDeviceExt.accelerationStructure)
            {
                VGFX_ASSERT(physicalDeviceExt.deferred_host_operations);

                // Required by VK_KHR_acceleration_structure
                enabledExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

                enabledExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
                renderer->acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
                *features_chain = &renderer->acceleration_structure_features;
                features_chain = &renderer->acceleration_structure_features.pNext;
                renderer->acceleration_structure_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
                *properties_chain = &renderer->acceleration_structure_properties;
                properties_chain = &renderer->acceleration_structure_properties.pNext;

                if (physicalDeviceExt.raytracingPipeline)
                {
                    enabledExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
                    renderer->raytracing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
                    *features_chain = &renderer->raytracing_features;
                    features_chain = &renderer->raytracing_features.pNext;
                    renderer->raytracing_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
                    *properties_chain = &renderer->raytracing_properties;
                    properties_chain = &renderer->raytracing_properties.pNext;
                }

                if (physicalDeviceExt.rayQuery)
                {
                    enabledExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
                    renderer->raytracing_query_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
                    *features_chain = &renderer->raytracing_query_features;
                    features_chain = &renderer->raytracing_query_features.pNext;
                }
            }

            if (physicalDeviceExt.renderPass2)
            {
                enabledExtensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
            }

            if (physicalDeviceExt.fragment_shading_rate)
            {
                VGFX_ASSERT(physicalDeviceExt.renderPass2 == true);
                enabledExtensions.push_back(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
                renderer->fragment_shading_rate_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
                *features_chain = &renderer->fragment_shading_rate_features;
                features_chain = &renderer->fragment_shading_rate_features.pNext;
                renderer->fragment_shading_rate_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR;
                *properties_chain = &renderer->fragment_shading_rate_properties;
                properties_chain = &renderer->fragment_shading_rate_properties.pNext;
            }

            if (physicalDeviceExt.NV_mesh_shader)
            {
                enabledExtensions.push_back(VK_NV_MESH_SHADER_EXTENSION_NAME);
                renderer->mesh_shader_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;
                *features_chain = &renderer->mesh_shader_features;
                features_chain = &renderer->mesh_shader_features.pNext;
                renderer->mesh_shader_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV;
                *properties_chain = &renderer->mesh_shader_properties;
                properties_chain = &renderer->mesh_shader_properties.pNext;
            }

            if (physicalDeviceExt.EXT_conditional_rendering)
            {
                enabledExtensions.push_back(VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME);
                renderer->conditional_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;
                *features_chain = &renderer->conditional_rendering_features;
                features_chain = &renderer->conditional_rendering_features.pNext;
            }

            if (physicalDeviceExt.vertex_attribute_divisor)
            {
                enabledExtensions.push_back(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME);
                renderer->vertexDivisorFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
                *features_chain = &renderer->vertexDivisorFeatures;
                features_chain = &renderer->vertexDivisorFeatures.pNext;
            }

            if (physicalDeviceExt.extended_dynamic_state)
            {
                enabledExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
                renderer->extendedDynamicStateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
                *features_chain = &renderer->extendedDynamicStateFeatures;
                features_chain = &renderer->extendedDynamicStateFeatures.pNext;
            }

            if (physicalDeviceExt.vertex_input_dynamic_state)
            {
                enabledExtensions.push_back(VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME);
                renderer->vertexInputDynamicStateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
                *features_chain = &renderer->vertexInputDynamicStateFeatures;
                features_chain = &renderer->vertexInputDynamicStateFeatures.pNext;
            }

            if (physicalDeviceExt.extended_dynamic_state2)
            {
                enabledExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
                renderer->extendedDynamicState2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;
                *features_chain = &renderer->extendedDynamicState2Features;
                features_chain = &renderer->extendedDynamicState2Features.pNext;
            }

            // TODO: Enable
            //if (physicalDeviceExt.dynamic_rendering)
            //{
            //    renderer->dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
            //    enabledExtensions.push_back("VK_KHR_dynamic_rendering");
            //    *features_chain = &renderer->dynamicRenderingFeatures;
            //    features_chain = &renderer->dynamicRenderingFeatures.pNext;
            //    renderer->dynamicRendering = true;
            //}

            vkGetPhysicalDeviceProperties2(candidatePhysicalDevice, &renderer->properties2);

            bool discrete = renderer->properties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            if (discrete || renderer->physicalDevice == VK_NULL_HANDLE)
            {
                renderer->supportedExtensions = physicalDeviceExt;

                renderer->physicalDevice = candidatePhysicalDevice;
                if (discrete)
                {
                    break; // if this is discrete GPU, look no further (prioritize discrete GPU)
                }
            }
        }

        if (renderer->physicalDevice == VK_NULL_HANDLE)
        {
            vgfxLogError("Vulkan: Failed to find a suitable GPU");
            delete renderer;
            return nullptr;
        }

        vkGetPhysicalDeviceFeatures2(renderer->physicalDevice, &renderer->features2);

        // Find queue families:
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(renderer->physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(renderer->physicalDevice, &queueFamilyCount, queueFamilies.data());

        std::vector<uint32_t> queueOffsets(queueFamilyCount);
        std::vector<std::vector<float>> queuePriorities(queueFamilyCount);

        uint32_t graphicsQueueIndex = 0;
        uint32_t computeQueueIndex = 0;
        uint32_t copyQueueIndex = 0;

        const auto FindVacantQueue = [&](uint32_t& family, uint32_t& index,
            VkQueueFlags required, VkQueueFlags ignore_flags,
            float priority) -> bool
        {
            for (uint32_t familyIndex = 0; familyIndex < queueFamilyCount; familyIndex++)
            {
                if ((queueFamilies[familyIndex].queueFlags & ignore_flags) != 0)
                    continue;

                // A graphics queue candidate must support present for us to select it.
                if ((required & VK_QUEUE_GRAPHICS_BIT) != 0)
                {
                    VkBool32 supported = vulkan_queryPresentationSupport(renderer->physicalDevice, familyIndex);
                    if (!supported)
                        continue;
                    //if (vkGetPhysicalDeviceSurfaceSupportKHR(renderer->physicalDevice, familyIndex, vk_surface, &supported) != VK_SUCCESS || !supported)
                    //    continue;
                }

                if (queueFamilies[familyIndex].queueCount &&
                    (queueFamilies[familyIndex].queueFlags & required) == required)
                {
                    family = familyIndex;
                    queueFamilies[familyIndex].queueCount--;
                    index = queueOffsets[familyIndex]++;
                    queuePriorities[familyIndex].push_back(priority);
                    return true;
                }
            }

            return false;
        };

        if (!FindVacantQueue(renderer->graphicsQueueFamily, graphicsQueueIndex,
            VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0, 0.5f))
        {
            vgfxLogError("Vulkan: Could not find suitable graphics queue.");
            delete renderer;
            return nullptr;
        }

        // Prefer another graphics queue since we can do async graphics that way.
        // The compute queue is to be treated as high priority since we also do async graphics on it.
        if (!FindVacantQueue(renderer->computeQueueFamily, computeQueueIndex, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0, 1.0f) &&
            !FindVacantQueue(renderer->computeQueueFamily, computeQueueIndex, VK_QUEUE_COMPUTE_BIT, 0, 1.0f))
        {
            // Fallback to the graphics queue if we must.
            renderer->computeQueueFamily = renderer->graphicsQueueFamily;
            computeQueueIndex = graphicsQueueIndex;
        }

        // For transfer, try to find a queue which only supports transfer, e.g. DMA queue.
        // If not, fallback to a dedicated compute queue.
        // Finally, fallback to same queue as compute.
        if (!FindVacantQueue(renderer->copyQueueFamily, copyQueueIndex, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0.5f) &&
            !FindVacantQueue(renderer->copyQueueFamily, copyQueueIndex, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT, 0.5f))
        {
            renderer->copyQueueFamily = renderer->computeQueueFamily;
            copyQueueIndex = computeQueueIndex;
        }

        VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };

        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        for (uint32_t familyIndex = 0; familyIndex < queueFamilyCount; familyIndex++)
        {
            if (queueOffsets[familyIndex] == 0)
                continue;

            VkDeviceQueueCreateInfo info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
            info.queueFamilyIndex = familyIndex;
            info.queueCount = queueOffsets[familyIndex];
            info.pQueuePriorities = queuePriorities[familyIndex].data();
            queueInfos.push_back(info);
        }

        createInfo.pNext = &renderer->features2;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
        createInfo.pQueueCreateInfos = queueInfos.data();
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
        createInfo.pEnabledFeatures = nullptr;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();

        VkResult result = vkCreateDevice(renderer->physicalDevice, &createInfo, nullptr, &renderer->device);
        if (result != VK_SUCCESS)
        {
            VK_LOG_ERROR(result, "Cannot create device");
            delete renderer;
            return nullptr;
        }

        volkLoadDevice(renderer->device);

        vkGetDeviceQueue(renderer->device, renderer->graphicsQueueFamily, graphicsQueueIndex, &renderer->graphicsQueue);
        vkGetDeviceQueue(renderer->device, renderer->computeQueueFamily, computeQueueIndex, &renderer->computeQueue);
        vkGetDeviceQueue(renderer->device, renderer->copyQueueFamily, copyQueueIndex, &renderer->copyQueue);

#ifdef _DEBUG
        vgfxLogInfo("Enabled %d Device Extensions:", createInfo.enabledExtensionCount);
        for (uint32_t i = 0; i < createInfo.enabledExtensionCount; ++i)
        {
            vgfxLogInfo("	\t%s", createInfo.ppEnabledExtensionNames[i]);
        }
#endif

        if (info->label)
        {
            vulkan_SetObjectName(renderer, VK_OBJECT_TYPE_DEVICE, (uint64_t)renderer->device, info->label);
        }

        if (renderer->supportedExtensions.driver_properties)
        {
            renderer->driverDescription = renderer->driverProperties.driverName;
            if (renderer->driverProperties.driverInfo[0] != '\0')
            {
                renderer->driverDescription += std::string(": ") + renderer->driverProperties.driverInfo;
            }
        }
        else
        {
            renderer->driverDescription = "Vulkan driver version: " + std::to_string(renderer->properties2.properties.driverVersion);
        }
    }

    // Create memory allocator
    {
        VmaVulkanFunctions vma_vulkan_func{};
        vma_vulkan_func.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vma_vulkan_func.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        vma_vulkan_func.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        vma_vulkan_func.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        vma_vulkan_func.vkAllocateMemory = vkAllocateMemory;
        vma_vulkan_func.vkFreeMemory = vkFreeMemory;
        vma_vulkan_func.vkMapMemory = vkMapMemory;
        vma_vulkan_func.vkUnmapMemory = vkUnmapMemory;
        vma_vulkan_func.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
        vma_vulkan_func.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
        vma_vulkan_func.vkBindBufferMemory = vkBindBufferMemory;
        vma_vulkan_func.vkBindImageMemory = vkBindImageMemory;
        vma_vulkan_func.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
        vma_vulkan_func.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
        vma_vulkan_func.vkCreateBuffer = vkCreateBuffer;
        vma_vulkan_func.vkDestroyBuffer = vkDestroyBuffer;
        vma_vulkan_func.vkCreateImage = vkCreateImage;
        vma_vulkan_func.vkDestroyImage = vkDestroyImage;
        vma_vulkan_func.vkCmdCopyBuffer = vkCmdCopyBuffer;

        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.physicalDevice = renderer->physicalDevice;
        allocatorInfo.device = renderer->device;
        allocatorInfo.instance = renderer->instance;

        // Core in 1.1
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT | VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
        vma_vulkan_func.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
        vma_vulkan_func.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;

        if (renderer->features_1_2.bufferDeviceAddress == VK_TRUE)
        {
            allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
            vma_vulkan_func.vkBindBufferMemory2KHR = vkBindBufferMemory2;
            vma_vulkan_func.vkBindImageMemory2KHR = vkBindImageMemory2;
        }

        allocatorInfo.pVulkanFunctions = &vma_vulkan_func;

        VkResult result = vmaCreateAllocator(&allocatorInfo, &renderer->allocator);

        if (result != VK_SUCCESS)
        {
            VK_LOG_ERROR(result, "Cannot create allocator");
            delete renderer;
            return nullptr;
        }
    }

    // Create frame resources:
    for (uint32_t i = 0; i < VGFX_MAX_INFLIGHT_FRAMES; ++i)
    {
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkResult result = vkCreateFence(renderer->device, &fenceInfo, nullptr, &renderer->frames[i].fence);
        if (result != VK_SUCCESS)
        {
            VK_LOG_ERROR(result, "Cannot create frame fence");
            delete renderer;
            return nullptr;
        }

        // Create resources for transition command buffer.
        {
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = renderer->graphicsQueueFamily;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            result = vkCreateCommandPool(renderer->device, &poolInfo, nullptr, &renderer->frames[i].initCommandPool);
            if (result != VK_SUCCESS)
            {
                VK_LOG_ERROR(result, "Cannot create frame command pool");
                delete renderer;
                return nullptr;
            }

            VkCommandBufferAllocateInfo commandBufferInfo = {};
            commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferInfo.commandBufferCount = 1;
            commandBufferInfo.commandPool = renderer->frames[i].initCommandPool;
            commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

            result = vkAllocateCommandBuffers(renderer->device, &commandBufferInfo, &renderer->frames[i].initCommandBuffer);
            if (result != VK_SUCCESS)
            {
                VK_LOG_ERROR(result, "vkAllocateCommandBuffers failed");
                delete renderer;
                return nullptr;
            }

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result = vkBeginCommandBuffer(renderer->frames[i].initCommandBuffer, &beginInfo);
            if (result != VK_SUCCESS)
            {
                VK_LOG_ERROR(result, "vkBeginCommandBuffer failed");
                delete renderer;
                return nullptr;
            }
        }

        // Create resources for frame command buffer.
        {
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = renderer->graphicsQueueFamily;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            result = vkCreateCommandPool(renderer->device, &poolInfo, nullptr, &renderer->frames[i].frameCommandPool);
            if (result != VK_SUCCESS)
            {
                VK_LOG_ERROR(result, "Cannot create frame command pool");
                delete renderer;
                return nullptr;
            }

            VkCommandBufferAllocateInfo commandBufferInfo = {};
            commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferInfo.commandBufferCount = 1;
            commandBufferInfo.commandPool = renderer->frames[i].frameCommandPool;
            commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            result = vkAllocateCommandBuffers(renderer->device, &commandBufferInfo, &renderer->frames[i].frameCommandBuffer);
            if (result != VK_SUCCESS)
            {
                VK_LOG_ERROR(result, "vkAllocateCommandBuffers failed");
                delete renderer;
                return nullptr;
            }

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result = vkBeginCommandBuffer(renderer->frames[i].frameCommandBuffer, &beginInfo);
            if (result != VK_SUCCESS)
            {
                VK_LOG_ERROR(result, "vkBeginCommandBuffer failed");
                delete renderer;
                return nullptr;
            }
        }
    }

    // Create default null descriptors.
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 4;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.flags = 0;
        VmaAllocationCreateInfo bufferAllocInfo = {};
        bufferAllocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        VkResult result = vmaCreateBuffer(renderer->allocator, &bufferInfo, &bufferAllocInfo, &renderer->nullBuffer, &renderer->nullBufferAllocation, nullptr);
        VGFX_ASSERT(result == VK_SUCCESS);

        VkBufferViewCreateInfo bufferViewInfo = {};
        bufferViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        bufferViewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        bufferViewInfo.range = VK_WHOLE_SIZE;
        bufferViewInfo.buffer = renderer->nullBuffer;
        result = vkCreateBufferView(renderer->device, &bufferViewInfo, nullptr, &renderer->nullBufferView);
        VGFX_ASSERT(result == VK_SUCCESS);

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.extent.width = 1;
        imageInfo.extent.height = 1;
        imageInfo.extent.depth = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.arrayLayers = 1;
        imageInfo.mipLevels = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        imageInfo.flags = 0;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        imageInfo.imageType = VK_IMAGE_TYPE_1D;
        result = vmaCreateImage(renderer->allocator, &imageInfo, &allocInfo, &renderer->nullImage1D, &renderer->nullImageAllocation1D, nullptr);
        VGFX_ASSERT(result == VK_SUCCESS);

        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imageInfo.arrayLayers = 6;
        result = vmaCreateImage(renderer->allocator, &imageInfo, &allocInfo, &renderer->nullImage2D, &renderer->nullImageAllocation2D, nullptr);
        VGFX_ASSERT(result == VK_SUCCESS);

        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        imageInfo.flags = 0;
        imageInfo.arrayLayers = 1;
        result = vmaCreateImage(renderer->allocator, &imageInfo, &allocInfo, &renderer->nullImage3D, &renderer->nullImageAllocation3D, nullptr);
        VGFX_ASSERT(result == VK_SUCCESS);

        // Transitions:
        renderer->initLocker.lock();
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = imageInfo.initialLayout;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = renderer->nullImage1D;
            barrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(
                renderer->frames[renderer->frameIndex].initCommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
            barrier.image = renderer->nullImage2D;
            barrier.subresourceRange.layerCount = 6;
            vkCmdPipelineBarrier(
                renderer->frames[renderer->frameIndex].initCommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
            barrier.image = renderer->nullImage3D;
            barrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(
                renderer->frames[renderer->frameIndex].initCommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
        }
        renderer->submitInits = true;
        renderer->initLocker.unlock();

        VkImageViewCreateInfo imageViewInfo = {};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
        imageViewInfo.image = renderer->nullImage1D;
        imageViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageView1D);
        VGFX_ASSERT(result == VK_SUCCESS);

        imageViewInfo.image = renderer->nullImage1D;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageView1DArray);
        VGFX_ASSERT(result == VK_SUCCESS);

        imageViewInfo.image = renderer->nullImage2D;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageView2D);
        VGFX_ASSERT(result == VK_SUCCESS);

        imageViewInfo.image = renderer->nullImage2D;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageView2DArray);
        VGFX_ASSERT(result == VK_SUCCESS);

        imageViewInfo.image = renderer->nullImage2D;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        imageViewInfo.subresourceRange.layerCount = 6;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageViewCube);
        VGFX_ASSERT(result == VK_SUCCESS);

        imageViewInfo.image = renderer->nullImage2D;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        imageViewInfo.subresourceRange.layerCount = 6;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageViewCubeArray);
        VGFX_ASSERT(result == VK_SUCCESS);

        imageViewInfo.image = renderer->nullImage3D;
        imageViewInfo.subresourceRange.layerCount = 1;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageView3D);
        VGFX_ASSERT(result == VK_SUCCESS);

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        result = vkCreateSampler(renderer->device, &samplerInfo, nullptr, &renderer->nullSampler);
        VGFX_ASSERT(result == VK_SUCCESS);
    }

    vgfxLogInfo("VGPU Driver: Vulkan");
    vgfxLogInfo("Vulkan Adapter: %s", renderer->properties2.properties.deviceName);

    VGFXDevice_T* device = (VGFXDevice_T*)VGFX_MALLOC(sizeof(VGFXDevice_T));
    ASSIGN_DRIVER(vulkan);

    device->driverData = (VGFXRenderer*)renderer;
    return device;
}

VGFXDriver Vulkan_Driver = {
    VGPU_BACKEND_TYPE_VULKAN,
    vulkan_isSupported,
    vulkan_createDevice
};

#endif /* VGPU_VULKAN_DRIVER */
