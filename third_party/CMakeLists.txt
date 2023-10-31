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
endif ()

# GLFW
if(NOT (ANDROID OR EMSCRIPTEN OR WINDOWS_STORE))
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
    set(GLFW_INSTALL OFF CACHE BOOL "")
    #set(BUILD_SHARED_LIBS ON CACHE BOOL "")

    if (VGPU_USE_WAYLAND)
        set(GLFW_USE_WAYLAND ON CACHE BOOL "")
    endif()

    add_subdirectory(glfw EXCLUDE_FROM_ALL)
    set_property(TARGET glfw PROPERTY FOLDER "ThirdParty")
    unset(LIB_SUFFIX CACHE)
endif()
