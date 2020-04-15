
#if defined(VULKAN_H_) && !defined(VK_NO_PROTOTYPES)
#	error To use vk, you need to define VK_NO_PROTOTYPES before including vulkan.h
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#ifndef VK_NO_PROTOTYPES
#   define VK_NO_PROTOTYPES
#endif

#include <vulkan/vulkan.h>
#include <stdbool.h>

#define VK_FUNC_DECLARE(fn) extern PFN_##fn fn;

// Functions that don't require an instance
#define VK_FOREACH_ANONYMOUS(X) \
X(vkCreateInstance)\
X(vkEnumerateInstanceExtensionProperties)\
X(vkEnumerateInstanceLayerProperties)\
X(vkEnumerateInstanceVersion)

// Functions that require an instance but don't require a device
#define VK_FOREACH_INSTANCE(X)\
X(vkDestroyInstance)\
X(vkCreateDevice)\
X(vkDestroyDevice)\
X(vkEnumerateDeviceExtensionProperties)\
X(vkEnumerateDeviceLayerProperties)\
X(vkEnumeratePhysicalDevices)\
X(vkGetDeviceProcAddr)\
X(vkGetPhysicalDeviceFeatures)\
X(vkGetPhysicalDeviceFormatProperties)\
X(vkGetPhysicalDeviceImageFormatProperties)\
X(vkGetPhysicalDeviceMemoryProperties)\
X(vkGetPhysicalDeviceProperties)\
X(vkGetPhysicalDeviceQueueFamilyProperties)\
X(vkGetPhysicalDeviceSparseImageFormatProperties)\
X(vkDestroySurfaceKHR)\
X(vkGetPhysicalDeviceSurfaceSupportKHR)\
X(vkGetPhysicalDeviceSurfacePresentModesKHR)\
X(vkGetPhysicalDeviceSurfaceFormatsKHR)\
X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)\
X(vkCreateDebugReportCallbackEXT)\
X(vkDebugReportMessageEXT)\
X(vkDestroyDebugReportCallbackEXT)\
X(vkCreateDebugUtilsMessengerEXT)\
X(vkDestroyDebugUtilsMessengerEXT)\
X(vkGetPhysicalDeviceFeatures2)\
X(vkGetPhysicalDeviceFeatures2KHR)\
X(vkGetPhysicalDeviceSurfaceCapabilities2KHR)\
X(vkGetPhysicalDeviceSurfaceFormats2KHR)\
X(vkGetPhysicalDeviceSurfacePresentModes2EXT)

#if VK_USE_PLATFORM_WIN32_KHR
#define VK_FOREACH_INSTANCE_SURFACE(X)\
X(vkGetPhysicalDeviceWin32PresentationSupportKHR)\
X(vkCreateWin32SurfaceKHR)
#endif /* VK_USE_PLATFORM_WIN32_KHR */

// Functions that require a device
#define VK_FOREACH_DEVICE(X)\
X(vkGetDeviceQueue)\
X(vkSetDebugUtilsObjectNameEXT)\
X(vkQueueSubmit)\
X(vkDeviceWaitIdle)\
X(vkCreateCommandPool)\
X(vkDestroyCommandPool)\
X(vkResetCommandPool)\
X(vkAllocateCommandBuffers)\
X(vkFreeCommandBuffers)\
X(vkBeginCommandBuffer)\
X(vkEndCommandBuffer)\
X(vkCreateFence)\
X(vkDestroyFence)\
X(vkWaitForFences)\
X(vkResetFences)\
X(vkCmdPipelineBarrier)\
X(vkCreateBuffer)\
X(vkDestroyBuffer)\
X(vkGetBufferMemoryRequirements)\
X(vkBindBufferMemory)\
X(vkCmdCopyBuffer)\
X(vkCreateImage)\
X(vkDestroyImage)\
X(vkGetImageMemoryRequirements)\
X(vkBindImageMemory)\
X(vkCmdCopyBufferToImage)\
X(vkAllocateMemory)\
X(vkFreeMemory)\
X(vkMapMemory)\
X(vkUnmapMemory)\
X(vkFlushMappedMemoryRanges)\
X(vkInvalidateMappedMemoryRanges)\
X(vkCreateSemaphore)\
X(vkDestroySemaphore)\
X(vkCreateSampler)\
X(vkDestroySampler)\
X(vkCreateRenderPass)\
X(vkDestroyRenderPass)\
X(vkCmdBeginRenderPass)\
X(vkCmdEndRenderPass)\
X(vkCreateImageView)\
X(vkDestroyImageView)\
X(vkCreateFramebuffer)\
X(vkDestroyFramebuffer)\
X(vkCreateShaderModule)\
X(vkDestroyShaderModule)\
X(vkCreateGraphicsPipelines)\
X(vkDestroyPipeline)\
X(vkCmdBindPipeline)\
X(vkCmdBindVertexBuffers)\
X(vkCmdBindIndexBuffer)\
X(vkCmdDraw)\
X(vkCmdDrawIndexed)\
X(vkCmdDrawIndirect)\
X(vkCmdDrawIndexedIndirect)\
X(vkCmdDispatch)\
X(vkCreateSwapchainKHR)\
X(vkDestroySwapchainKHR)\
X(vkGetSwapchainImagesKHR)\
X(vkAcquireNextImageKHR)\
X(vkQueuePresentKHR)

#ifdef __cplusplus
extern "C" {
#endif
    extern PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;

    VK_FOREACH_ANONYMOUS(VK_FUNC_DECLARE);

    /* Instance functions */
    VK_FOREACH_INSTANCE(VK_FUNC_DECLARE);

    /* Instance surface functions */
#if defined(_WIN32)
    VK_FOREACH_INSTANCE_SURFACE(VK_FUNC_DECLARE);
#endif /* defined(_WIN32) */

    /* Device functions */
    VK_FOREACH_DEVICE(VK_FUNC_DECLARE);

    bool agpu_vk_init_loader();
    uint32_t agpu_vk_get_instance_version(void);
    void agpu_vk_init_instance(VkInstance instance);
    void agpu_vk_init_device(VkDevice device);

#ifdef __cplusplus
    }
#endif
