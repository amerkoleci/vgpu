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

#include "vk/vk.h"
#include "vk/vk_mem_alloc.h"

#define _VK_GPU_MAX_PHYSICAL_DEVICES (32u)
#define VK_CHECK(res) do { VkResult r = (res); VGPU_CHECK(r >= 0, vkGetErrorString(r)); } while (0)

#if !defined(NDEBUG) || defined(DEBUG) || defined(_DEBUG)
#   define VULKAN_DEBUG
#endif

typedef struct vgpu_vk_renderer {
    /* Associated vgpu_device */
    VGPUDevice gpu_device;

    vgpu_features features;
    vgpu_limits limits;
} vgpu_vk_renderer;

static struct {
    bool available_initialized;
    bool available;

    bool apiVersion12;
    bool apiVersion11;
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

static bool vk_create_surface(void* native_handle, VkSurfaceKHR* pSurface, uint32_t* width, uint32_t* height) {
    VkResult result = VK_SUCCESS;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    HWND window = (HWND)native_handle;

    const VkWin32SurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .hinstance = GetModuleHandle(NULL),
        .hwnd = window
    };
    result = vkCreateWin32SurfaceKHR(vk.instance, &surface_info, NULL, pSurface);
    if (result != VK_SUCCESS) {
        vgpu_log_error("Failed to create surface");
        return false;
    }

    RECT rect;
    BOOL success = GetClientRect(window, &rect);
    VGPU_CHECK(success, "GetWindowRect error.");
    *width = rect.right - rect.left;
    *height = rect.bottom - rect.top;
#endif

    return true;
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
        VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, NULL));

        VkExtensionProperties* available_instance_extensions = VGPU_ALLOCA(VkExtensionProperties, instance_extension_count);
        VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, available_instance_extensions));

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
            VK_CHECK(vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr));

            VkLayerProperties* supported_validation_layers = VGPU_ALLOCA(VkLayerProperties, instance_layer_count);
            assert(supported_validation_layers);
            VK_CHECK(vkEnumerateInstanceLayerProperties(&instance_layer_count, supported_validation_layers));

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

        const uint32_t instance_version = agpu_vk_get_instance_version();
        vk.apiVersion12 = instance_version >= VK_API_VERSION_1_2;
        vk.apiVersion11 = instance_version >= VK_API_VERSION_1_1;
        if (vk.apiVersion12) {
            vk.apiVersion11 = true;
        }

        VkInstanceCreateInfo instance_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &(VkApplicationInfo) {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = app_name,
                .applicationVersion = 0,
                .pEngineName = "alimer",
                .engineVersion = 0,
                .apiVersion = instance_version
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

        VkResult result = vkCreateInstance(&instance_info, NULL, &vk.instance);
        if (result != VK_SUCCESS) {
            vgpuDeviceDestroy(device);
            return false;
        }

        agpu_vk_init_instance(vk.instance);

#if defined(VULKAN_DEBUG)
        if (vk.debug_utils)
        {
            result = vkCreateDebugUtilsMessengerEXT(vk.instance, &debug_utils_create_info, NULL, &vk.debug_utils_messenger);
            if (result != VK_SUCCESS)
            {
                vgpu_log_error("Could not create debug utils messenger");
                vgpuDeviceDestroy(device);
                return false;
            }
        }
        else
        {
            result = vkCreateDebugReportCallbackEXT(vk.instance, &debug_report_create_info, NULL, &vk.debug_report_callback);
            if (result != VK_SUCCESS) {
                vgpu_log_error("Could not create debug report callback");
                vgpuDeviceDestroy(device);
                return false;
            }
        }
#endif

        /* Enumerate all physical device. */
        vk.physical_device_count = _VK_GPU_MAX_PHYSICAL_DEVICES;
        result = vkEnumeratePhysicalDevices(vk.instance, &vk.physical_device_count, vk.physical_devices);
        if (result != VK_SUCCESS) {
            vgpu_log_error("Vulkan: Cannot enumerate physical devices.");
            vgpuDeviceDestroy(device);
            return false;
        }
    }

    /* Create surface if required. */
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    uint32_t backbuffer_width;
    uint32_t backbuffer_height;
    if (desc->swapchain != nullptr)
    {
        if (!vk_create_surface(desc->swapchain->native_handle, &surface, &backbuffer_width, &backbuffer_height))
        {
            vgpuDeviceDestroy(device);
            return false;
        }
    }

    return true;
}

static void vk_destroy(VGPUDevice device)
{

}

static VGPUBackendType vk_getBackend(void) {
    return VGPUBackendType_Vulkan;
}

static vgpu_features vk_get_features(vgpu_renderer* driver_data)
{
    vgpu_vk_renderer* renderer = (vgpu_vk_renderer*)driver_data;
    return renderer->features;
}

static vgpu_limits vk_get_limits(vgpu_renderer* driver_data)
{
    vgpu_vk_renderer* renderer = (vgpu_vk_renderer*)driver_data;
    return renderer->limits;
}

static vgpu_render_pass* vk_get_default_render_pass(vgpu_renderer* driver_data)
{
    vgpu_vk_renderer* renderer = (vgpu_vk_renderer*)driver_data;
    return nullptr;
}

static void vk_begin_frame(vgpu_renderer* driver_data)
{

}

static void vk_end_frame(vgpu_renderer* driver_data)
{

}

/* Texture */
static VGPUTexture vk_textureCreate(vgpu_renderer* driver_data, const VGPUTextureDescriptor* desc, const void* initial_data)
{
    return nullptr;
}

static VGPUTexture vk_textureCreateExternal(vgpu_renderer* driver_data, const VGPUTextureDescriptor* desc, void* handle)
{
    return nullptr;
}

static void vk_textureDestroy(vgpu_renderer* driver_data, VGPUTexture texture)
{

}

/* Sampler */
static VGPUSampler vk_samplerCreate(vgpu_renderer* driver_data, const VGPUSamplerDescriptor* descriptor)
{
    return nullptr;
}

static void vk_samplerDestroy(vgpu_renderer* driver_data, VGPUSampler sampler)
{

}

/* RenderPass */
static vgpu_render_pass* vk_render_pass_create(vgpu_renderer* driver_data, const vgpu_render_pass_desc* desc)
{
    return nullptr;
}

static void vk_render_pass_destroy(vgpu_renderer* driver_data, vgpu_render_pass* render_pass)
{

}

static void vk_render_pass_get_extent(vgpu_renderer* driver_data, vgpu_render_pass* render_pass, uint32_t* width, uint32_t* height)
{

}

/* Commands */
static void vk_cmd_begin_render_pass(vgpu_renderer* driver_data, const vgpu_begin_render_pass_desc* begin_pass_desc)
{

}

static void vk_cmd_end_render_pass(vgpu_renderer* driver_data)
{

}

bool vgpu_vk_supported(void) {
    if (vk.available_initialized) {
        return vk.available;
    }

    vk.available_initialized = true;

    if (!agpu_vk_init_loader()) {
        return false;
    }

    vk.available = true;
    return true;
}

VGPUDevice vk_create_device(void) {
    VGPUDevice device;
    vgpu_vk_renderer* renderer;

    device = (VGPUDevice_T*)VGPU_MALLOC(sizeof(VGPUDevice_T));
    ASSIGN_DRIVER(vk);

    /* Init the vk renderer */
    renderer = (vgpu_vk_renderer*)_VGPU_ALLOC_HANDLE(vgpu_vk_renderer);

    /* Reference vgpu_device and renderer together. */
    renderer->gpu_device = device;
    device->renderer = (vgpu_renderer*)renderer;

    return device;
}
