// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGPU_VULKAN_DRIVER)

#include "vgpu_driver.h"

VGPU_DISABLE_WARNINGS()
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <spirv_reflect.h>
VGPU_ENABLE_WARNINGS()

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
        VGPU_UNUSED(pUserData);

        const char* messageTypeStr = "General";

        if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
            messageTypeStr = "Validation";
        else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
            messageTypeStr = "Performance";

        // Log debug messge
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            vgpu_log_warn("Vulkan - %s: %s", messageTypeStr, pCallbackData->pMessage);
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            vgpuLogError("Vulkan - %s: %s", messageTypeStr, pCallbackData->pMessage);
        }

        return VK_FALSE;
    }

    inline bool ValidateLayers(const std::vector<const char*>& required, const std::vector<VkLayerProperties>& available)
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
                vgpu_log_warn("Validation Layer '%s' not found", layer);
                return false;
            }
        }

        return true;
    }

    inline std::vector<const char*> GetOptimalValidationLayers(const std::vector<VkLayerProperties>& supported_instance_layers)
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

            vgpu_log_warn("Couldn't enable validation layers (see log for error) - falling back");
        }

        // Else return nothing
        return {};
    }

    inline VkBool32 vulkan_queryPresentationSupport(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex)
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
        bool dynamicRendering;
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
            else if (strcmp(vk_extensions[i].extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0) {
                extensions.dynamicRendering = true;
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
            extensions.dynamicRendering = true;
            extensions.extended_dynamic_state = true;
            extensions.extended_dynamic_state2 = true;
        }

        return extensions;
    }

    inline bool IsDepthStencilFormatSupported(VkPhysicalDevice physicalDevice, VkFormat format)
    {
        VGPU_ASSERT(
            format == VK_FORMAT_D16_UNORM_S8_UINT ||
            format == VK_FORMAT_D24_UNORM_S8_UINT ||
            format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            format == VK_FORMAT_S8_UINT
        );
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
        return properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    constexpr VkFormat ToVkFormat(VGPUTextureFormat format)
    {
        switch (format)
        {
            // 8-bit formats
            case VGPUTextureFormat_R8Unorm:         return VK_FORMAT_R8_UNORM;
            case VGPUTextureFormat_R8Snorm:         return VK_FORMAT_R8_SNORM;
            case VGPUTextureFormat_R8Uint:          return VK_FORMAT_R8_UINT;
            case VGPUTextureFormat_R8Sint:          return VK_FORMAT_R8_SINT;
                // 16-bit formats
            case VGPUTextureFormat_R16Unorm:        return VK_FORMAT_R16_UNORM;
            case VGPUTextureFormat_R16Snorm:        return VK_FORMAT_R16_SNORM;
            case VGPUTextureFormat_R16Uint:         return VK_FORMAT_R16_UINT;
            case VGPUTextureFormat_R16Sint:         return VK_FORMAT_R16_SINT;
            case VGPUTextureFormat_R16Float:        return VK_FORMAT_R16_SFLOAT;
            case VGPUTextureFormat_RG8Unorm:        return VK_FORMAT_R8G8_UNORM;
            case VGPUTextureFormat_RG8Snorm:        return VK_FORMAT_R8G8_SNORM;
            case VGPUTextureFormat_RG8Uint:         return VK_FORMAT_R8G8_UINT;
            case VGPUTextureFormat_RG8Sint:         return VK_FORMAT_R8G8_SINT;
                // Packed 16-Bit Pixel Formats
            case VGPUTextureFormat_BGRA4Unorm:       return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
            case VGPUTextureFormat_B5G6R5Unorm:      return VK_FORMAT_B5G6R5_UNORM_PACK16;
            case VGPUTextureFormat_B5G5R5A1Unorm:    return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
                // 32-bit formats
            case VGPUTextureFormat_R32Uint:          return VK_FORMAT_R32_UINT;
            case VGPUTextureFormat_R32Sint:          return VK_FORMAT_R32_SINT;
            case VGPUTextureFormat_R32Float:         return VK_FORMAT_R32_SFLOAT;
            case VGPUTextureFormat_RG16Unorm:        return VK_FORMAT_R16G16_UNORM;
            case VGPUTextureFormat_RG16Snorm:        return VK_FORMAT_R16G16_SNORM;
            case VGPUTextureFormat_RG16Uint:         return VK_FORMAT_R16G16_UINT;
            case VGPUTextureFormat_RG16Sint:         return VK_FORMAT_R16G16_SINT;
            case VGPUTextureFormat_RG16Float:        return VK_FORMAT_R16G16_SFLOAT;
            case VGPUTextureFormat_RGBA8Uint:        return VK_FORMAT_R8G8B8A8_UINT;
            case VGPUTextureFormat_RGBA8Sint:        return VK_FORMAT_R8G8B8A8_SINT;
            case VGPUTextureFormat_BGRA8Unorm:       return VK_FORMAT_B8G8R8A8_UNORM;
            case VGPUTextureFormat_RGBA8Unorm:       return VK_FORMAT_R8G8B8A8_UNORM;
            case VGPUTextureFormat_RGBA8UnormSrgb:   return VK_FORMAT_R8G8B8A8_SRGB;
            case VGPUTextureFormat_RGBA8Snorm:       return VK_FORMAT_R8G8B8A8_SNORM;
            case VGPUTextureFormat_BGRA8UnormSrgb:   return VK_FORMAT_B8G8R8A8_SRGB;
                // Packed 32-Bit formats
            case VGPUTextureFormat_RGB9E5Ufloat:        return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
            case VGPUTextureFormat_RGB10A2Unorm:        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            case VGPUTextureFormat_RGB10A2Uint:         return VK_FORMAT_A2R10G10B10_UINT_PACK32;
            case VGPUTextureFormat_RG11B10Float:        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
                // 64-Bit formats
            case VGPUTextureFormat_RG32Uint:         return VK_FORMAT_R32G32_UINT;
            case VGPUTextureFormat_RG32Sint:         return VK_FORMAT_R32G32_SINT;
            case VGPUTextureFormat_RG32Float:        return VK_FORMAT_R32G32_SFLOAT;
            case VGPUTextureFormat_RGBA16Unorm:      return VK_FORMAT_R16G16B16A16_UNORM;
            case VGPUTextureFormat_RGBA16Snorm:      return VK_FORMAT_R16G16B16A16_SNORM;
            case VGPUTextureFormat_RGBA16Uint:       return VK_FORMAT_R16G16B16A16_UINT;
            case VGPUTextureFormat_RGBA16Sint:       return VK_FORMAT_R16G16B16A16_SINT;
            case VGPUTextureFormat_RGBA16Float:      return VK_FORMAT_R16G16B16A16_SFLOAT;
                // 128-Bit formats
            case VGPUTextureFormat_RGBA32Uint:       return VK_FORMAT_R32G32B32A32_UINT;
            case VGPUTextureFormat_RGBA32Sint:       return VK_FORMAT_R32G32B32A32_SINT;
            case VGPUTextureFormat_RGBA32Float:      return VK_FORMAT_R32G32B32A32_SFLOAT;
                // Depth-stencil formats
            case VGPUTextureFormat_Depth16Unorm:            return VK_FORMAT_D16_UNORM;
            case VGPUTextureFormat_Depth32Float:            return VK_FORMAT_D32_SFLOAT;
            case VGPUTextureFormat_Stencil8:                return VK_FORMAT_S8_UINT;
            case VGPUTextureFormat_Depth24UnormStencil8:    return VK_FORMAT_D24_UNORM_S8_UINT;
            case VGPUTextureFormat_Depth32FloatStencil8:    return VK_FORMAT_D32_SFLOAT_S8_UINT;
                // Compressed BC formats
            case VGPUTextureFormat_Bc1RgbaUnorm:                return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            case VGPUTextureFormat_Bc1RgbaUnormSrgb:            return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
            case VGPUTextureFormat_Bc2RgbaUnorm:                return VK_FORMAT_BC2_UNORM_BLOCK;
            case VGPUTextureFormat_Bc2RgbaUnormSrgb:            return VK_FORMAT_BC2_SRGB_BLOCK;
            case VGPUTextureFormat_Bc3RgbaUnorm:                return VK_FORMAT_BC3_UNORM_BLOCK;
            case VGPUTextureFormat_Bc3RgbaUnormSrgb:            return VK_FORMAT_BC3_SRGB_BLOCK;
            case VGPUTextureFormat_Bc4RSnorm:                   return VK_FORMAT_BC4_SNORM_BLOCK;
            case VGPUTextureFormat_Bc4RUnorm:                   return VK_FORMAT_BC4_UNORM_BLOCK;
            case VGPUTextureFormat_Bc5RgSnorm:                  return VK_FORMAT_BC5_SNORM_BLOCK;
            case VGPUTextureFormat_Bc5RgUnorm:                  return VK_FORMAT_BC5_UNORM_BLOCK;
            case VGPUTextureFormat_Bc6hRgbUfloat:               return VK_FORMAT_BC6H_UFLOAT_BLOCK;
            case VGPUTextureFormat_Bc6hRgbSfloat:               return VK_FORMAT_BC6H_SFLOAT_BLOCK;
            case VGPUTextureFormat_Bc7RgbaUnorm:                return VK_FORMAT_BC7_UNORM_BLOCK;
            case VGPUTextureFormat_Bc7RgbaUnormSrgb:            return VK_FORMAT_BC7_SRGB_BLOCK;
                // EAC/ETC compressed formats
            case VGPUTextureFormat_Etc2Rgb8Unorm:               return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
            case VGPUTextureFormat_Etc2Rgb8UnormSrgb:           return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
            case VGPUTextureFormat_Etc2Rgb8A1Unorm:             return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
            case VGPUTextureFormat_Etc2Rgb8A1UnormSrgb:         return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
            case VGPUTextureFormat_Etc2Rgba8Unorm:              return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
            case VGPUTextureFormat_Etc2Rgba8UnormSrgb:          return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
            case VGPUTextureFormat_EacR11Unorm:                 return VK_FORMAT_EAC_R11_UNORM_BLOCK;
            case VGPUTextureFormat_EacR11Snorm:                 return VK_FORMAT_EAC_R11_SNORM_BLOCK;
            case VGPUTextureFormat_EacRg11Unorm:                return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
            case VGPUTextureFormat_EacRg11Snorm:                return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
                // ASTC compressed formats
            case VGPUTextureFormat_Astc4x4Unorm:               return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
            case VGPUTextureFormat_Astc4x4UnormSrgb:           return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
            case VGPUTextureFormat_Astc5x4Unorm:               return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
            case VGPUTextureFormat_Astc5x4UnormSrgb:           return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
            case VGPUTextureFormat_Astc5x5Unorm:               return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
            case VGPUTextureFormat_Astc5x5UnormSrgb:           return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
            case VGPUTextureFormat_Astc6x5Unorm:               return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
            case VGPUTextureFormat_Astc6x5UnormSrgb:           return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
            case VGPUTextureFormat_Astc6x6Unorm:               return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
            case VGPUTextureFormat_Astc6x6UnormSrgb:           return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
            case VGPUTextureFormat_Astc8x5Unorm:               return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
            case VGPUTextureFormat_Astc8x5UnormSrgb:           return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
            case VGPUTextureFormat_Astc8x6Unorm:               return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
            case VGPUTextureFormat_Astc8x6UnormSrgb:           return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
            case VGPUTextureFormat_Astc8x8Unorm:               return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
            case VGPUTextureFormat_Astc8x8UnormSrgb:           return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
            case VGPUTextureFormat_Astc10x5Unorm:              return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
            case VGPUTextureFormat_Astc10x5UnormSrgb:          return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
            case VGPUTextureFormat_Astc10x6Unorm:              return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
            case VGPUTextureFormat_Astc10x6UnormSrgb:          return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
            case VGPUTextureFormat_Astc10x8Unorm:              return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
            case VGPUTextureFormat_Astc10x8UnormSrgb:          return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
            case VGPUTextureFormat_Astc10x10Unorm:             return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
            case VGPUTextureFormat_Astc10x10UnormSrgb:         return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
            case VGPUTextureFormat_Astc12x10Unorm:             return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
            case VGPUTextureFormat_Astc12x10UnormSrgb:         return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
            case VGPUTextureFormat_Astc12x12Unorm:             return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
            case VGPUTextureFormat_Astc12x12UnormSrgb:         return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;

            default:
                return VK_FORMAT_UNDEFINED;
        }
    }

    constexpr VkAttachmentLoadOp ToVkAttachmentLoadOp(VGPULoadAction op)
    {
        switch (op)
        {
            case VGPULoadAction_Clear:
                return VK_ATTACHMENT_LOAD_OP_CLEAR;
            case VGPULoadAction_DontCare:
                return VK_ATTACHMENT_LOAD_OP_DONT_CARE;

            default:
                return VK_ATTACHMENT_LOAD_OP_LOAD;
        }
    }

    constexpr VkAttachmentStoreOp ToVkAttachmentStoreOp(VGPUStoreOp op)
    {
        switch (op)
        {
            case VGPUStoreOp_DontCare:
                return VK_ATTACHMENT_STORE_OP_DONT_CARE;

            default:
                return VK_ATTACHMENT_STORE_OP_STORE;
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

    constexpr VkCompareOp ToVkCompareOp(VGPUCompareFunction function)
    {
        switch (function)
        {
            case VGPUCompareFunction_Never:        return VK_COMPARE_OP_NEVER;
            case VGPUCompareFunction_Less:         return VK_COMPARE_OP_LESS;
            case VGPUCompareFunction_Equal:        return VK_COMPARE_OP_EQUAL;
            case VGPUCompareFunction_LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
            case VGPUCompareFunction_Greater:      return VK_COMPARE_OP_GREATER;
            case VGPUCompareFunction_NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
            case VGPUCompareFunction_GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case VGPUCompareFunction_Always:       return VK_COMPARE_OP_ALWAYS;
            default:
                return VK_COMPARE_OP_NEVER;
        }
    }

    constexpr VkFilter ToVkFilter(VGPUSamplerFilter mode)
    {
        switch (mode)
        {
            case VGPUSamplerFilter_Linear:
                return VK_FILTER_LINEAR;
            default:
                return VK_FILTER_NEAREST;
        }
    }

    constexpr VkSamplerMipmapMode ToVkMipmapMode(VGPUSamplerMipFilter mode)
    {
        switch (mode)
        {
            case VGPUSamplerMipFilter_Linear:
                return VK_SAMPLER_MIPMAP_MODE_LINEAR;
            default:
                return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        }
    }

    constexpr VkSamplerAddressMode ToVkSamplerAddressMode(VGPUSamplerAddressMode mode)
    {
        switch (mode)
        {
            case VGPUSamplerAddressMode_Mirror:
                return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

            case VGPUSamplerAddressMode_Clamp:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

            case VGPUSamplerAddressMode_Border:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

            default:
            case VGPUSamplerAddressMode_Wrap:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }

    constexpr VkBorderColor ToVkBorderColor(VGPUSamplerBorderColor value)
    {
        switch (value)
        {
            case VGPUSamplerBorderColor_OpaqueBlack:
                return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
            case VGPUSamplerBorderColor_OpaqueWhite:
                return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            default:
            case VGPUSamplerBorderColor_TransparentBlack:
                return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        }
    }

    static_assert(sizeof(VGPUViewport) == sizeof(VkViewport), "Viewport mismatch");
    static_assert(offsetof(VGPUViewport, x) == offsetof(VkViewport, x), "Layout mismatch");
    static_assert(offsetof(VGPUViewport, y) == offsetof(VkViewport, y), "Layout mismatch");
    static_assert(offsetof(VGPUViewport, width) == offsetof(VkViewport, width), "Layout mismatch");
    static_assert(offsetof(VGPUViewport, height) == offsetof(VkViewport, height), "Layout mismatch");
    static_assert(offsetof(VGPUViewport, minDepth) == offsetof(VkViewport, minDepth), "Layout mismatch");
    static_assert(offsetof(VGPUViewport, maxDepth) == offsetof(VkViewport, maxDepth), "Layout mismatch");

    static_assert(sizeof(VGPURect) == sizeof(VkRect2D), "vgpu_rect mismatch");
    static_assert(offsetof(VGPURect, x) == offsetof(VkRect2D, offset.x), "vgpu_rect Layout mismatch");
    static_assert(offsetof(VGPURect, y) == offsetof(VkRect2D, offset.y), "vgpu_rect Layout mismatch");
    static_assert(offsetof(VGPURect, width) == offsetof(VkRect2D, extent.width), "vgpu_rect Layout mismatch");
    static_assert(offsetof(VGPURect, height) == offsetof(VkRect2D, extent.height), "vgpu_rect Layout mismatch");
}

/// Helper macro to test the result of Vulkan calls which can return an error.
#define VK_CHECK(x) \
	do \
	{ \
		VkResult err = x; \
		if (err) \
		{ \
			vgpuLogError("Detected Vulkan error: %s", ToString(err)); \
		} \
	} while (0)

#define VK_LOG_ERROR(result, message) vgpuLogError("Vulkan: %s, error: %s", message, ToString(result));

struct VulkanRenderer;

struct VulkanBuffer
{
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation  allocation = nullptr;
    uint64_t size = 0;
    uint64_t allocatedSize = 0;
    VkDeviceAddress gpuAddress = 0;
    void* pMappedData = nullptr;
};

struct VulkanTexture
{
    VkImage handle = VK_NULL_HANDLE;
    VmaAllocation  allocation = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    std::unordered_map<size_t, VkImageView> viewCache;
};

struct VulkanSampler
{
    VkSampler handle = VK_NULL_HANDLE;
};

struct VulkanShader
{
    VkShaderModule handle;
};

struct VulkanPipeline
{
    VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline handle = VK_NULL_HANDLE;
};

struct VulkanSwapChain
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkExtent2D extent;
    bool vsync = false;
    VGPUTextureFormat colorFormat;
    bool allowHDR = true;
    uint32_t imageIndex;
    std::vector<VulkanTexture*> backbufferTextures;

    VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
    VkSemaphore releaseSemaphore = VK_NULL_HANDLE;
};

struct VulkanCommandBuffer
{
    VulkanRenderer* renderer;
    bool hasLabel;
    VkCommandPool commandPools[VGPU_MAX_INFLIGHT_FRAMES];
    VkCommandBuffer commandBuffers[VGPU_MAX_INFLIGHT_FRAMES];
    VkCommandBuffer handle;

    uint32_t clearValueCount = 0;
    VkClearValue clearValues[VGPU_MAX_COLOR_ATTACHMENTS + 1];

    bool insideRenderPass;
    std::vector<VulkanSwapChain*> swapChains;
};

struct Vulkan_UploadContext
{
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    uint64_t target = 0;

    uint64_t uploadBufferSize = 0;
    vgpu_buffer* uploadBuffer = nullptr;
    void* uploadBufferData = nullptr;
};

struct VulkanRenderer
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
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;

    uint32_t frameIndex = 0;
    uint64_t frameCount = 0;

    struct FrameResources
    {
        VkFence fence = VK_NULL_HANDLE;
        VkCommandPool initCommandPool = VK_NULL_HANDLE;
        VkCommandBuffer initCommandBuffer = VK_NULL_HANDLE;
    };
    FrameResources frames[VGPU_MAX_INFLIGHT_FRAMES];
    std::mutex initLocker;
    bool submitInits = false;

    const FrameResources& GetFrameResources() const { return frames[frameIndex]; }
    FrameResources& GetFrameResources() { return frames[frameIndex]; }

    /* Command contexts */
    std::mutex cmdBuffersLocker;
    uint32_t cmdBuffersCount{ 0 };
    std::vector<VGPUCommandBuffer_T*> commandBuffers;

    std::mutex uploadLocker;
    VkSemaphore uploadSemaphore = VK_NULL_HANDLE;
    std::vector<Vulkan_UploadContext> uploadFreeList;
    std::vector<Vulkan_UploadContext> uploadWorkList; // In progress
    uint64_t uploadFenceValue = 0;
    std::vector<VkCommandBuffer> uploadSubmitCmds; // for next submit
    uint64_t uploadSubmitWait = 0; // last submit wait value

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

    std::vector<VkDynamicState> psoDynamicStates;
    VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};

    std::unordered_map<size_t, VkRenderPass> renderPassCache;
    std::unordered_map<size_t, VkFramebuffer> framebufferCache;

    // Deletion queue objects
    std::mutex destroyMutex;
    std::deque<std::pair<std::pair<VkBuffer, VmaAllocation>, uint64_t>> destroyedBuffers;
    std::deque<std::pair<std::pair<VkImage, VmaAllocation>, uint64_t>> destroyedImages;
    std::deque<std::pair<VkImageView, uint64_t>> destroyedImageViews;
    std::deque<std::pair<VkSampler, uint64_t>> destroyedSamplers;
    std::deque<std::pair<VkShaderModule, uint64_t>> destroyedShaderModules;
    std::deque<std::pair<VkPipelineLayout, uint64_t>> destroyedPipelineLayouts;
    std::deque<std::pair<VkPipeline, uint64_t>> destroyedPipelines;
    std::deque<std::pair<VkDescriptorPool, uint64_t>> destroyedDescriptorPools;
    std::deque<std::pair<VkQueryPool, uint64_t>> destroyedQueryPools;
};

static void vulkan_SetObjectName(VulkanRenderer* renderer, VkObjectType type, uint64_t handle, const char* name)
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

static VkSurfaceKHR vulkan_createSurface(VulkanRenderer* renderer, void* windowHandle)
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

vgpu_buffer* vulkan_createBuffer(VGPURenderer* driverData, const vgpu_buffer_desc* desc, const void* pInitialData);
void vulkan_destroyBuffer(VGPURenderer* driverData, vgpu_buffer* resource);

static Vulkan_UploadContext vulkan_AllocateUpload(VGPURenderer* driverData, uint64_t size)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    renderer->uploadLocker.lock();

    HRESULT hr = S_OK;

    // Create a new command list if there are no free ones.
    if (renderer->uploadFreeList.empty())
    {
        Vulkan_UploadContext context;

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = renderer->copyQueueFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        VK_CHECK(vkCreateCommandPool(renderer->device, &poolInfo, nullptr, &context.commandPool));

        VkCommandBufferAllocateInfo commandBufferInfo = {};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferInfo.commandBufferCount = 1;
        commandBufferInfo.commandPool = context.commandPool;
        commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        VK_CHECK(vkAllocateCommandBuffers(renderer->device, &commandBufferInfo, &context.commandBuffer));

        renderer->uploadFreeList.push_back(context);
    }

    Vulkan_UploadContext context = renderer->uploadFreeList.back();
    if (context.uploadBufferSize < size)
    {
        // Try to search for a staging buffer that can fit the request:
        for (size_t i = 0; i < renderer->uploadFreeList.size(); ++i)
        {
            if (renderer->uploadFreeList[i].uploadBufferSize >= size)
            {
                context = renderer->uploadFreeList[i];
                std::swap(renderer->uploadFreeList[i], renderer->uploadFreeList.back());
                break;
            }
        }
    }
    renderer->uploadFreeList.pop_back();
    renderer->uploadLocker.unlock();

    // If no buffer was found that fits the data, create one:
    if (context.uploadBufferSize < size)
    {
        // Release old one first
        if (context.uploadBuffer)
            vulkan_destroyBuffer(driverData, context.uploadBuffer);

        uint64_t newSize = vgpuNextPowerOfTwo(size);

        vgpu_buffer_desc buffer_desc{};
        buffer_desc.label = "UploadBuffer";
        buffer_desc.size = vgpuNextPowerOfTwo(size);
        buffer_desc.access = VGPU_CPU_ACCESS_WRITE;
        context.uploadBuffer = vulkan_createBuffer(driverData, &buffer_desc, nullptr);
        context.uploadBufferData = ((VulkanBuffer*)context.uploadBuffer)->pMappedData;
    }

    // begin command list in valid state:
    VK_CHECK(vkResetCommandPool(renderer->device, context.commandPool, 0));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    VK_CHECK(vkBeginCommandBuffer(context.commandBuffer, &beginInfo));

    return context;
}

static void vulkan_UploadSubmit(VulkanRenderer* renderer, Vulkan_UploadContext context)
{
    VkResult result = vkEndCommandBuffer(context.commandBuffer);
    VGPU_ASSERT(result == VK_SUCCESS);

    // It was very slow in Vulkan to submit the copies immediately
    //	In Vulkan, the submit is not thread safe, so it had to be locked
    //	Instead, the submits are batched and performed in flush() function
    renderer->uploadLocker.lock();
    context.target = ++renderer->uploadFenceValue;
    renderer->uploadWorkList.push_back(context);
    renderer->uploadSubmitCmds.push_back(context.commandBuffer);
    renderer->uploadSubmitWait = _VGPU_MAX(renderer->uploadSubmitWait, context.target);
    renderer->uploadLocker.unlock();
}

static uint64_t vulkan_UploadFlush(VulkanRenderer* renderer)
{
    renderer->uploadLocker.lock();
    if (!renderer->uploadSubmitCmds.empty())
    {
        VkTimelineSemaphoreSubmitInfo timelineInfo = {};
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfo.pNext = nullptr;
        timelineInfo.waitSemaphoreValueCount = 0;
        timelineInfo.pWaitSemaphoreValues = nullptr;
        timelineInfo.signalSemaphoreValueCount = 1;
        timelineInfo.pSignalSemaphoreValues = &renderer->uploadSubmitWait;

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = &timelineInfo;
        submitInfo.commandBufferCount = (uint32_t)renderer->uploadSubmitCmds.size();
        submitInfo.pCommandBuffers = renderer->uploadSubmitCmds.data();
        submitInfo.pSignalSemaphores = &renderer->uploadSemaphore;
        submitInfo.signalSemaphoreCount = 1;

        VkResult res = vkQueueSubmit(renderer->copyQueue, 1, &submitInfo, VK_NULL_HANDLE);
        assert(res == VK_SUCCESS);

        renderer->uploadSubmitCmds.clear();
    }

    // free up the finished command lists:
    uint64_t completedFenceValue;
    VkResult res = vkGetSemaphoreCounterValue(renderer->device, renderer->uploadSemaphore, &completedFenceValue);
    assert(res == VK_SUCCESS);
    for (size_t i = 0; i < renderer->uploadWorkList.size(); ++i)
    {
        if (renderer->uploadWorkList[i].target <= completedFenceValue)
        {
            renderer->uploadFreeList.push_back(renderer->uploadWorkList[i]);
            renderer->uploadWorkList[i] = renderer->uploadWorkList.back();
            renderer->uploadWorkList.pop_back();
            i--;
        }
    }

    uint64_t value = renderer->uploadSubmitWait;
    renderer->uploadSubmitWait = 0;
    renderer->uploadLocker.unlock();

    return value;
}

static VkImageView vulkan_GetView(VulkanRenderer* renderer, VulkanTexture* texture, uint32_t baseMipLevel, uint32_t levelCount, uint32_t baseArrayLayer, uint32_t layerCount)
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

static VkImageView vulkan_GetRTV(VulkanRenderer* renderer, VulkanTexture* texture, uint32_t level, uint32_t slice)
{
    return vulkan_GetView(renderer, texture, level, 1, slice, 1);
}

struct VulkanRenderPassAttachment
{
    VkFormat format;
    VGPULoadAction loadAction;
    VGPUStoreOp storeAction;
};

struct VulkanRenderPassKey
{
    uint32_t colorAttachmentCount = 0;
    const VulkanRenderPassAttachment* colorAttachments = nullptr;
    const VulkanRenderPassAttachment* depthAttachment = nullptr;
    const VulkanRenderPassAttachment* stencilAttachment = nullptr;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
};

static VkRenderPass vulkan_RequestRenderPass(VulkanRenderer* renderer, const VulkanRenderPassKey* key)
{
    size_t hash = 0;

    hash_combine(hash, key->colorAttachmentCount);
    for (uint32_t i = 0; i < key->colorAttachmentCount; ++i)
    {
        hash_combine(hash, (uint32_t)key->colorAttachments[i].format);
        hash_combine(hash, (uint32_t)key->colorAttachments[i].loadAction);
        hash_combine(hash, (uint32_t)key->colorAttachments[i].storeAction);
    }

    if (key->depthAttachment != nullptr)
    {
        hash_combine(hash, (uint32_t)key->depthAttachment->format);
        hash_combine(hash, (uint32_t)key->depthAttachment->loadAction);
        hash_combine(hash, (uint32_t)key->depthAttachment->storeAction);
    }

    if (key->stencilAttachment != nullptr)
    {
        hash_combine(hash, (uint32_t)key->stencilAttachment->format);
        hash_combine(hash, (uint32_t)key->stencilAttachment->loadAction);
        hash_combine(hash, (uint32_t)key->stencilAttachment->storeAction);
    }

    auto it = renderer->renderPassCache.find(hash);
    if (it == renderer->renderPassCache.end())
    {
        // Not found -> create new
        VkAttachmentDescription2  attachmentDescriptions[VGPU_MAX_COLOR_ATTACHMENTS * 2 + 1] = {};
        VkAttachmentReference2 colorAttachmentRefs[VGPU_MAX_COLOR_ATTACHMENTS] = {};
        VkAttachmentReference2 resolveAttachmentRefs[VGPU_MAX_COLOR_ATTACHMENTS] = {};
        VkAttachmentReference2 shadingRateAttachmentRef = {};
        VkAttachmentReference2 depthStencilAttachmentRef = {};

        const VkSampleCountFlagBits sampleCount = key->sampleCount;

        VkSubpassDescription2 subpass = {};
        subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        for (uint32_t i = 0; i < key->colorAttachmentCount; ++i)
        {
            const VulkanRenderPassAttachment& attachment = key->colorAttachments[i];

            auto& attachmentRef = colorAttachmentRefs[subpass.colorAttachmentCount];
            auto& attachmentDesc = attachmentDescriptions[subpass.colorAttachmentCount];

            attachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
            attachmentRef.attachment = subpass.colorAttachmentCount;
            attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentRef.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            attachmentDesc.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
            attachmentDesc.flags = 0;
            attachmentDesc.format = attachment.format;
            attachmentDesc.samples = sampleCount;
            attachmentDesc.loadOp = ToVkAttachmentLoadOp(attachment.loadAction);
            attachmentDesc.storeOp = ToVkAttachmentStoreOp(attachment.storeAction);
            attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            subpass.colorAttachmentCount++;
        }

        uint32_t attachmentCount = subpass.colorAttachmentCount;
        VkAttachmentReference2* depthStencilAttachment = nullptr;
        if (key->depthAttachment != nullptr || key->stencilAttachment)
        {
            const VulkanRenderPassAttachment* depthAttachment = key->depthAttachment;
            const VulkanRenderPassAttachment* stencilAttachment = key->stencilAttachment;

            const VkFormat depthFormat = depthAttachment->format;

            auto& attachmentDesc = attachmentDescriptions[subpass.colorAttachmentCount];
            depthStencilAttachment = &depthStencilAttachmentRef;

            depthStencilAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
            depthStencilAttachmentRef.attachment = subpass.colorAttachmentCount;
            depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthStencilAttachmentRef.aspectMask = GetImageAspectFlags(depthFormat);

            attachmentDesc.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
            attachmentDesc.flags = 0;
            attachmentDesc.format = depthAttachment->format;
            attachmentDesc.samples = sampleCount;
            attachmentDesc.loadOp = ToVkAttachmentLoadOp(depthAttachment->loadAction);
            attachmentDesc.storeOp = ToVkAttachmentStoreOp(depthAttachment->storeAction);
            if (depthStencilAttachmentRef.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
            {
                attachmentDesc.stencilLoadOp = ToVkAttachmentLoadOp(stencilAttachment->loadAction);
                attachmentDesc.stencilStoreOp = ToVkAttachmentStoreOp(stencilAttachment->storeAction);
            }
            else
            {
                attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            }
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

        if (key->depthAttachment != nullptr || key->stencilAttachment)
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
    VulkanRenderer* renderer,
    VkRenderPass renderPass,
    const VGPURenderPassDesc* desc,
    uint32_t width, uint32_t height)
{
    size_t hash = 0;
    hash_combine(hash, renderPass);

    uint32_t attachmentCount = 0;
    VkImageView attachments[VGPU_MAX_COLOR_ATTACHMENTS + 1];

    for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i)
    {
        const VGPURenderPassColorAttachment* attachment = &desc->colorAttachments[i];
        VulkanTexture* texture = (VulkanTexture*)attachment->texture;
        const uint32_t level = attachment->level;
        const uint32_t slice = attachment->slice;

        attachments[attachmentCount] = vulkan_GetRTV(renderer, texture, level, slice);
        hash_combine(hash, (uint64_t)attachments[attachmentCount]);
        attachmentCount++;
    }

    if (desc->depthStencilAttachment != nullptr)
    {
        const VGPURenderPassDepthStencilAttachment* attachment = desc->depthStencilAttachment;
        VulkanTexture* texture = (VulkanTexture*)attachment->texture;
        const uint32_t level = attachment->level;
        const uint32_t slice = attachment->slice;

        attachments[attachmentCount] = vulkan_GetRTV(renderer, texture, level, slice);
        hash_combine(hash, (uint64_t)attachments[attachmentCount]);
        attachmentCount++;
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
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layers = 1;

        VkFramebuffer framebuffer;
        VK_CHECK(vkCreateFramebuffer(renderer->device, &createInfo, nullptr, &framebuffer));
        renderer->framebufferCache[hash] = framebuffer;

        return framebuffer;
    }

    return it->second;
}

static void vulkan_ProcessDeletionQueue(VulkanRenderer* renderer)
{
    renderer->destroyMutex.lock();

    while (!renderer->destroyedBuffers.empty())
    {
        if (renderer->destroyedBuffers.front().second + VGPU_MAX_INFLIGHT_FRAMES < renderer->frameCount)
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

    while (!renderer->destroyedImages.empty())
    {
        if (renderer->destroyedImages.front().second + VGPU_MAX_INFLIGHT_FRAMES < renderer->frameCount)
        {
            auto item = renderer->destroyedImages.front();
            renderer->destroyedImages.pop_front();
            vmaDestroyImage(renderer->allocator, item.first.first, item.first.second);
        }
        else
        {
            break;
        }
    }

    while (!renderer->destroyedImageViews.empty())
    {
        if (renderer->destroyedImageViews.front().second + VGPU_MAX_INFLIGHT_FRAMES < renderer->frameCount)
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
        if (renderer->destroyedSamplers.front().second + VGPU_MAX_INFLIGHT_FRAMES < renderer->frameCount)
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

    while (!renderer->destroyedShaderModules.empty())
    {
        if (renderer->destroyedShaderModules.front().second + VGPU_MAX_INFLIGHT_FRAMES < renderer->frameCount)
        {
            auto item = renderer->destroyedShaderModules.front();
            renderer->destroyedShaderModules.pop_front();
            vkDestroyShaderModule(renderer->device, item.first, nullptr);
        }
        else
        {
            break;
        }
    }

    while (!renderer->destroyedPipelineLayouts.empty())
    {
        if (renderer->destroyedPipelineLayouts.front().second + VGPU_MAX_INFLIGHT_FRAMES < renderer->frameCount)
        {
            auto item = renderer->destroyedPipelineLayouts.front();
            renderer->destroyedPipelineLayouts.pop_front();
            vkDestroyPipelineLayout(renderer->device, item.first, nullptr);
        }
        else
        {
            break;
        }
    }

    while (!renderer->destroyedPipelines.empty())
    {
        if (renderer->destroyedPipelines.front().second + VGPU_MAX_INFLIGHT_FRAMES < renderer->frameCount)
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
        if (renderer->destroyedDescriptorPools.front().second + VGPU_MAX_INFLIGHT_FRAMES < renderer->frameCount)
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
        if (renderer->destroyedQueryPools.front().second + VGPU_MAX_INFLIGHT_FRAMES < renderer->frameCount)
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
    VulkanRenderer* renderer = (VulkanRenderer*)device->driverData;
    VK_CHECK(vkDeviceWaitIdle(renderer->device));

    for (auto& frame : renderer->frames)
    {
        vkDestroyFence(renderer->device, frame.fence, nullptr);
        vkDestroyCommandPool(renderer->device, frame.initCommandPool, nullptr);
    }

    for (size_t i = 0; i < renderer->commandBuffers.size(); ++i)
    {
        VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)renderer->commandBuffers[i]->driverData;
        for (uint32_t i = 0; i < VGPU_MAX_INFLIGHT_FRAMES; ++i)
        {
            vkFreeCommandBuffers(renderer->device, commandBuffer->commandPools[i], 1, &commandBuffer->commandBuffers[i]);
            vkDestroyCommandPool(renderer->device, commandBuffer->commandPools[i], nullptr);
            //binderPools[i].Shutdown();
        }
    }
    renderer->commandBuffers.clear();

    // Destroy upload stuff
    vkQueueWaitIdle(renderer->copyQueue);
    for (auto& x : renderer->uploadFreeList)
    {
        vulkan_destroyBuffer(device->driverData, x.uploadBuffer);
        vkDestroyCommandPool(renderer->device, x.commandPool, nullptr);
    }
    vkDestroySemaphore(renderer->device, renderer->uploadSemaphore, nullptr);

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
#if defined(_DEBUG)
        VmaTotalStatistics stats;
        vmaCalculateStatistics(renderer->allocator, &stats);

        if (stats.total.statistics.allocationBytes > 0)
        {
            //    vgpuLogError("Total device memory leaked: {} bytes.", stats.total.usedBytes);
        }
#endif

        vmaDestroyAllocator(renderer->allocator);
        renderer->allocator = VK_NULL_HANDLE;
    }

    if (renderer->pipelineCache != VK_NULL_HANDLE)
    {
        // Get size of pipeline cache
        //size_t size{};
        //VK_CHECK(vkGetPipelineCacheData(renderer->device, renderer->pipelineCache, &size, nullptr));

        // Get data of pipeline cache 
        //std::vector<uint8_t> data(size);
        //res = vkGetPipelineCacheData(device, pipelineCache, &size, data.data());
        //assert(res == VK_SUCCESS);

        // Destroy Vulkan pipeline cache 
        vkDestroyPipelineCache(renderer->device, renderer->pipelineCache, nullptr);
        renderer->pipelineCache = VK_NULL_HANDLE;
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
    VGPU_FREE(device);
}

static uint64_t vulkan_frame(VGPURenderer* driverData)
{
    VkResult result = VK_SUCCESS;
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;

    renderer->initLocker.lock();

    renderer->frameCount++;
    renderer->frameIndex = renderer->frameCount % VGPU_MAX_INFLIGHT_FRAMES;

    // Begin new frame
    {
        auto& frame = renderer->frames[renderer->frameIndex];

        // Initiate stalling CPU when GPU is not yet finished with next frame.
        if (renderer->frameCount >= VGPU_MAX_INFLIGHT_FRAMES)
        {
            result = vkWaitForFences(renderer->device, 1, &frame.fence, VK_TRUE, 0xFFFFFFFFFFFFFFFF);
            VGPU_ASSERT(result == VK_SUCCESS);

            result = vkResetFences(renderer->device, 1, &frame.fence);
            VGPU_ASSERT(result == VK_SUCCESS);
        }

        // Safe delete deferred destroys
        vulkan_ProcessDeletionQueue(renderer);

        // Restart transition command buffers first.
        result = vkResetCommandPool(renderer->device, frame.initCommandPool, 0);
        VGPU_ASSERT(result == VK_SUCCESS);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = vkBeginCommandBuffer(frame.initCommandBuffer, &beginInfo);
        VGPU_ASSERT(result == VK_SUCCESS);
    }

    renderer->submitInits = false;
    renderer->initLocker.unlock();

    // Return current frame
    return renderer->frameCount - 1;
}

static void vulkan_waitIdle(VGPURenderer* driverData)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    VK_CHECK(vkDeviceWaitIdle(renderer->device));
}

static VGPUBackendType vulkan_getBackendType(void)
{
    return VGPUBackendType_Vulkan;
}

static VGPUBool32 vulkan_queryFeature(VGPURenderer* driverData, VGPUFeature feature, void* pInfo, uint32_t infoSize)
{
    (void)pInfo;
    (void)infoSize;

    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    switch (feature)
    {
        case VGPUFeature_TextureCompressionBC:
            return renderer->features2.features.textureCompressionBC;

        case VGPUFeature_TextureCompressionETC2:
            return renderer->features2.features.textureCompressionETC2;

        case VGPUFeature_TextureCompressionASTC:
            return renderer->features2.features.textureCompressionASTC_LDR;

        case VGPUFeature_ShaderFloat16:
            return renderer->features_1_2.shaderFloat16 == VK_TRUE;

        case VGPUFeature_PipelineStatisticsQuery:
            return renderer->features2.features.pipelineStatisticsQuery == VK_TRUE;

        case VGPUFeature_TimestampQuery:
            return renderer->properties2.properties.limits.timestampComputeAndGraphics == VK_TRUE;

        case VGPUFeature_DepthClamping:
            return renderer->features2.features.depthClamp == VK_TRUE;

        case VGPUFeature_Depth24UNormStencil8:
            return IsDepthStencilFormatSupported(renderer->physicalDevice, VK_FORMAT_D24_UNORM_S8_UINT);

        case VGPUFeature_Depth32FloatStencil8:
            return IsDepthStencilFormatSupported(renderer->physicalDevice, VK_FORMAT_D32_SFLOAT_S8_UINT);

        case VGPUFeature_TextureCubeArray:
            return renderer->features2.features.imageCubeArray == VK_TRUE;

        case VGPUFeature_IndependentBlend:
            return renderer->features2.features.independentBlend == VK_TRUE;

        case VGPUFeature_Tessellation:
            return renderer->features2.features.tessellationShader == VK_TRUE;

        case VGPUFeature_DescriptorIndexing:
            return renderer->features_1_2.descriptorIndexing == VK_TRUE;

        case VGPUFeature_ConditionalRendering:
            return renderer->conditional_rendering_features.conditionalRendering == VK_TRUE;

        case VGPUFeature_DrawIndirectFirstInstance:
            return renderer->features2.features.drawIndirectFirstInstance == VK_TRUE;

        case VGPUFeature_ShaderOutputViewportIndex:
            return renderer->features_1_2.shaderOutputLayer == VK_TRUE && renderer->features_1_2.shaderOutputViewportIndex == VK_TRUE;

        case VGPUFeature_SamplerMinMax:
            return renderer->features_1_2.samplerFilterMinmax == VK_TRUE;

        case VGPUFeature_MeshShader:
            return renderer->mesh_shader_features.meshShader == VK_TRUE && renderer->mesh_shader_features.taskShader == VK_TRUE;

        case VGPUFeature_RayTracing:
            if (renderer->raytracing_features.rayTracingPipeline == VK_TRUE &&
                renderer->raytracing_query_features.rayQuery == VK_TRUE &&
                renderer->acceleration_structure_features.accelerationStructure == VK_TRUE &&
                renderer->features_1_2.bufferDeviceAddress == VK_TRUE)
            {
                if (infoSize == sizeof(uint32_t))
                {
                    auto* pShaderGroupHandleSize = reinterpret_cast<uint32_t*>(pInfo);
                    *pShaderGroupHandleSize = renderer->raytracing_properties.shaderGroupHandleSize;
                }

                return true;
            }

            return false;

        default:
            return false;
    }
}

static void vulkan_getAdapterProperties(VGPURenderer* driverData, VGPUAdapterProperties* properties)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;

    properties->vendorID = renderer->properties2.properties.vendorID;
    properties->deviceID = renderer->properties2.properties.deviceID;
    properties->name = renderer->properties2.properties.deviceName;
    properties->driverDescription = renderer->driverDescription.c_str();

    switch (renderer->properties2.properties.deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            properties->adapterType = VGPUAdapterType_IntegratedGPU;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            properties->adapterType = VGPUAdapterType_DiscreteGPU;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            properties->adapterType = VGPUAdapterType_VirtualGPU;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            properties->adapterType = VGPUAdapterType_CPU;
            break;
        default:
            properties->adapterType = VGPUAdapterType_Other;
            break;
    }
}

static void vulkan_getLimits(VGPURenderer* driverData, VGPULimits* limits)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;

#define SET_LIMIT_FROM_VULKAN(vulkanName, name) limits->name = renderer->properties2.properties.limits.vulkanName
    SET_LIMIT_FROM_VULKAN(maxImageDimension1D, maxTextureDimension1D);
    SET_LIMIT_FROM_VULKAN(maxImageDimension2D, maxTextureDimension2D);
    SET_LIMIT_FROM_VULKAN(maxImageDimension3D, maxTextureDimension3D);
    SET_LIMIT_FROM_VULKAN(maxImageDimensionCube, maxTextureDimensionCube);
    SET_LIMIT_FROM_VULKAN(maxImageArrayLayers, maxTextureArrayLayers);
    //SET_LIMIT_FROM_VULKAN(maxBoundDescriptorSets, maxBindGroups);
    //SET_LIMIT_FROM_VULKAN(maxDescriptorSetUniformBuffersDynamic, maxDynamicUniformBuffersPerPipelineLayout);
    //SET_LIMIT_FROM_VULKAN(maxDescriptorSetStorageBuffersDynamic, maxDynamicStorageBuffersPerPipelineLayout);
    //SET_LIMIT_FROM_VULKAN(maxPerStageDescriptorSampledImages, maxSampledTexturesPerShaderStage);
    //SET_LIMIT_FROM_VULKAN(maxPerStageDescriptorSamplers, maxSamplersPerShaderStage);
    //SET_LIMIT_FROM_VULKAN(maxPerStageDescriptorStorageBuffers, maxStorageBuffersPerShaderStage);
    //SET_LIMIT_FROM_VULKAN(maxPerStageDescriptorStorageImages, maxStorageTexturesPerShaderStage);
    //SET_LIMIT_FROM_VULKAN(maxPerStageDescriptorUniformBuffers, maxUniformBuffersPerShaderStage);

    limits->maxUniformBufferBindingSize = renderer->properties2.properties.limits.maxUniformBufferRange;
    limits->maxStorageBufferBindingSize = renderer->properties2.properties.limits.maxStorageBufferRange;
    limits->minUniformBufferOffsetAlignment = (uint32_t)renderer->properties2.properties.limits.minUniformBufferOffsetAlignment;
    limits->minStorageBufferOffsetAlignment = (uint32_t)renderer->properties2.properties.limits.minStorageBufferOffsetAlignment;

    limits->maxVertexBuffers = renderer->properties2.properties.limits.maxVertexInputBindings;
    limits->maxVertexAttributes = renderer->properties2.properties.limits.maxVertexInputAttributes;

    limits->maxVertexBufferArrayStride = _VGPU_MIN(
        renderer->properties2.properties.limits.maxVertexInputBindingStride,
        renderer->properties2.properties.limits.maxVertexInputAttributeOffset + 1);

    SET_LIMIT_FROM_VULKAN(maxComputeSharedMemorySize, maxComputeWorkgroupStorageSize);
    SET_LIMIT_FROM_VULKAN(maxComputeWorkGroupInvocations, maxComputeInvocationsPerWorkGroup);
    SET_LIMIT_FROM_VULKAN(maxComputeWorkGroupSize[0], maxComputeWorkGroupSizeX);
    SET_LIMIT_FROM_VULKAN(maxComputeWorkGroupSize[1], maxComputeWorkGroupSizeY);
    SET_LIMIT_FROM_VULKAN(maxComputeWorkGroupSize[2], maxComputeWorkGroupSizeZ);
    SET_LIMIT_FROM_VULKAN(maxComputeWorkGroupSize[2], maxComputeWorkGroupsPerDimension);

    limits->maxComputeWorkGroupsPerDimension = _VGPU_MIN(
        _VGPU_MIN(
            renderer->properties2.properties.limits.maxComputeWorkGroupCount[0],
            renderer->properties2.properties.limits.maxComputeWorkGroupCount[1]),
        renderer->properties2.properties.limits.maxComputeWorkGroupCount[2]
    );

    limits->maxViewports = renderer->properties2.properties.limits.maxViewports;
    limits->maxViewportDimensions[0] = renderer->properties2.properties.limits.maxViewportDimensions[0];
    limits->maxViewportDimensions[1] = renderer->properties2.properties.limits.maxViewportDimensions[1];
    limits->maxColorAttachments = renderer->properties2.properties.limits.maxColorAttachments;

#undef SET_LIMIT_FROM_VULKAN
}

static void vulkan_setLabel(VGPURenderer* driverData, const char* label)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    vulkan_SetObjectName(renderer, VK_OBJECT_TYPE_DEVICE, (uint64_t)renderer->device, label);
}

/* Buffer */
static vgpu_buffer* vulkan_createBuffer(VGPURenderer* driverData, const vgpu_buffer_desc* desc, const void* pInitialData)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;

    if (desc->handle)
    {
        VulkanBuffer* buffer = new VulkanBuffer();
        buffer->handle = (VkBuffer)desc->handle;
        buffer->allocation = VK_NULL_HANDLE;
        buffer->allocatedSize = 0u;
        buffer->gpuAddress = 0;

        if (desc->label)
        {
            vulkan_SetObjectName(renderer, VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer->handle, desc->label);
        }

        return (vgpu_buffer*)buffer;
    }

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc->size;
    bufferInfo.usage = 0;

    if (desc->usage & VGPU_BUFFER_USAGE_VERTEX)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (desc->usage & VGPU_BUFFER_USAGE_INDEX)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }

    if (desc->usage & VGPU_BUFFER_USAGE_CONSTANT)
    {
        bufferInfo.size = VmaAlignUp(bufferInfo.size, renderer->properties2.properties.limits.minUniformBufferOffsetAlignment);
        bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    if (desc->usage & VGPU_BUFFER_USAGE_SHADER_READ)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    }

    if (desc->usage & VGPU_BUFFER_USAGE_SHADER_WRITE)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    }

    if (desc->usage & VGPU_BUFFER_USAGE_INDIRECT)
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
        desc->usage & (VGPU_BUFFER_USAGE_VERTEX | VGPU_BUFFER_USAGE_INDEX | VGPU_BUFFER_USAGE_RAY_TRACING))
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
    if (desc->access == VGPU_CPU_ACCESS_READ)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        memoryInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    else if (desc->access == VGPU_CPU_ACCESS_WRITE)
    {
        bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        memoryInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VmaAllocationInfo allocationInfo{};
    VulkanBuffer* buffer = new VulkanBuffer();
    VkResult result = vmaCreateBuffer(renderer->allocator, &bufferInfo, &memoryInfo,
        &buffer->handle,
        &buffer->allocation,
        &allocationInfo);

    if (result != VK_SUCCESS)
    {
        VK_LOG_ERROR(result, "Failed to create buffer.");
        return nullptr;
    }

    buffer->size = desc->size;

    if (desc->label)
    {
        vulkan_SetObjectName(renderer, VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer->handle, desc->label);
    }

    if (memoryInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)
    {
        buffer->pMappedData = allocationInfo.pMappedData;
    }

    if (bufferInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = buffer->handle;
        buffer->gpuAddress = vkGetBufferDeviceAddress(renderer->device, &info);
    }

    // Issue data copy.
    if (pInitialData != nullptr)
    {
        if (buffer->pMappedData != nullptr)
        {
            memcpy(buffer->pMappedData, pInitialData, desc->size);
        }
        else
        {
            auto context = vulkan_AllocateUpload(driverData, desc->size);
            memcpy(context.uploadBufferData, pInitialData, desc->size);

            VkBufferMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.buffer = buffer->handle;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.size = VK_WHOLE_SIZE;

            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            vkCmdPipelineBarrier(
                context.commandBuffer,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                1, &barrier,
                0, nullptr
            );

            VkBufferCopy copyRegion = {};
            copyRegion.size = buffer->size;
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;

            vkCmdCopyBuffer(
                context.commandBuffer,
                ((VulkanBuffer*)context.uploadBuffer)->handle,
                buffer->handle,
                1,
                &copyRegion
            );

            std::swap(barrier.srcAccessMask, barrier.dstAccessMask);

            if (desc->usage & VGPU_BUFFER_USAGE_VERTEX)
            {
                barrier.dstAccessMask |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            }
            if (desc->usage & VGPU_BUFFER_USAGE_INDEX)
            {
                barrier.dstAccessMask |= VK_ACCESS_INDEX_READ_BIT;
            }
            if (desc->usage & VGPU_BUFFER_USAGE_CONSTANT)
            {
                barrier.dstAccessMask |= VK_ACCESS_UNIFORM_READ_BIT;
            }
            if (desc->usage & VGPU_BUFFER_USAGE_SHADER_READ)
            {
                barrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
            }
            if (desc->usage & VGPU_BUFFER_USAGE_SHADER_WRITE)
            {
                barrier.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
            }
            if (desc->usage & VGPU_BUFFER_USAGE_INDIRECT)
            {
                barrier.dstAccessMask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
            }
            if (desc->usage & VGPU_BUFFER_USAGE_RAY_TRACING)
            {
                barrier.dstAccessMask |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
            }

            vkCmdPipelineBarrier(
                context.commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                0, nullptr,
                1, &barrier,
                0, nullptr
            );

            vulkan_UploadSubmit(renderer, context);
        }
    }

    return (vgpu_buffer*)buffer;
}

static void vulkan_destroyBuffer(VGPURenderer* driverData, vgpu_buffer* resource)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    VulkanBuffer* buffer = (VulkanBuffer*)resource;

    renderer->destroyMutex.lock();
    if (buffer->allocation)
    {
        renderer->destroyedBuffers.push_back(std::make_pair(std::make_pair(buffer->handle, buffer->allocation), renderer->frameCount));
    }
    renderer->destroyMutex.unlock();

    delete buffer;
}

static VGPUDeviceAddress vulkan_buffer_get_device_address(vgpu_buffer* resource)
{
    VulkanBuffer* buffer = (VulkanBuffer*)resource;
    return buffer->gpuAddress;
}

static void vulkan_buffer_set_label(VGPURenderer* driverData, vgpu_buffer* resource, const char* label)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    VulkanBuffer* buffer = (VulkanBuffer*)resource;

    vulkan_SetObjectName(renderer, VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer->handle, label);
}

/* Texture */
static VGPUTexture vulkan_createTexture(VGPURenderer* driverData, const VGPUTextureDesc* desc, const void* pInitialData)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;

    VmaAllocationCreateInfo memoryInfo = {};
    memoryInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = nullptr;
    imageInfo.flags = 0;
    imageInfo.format = ToVkFormat(desc->format);
    imageInfo.extent.width = desc->size.width;
    imageInfo.extent.height = 1;
    imageInfo.extent.depth = 1;

    if (desc->type == VGPUTexture_Type1D)
    {
        imageInfo.imageType = VK_IMAGE_TYPE_1D;
        imageInfo.arrayLayers = desc->size.depthOrArrayLayers;
    }
    else if (desc->type == VGPUTexture_Type3D)
    {
        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        imageInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
        imageInfo.extent.height = desc->size.height;
        imageInfo.extent.depth = desc->size.depthOrArrayLayers;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    }
    else
    {
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.height = desc->size.height;
        imageInfo.extent.depth = 1;
        imageInfo.arrayLayers = desc->size.depthOrArrayLayers;
        imageInfo.samples = (VkSampleCountFlagBits)desc->sampleCount;

        if (desc->type == VGPUTexture_Type2D &&
            desc->size.width == desc->size.height &&
            desc->size.depthOrArrayLayers >= 6)
        {
            imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }
    }
    imageInfo.mipLevels = desc->mipLevelCount;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (desc->usage & VGPUTextureUsage_ShaderRead)
    {
        imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    if (desc->usage & VGPUTextureUsage_ShaderWrite)
    {
        imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    if (desc->usage & VGPUTextureUsage_RenderTarget)
    {
        if (vgpuIsDepthStencilFormat(desc->format))
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

    VulkanTexture* texture = new VulkanTexture();
    texture->width = imageInfo.extent.width;
    texture->height = imageInfo.extent.height;
    texture->format = imageInfo.format;
    texture->viewCache.clear();

    VkResult result = vmaCreateImage(renderer->allocator,
        &imageInfo, &memoryInfo,
        &texture->handle,
        &texture->allocation,
        nullptr);

    if (result != VK_SUCCESS)
    {
        vgpuLogError("Vulkan: Failed to create texture");
        delete texture;
        return nullptr;
    }

    // Barrier
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = texture->handle;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    barrier.subresourceRange.aspectMask = GetImageAspectFlags(imageInfo.format);
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = imageInfo.arrayLayers;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = imageInfo.mipLevels;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    renderer->initLocker.lock();
    vkCmdPipelineBarrier(
        renderer->GetFrameResources().initCommandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    renderer->submitInits = true;
    renderer->initLocker.unlock();

    return (VGPUTexture)texture;
}

static void vulkan_destroyTexture(VGPURenderer* driverData, VGPUTexture texture)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    VulkanTexture* vulkanTexture = (VulkanTexture*)texture;

    renderer->destroyMutex.lock();
    for (auto& it : vulkanTexture->viewCache)
    {
        renderer->destroyedImageViews.push_back(std::make_pair(it.second, renderer->frameCount));
    }
    vulkanTexture->viewCache.clear();
    if (vulkanTexture->allocation)
    {
        renderer->destroyedImages.push_back(std::make_pair(std::make_pair(vulkanTexture->handle, vulkanTexture->allocation), renderer->frameCount));
    }
    renderer->destroyMutex.unlock();

    delete vulkanTexture;
}

/* Sampler */
static VGPUSampler* vulkan_createSampler(VGPURenderer* driverData, const VGPUSamplerDesc* desc)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    VulkanSampler* sampler = new VulkanSampler();

    VkSamplerCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter = ToVkFilter(desc->magFilter);
    createInfo.minFilter = ToVkFilter(desc->minFilter);
    createInfo.mipmapMode = ToVkMipmapMode(desc->mipFilter);
    createInfo.addressModeU = ToVkSamplerAddressMode(desc->addressU);
    createInfo.addressModeV = ToVkSamplerAddressMode(desc->addressV);
    createInfo.addressModeW = ToVkSamplerAddressMode(desc->addressW);
    createInfo.mipLodBias = desc->mipLodBias;
    // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkSamplerCreateInfo.html
    if (desc->maxAnisotropy > 1)
    {
        createInfo.anisotropyEnable = VK_TRUE;
        createInfo.maxAnisotropy = _VGPU_MIN(float(desc->maxAnisotropy), renderer->properties2.properties.limits.maxSamplerAnisotropy);
    }
    else
    {
        createInfo.anisotropyEnable = VK_FALSE;
        createInfo.maxAnisotropy = 1;
    }

    if (desc->compareFunction != VGPUCompareFunction_Never)
    {
        createInfo.compareOp = ToVkCompareOp(desc->compareFunction);
        createInfo.compareEnable = VK_TRUE;
    }
    else
    {
        createInfo.compareOp = VK_COMPARE_OP_NEVER;
        createInfo.compareEnable = VK_FALSE;
    }

    createInfo.minLod = desc->lodMinClamp;
    createInfo.maxLod = desc->lodMaxClamp;
    createInfo.borderColor = ToVkBorderColor(desc->borderColor);
    createInfo.unnormalizedCoordinates = VK_FALSE;

    VkResult result = vkCreateSampler(renderer->device, &createInfo, nullptr, &sampler->handle);

    if (result != VK_SUCCESS)
    {
        VK_LOG_ERROR(result, "Failed to create sampler.");
        delete sampler;
        return nullptr;
    }

    if (desc->label)
    {
        vulkan_SetObjectName(renderer, VK_OBJECT_TYPE_SAMPLER, (uint64_t)sampler->handle, desc->label);
    }

    return (VGPUSampler*)sampler;
}

static void vulkan_destroySampler(VGPURenderer* driverData, VGPUSampler* resource)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    VulkanSampler* sampler = (VulkanSampler*)resource;

    renderer->destroyMutex.lock();
    renderer->destroyedSamplers.push_back(std::make_pair(sampler->handle, renderer->frameCount));
    renderer->destroyMutex.unlock();
    delete sampler;
}

/* ShaderModule */
static VGPUShaderModule vulkan_createShaderModule(VGPURenderer* driverData, const void* pCode, size_t codeSize)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = (const uint32_t*)pCode;

    VkShaderModule handle;
    VkResult result = vkCreateShaderModule(renderer->device, &createInfo, nullptr, &handle);
    if (result != VK_SUCCESS)
    {
        return nullptr;
    }

    VulkanShader* shader = VGPU_ALLOC(VulkanShader);
    shader->handle = handle;
    return (VGPUShaderModule)shader;
}

static void vulkan_destroyShaderModule(VGPURenderer* driverData, VGPUShaderModule resource)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    VulkanShader* shader = (VulkanShader*)resource;

    renderer->destroyMutex.lock();
    renderer->destroyedShaderModules.push_back(std::make_pair(shader->handle, renderer->frameCount));
    renderer->destroyMutex.unlock();

    delete shader;
}

/* Pipeline */
static VGPUPipeline* vulkan_createRenderPipeline(VGPURenderer* driverData, const VGPURenderPipelineDesc* desc)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;

    VulkanShader* vertexShader = (VulkanShader*)desc->vertex;
    VulkanShader* fragmentShader = (VulkanShader*)desc->fragment;

    //VkPipelineRenderingCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };
    //pipeline_create.pNext = VK_NULL_HANDLE;
    //pipeline_create.colorAttachmentCount = 1;
    //pipeline_create.pColorAttachmentFormats = &color_rendering_format;
    //pipeline_create.depthAttachmentFormat = depth_format;
    //pipeline_create.stencilAttachmentFormat = depth_format;

    // Stage info
    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShader->handle;
    shaderStages[0].pName = "vertexMain";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShader->handle;
    shaderStages[1].pName = "fragmentMain";

    // VertexInputState
    VkVertexInputBindingDescription vertexInputBinding = {};
    vertexInputBinding.binding = 0;
    vertexInputBinding.stride = 28;
    vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexInputAttributs[2] = {};
    // Attribute location 0: Position
    vertexInputAttributs[0].binding = 0;
    vertexInputAttributs[0].location = 0;
    vertexInputAttributs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributs[0].offset = 0;
    // Attribute location 1: Color
    vertexInputAttributs[1].binding = 0;
    vertexInputAttributs[1].location = 1;
    vertexInputAttributs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertexInputAttributs[1].offset = 12;

    VkPipelineVertexInputStateCreateInfo vertexInputState = {};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
    vertexInputState.vertexAttributeDescriptionCount = 2;
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributs;

    // InputAssemblyState
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // ViewportState
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // RasterizationState
    VkPipelineRasterizationStateCreateInfo rasterizationState = {};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.lineWidth = 1.0f;

    // Multi sampling state
    VkPipelineMultisampleStateCreateInfo multisampleState = {};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.pSampleMask = nullptr;

    // DepthStencilState
    VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.front = depthStencilState.back;

    // Color blend state
    VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
    blendAttachmentState[0].colorWriteMask = 0xf;
    blendAttachmentState[0].blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = blendAttachmentState;

    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};

    VkGraphicsPipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.stageCount = 2u;
    createInfo.pStages = shaderStages;
    createInfo.pVertexInputState = &vertexInputState;
    createInfo.pInputAssemblyState = &inputAssemblyState;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewportState;
    createInfo.pRasterizationState = &rasterizationState;
    createInfo.pMultisampleState = &multisampleState;
    createInfo.pDepthStencilState = &depthStencilState;
    createInfo.pColorBlendState = &colorBlendState;
    createInfo.pDynamicState = &renderer->dynamicStateInfo;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0u;
    VkResult result = vkCreatePipelineLayout(renderer->device, &pipelineLayoutInfo, nullptr, &createInfo.layout);
    VK_CHECK(result);

    if (renderer->dynamicRenderingFeatures.dynamicRendering == VK_TRUE)
    {
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

        VkFormat colorAttachmentFormats[VGPU_MAX_COLOR_ATTACHMENTS];
        for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i)
        {
            VGPU_ASSERT(desc->colorAttachments[i].format != VGPUTextureFormat_Undefined);

            colorAttachmentFormats[pipelineRenderingCreateInfo.colorAttachmentCount] = ToVkFormat(desc->colorAttachments[i].format);
            pipelineRenderingCreateInfo.colorAttachmentCount++;
        }
        pipelineRenderingCreateInfo.pColorAttachmentFormats = colorAttachmentFormats;
        pipelineRenderingCreateInfo.depthAttachmentFormat = ToVkFormat(desc->depthAttachmentFormat);
        pipelineRenderingCreateInfo.stencilAttachmentFormat = ToVkFormat(desc->stencilAttachmentFormat);

        // Chain into the pipeline create info
        createInfo.pNext = &pipelineRenderingCreateInfo;
        createInfo.renderPass = VK_NULL_HANDLE;
    }
    else
    {
        VulkanRenderPassKey renderPassKey{};
        VulkanRenderPassAttachment colorAttachments[VGPU_MAX_COLOR_ATTACHMENTS] = {};
        VulkanRenderPassAttachment depthAttachment{};
        VulkanRenderPassAttachment stencilAttachment{};

        for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i)
        {
            VGPU_ASSERT(desc->colorAttachments[i].format != VGPUTextureFormat_Undefined);

            colorAttachments[renderPassKey.colorAttachmentCount].format = ToVkFormat(desc->colorAttachments[i].format);
            colorAttachments[renderPassKey.colorAttachmentCount].loadAction = VGPULoadAction_DontCare;
            colorAttachments[renderPassKey.colorAttachmentCount].storeAction = VGPUStoreOp_Store;
            renderPassKey.colorAttachmentCount++;
        }

        renderPassKey.colorAttachments = colorAttachments;

        if (desc->depthAttachmentFormat != VGPUTextureFormat_Undefined)
        {
            depthAttachment.format = ToVkFormat(desc->depthAttachmentFormat);
            depthAttachment.loadAction = VGPULoadAction_Load;
            depthAttachment.storeAction = VGPUStoreOp_DontCare;
            renderPassKey.depthAttachment = &depthAttachment;

            //if (IsStencilFormat(desc.depthStencilFormat))
            //{
            //    stencilAttachment.format = desc.depthStencilFormat;
            //    stencilAttachment.loadAction = VGPULoadAction_Load;
            //    stencilAttachment.storeAction = VGPUStoreOp_DontCare;
            //
            //    renderPassKey.stencilAttachment = &stencilAttachment;
            //}
        }

        VkRenderPass renderPass = vulkan_RequestRenderPass(renderer, &renderPassKey);
        createInfo.renderPass = renderPass;
    }

    VkPipeline handle = VK_NULL_HANDLE;
    result = vkCreateGraphicsPipelines(renderer->device, renderer->pipelineCache, 1, &createInfo, nullptr, &handle);
    if (result != VK_SUCCESS)
    {
        return nullptr;
    }

    VulkanPipeline* pipeline = VGPU_ALLOC_CLEAR(VulkanPipeline);
    pipeline->pipelineLayout = createInfo.layout;
    pipeline->handle = handle;
    return (VGPUPipeline*)pipeline;
}

static VGPUPipeline* vulkan_createComputePipeline(VGPURenderer* driverData, const VGPUComputePipelineDesc* desc)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    VulkanShader* shader = (VulkanShader*)desc->shader;

    VkComputePipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    createInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfo.stage.module = shader->handle;
    createInfo.stage.pName = "main";
    createInfo.layout = VK_NULL_HANDLE;

    VkPipeline handle;
    VkResult result = vkCreateComputePipelines(renderer->device,
        VK_NULL_HANDLE,
        1, &createInfo,
        nullptr,
        &handle);

    if (result != VK_SUCCESS)
    {
        return nullptr;
    }

    VulkanPipeline* pipeline = VGPU_ALLOC_CLEAR(VulkanPipeline);
    pipeline->handle = handle;
    return (VGPUPipeline*)pipeline;
}

static VGPUPipeline* vulkan_createRayTracingPipeline(VGPURenderer* driverData, const VGPURayTracingPipelineDesc* desc)
{
    VulkanPipeline* pipeline = VGPU_ALLOC_CLEAR(VulkanPipeline);
    return (VGPUPipeline*)pipeline;
}

static void vulkan_destroyPipeline(VGPURenderer* driverData, VGPUPipeline* resource)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    VulkanPipeline* pipeline = (VulkanPipeline*)resource;

    renderer->destroyMutex.lock();
    renderer->destroyedPipelineLayouts.push_back(std::make_pair(pipeline->pipelineLayout, renderer->frameCount));
    renderer->destroyedPipelines.push_back(std::make_pair(pipeline->handle, renderer->frameCount));
    renderer->destroyMutex.unlock();

    VGPU_FREE(pipeline);
}

/* SwapChain */
static void vulkan_updateSwapChain(VulkanRenderer* renderer, VulkanSwapChain* swapChain)
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
    surfaceFormat.format = ToVkFormat(swapChain->colorFormat);
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
        swapChain->extent = caps.currentExtent;
    }
    else
    {
        swapChain->extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, swapChain->extent.width));
        swapChain->extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, swapChain->extent.height));
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
    createInfo.imageExtent.width = swapChain->extent.width;
    createInfo.imageExtent.height = swapChain->extent.height;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Enable transfer source on swap chain images if supported
    if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    // Enable transfer destination on swap chain images if supported
    if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

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
        for (size_t i = 0, count = swapChain->backbufferTextures.size(); i < count; ++i)
        {
            vulkan_destroyTexture((VGPURenderer*)renderer, (VGPUTexture)swapChain->backbufferTextures[i]);
        }
        swapChain->backbufferTextures.clear();

        vkDestroySwapchainKHR(renderer->device, createInfo.oldSwapchain, nullptr);
    }

    VK_CHECK(vkGetSwapchainImagesKHR(renderer->device, swapChain->handle, &imageCount, nullptr));
    std::vector<VkImage> swapchainImages(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(renderer->device, swapChain->handle, &imageCount, swapchainImages.data()));

    swapChain->imageIndex = 0;
    swapChain->backbufferTextures.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VulkanTexture* texture = new VulkanTexture();
        texture->handle = swapchainImages[i];
        texture->width = createInfo.imageExtent.width;
        texture->height = createInfo.imageExtent.height;
        texture->format = createInfo.imageFormat;
        swapChain->backbufferTextures[i] = texture;
    }

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (swapChain->acquireSemaphore == VK_NULL_HANDLE)
    {
        result = vkCreateSemaphore(renderer->device, &semaphoreInfo, nullptr, &swapChain->acquireSemaphore);
        VGPU_ASSERT(result == VK_SUCCESS);
    }

    if (swapChain->releaseSemaphore == VK_NULL_HANDLE)
    {
        result = vkCreateSemaphore(renderer->device, &semaphoreInfo, nullptr, &swapChain->releaseSemaphore);
        VGPU_ASSERT(result == VK_SUCCESS);
    }

    if (createInfo.imageFormat == VK_FORMAT_B8G8R8A8_UNORM)
    {
        swapChain->colorFormat = VGPUTextureFormat_BGRA8Unorm;
    }
    else if (createInfo.imageFormat == VK_FORMAT_R8G8B8A8_SRGB)
    {
        swapChain->colorFormat = VGPUTextureFormat_BGRA8UnormSrgb;
    }

    swapChain->extent = createInfo.imageExtent;
}

static VGPUSwapChain* vulkan_createSwapChain(VGPURenderer* driverData, void* windowHandle, const VGPUSwapChainDesc* desc)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
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

    VulkanSwapChain* swapChain = new VulkanSwapChain();
    swapChain->surface = vk_surface;
    swapChain->extent.width = desc->width;
    swapChain->extent.height = desc->height;
    swapChain->colorFormat = desc->format;
    swapChain->vsync = desc->presentMode == VGPUPresentMode_Fifo;
    vulkan_updateSwapChain(renderer, swapChain);

    return (VGPUSwapChain*)swapChain;
}

static void vulkan_destroySwapChain(VGPURenderer* driverData, VGPUSwapChain* swapChain)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    VulkanSwapChain* vulkanSwapChain = (VulkanSwapChain*)swapChain;
    //vulkan_destroyTexture(nullptr, d3dSwapChain->backbufferTexture);

    for (size_t i = 0, count = vulkanSwapChain->backbufferTextures.size(); i < count; ++i)
    {
        vulkan_destroyTexture(driverData, (VGPUTexture)vulkanSwapChain->backbufferTextures[i]);
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

static VGPUTextureFormat vulkan_getSwapChainFormat(VGPURenderer*, VGPUSwapChain* swapChain)
{
    VulkanSwapChain* vulkanSwapChain = (VulkanSwapChain*)swapChain;
    return vulkanSwapChain->colorFormat;
}

/* Commands */
static void vulkan_insertImageMemoryBarrier(
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

static void vulkan_pushDebugGroup(VGPUCommandBufferImpl* driverData, const char* groupLabel)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;

    if (!commandBuffer->renderer->debugUtils)
        return;

    VkDebugUtilsLabelEXT label;
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pNext = nullptr;
    label.pLabelName = groupLabel;
    label.color[0] = 0.0f;
    label.color[1] = 0.0f;
    label.color[2] = 0.0f;
    label.color[3] = 1.0f;
    vkCmdBeginDebugUtilsLabelEXT(commandBuffer->handle, &label);
}

static void vulkan_popDebugGroup(VGPUCommandBufferImpl* driverData)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;

    if (!commandBuffer->renderer->debugUtils)
        return;

    vkCmdEndDebugUtilsLabelEXT(commandBuffer->handle);
}

static void vulkan_insertDebugMarker(VGPUCommandBufferImpl* driverData, const char* groupLabel)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;

    if (!commandBuffer->renderer->debugUtils)
        return;

    VkDebugUtilsLabelEXT label;
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pNext = nullptr;
    label.pLabelName = groupLabel;
    label.color[0] = 0.0f;
    label.color[1] = 0.0f;
    label.color[2] = 0.0f;
    label.color[3] = 1.0f;
    vkCmdInsertDebugUtilsLabelEXT(commandBuffer->handle, &label);
}

static void vulkan_setPipeline(VGPUCommandBufferImpl* driverData, VGPUPipeline* pipeline)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;
    VulkanPipeline* vulkanPipeline = (VulkanPipeline*)pipeline;
    vkCmdBindPipeline(commandBuffer->handle, vulkanPipeline->bindPoint, vulkanPipeline->handle);
}

static void vulkan_prepareDispatch(VulkanCommandBuffer* commandBuffer)
{
    VGPU_ASSERT(commandBuffer->insideRenderPass);
}

static void vulkan_dispatch(VGPUCommandBufferImpl* driverData, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;
    vulkan_prepareDispatch(commandBuffer);
    vkCmdDispatch(commandBuffer->handle, groupCountX, groupCountY, groupCountZ);
}

static void vulkan_dispatchIndirect(VGPUCommandBufferImpl* driverData, vgpu_buffer* buffer, uint64_t offset)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;
    vulkan_prepareDispatch(commandBuffer);

    VulkanBuffer* vulkanBuffer = (VulkanBuffer*)buffer;
    vkCmdDispatchIndirect(commandBuffer->handle, vulkanBuffer->handle, offset);
}

static VGPUTexture vulkan_acquireSwapchainTexture(VGPUCommandBufferImpl* driverData, VGPUSwapChain* swapChain, uint32_t* pWidth, uint32_t* pHeight)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;
    VulkanSwapChain* vulkanSwapChain = (VulkanSwapChain*)swapChain;

    // Check if window is minimized
    VkSurfaceCapabilitiesKHR surfaceProperties;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        commandBuffer->renderer->physicalDevice,
        vulkanSwapChain->surface,
        &surfaceProperties));

    if (surfaceProperties.currentExtent.width == 0 ||
        surfaceProperties.currentExtent.width == 0xFFFFFFFF)
    {
        return nullptr;
    }

    if (vulkanSwapChain->extent.width != surfaceProperties.currentExtent.width ||
        vulkanSwapChain->extent.height != surfaceProperties.currentExtent.height)
    {
        vulkan_waitIdle((VGPURenderer*)commandBuffer->renderer);
        vulkan_updateSwapChain(commandBuffer->renderer, vulkanSwapChain);
    }

    VkResult result = vkAcquireNextImageKHR(commandBuffer->renderer->device,
        vulkanSwapChain->handle,
        UINT64_MAX,
        vulkanSwapChain->acquireSemaphore, VK_NULL_HANDLE,
        &vulkanSwapChain->imageIndex);

    if (result != VK_SUCCESS)
    {
        // Handle outdated error in acquire
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            vulkan_waitIdle((VGPURenderer*)commandBuffer->renderer);
            vulkan_updateSwapChain(commandBuffer->renderer, vulkanSwapChain);
            return vulkan_acquireSwapchainTexture(driverData, swapChain, pWidth, pHeight);
        }
    }

    VulkanTexture* swapchainTexture = vulkanSwapChain->backbufferTextures[vulkanSwapChain->imageIndex];

    // Transition from undefined -> render target
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = VK_REMAINING_MIP_LEVELS;
    range.baseArrayLayer = 0;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;
    vulkan_insertImageMemoryBarrier(
        commandBuffer->handle,
        swapchainTexture->handle,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        range
    );

    commandBuffer->swapChains.push_back(vulkanSwapChain);

    if (pWidth) {
        *pWidth = vulkanSwapChain->extent.width;
    }

    if (pHeight) {
        *pHeight = vulkanSwapChain->extent.height;
    }

    return (VGPUTexture)swapchainTexture;
}

static void vulkan_beginRenderPass(VGPUCommandBufferImpl* driverData, const VGPURenderPassDesc* desc)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;
    uint32_t width = UINT32_MAX;
    uint32_t height = UINT32_MAX;

    if (commandBuffer->renderer->dynamicRenderingFeatures.dynamicRendering == VK_TRUE)
    {
        VkRenderingInfo renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.pNext = VK_NULL_HANDLE;
        renderingInfo.flags = 0u;
        renderingInfo.renderArea.extent = { UINT32_MAX, UINT32_MAX };
        renderingInfo.layerCount = 1;
        renderingInfo.viewMask = 0;

        VkRenderingAttachmentInfo colorAttachments[VGPU_MAX_COLOR_ATTACHMENTS] = {};
        VkRenderingAttachmentInfo depthStencilAttachmentInfo{};

        for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i)
        {
            const VGPURenderPassColorAttachment* attachment = &desc->colorAttachments[i];
            VulkanTexture* texture = (VulkanTexture*)attachment->texture;
            const uint32_t level = attachment->level;
            const uint32_t slice = attachment->slice;

            renderingInfo.renderArea.extent.width = _VGPU_MIN(renderingInfo.renderArea.extent.width, _VGPU_MAX(1U, texture->width >> level));
            renderingInfo.renderArea.extent.height = _VGPU_MIN(renderingInfo.renderArea.extent.height, _VGPU_MAX(1U, texture->height >> level));

            colorAttachments[renderingInfo.colorAttachmentCount].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachments[renderingInfo.colorAttachmentCount].pNext = nullptr;
            colorAttachments[renderingInfo.colorAttachmentCount].imageView = vulkan_GetRTV(commandBuffer->renderer, texture, level, slice);
            colorAttachments[renderingInfo.colorAttachmentCount].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachments[renderingInfo.colorAttachmentCount].resolveMode = VK_RESOLVE_MODE_NONE;
            colorAttachments[renderingInfo.colorAttachmentCount].loadOp = ToVkAttachmentLoadOp(attachment->loadOp);
            colorAttachments[renderingInfo.colorAttachmentCount].storeOp = ToVkAttachmentStoreOp(attachment->storeOp);

            colorAttachments[renderingInfo.colorAttachmentCount].clearValue.color.float32[0] = attachment->clearColor.r;
            colorAttachments[renderingInfo.colorAttachmentCount].clearValue.color.float32[1] = attachment->clearColor.g;
            colorAttachments[renderingInfo.colorAttachmentCount].clearValue.color.float32[2] = attachment->clearColor.b;
            colorAttachments[renderingInfo.colorAttachmentCount].clearValue.color.float32[3] = attachment->clearColor.a;

            renderingInfo.colorAttachmentCount++;
        }
        renderingInfo.pColorAttachments = colorAttachments;

        if (desc->depthStencilAttachment != nullptr)
        {
            const VGPURenderPassDepthStencilAttachment* attachment = desc->depthStencilAttachment;
            VulkanTexture* texture = (VulkanTexture*)attachment->texture;
            const uint32_t level = attachment->level;
            const uint32_t slice = attachment->slice;

            depthStencilAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            depthStencilAttachmentInfo.pNext = VK_NULL_HANDLE;
            depthStencilAttachmentInfo.imageView = vulkan_GetRTV(commandBuffer->renderer, texture, level, slice);
            depthStencilAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthStencilAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
            depthStencilAttachmentInfo.loadOp = ToVkAttachmentLoadOp(attachment->depthLoadOp);
            depthStencilAttachmentInfo.storeOp = ToVkAttachmentStoreOp(attachment->depthStoreOp);
            depthStencilAttachmentInfo.clearValue.depthStencil.depth = attachment->clearDepth;

            // Barrier
            VkImageSubresourceRange depthTextureRange{};
            depthTextureRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            depthTextureRange.baseMipLevel = 0;
            depthTextureRange.levelCount = VK_REMAINING_MIP_LEVELS;
            depthTextureRange.baseArrayLayer = 0;
            depthTextureRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

            vulkan_insertImageMemoryBarrier(
                commandBuffer->handle,
                texture->handle,
                0,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                depthTextureRange
            );

            renderingInfo.pDepthAttachment = &depthStencilAttachmentInfo;
            renderingInfo.pStencilAttachment = &depthStencilAttachmentInfo;
        }

        vkCmdBeginRenderingKHR(commandBuffer->handle, &renderingInfo);
    }
    else
    {
        VulkanRenderPassKey renderPassKey{};
        VulkanRenderPassAttachment colorAttachments[VGPU_MAX_COLOR_ATTACHMENTS] = {};
        VulkanRenderPassAttachment depthAttachment{};
        VulkanRenderPassAttachment stencilAttachment{};

        for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i)
        {
            const VGPURenderPassColorAttachment* attachment = &desc->colorAttachments[i];
            VulkanTexture* texture = (VulkanTexture*)attachment->texture;
            const uint32_t level = attachment->level;

            commandBuffer->clearValues[commandBuffer->clearValueCount].color.float32[0] = attachment->clearColor.r;
            commandBuffer->clearValues[commandBuffer->clearValueCount].color.float32[1] = attachment->clearColor.g;
            commandBuffer->clearValues[commandBuffer->clearValueCount].color.float32[2] = attachment->clearColor.b;
            commandBuffer->clearValues[commandBuffer->clearValueCount].color.float32[3] = attachment->clearColor.a;
            commandBuffer->clearValueCount++;

            width = _VGPU_MIN(width, _VGPU_MAX(1U, texture->width >> level));
            height = _VGPU_MIN(height, _VGPU_MAX(1U, texture->height >> level));

            colorAttachments[renderPassKey.colorAttachmentCount].format = texture->format;
            colorAttachments[renderPassKey.colorAttachmentCount].loadAction = attachment->loadOp;
            colorAttachments[renderPassKey.colorAttachmentCount].storeAction = attachment->storeOp;
            renderPassKey.colorAttachmentCount++;
        }
        renderPassKey.colorAttachments = colorAttachments;

        if (desc->depthStencilAttachment)
        {
            const VGPURenderPassDepthStencilAttachment* attachment = desc->depthStencilAttachment;
            VulkanTexture* texture = (VulkanTexture*)attachment->texture;
            const uint32_t level = attachment->level;

            commandBuffer->clearValues[commandBuffer->clearValueCount].depthStencil.depth = attachment->clearDepth;
            commandBuffer->clearValues[commandBuffer->clearValueCount].depthStencil.stencil = attachment->clearStencil;
            commandBuffer->clearValueCount++;

            width = _VGPU_MIN(width, _VGPU_MAX(1U, texture->width >> level));
            height = _VGPU_MIN(height, _VGPU_MAX(1U, texture->height >> level));

            depthAttachment.format = texture->format;
            depthAttachment.loadAction = attachment->depthLoadOp;
            depthAttachment.storeAction = attachment->depthStoreOp;
            renderPassKey.depthAttachment = &depthAttachment;
        }

        VkRenderPass renderPass = vulkan_RequestRenderPass(commandBuffer->renderer, &renderPassKey);

        VkRenderPassBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.renderPass = renderPass;
        beginInfo.framebuffer = vulkan_RequestFramebuffer(commandBuffer->renderer, renderPass, desc, width, height);
        beginInfo.renderArea.offset.x = 0;
        beginInfo.renderArea.offset.y = 0;
        beginInfo.renderArea.extent.width = width;
        beginInfo.renderArea.extent.height = height;
        beginInfo.clearValueCount = commandBuffer->clearValueCount;
        beginInfo.pClearValues = commandBuffer->clearValues;
        vkCmdBeginRenderPass(commandBuffer->handle, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    // The viewport and scissor default to cover all of the attachments
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = static_cast<float>(height);
    viewport.width = static_cast<float>(width);
    viewport.height = -static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer->handle, 0, 1, &viewport);

    VkRect2D scissorRect;
    scissorRect.offset.x = 0;
    scissorRect.offset.y = 0;
    scissorRect.extent.width = width;
    scissorRect.extent.height = height;
    vkCmdSetScissor(commandBuffer->handle, 0, 1, &scissorRect);

    commandBuffer->insideRenderPass = true;
}

static void vulkan_endRenderPass(VGPUCommandBufferImpl* driverData)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;

    if (commandBuffer->renderer->dynamicRenderingFeatures.dynamicRendering == VK_TRUE)
    {
        vkCmdEndRenderingKHR(commandBuffer->handle);
    }
    else
    {
        vkCmdEndRenderPass(commandBuffer->handle);
    }

    commandBuffer->insideRenderPass = false;
}

static void vulkan_setVertexBuffer(VGPUCommandBufferImpl* driverData, uint32_t index, vgpu_buffer* buffer, uint64_t offset)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;
    VulkanBuffer* vulkanBuffer = (VulkanBuffer*)buffer;
    vkCmdBindVertexBuffers(commandBuffer->handle, index, 1, &vulkanBuffer->handle, &offset);
}

static void vulkan_setIndexBuffer(VGPUCommandBufferImpl* driverData, vgpu_buffer* buffer, uint64_t offset, VGPUIndexType type)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;
    VulkanBuffer* vulkanBuffer = (VulkanBuffer*)buffer;
    const VkIndexType vkIndexType = (type == VGPU_INDEX_TYPE_UINT16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(commandBuffer->handle, vulkanBuffer->handle, offset, vkIndexType);
}

static void vulkan_prepareDraw(VulkanCommandBuffer* commandBuffer)
{
    VGPU_ASSERT(commandBuffer->insideRenderPass);
}

static void vulkan_setViewport(VGPUCommandBufferImpl* driverData, const VGPUViewport* viewport)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;

    VkViewport vkViewport;
    vkViewport.x = viewport->x;
    vkViewport.y = viewport->height - viewport->y;
    vkViewport.width = viewport->width;
    vkViewport.height = -viewport->height;
    vkViewport.minDepth = viewport->minDepth;
    vkViewport.maxDepth = viewport->maxDepth;

    vkCmdSetViewport(commandBuffer->handle, 0, 1, &vkViewport);
}

static void vulkan_setViewports(VGPUCommandBufferImpl* driverData, uint32_t count, const VGPUViewport* viewports)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;

    VGPU_ASSERT(count < commandBuffer->renderer->properties2.properties.limits.maxViewports);

    // Flip viewport to match DirectX coordinate system
    VkViewport vkViewports[16];
    for (uint32_t i = 0; i < count; i++)
    {
        const VkViewport& viewport = *(const VkViewport*)&viewports[i];
        VkViewport& vkViewport = vkViewports[i];
        vkViewport = viewport;
        vkViewport.y = viewport.height - viewport.y;
        vkViewport.height = -viewport.height;
    }

    vkCmdSetViewport(commandBuffer->handle, 0, count, vkViewports);
}

static void vulkan_setScissorRect(VGPUCommandBufferImpl* driverData, const VGPURect* rect)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;
    VkRect2D vk_rect = {};
    vk_rect.offset.x = rect->x;
    vk_rect.offset.y = rect->y;
    vk_rect.extent.width = (uint32_t)rect->width;
    vk_rect.extent.height = (uint32_t)rect->height;

    vkCmdSetScissor(commandBuffer->handle, 0u, 1u, &vk_rect);
}

static void vulkan_setScissorRects(VGPUCommandBufferImpl* driverData, uint32_t count, const VGPURect* rects)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;
    VGPU_ASSERT(count < commandBuffer->renderer->properties2.properties.limits.maxViewports);

    vkCmdSetScissor(commandBuffer->handle, 0, count, (const VkRect2D*)rects);
}

static void vulkan_draw(VGPUCommandBufferImpl* driverData, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t baseInstance)
{
    VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)driverData;
    vulkan_prepareDraw(commandBuffer);
    vkCmdDraw(commandBuffer->handle, vertexCount, instanceCount, vertexStart, baseInstance);
}

static VGPUCommandBuffer vulkan_beginCommandBuffer(VGPURenderer* driverData, VGPUCommandQueue queueType, const char* label)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;
    VulkanCommandBuffer* impl = nullptr;

    renderer->cmdBuffersLocker.lock();
    uint32_t cmd_current = renderer->cmdBuffersCount++;
    if (cmd_current >= renderer->commandBuffers.size())
    {
        impl = new VulkanCommandBuffer();
        impl->renderer = renderer;

        for (uint32_t i = 0; i < VGPU_MAX_INFLIGHT_FRAMES; ++i)
        {
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = renderer->graphicsQueueFamily;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            //poolInfo.queueFamilyIndex = renderer->computeQueueFamily;
            VK_CHECK(vkCreateCommandPool(renderer->device, &poolInfo, nullptr, &impl->commandPools[i]));

            VkCommandBufferAllocateInfo commandBufferInfo = {};
            commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferInfo.commandBufferCount = 1;
            commandBufferInfo.commandPool = impl->commandPools[i];
            commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            VK_CHECK(vkAllocateCommandBuffers(renderer->device, &commandBufferInfo, &impl->commandBuffers[i]));
            //binderPools[i].Init(device);
        }

        VGPUCommandBuffer_T* commandBuffer = VGPU_ALLOC_CLEAR(VGPUCommandBuffer_T);
        ASSIGN_COMMAND_BUFFER(vulkan);
        commandBuffer->driverData = (VGPUCommandBufferImpl*)impl;

        renderer->commandBuffers.push_back(commandBuffer);
    }
    else
    {
        impl = (VulkanCommandBuffer*)renderer->commandBuffers.back()->driverData;
    }

    renderer->cmdBuffersLocker.unlock();

    // Begin recording
    VK_CHECK(vkResetCommandPool(renderer->device, impl->commandPools[renderer->frameIndex], 0));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr; // Optional
    VK_CHECK(vkBeginCommandBuffer(impl->commandBuffers[renderer->frameIndex], &beginInfo));

    impl->handle = impl->commandBuffers[renderer->frameIndex];
    impl->hasLabel = false;
    impl->clearValueCount = 0u;
    impl->insideRenderPass = false;
    impl->swapChains.clear();

    //binderPools[frameIndex].Reset();
    //binder.Reset();
    //frameAllocators[frameIndex].Reset();
    //
    //SetStencilReference(0);
    //SetBlendColor(Colors::Transparent);
    //
    //if (device->features2.features.depthBounds == VK_TRUE)
    //{
    //    vkCmdSetDepthBounds(commandBuffer, 0.0f, 1.0f);
    //}

    if (label)
    {
        vulkan_pushDebugGroup((VGPUCommandBufferImpl*)impl, label);
        impl->hasLabel = true;
    }

    return renderer->commandBuffers.back();
}

static void vulkan_submit(VGPURenderer* driverData, VGPUCommandBuffer* commandBuffers, uint32_t count)
{
    VulkanRenderer* renderer = (VulkanRenderer*)driverData;

    // Collect present swapchains
    std::vector<VkSemaphore> waitSemaphores;
    std::vector<VkPipelineStageFlags> waitStages;
    std::vector<uint64_t> submitWaitValues;
    std::vector<VkSemaphore> submitSignalSemaphores;
    std::vector<uint64_t> submitSignalValues;
    std::vector<VulkanSwapChain*> presentSwapChains;
    std::vector<VkSwapchainKHR> presentVkSwapChains;
    std::vector<uint32_t> swapChainImageIndices;

    VkResult result = VK_SUCCESS;
    renderer->initLocker.lock();
    renderer->cmdBuffersCount = 0;

    // Submit current frame.
    {
        auto& frame = renderer->frames[renderer->frameIndex];

        // Transitions first.
        std::vector<VkCommandBuffer> submitCommandBuffers;
        if (renderer->submitInits)
        {
            result = vkEndCommandBuffer(frame.initCommandBuffer);
            VGPU_ASSERT(result == VK_SUCCESS);
            submitCommandBuffers.push_back(frame.initCommandBuffer);
        }

        // Sync up with copyallocator before first submit
        uint64_t copySyncValue = vulkan_UploadFlush(renderer);
        if (copySyncValue > 0) 
        {
            waitStages.push_back(VK_PIPELINE_STAGE_TRANSFER_BIT);
            waitSemaphores.push_back(renderer->uploadSemaphore);
            submitWaitValues.push_back(copySyncValue);
            copySyncValue = 0;
        }

        for (uint32_t i = 0; i < count; i += 1)
        {
            VulkanCommandBuffer* commandBuffer = (VulkanCommandBuffer*)commandBuffers[i]->driverData;

            for (size_t j = 0, count = commandBuffer->swapChains.size(); j < count; ++j)
            {
                VulkanSwapChain* swapChain = commandBuffer->swapChains[j];
                presentSwapChains.push_back(swapChain);

                waitSemaphores.push_back(swapChain->acquireSemaphore);
                waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
                submitWaitValues.push_back(0); // not a timeline semaphore
                submitSignalSemaphores.push_back(swapChain->releaseSemaphore);
                submitSignalValues.push_back(0); // not a timeline semaphore

                presentVkSwapChains.push_back(swapChain->handle);
                swapChainImageIndices.push_back(swapChain->imageIndex);

                VkImageSubresourceRange range{};
                range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                range.baseMipLevel = 0;
                range.levelCount = VK_REMAINING_MIP_LEVELS;
                range.baseArrayLayer = 0;
                range.layerCount = VK_REMAINING_ARRAY_LAYERS;
                vulkan_insertImageMemoryBarrier(
                    commandBuffer->handle,
                    swapChain->backbufferTextures[swapChain->imageIndex]->handle,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    0,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    range);
            }

            if (commandBuffer->hasLabel)
            {
                vulkan_popDebugGroup((VGPUCommandBufferImpl*)commandBuffer);
            }

            result = vkEndCommandBuffer(commandBuffer->handle);
            VGPU_ASSERT(result == VK_SUCCESS);
            submitCommandBuffers.push_back(commandBuffer->handle);
        }

        // Submit now
        VkTimelineSemaphoreSubmitInfo timelineInfo = {};
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfo.pNext = nullptr;
        timelineInfo.waitSemaphoreValueCount = (uint32_t)submitWaitValues.size();
        timelineInfo.pWaitSemaphoreValues = submitWaitValues.data();
        timelineInfo.signalSemaphoreValueCount = (uint32_t)submitSignalValues.size();
        timelineInfo.pSignalSemaphoreValues = submitSignalValues.data();

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = &timelineInfo;
        submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages.data();
        submitInfo.commandBufferCount = (uint32_t)submitCommandBuffers.size();
        submitInfo.pCommandBuffers = submitCommandBuffers.data();
        submitInfo.signalSemaphoreCount = (uint32_t)submitSignalSemaphores.size();
        submitInfo.pSignalSemaphores = submitSignalSemaphores.data();

        result = vkQueueSubmit(renderer->graphicsQueue, 1, &submitInfo, frame.fence);
        VGPU_ASSERT(result == VK_SUCCESS);

        if (!presentSwapChains.empty())
        {
            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = (uint32_t)submitSignalSemaphores.size();
            presentInfo.pWaitSemaphores = submitSignalSemaphores.data();
            presentInfo.swapchainCount = (uint32_t)presentSwapChains.size();
            presentInfo.pSwapchains = presentVkSwapChains.data();
            presentInfo.pImageIndices = swapChainImageIndices.data();
            result = vkQueuePresentKHR(renderer->graphicsQueue, &presentInfo);
            if (result != VK_SUCCESS)
            {
                // Handle outdated error in present:
                if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    for (size_t i = 0, count = presentSwapChains.size(); i < count; ++i)
                    {
                        VulkanSwapChain* vulkanSwapChain = presentSwapChains[i];
                        vulkan_updateSwapChain(renderer, vulkanSwapChain);
                    }
                }
                else
                {
                    VGPU_ASSERT(false);
                }
            }
        }
    }

    renderer->submitInits = false;
    renderer->initLocker.unlock();
}

static VGPUBool32 vulkan_isSupported(void)
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
    VulkanRenderer* renderer = new VulkanRenderer();

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

    VkResult result = VK_SUCCESS;
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

        if (info->validationMode != VGPUValidationMode_Disabled)
        {
            // Determine the optimal validation layers to enable that are necessary for useful debugging
            std::vector<const char*> optimalValidationLyers = GetOptimalValidationLayers(availableInstanceLayers);
            instanceLayers.insert(instanceLayers.end(), optimalValidationLyers.begin(), optimalValidationLyers.end());
    }

#if defined(_DEBUG)
        bool validationFeatures = false;
        if (info->validationMode == VGPUValidationMode_GPU)
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

        if (info->validationMode != VGPUValidationMode_Disabled && renderer->debugUtils)
        {
            debugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugUtilsCreateInfo.pNext = nullptr;
            debugUtilsCreateInfo.flags = 0;

            debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            if (info->validationMode == VGPUValidationMode_Verbose)
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

        result = vkCreateInstance(&createInfo, nullptr, &renderer->instance);
        if (result != VK_SUCCESS)
        {
            VK_LOG_ERROR(result, "Failed to create Vulkan instance.");
            delete renderer;
            return nullptr;
        }

        volkLoadInstanceOnly(renderer->instance);

        if (info->validationMode != VGPUValidationMode_Disabled && renderer->debugUtils)
        {
            result = vkCreateDebugUtilsMessengerEXT(renderer->instance, &debugUtilsCreateInfo, nullptr, &renderer->debugUtilsMessenger);
            if (result != VK_SUCCESS)
            {
                VK_LOG_ERROR(result, "Could not create debug utils messenger");
            }
        }

#ifdef _DEBUG
        vgpuLogInfo("Created VkInstance with version: %d.%d.%d",
            VK_VERSION_MAJOR(appInfo.apiVersion),
            VK_VERSION_MINOR(appInfo.apiVersion),
            VK_VERSION_PATCH(appInfo.apiVersion)
        );

        if (createInfo.enabledLayerCount)
        {
            vgpuLogInfo("Enabled %d Validation Layers:", createInfo.enabledLayerCount);

            for (uint32_t i = 0; i < createInfo.enabledLayerCount; ++i)
            {
                vgpuLogInfo("	\t%s", createInfo.ppEnabledLayerNames[i]);
            }
        }

        vgpuLogInfo("Enabled %d Instance Extensions:", createInfo.enabledExtensionCount);
        for (uint32_t i = 0; i < createInfo.enabledExtensionCount; ++i)
        {
            vgpuLogInfo("	\t%s", createInfo.ppEnabledExtensionNames[i]);
        }
#endif
}

    // Enumerate physical device and create logical device.
    {
        uint32_t deviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(renderer->instance, &deviceCount, nullptr));

        if (deviceCount == 0)
        {
            vgpuLogError("Vulkan: Failed to find GPUs with Vulkan support");
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
                VGPU_ASSERT(physicalDeviceExt.deferred_host_operations);

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
                VGPU_ASSERT(physicalDeviceExt.renderPass2 == true);
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
            //if (physicalDeviceExt.dynamicRendering)
            //{
            //    enabledExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
            //
            //    renderer->dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
            //    *features_chain = &renderer->dynamicRenderingFeatures;
            //    features_chain = &renderer->dynamicRenderingFeatures.pNext;
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
            vgpuLogError("Vulkan: Failed to find a suitable GPU");
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
            vgpuLogError("Vulkan: Could not find suitable graphics queue.");
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

        result = vkCreateDevice(renderer->physicalDevice, &createInfo, nullptr, &renderer->device);
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
        vgpuLogInfo("Enabled %d Device Extensions:", createInfo.enabledExtensionCount);
        for (uint32_t i = 0; i < createInfo.enabledExtensionCount; ++i)
        {
            vgpuLogInfo("	\t%s", createInfo.ppEnabledExtensionNames[i]);
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

        result = vmaCreateAllocator(&allocatorInfo, &renderer->allocator);

        if (result != VK_SUCCESS)
        {
            VK_LOG_ERROR(result, "Cannot create allocator");
            delete renderer;
            return nullptr;
        }
    }

    // Upload 
    {
        VkSemaphoreTypeCreateInfo timelineCreateInfo = {};
        timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineCreateInfo.pNext = nullptr;
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue = 0;

        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = &timelineCreateInfo;
        createInfo.flags = 0;

        result = vkCreateSemaphore(renderer->device, &createInfo, nullptr, &renderer->uploadSemaphore);
        if (result != VK_SUCCESS)
        {
            VK_LOG_ERROR(result, "Cannot create upload semaphore");
            delete renderer;
            return nullptr;
        }
    }

    // Create frame resources
    for (uint32_t i = 0; i < VGPU_MAX_INFLIGHT_FRAMES; ++i)
    {
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        result = vkCreateFence(renderer->device, &fenceInfo, nullptr, &renderer->frames[i].fence);
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
    }

    // Pipeline cache
    {
        VkPipelineCacheCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

        // Create Vulkan pipeline cache
        result = vkCreatePipelineCache(renderer->device, &createInfo, nullptr, &renderer->pipelineCache);
        if (result != VK_SUCCESS)
        {
            VK_LOG_ERROR(result, "Failed to create pipeline cache");
            delete renderer;
            return nullptr;
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
        result = vmaCreateBuffer(renderer->allocator, &bufferInfo, &bufferAllocInfo, &renderer->nullBuffer, &renderer->nullBufferAllocation, nullptr);
        VGPU_ASSERT(result == VK_SUCCESS);

        VkBufferViewCreateInfo bufferViewInfo = {};
        bufferViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        bufferViewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        bufferViewInfo.range = VK_WHOLE_SIZE;
        bufferViewInfo.buffer = renderer->nullBuffer;
        result = vkCreateBufferView(renderer->device, &bufferViewInfo, nullptr, &renderer->nullBufferView);
        VGPU_ASSERT(result == VK_SUCCESS);

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
        VGPU_ASSERT(result == VK_SUCCESS);

        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imageInfo.arrayLayers = 6;
        result = vmaCreateImage(renderer->allocator, &imageInfo, &allocInfo, &renderer->nullImage2D, &renderer->nullImageAllocation2D, nullptr);
        VGPU_ASSERT(result == VK_SUCCESS);

        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        imageInfo.flags = 0;
        imageInfo.arrayLayers = 1;
        result = vmaCreateImage(renderer->allocator, &imageInfo, &allocInfo, &renderer->nullImage3D, &renderer->nullImageAllocation3D, nullptr);
        VGPU_ASSERT(result == VK_SUCCESS);

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
        VGPU_ASSERT(result == VK_SUCCESS);

        imageViewInfo.image = renderer->nullImage1D;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageView1DArray);
        VGPU_ASSERT(result == VK_SUCCESS);

        imageViewInfo.image = renderer->nullImage2D;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageView2D);
        VGPU_ASSERT(result == VK_SUCCESS);

        imageViewInfo.image = renderer->nullImage2D;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageView2DArray);
        VGPU_ASSERT(result == VK_SUCCESS);

        imageViewInfo.image = renderer->nullImage2D;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        imageViewInfo.subresourceRange.layerCount = 6;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageViewCube);
        VGPU_ASSERT(result == VK_SUCCESS);

        imageViewInfo.image = renderer->nullImage2D;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        imageViewInfo.subresourceRange.layerCount = 6;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageViewCubeArray);
        VGPU_ASSERT(result == VK_SUCCESS);

        imageViewInfo.image = renderer->nullImage3D;
        imageViewInfo.subresourceRange.layerCount = 1;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        result = vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &renderer->nullImageView3D);
        VGPU_ASSERT(result == VK_SUCCESS);

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        result = vkCreateSampler(renderer->device, &samplerInfo, nullptr, &renderer->nullSampler);
        VGPU_ASSERT(result == VK_SUCCESS);
    }

    // Dynamic PSO states:
    renderer->psoDynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    renderer->psoDynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
    renderer->psoDynamicStates.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
    renderer->psoDynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
    //if (CheckCapability(GraphicsDeviceCapability::DEPTH_BOUNDS_TEST))
    //{
    //    renderer->psoDynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
    //}
    //if (CheckCapability(GraphicsDeviceCapability::VARIABLE_RATE_SHADING))
    //{
    //    renderer->psoDynamicStates.push_back(VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR);
    //}

    renderer->dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    renderer->dynamicStateInfo.dynamicStateCount = (uint32_t)renderer->psoDynamicStates.size();
    renderer->dynamicStateInfo.pDynamicStates = renderer->psoDynamicStates.data();


    vgpuLogInfo("VGPU Driver: Vulkan");
    vgpuLogInfo("Vulkan Adapter: %s", renderer->properties2.properties.deviceName);

    VGPUDevice_T* device = VGPU_ALLOC_CLEAR(VGPUDevice_T);
    ASSIGN_DRIVER(vulkan);

    device->driverData = (VGPURenderer*)renderer;
    return device;
}

VGFXDriver Vulkan_Driver = {
    VGPUBackendType_Vulkan,
    vulkan_isSupported,
    vulkan_createDevice
};

uint32_t vgpuToVkFormat(VGPUTextureFormat format)
{
    return ToVkFormat(format);
}

#else

uint32_t vgpuToVkFormat(VGPUTextureFormat format)
{
    _VGPU_UNUSED(format);
    return 0;
}

#endif /* VGPU_VULKAN_DRIVER */
