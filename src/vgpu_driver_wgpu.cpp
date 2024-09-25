// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.
 
#if defined(VGPU_WGPU_DRIVER) && defined(TODO)

#include "vgpu_driver.h"

#if defined(WEBGPU_BACKEND_EMSCRIPTEN) 
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

#if defined(WEBGPU_BACKEND_WGPU)
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
#endif
// clang-format on

#define WGPU_DECLARE_PROC(name) WGPUProc##name wgpu##name;
ALIMER_RHI_WGPU_PROCS(WGPU_DECLARE_PROC);

#if defined(WEBGPU_BACKEND_WGPU)
ALIMER_RHI_WGPU_NATIVE_PROCS(WGPU_DECLARE_PROC);
#endif

#if defined(WEBGPU_BACKEND_DAWN)
#endif

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

#endif

struct VWGPUDevice final : public VGPUDeviceImpl
{
public:
    WGPUDevice device = nullptr;
    uint64_t timestampFrequency = 0;

    ~VWGPUDevice() override;

    bool Init(const VGPUDeviceDescriptor* info);
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

bool VWGPUDevice::Init(const VGPUDeviceDescriptor* info)
{
#if defined(WEBGPU_BACKEND_EMSCRIPTEN)
    renderer->device = emscripten_webgpu_get_device();
#else
#endif

    //renderer->queue = wgpuDeviceGetQueue(renderer->device);

#if defined(__EMSCRIPTEN__)
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
}

void VWGPUDevice::GetLimits(VGPULimits* limits) const
{
}

static VGPUBool32 wgpu_isSupported(void)
{
    static bool available_initialized = false;
    static bool available = false;

    if (available_initialized) {
        return available;
    }

    available_initialized = true;

#if defined(_WIN32)
    HMODULE module = LoadLibraryA("wgpu_native.dll");
    if (!module)
        module = LoadLibraryA("dawn.dll");

    if (!module)
        return false;
#elif defined(__APPLE__)
    void* module = dlopen("libvulkan.dylib", RTLD_LAZY);
    if (!module)
        module = dlopen("libdawn.dylib", RTLD_LAZY);

    if (!module)
        return false;
#else
    void* module = dlopen("libwgpu_native.so", RTLD_LAZY);
    if (!module)
        module = dlopen("libdawn.so", RTLD_LAZY);

    if (!module)
        return false;
#endif

#if defined(_WIN32)
#define LOAD_PROC(name) wgpu##name = (WGPUProc##name)GetProcAddress(module, "wgpu" #name);
#else
#define LOAD_PROC(name) wgpu##name = (WGPUProc##name)dlsym(module, "wgpu" #name);
#endif

    ALIMER_RHI_WGPU_PROCS(LOAD_PROC);
#if defined(WEBGPU_BACKEND_WGPU)
    ALIMER_RHI_WGPU_NATIVE_PROCS(LOAD_PROC);
#endif
#undef LOAD_PROC

    available = true;
    return true;
}

static VGPUDeviceImpl* wgpu_createDevice(const VGPUDeviceDescriptor* info)
{
    VGPU_ASSERT(info);

    VWGPUDevice* device = new VWGPUDevice();

    if (!device->Init(info))
    {
        delete device;
        return nullptr;
    }

    return device;
}

VGPUDriver WGPU_Driver = {
    VGPUBackend_WGPU,
    wgpu_isSupported,
    wgpu_createDevice
};

#endif /* VGPU_WEBGPU_DRIVER */
