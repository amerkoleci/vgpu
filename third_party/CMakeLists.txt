# Graphics backend dependencies
if(VGFX_VULKAN_DRIVER)
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

    if(ANDROID)
        target_sources(vulkan INTERFACE ${VULKAN_INCLUDE_DIR}/vulkan/vulkan_android.h)
        target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_ANDROID_KHR)
    elseif(WIN32)
        target_sources(vulkan INTERFACE ${VULKAN_INCLUDE_DIR}/vulkan/vulkan_win32.h)
        target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_WIN32_KHR)
    elseif(APPLE)
        target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_MACOS_MVK)
    elseif(UNIX)
        find_package(PkgConfig REQUIRED)

        pkg_check_modules(XCB xcb)
        if (XCB_FOUND)
            target_include_directories(vulkan PUBLIC ${XCB_INCLUDE_DIRS})
            target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_XCB_KHR)
        endif()

        pkg_check_modules(X11 x11)
        if (X11_FOUND)
            target_include_directories(vulkan PUBLIC ${X11_INCLUDE_DIRS})
            target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_XLIB_KHR)
        endif()
    endif()

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

if (VGFX_SAMPLES)
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
endif (VGFX_SAMPLES)
