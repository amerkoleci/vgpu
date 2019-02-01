cmake_minimum_required(VERSION 3.0)
project(volk)

add_library(volk STATIC
    ${CMAKE_CURRENT_LIST_DIR}/volk.h
    ${CMAKE_CURRENT_LIST_DIR}/volk.c
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(volk PRIVATE VK_USE_PLATFORM_WIN32_KHR VK_KHR_win32_surface)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(volk PRIVATE VK_USE_PLATFORM_XCB_KHR VK_KHR_xcb_surface)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_compile_definitions(volk PRIVATE VK_USE_PLATFORM_ANDROID_KHR VK_KHR_android_surface)
else()
    message(FATAL_ERROR "Volk can't be cofigured for " ${CMAKE_SYSTEM_NAME})
endif()

target_include_directories(volk PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${VGPU_ROOT_DIR}/third_party/khronos)
