#include "vk.h"

#define VK_FUNC_INIT(fn) PFN_##fn fn;
#define VK_LOAD_ANONYMOUS(fn) fn = (PFN_##fn) vkGetInstanceProcAddr(NULL, #fn);
#define VK_LOAD_INSTANCE(fn) fn = (PFN_##fn) vkGetInstanceProcAddr(instance, #fn);
#define VK_LOAD_DEVICE(fn) fn = (PFN_##fn) vkGetDeviceProcAddr(device, #fn);

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;

VK_FOREACH_ANONYMOUS(VK_FUNC_INIT);

/* Instance functions */
VK_FOREACH_INSTANCE(VK_FUNC_INIT);

/* Instance surface functions */
VK_FOREACH_INSTANCE_SURFACE(VK_FUNC_INIT);

/* Device functions */
VK_FOREACH_DEVICE(VK_FUNC_INIT);

#if defined(_WIN32)
HMODULE vk_module = NULL;
#else
void* vk_module = NULL;
#endif

bool agpu_vk_init_loader() {
#if defined(_WIN32)
    vk_module = LoadLibraryA("vulkan-1.dll");
    if (!vk_module)
        return false;

    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(vk_module, "vkGetInstanceProcAddr");
#elif defined(__APPLE__)
    vk_module = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
    if (!vk_module)
        vk_module = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
    if (!vk_module)
        vk_module = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
    if (!vk_module)
        return false;

    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(vk_module, "vkGetInstanceProcAddr");
#else
    vk_module = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!vk_module)
        vk_module = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!vk_module)
        return false;

    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(vk_module, "vkGetInstanceProcAddr");
#endif

    VK_FOREACH_ANONYMOUS(VK_LOAD_ANONYMOUS);

    return true;
}

uint32_t agpu_vk_get_instance_version(void) {
#if defined(VK_VERSION_1_1)
    uint32_t apiVersion = 0;
    if (vkEnumerateInstanceVersion && vkEnumerateInstanceVersion(&apiVersion) == VK_SUCCESS)
        return apiVersion;
#endif

    return VK_API_VERSION_1_0;
}

void agpu_vk_init_instance(VkInstance instance) {
    VK_FOREACH_INSTANCE(VK_LOAD_INSTANCE);

#if defined(_WIN32)
    VK_FOREACH_INSTANCE_SURFACE(VK_LOAD_INSTANCE);
#endif
}

void agpu_vk_init_device(VkDevice device) {
    VK_FOREACH_DEVICE(VK_LOAD_DEVICE);
}
