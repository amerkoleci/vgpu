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
    )
    target_include_directories(vulkan INTERFACE ${VULKAN_INCLUDE_DIR})
    target_compile_definitions(vulkan INTERFACE VK_NO_PROTOTYPES)

    if (ANDROID)
        target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_ANDROID_KHR)
    elseif(WIN32)
        target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_WIN32_KHR)
    elseif(APPLE)
        target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_METAL_EXT)
    elseif(UNIX)
        # Choose WSI based on ALIMER_LINUX_WSI_SELECTION
        if (ALIMER_LINUX_WSI_SELECTION STREQUAL XCB OR ALIMER_LINUX_WSI_SELECTION STREQUAL XLIB OR ALIMER_LINUX_WSI_SELECTION STREQUAL WAYLAND)
            find_package(PkgConfig REQUIRED)
        endif()
        if (ALIMER_LINUX_WSI_SELECTION STREQUAL XCB)
            pkg_check_modules(XCB xcb REQUIRED)
            if (XCB_FOUND)
                target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_XCB_KHR)
            endif()
        elseif (ALIMER_LINUX_WSI_SELECTION STREQUAL XLIB)
            pkg_check_modules(X11 x11 REQUIRED)
            if (X11_FOUND)
                target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_XLIB_KHR)
            endif()
        elseif (ALIMER_LINUX_WSI_SELECTION STREQUAL WAYLAND)
            pkg_check_modules(WAYLAND wayland-client REQUIRED)
            if (WAYLAND_FOUND)
                target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_WAYLAND_KHR)
            endif()
        elseif (ALIMER_LINUX_WSI_SELECTION STREQUAL D2D)
            set(DIRECT_TO_DISPLAY TRUE)
            set(DIRECT_TO_DISPLAY TRUE PARENT_SCOPE)
            target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_DISPLAY_KHR)
        else()
            message(FATAL_ERROR "Unknown WSI")
        endif()
    endif()

    # volk
    set(VOLK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/volk")
    set(VOLK_FILES
        "${VOLK_DIR}/volk.c"
        "${VOLK_DIR}/volk.h"
    )

    add_library(volk STATIC ${VOLK_FILES})
    target_link_libraries(volk PUBLIC vulkan)

    target_include_directories(volk PUBLIC ${VOLK_DIR})
    if (ALIMER_LINUX_WSI_SELECTION STREQUAL XCB)
        target_include_directories(volk PUBLIC ${XCB_INCLUDE_DIRS})
    elseif (ALIMER_LINUX_WSI_SELECTION STREQUAL XLIB)
        target_include_directories(volk PUBLIC ${X11_INCLUDE_DIRS})
    elseif (ALIMER_LINUX_WSI_SELECTION STREQUAL WAYLAND)
        target_include_directories(volk PUBLIC ${WAYLAND_INCLUDE_DIRS})
    endif()

    set_property(TARGET volk PROPERTY FOLDER "ThirdParty")

    # vma
    add_library(vma INTERFACE)
    set(VMA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vma/include)
    target_sources(vma INTERFACE ${VMA_DIR}/vk_mem_alloc.h)
    target_include_directories(vma INTERFACE ${VMA_DIR})
    target_link_libraries(vma INTERFACE vulkan)
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

# GLFW
if(NOT (ANDROID OR EMSCRIPTEN OR WINDOWS_STORE))
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
    set(GLFW_INSTALL OFF CACHE BOOL "")
    #set(BUILD_SHARED_LIBS ON CACHE BOOL "")

    if (VGPU_WSI_SELECTION STREQUAL WAYLAND)
        set(GLFW_USE_WAYLAND ON CACHE BOOL "")
    endif()

    add_subdirectory(glfw EXCLUDE_FROM_ALL)
    set_property(TARGET glfw PROPERTY FOLDER "ThirdParty")
    unset(LIB_SUFFIX CACHE)
endif()
