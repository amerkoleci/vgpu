# stb
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb")

# Graphics backend dependencies
if(VGPU_VULKAN_DRIVER)
    # vulkan
    add_library(vulkan INTERFACE)
    set(VULKAN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/include)
    target_sources(vulkan INTERFACE 
        ${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h
        ${VULKAN_INCLUDE_DIR}/vulkan/vk_platform.h
        ${VULKAN_INCLUDE_DIR}/vulkan/vulkan_core.h
    )
    target_include_directories(vulkan INTERFACE ${VULKAN_INCLUDE_DIR})
    target_compile_definitions(vulkan INTERFACE VK_NO_PROTOTYPES)
    #target_compile_definitions(vulkan INTERFACE VK_ENABLE_BETA_EXTENSIONS)

     # spirv-reflect
    set(SPIRV_REFLECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spirv-reflect")
    set(SPIRV_REFLECT_FILES
        "${SPIRV_REFLECT_DIR}/spirv_reflect.c"
        "${SPIRV_REFLECT_DIR}/spirv_reflect.h"
    )
 
    add_library(spirv-reflect STATIC ${SPIRV_REFLECT_FILES})
    target_include_directories(spirv-reflect PUBLIC ${SPIRV_REFLECT_DIR})
    set_property(TARGET spirv-reflect PROPERTY FOLDER "ThirdParty")
endif()

if (VGPU_D3D12_DRIVER)
    # DirectX-Headers
    add_library(DirectX-Headers INTERFACE)
    set(DIRECTX_HEADERS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/DirectX-Headers/include)
    target_sources(DirectX-Headers INTERFACE 
        ${DIRECTX_HEADERS_INCLUDE_DIR}/directx/d3d12.h
        ${DIRECTX_HEADERS_INCLUDE_DIR}/directx/d3dx12.h
        ${DIRECTX_HEADERS_INCLUDE_DIR}/directx/d3d12shader.h
    )
    target_include_directories(DirectX-Headers INTERFACE ${DIRECTX_HEADERS_INCLUDE_DIR})

    # D3D12MemoryAllocator
    set(D3D12MA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/D3D12MemoryAllocator")
    set(D3D12MA_FILES
        "${D3D12MA_DIR}/include/D3D12MemAlloc.h"
        "${D3D12MA_DIR}/src/D3D12MemAlloc.cpp"
    )

    add_library(D3D12MemoryAllocator STATIC ${D3D12MA_FILES})
    target_include_directories(D3D12MemoryAllocator PUBLIC "${D3D12MA_DIR}/include")
    set_property(TARGET D3D12MemoryAllocator PROPERTY FOLDER "ThirdParty")
endif ()

if (VGPU_OPENGL_DRIVER AND NOT EMSCRIPTEN)
    set(GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glad")
    set(GLAD_FILES
        "${GLAD_DIR}/include/KHR/khrplatform.h"
        "${GLAD_DIR}/include/glad/glad.h"
        "${GLAD_DIR}/src/glad.c"
    )

    add_library(glad STATIC ${GLAD_FILES})
    target_include_directories(glad PUBLIC "${GLAD_DIR}/include")
    set_property(TARGET glad PROPERTY FOLDER "ThirdParty")
endif()

if (VGPU_SAMPLES)
    # Windowing
    if(ANDROID)
        # native_app_glue
        set(NATIVE_APP_GLUE_DIR "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")

        file(GLOB NATIVE_APP_GLUE_FILES "${NATIVE_APP_GLUE_DIR}/*.c" "${NATIVE_APP_GLUE_DIR}/*.h")
    
        add_library(native_app_glue STATIC ${NATIVE_APP_GLUE_FILES})
        
        target_include_directories(native_app_glue PUBLIC ${NATIVE_APP_GLUE_DIR})
    
        set_property(TARGET native_app_glue PROPERTY FOLDER "ThirdParty")
    elseif(NOT (EMSCRIPTEN OR WINDOWS_STORE))
        # GLFW
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
        set(GLFW_INSTALL OFF CACHE BOOL "")
        #set(BUILD_SHARED_LIBS ON CACHE BOOL "")

        add_subdirectory(glfw EXCLUDE_FROM_ALL)
        set_property(TARGET glfw PROPERTY FOLDER "ThirdParty")
        unset(LIB_SUFFIX CACHE)
    endif()
endif ()
