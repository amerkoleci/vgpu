// Copyright (c) Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.
 
#if defined(VGPU_WGPU_DRIVER)

#include "vgpu_driver.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5_webgpu.h>
#else
#define WGPU_SKIP_DECLARATIONS
#include <webgpu/webgpu.h>

// clang-format off
#define ALIMER_RHI_WGPU_PROCS(x) \
    x(CreateInstance)\
    x(GetProcAddress)\
    x(InstanceCreateSurface)\
    x(InstanceHasWGSLLanguageFeature)\
    x(InstanceProcessEvents)\
    x(InstanceRequestAdapter)\
    x(InstanceReference) \
    x(InstanceRelease) \
    x(AdapterEnumerateFeatures) \
    x(AdapterGetInfo) \
    x(AdapterGetLimits) \
    x(AdapterHasFeature) \
    x(AdapterRequestDevice) \
    x(AdapterReference) \
    x(AdapterRelease) \
    x(AdapterInfoFreeMembers) \
    x(DeviceCreateCommandEncoder) \
    x(DeviceDestroy) \
    x(DeviceEnumerateFeatures) \
    x(DeviceGetLimits) \
    x(DeviceGetQueue) \
    x(DeviceHasFeature) \
    x(DevicePopErrorScope) \
    x(DevicePushErrorScope) \
    x(DeviceSetLabel) \
    x(DeviceReference) \
    x(DeviceRelease) \
    x(QueueOnSubmittedWorkDone) \
    x(QueueSetLabel) \
    x(QueueSubmit) \
    x(QueueWriteBuffer) \
    x(QueueWriteTexture) \
    x(QueueReference) \
    x(QueueRelease) \
    x(CommandBufferSetLabel) \
    x(CommandBufferReference) \
    x(CommandBufferRelease) \
    x(CommandEncoderBeginComputePass) \
    x(CommandEncoderBeginRenderPass) \
    x(CommandEncoderClearBuffer) \
    x(CommandEncoderCopyBufferToBuffer) \
    x(CommandEncoderCopyBufferToTexture) \
    x(CommandEncoderCopyTextureToBuffer) \
    x(CommandEncoderCopyTextureToTexture) \
    x(CommandEncoderFinish) \
    x(CommandEncoderInsertDebugMarker) \
    x(CommandEncoderPopDebugGroup) \
    x(CommandEncoderPushDebugGroup) \
    x(CommandEncoderResolveQuerySet) \
    x(CommandEncoderWriteTimestamp) \
    x(CommandEncoderReference) \
    x(CommandEncoderRelease) \
    x(SurfaceConfigure) \
    x(SurfaceGetCapabilities) \
    x(SurfaceGetCurrentTexture) \
    x(SurfacePresent) \
    x(SurfaceSetLabel) \
    x(SurfaceUnconfigure) \
    x(SurfaceReference) \
    x(SurfaceRelease) \
    x(SurfaceCapabilitiesFreeMembers) \
    x(TextureCreateView) \
    x(TextureDestroy) \
    x(TextureSetLabel) \
    x(TextureReference) \
    x(TextureRelease) 

#define WGPU_DECLARE_PROC(name) WGPUProc##name wgpu##name;
ALIMER_RHI_WGPU_PROCS(WGPU_DECLARE_PROC);

#if defined(WEBGPU_BACKEND_WGPU)
// wgpu_native 
//#include <webgpu/wgpu.h>
typedef enum WGPULogLevel {
    WGPULogLevel_Off = 0x00000000,
    WGPULogLevel_Error = 0x00000001,
    WGPULogLevel_Warn = 0x00000002,
    WGPULogLevel_Info = 0x00000003,
    WGPULogLevel_Debug = 0x00000004,
    WGPULogLevel_Trace = 0x00000005,
    WGPULogLevel_Force32 = 0x7FFFFFFF
} WGPULogLevel;

typedef void (*WGPULogCallback)(WGPULogLevel level, char const* message, void* userdata);

typedef void(*WGPUProcSetLogCallback)(WGPULogCallback callback, void* userdata) WGPU_FUNCTION_ATTRIBUTE;
typedef void(*WGPUProcSetLogLevel)(WGPULogLevel level) WGPU_FUNCTION_ATTRIBUTE;
typedef uint32_t(*WGPUProcGetVersion)() WGPU_FUNCTION_ATTRIBUTE;

#define ALIMER_RHI_WGPU_NATIVE_PROCS(x) \
    x(SetLogCallback)\
    x(SetLogLevel)\
    x(GetVersion)

ALIMER_RHI_WGPU_NATIVE_PROCS(WGPU_DECLARE_PROC);
#endif

#undef WGPU_DECLARE_PROC

#if defined(_WIN32)
// Use the C++ standard templated min/max
#define NOMINMAX
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

struct WGPU_State
{
#if defined(_WIN32)
    HMODULE nativeLibrary = nullptr;
#else
    void nativeLibrary = nullptr;
#endif
    bool dawn = false;

    ~WGPU_State()
    {
        if (nativeLibrary)
        {
#if defined(_WIN32)
            FreeLibrary(nativeLibrary);
#else
            dlclose(nativeLibrary);
#endif
            nativeLibrary = nullptr;
        }
    }
} wgpu_state;

#endif

struct VWGPUDevice final : public VGPUDeviceImpl
{
public:
    WGPUDevice device = nullptr;
    uint64_t timestampFrequency = 0;

    ~VWGPUDevice() override;

    bool Init(const VGPUDeviceDesc* desc);
    void SetLabel(const char* label) override;
    void WaitIdle() override;
    VGPUBackend GetBackendType() const override { return VGPUBackend_Vulkan; }
    VGPUBool32 QueryFeatureSupport(VGPUFeature feature) const override;
    void GetAdapterProperties(VGPUAdapterProperties* properties) const override;
    void GetLimits(VGPULimits* limits) const override;
    uint64_t GetTimestampFrequency() const  override { return timestampFrequency; }

    VGPUBuffer CreateBuffer(const VGPUBufferDesc* desc, const void* pInitialData) override;
    VGPUTexture CreateTexture(const VGPUTextureDesc* desc, const VGPUTextureData* pInitialData) override;
    VGPUSampler CreateSampler(const VGPUSamplerDesc* desc) override;

    VGPUBindGroupLayout CreateBindGroupLayout(const VGPUBindGroupLayoutDesc* desc) override;
    VGPUPipelineLayout CreatePipelineLayout(const VGPUPipelineLayoutDesc* desc) override;
    VGPUBindGroup CreateBindGroup(const VGPUBindGroupLayout layout, const VGPUBindGroupDesc* desc) override;

    VGPUPipeline CreateRenderPipeline(const VGPURenderPipelineDesc* desc) override;
    VGPUPipeline CreateComputePipeline(const VGPUComputePipelineDesc* desc) override;
    VGPUPipeline CreateRayTracingPipeline(const VGPURayTracingPipelineDesc* desc) override;

    VGPUQueryHeap CreateQueryHeap(const VGPUQueryHeapDesc* desc) override;

    VGPUSwapChain CreateSwapChain(const VGPUSwapChainDesc* desc) override;

    VGPUCommandBuffer BeginCommandBuffer(VGPUCommandQueue queueType, const char* label) override;
    uint64_t Submit(VGPUCommandBuffer* commandBuffers, uint32_t count) override;

    void* GetNativeObject(VGPUNativeObjectType objectType) const override;
};

VWGPUDevice::~VWGPUDevice()
{

}

bool VWGPUDevice::Init(const VGPUDeviceDesc* desc)
{
    VGPU_UNUSED(desc);

#if defined(__EMSCRIPTEN__) && defined(TODO)
    renderer->device = emscripten_webgpu_get_device();

    // Create surface first.
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvDesc = {};
    canvDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    canvDesc.selector = surface->selector;
    WGPUSurfaceDescriptor surfDesc = {};
    surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&canvDesc);
    WGPUSurface wgpuSurface = wgpuInstanceCreateSurface(nullptr, &surfDesc);

    WGPUSwapChainDescriptor swapDesc = {};
    swapDesc.usage = WGPUTextureUsage_RenderAttachment;
    swapDesc.format = WGPUTextureFormat_BGRA8Unorm;
    swapDesc.width = 800;
    swapDesc.height = 450;
    swapDesc.presentMode = WGPUPresentMode_Fifo;
    renderer->swapchain = wgpuDeviceCreateSwapChain(renderer->device, wgpuSurface, &swapDesc);
#else

#endif

    // Log some info
    vgpuLogInfo("VGPU Driver: WGPU");
    //vgpuLogInfo("Vulkan Adapter: %s", properties2.properties.deviceName);

    return true;
}

void VWGPUDevice::SetLabel(const char* label)
{
    wgpuDeviceSetLabel(device, label);
}

void VWGPUDevice::WaitIdle()
{
}

VGPUBool32 VWGPUDevice::QueryFeatureSupport(VGPUFeature feature) const
{
    switch (feature)
    {
        case VGPUFeature_Depth32FloatStencil8:
            return false;

        case VGPUFeature_TimestampQuery:
            return false;

        case VGPUFeature_PipelineStatisticsQuery:
            return false;

        case VGPUFeature_TextureCompressionBC:
            return false;

        case VGPUFeature_TextureCompressionETC2:
            return false;

        case VGPUFeature_TextureCompressionASTC:
            return false;

        case VGPUFeature_IndirectFirstInstance:
            return false;

        case VGPUFeature_ShaderFloat16:
            return false;

        case VGPUFeature_CacheCoherentUMA:
            return false;

        case VGPUFeature_GeometryShader:
            return false;

        case VGPUFeature_TessellationShader:
            return false;

        case VGPUFeature_DepthBoundsTest:
            return false;

        case VGPUFeature_SamplerClampToBorder:
            return false;

        case VGPUFeature_SamplerMirrorClampToEdge:
            return false;

        case VGPUFeature_SamplerMinMax:
            return false;

        case VGPUFeature_DepthResolveMinMax:
            return false;

        case VGPUFeature_StencilResolveMinMax:
            return false;

        case VGPUFeature_ShaderOutputViewportIndex:
            return false;

        case VGPUFeature_ConservativeRasterization:
            return false;

        case VGPUFeature_DescriptorIndexing:
            return false;

        case VGPUFeature_Predication:
            return false;

        case VGPUFeature_VariableRateShading:
        case VGPUFeature_VariableRateShadingTier2:
        case VGPUFeature_RayTracing:
        case VGPUFeature_RayTracingTier2:
        case VGPUFeature_MeshShader:
            return false;

        default:
            return false;
    }
}

void VWGPUDevice::GetAdapterProperties(VGPUAdapterProperties* properties) const
{
    VGPU_UNUSED(properties);
}

void VWGPUDevice::GetLimits(VGPULimits* limits) const
{
    VGPU_UNUSED(limits);
}

VGPUBuffer VWGPUDevice::CreateBuffer(const VGPUBufferDesc* desc, const void* pInitialData)
{
    VGPU_UNUSED(desc);
    VGPU_UNUSED(pInitialData);

    return nullptr;
}

VGPUTexture VWGPUDevice::CreateTexture(const VGPUTextureDesc* desc, const VGPUTextureData* pInitialData)
{
    VGPU_UNUSED(desc);
    VGPU_UNUSED(pInitialData);

    return nullptr;
}

VGPUSampler VWGPUDevice::CreateSampler(const VGPUSamplerDesc* desc)
{
    VGPU_UNUSED(desc);

    return nullptr;
}

VGPUBindGroupLayout VWGPUDevice::CreateBindGroupLayout(const VGPUBindGroupLayoutDesc* desc)
{
    VGPU_UNUSED(desc);

    return nullptr;
}

VGPUPipelineLayout VWGPUDevice::CreatePipelineLayout(const VGPUPipelineLayoutDesc* desc)
{
    VGPU_UNUSED(desc);

    return nullptr;
}

VGPUBindGroup VWGPUDevice::CreateBindGroup(const VGPUBindGroupLayout layout, const VGPUBindGroupDesc* desc)
{
    VGPU_UNUSED(layout);
    VGPU_UNUSED(desc);

    return nullptr;
}

VGPUPipeline VWGPUDevice::CreateRenderPipeline(const VGPURenderPipelineDesc* desc)
{
    VGPU_UNUSED(desc);

    return nullptr;
}

VGPUPipeline VWGPUDevice::CreateComputePipeline(const VGPUComputePipelineDesc* desc)
{
    VGPU_UNUSED(desc);

    return nullptr;
}

VGPUPipeline VWGPUDevice::CreateRayTracingPipeline(const VGPURayTracingPipelineDesc* desc)
{
    VGPU_UNUSED(desc);

    return nullptr;
}

VGPUQueryHeap VWGPUDevice::CreateQueryHeap(const VGPUQueryHeapDesc* desc)
{
    VGPU_UNUSED(desc);

    return nullptr;
}

VGPUSwapChain VWGPUDevice::CreateSwapChain(const VGPUSwapChainDesc* desc)
{
    VGPU_UNUSED(desc);

    return nullptr;
}

VGPUCommandBuffer VWGPUDevice::BeginCommandBuffer(VGPUCommandQueue queueType, const char* label)
{
    VGPU_UNUSED(queueType);
    VGPU_UNUSED(label);

    return nullptr;
}

uint64_t VWGPUDevice::Submit(VGPUCommandBuffer* commandBuffers, uint32_t count)
{
    VGPU_UNUSED(commandBuffers);
    VGPU_UNUSED(count);

    return 0;
}

void* VWGPUDevice::GetNativeObject(VGPUNativeObjectType objectType) const
{
    VGPU_UNUSED(objectType);

    return nullptr;
}

static bool wgpu_IsSupported(void)
{
    static bool available_initialized = false;
    static bool available = false;

    if (available_initialized) {
        return available;
    }

    available_initialized = true;

#if defined(__EMSCRIPTEN__)
#else
#if defined(_WIN32)
    wgpu_state.nativeLibrary = LoadLibraryA("wgpu_native.dll");
    if (!wgpu_state.nativeLibrary)
    {
        wgpu_state.nativeLibrary = LoadLibraryA("dawn.dll");
        wgpu_state.dawn = true;
    }

    if (!wgpu_state.nativeLibrary)
        return false;
#elif defined(__APPLE__)
    wgpu_state.nativeLibrary = dlopen("libvulkan.dylib", RTLD_LAZY);
    if (!wgpu_state.nativeLibrary)
    {
        wgpu_state.nativeLibrary = dlopen("libdawn.dylib", RTLD_LAZY);
        wgpu_state.dawn = true;
    } 

    if (!wgpu_state.nativeLibrary)
        return false;
#else
    wgpu_state.nativeLibrary = dlopen("libwgpu_native.so", RTLD_LAZY);
    if (!wgpu_state.nativeLibrary)
    {
        wgpu_state.nativeLibrary = dlopen("libdawn.so", RTLD_LAZY);
        wgpu_state.dawn = true;
    }

    if (!wgpu_state.nativeLibrary)
        return false;
#endif

#if defined(_WIN32)
#define LOAD_PROC(name) wgpu##name = (WGPUProc##name)GetProcAddress(wgpu_state.nativeLibrary, "wgpu" #name);
#else
#define LOAD_PROC(name) wgpu##name = (WGPUProc##name)dlsym(wgpu_state.nativeLibrary, "wgpu" #name);
#endif

    ALIMER_RHI_WGPU_PROCS(LOAD_PROC);
#if defined(WEBGPU_BACKEND_WGPU)
    ALIMER_RHI_WGPU_NATIVE_PROCS(LOAD_PROC);
#endif
#undef LOAD_PROC

#endif

    available = true;
    return true;
}

static VGPUInstanceImpl* wgpu_CreateInstance(const VGPUInstanceDesc* desc)
{
    VGPU_UNUSED(desc);

    return nullptr;
}

static VGPUDeviceImpl* wgpu_CreateDevice(const VGPUDeviceDesc* desc)
{
    VGPU_ASSERT(desc);

    VWGPUDevice* device = new VWGPUDevice();

    if (!device->Init(desc))
    {
        delete device;
        return nullptr;
    }

    return device;
}

VGPUDriver WGPU_Driver = {
    VGPUBackend_WGPU,
    wgpu_IsSupported,
    wgpu_CreateInstance,
    wgpu_CreateDevice
};

#endif /* VGPU_WEBGPU_DRIVER */
