# CMake version
cmake_minimum_required (VERSION 3.14)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds are not supported. Please choose a different binary directory.")
	return()
endif()

# Determine if engine is built as a subproject (using add_subdirectory)
# or if it is the master project.
if (NOT DEFINED VGPU_MASTER_PROJECT)
    set(VGPU_MASTER_PROJECT OFF)
    if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(VGPU_MASTER_PROJECT ON)
        message(STATUS "CMake version: ${CMAKE_VERSION}")
    endif ()
endif ()

# Use solution folders to organize projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Configure CMake global variables
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_INSTALL_MESSAGE LAZY)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for OS X")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Build version
set (VGPU_VERSION_MAJOR 1)
set (VGPU_VERSION_MINOR 0)
set (VGPU_VERSION_PATCH 0)
set (VGPU_VERSION ${VGPU_VERSION_MAJOR}.${VGPU_VERSION_MINOR}.${VGPU_VERSION_PATCH})

# Project declaration
project (vgpu
    VERSION ${VGPU_VERSION}
    DESCRIPTION "Cross platform low-level libraries"
    HOMEPAGE_URL "https://github.com/amerkoleci/vgpu"
    LANGUAGES CXX C
)

# Set the default CMAKE_BUILD_TYPE to Release.
# This should be done before the project command since the latter can set
# CMAKE_BUILD_TYPE itself (it does so for nmake).
if (VGPU_MASTER_PROJECT AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "The default build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${CMAKE_CONFIGURATION_TYPES})
endif ()

# Setting Platform
if(EMSCRIPTEN)
    set(GPU_PLATFORM_NAME "Web")
elseif(ANDROID)
    set(GPU_PLATFORM_NAME "Android")
elseif(IOS)
    set(GPU_PLATFORM_NAME "iOS")
elseif(APPLE)
    set(GPU_PLATFORM_NAME "macOS")
elseif(WINDOWS_STORE)
    set(GPU_PLATFORM_NAME "UWP")
elseif(WIN32)
    set(GPU_PLATFORM_NAME "Win32")
elseif(UNIX)
    set(GPU_PLATFORM_NAME "Unix")
    set(GPU_LINUX_WSI_SELECTION "XCB" CACHE STRING "Select WSI target (XCB, XLIB, WAYLAND, D2D)")
else()
    message(FATAL_ERROR "Unrecognized platform: ${CMAKE_SYSTEM_NAME}")
endif()

# Setting Architecture
if (DEFINED VCPKG_TARGET_ARCHITECTURE)
    set(GPU_ARCH ${VCPKG_TARGET_ARCHITECTURE})
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^[Ww][Ii][Nn]32$")
    set(GPU_ARCH x86)
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^[Xx]64$")
    set(GPU_ARCH x64)
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^[Aa][Rr][Mm]$")
    set(GPU_ARCH arm)
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^[Aa][Rr][Mm]64$")
    set(GPU_ARCH arm64)
endif()

# Options
if (IOS OR EMSCRIPTEN)
    set(GPU_BUILD_SHARED OFF CACHE BOOL "Disable shared library on (IOS, WEB)" FORCE)
elseif(ANDROID)
    set(GPU_BUILD_SHARED ON CACHE BOOL "Always enable shared library on (ANDROID)" FORCE)  
else()
    option(GPU_BUILD_SHARED "Build shared library" ON)
endif ()

# Graphics backends
if(EMSCRIPTEN)
    option(VGPU_GL_DRIVER "Enable OpenGL (WebGL 2.0) backend" ON)
    option(VGPU_WEBGPU_DRIVER "Enable WebGPU backend" ON)
elseif(IOS OR APPLE)
    set(VGPU_METAL_DRIVER ON CACHE BOOL "Use Metal backend" FORCE)
elseif(WINDOWS_STORE)
    option(VGPU_D3D12_DRIVER "Enable Direct3D12 backend" ON)
elseif(WIN32)
    option(VGPU_VULKAN_DRIVER "Enable Vulkan backend" ON)
    option(VGPU_D3D12_DRIVER "Enable Direct3D12 backend" ON)

    #option(VGPU_D3D12_DRIVER_AGILITY_SDK "Use Direct3D12 Agility SDK" OFF)
else()
    set(VGPU_VULKAN_DRIVER ON CACHE BOOL "Use Vulkan backend" FORCE)
endif ()

option(VGPU_INSTALL "Generate the install target" ${VGPU_MASTER_PROJECT})

if(EMSCRIPTEN OR WINDOWS_STORE)
    set(VGPU_SAMPLES OFF CACHE BOOL "Disable samples" FORCE)
else()
    option(VGPU_SAMPLES "Enable samples" ${VGPU_MASTER_PROJECT})
endif()

if(VGPU_INSTALL)
    include(GNUInstallDirs)
endif ()

# Detect GIT stuff
execute_process(COMMAND git rev-list --count HEAD
    OUTPUT_VARIABLE VGPU_REVISION_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if ("${VGPU_REVISION_ID}" STREQUAL "")
    # We don't have any source control so just hard-code all of this information to empty
    set (VGPU_REVISION_ID 0)
    set (VGPU_SHORT_CHANGE_SET 0)
    set (VGPU_CHANGE_SET 0)
    set (VGPU_CHANGE_SET_DATE "")
else ()
    execute_process(COMMAND git rev-parse --short HEAD
        OUTPUT_VARIABLE VGPU_SHORT_CHANGE_SET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(COMMAND git rev-parse HEAD
        OUTPUT_VARIABLE VGPU_CHANGE_SET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(COMMAND git log -1 --abbrev=12 --date=format:%Y-%m-%d --pretty=format:%cd
        OUTPUT_VARIABLE VGPU_CHANGE_SET_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif ()

# Print current build configuration
message(STATUS "Version ${VGPU_VERSION}")
message(STATUS "GIT Revision ID: ${VGPU_REVISION_ID}")
message(STATUS "GIT Short ChangeSet: ${VGPU_SHORT_CHANGE_SET}")
message(STATUS "GIT ChangeSet: ${VGPU_CHANGE_SET}")
message(STATUS "GIT ChangeSet Date: ${VGPU_CHANGE_SET_DATE}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")

message(STATUS "Detected platform: ${GPU_PLATFORM_NAME}")
message(STATUS "Detected architecture: ${GPU_ARCH}")

if (GPU_BUILD_SHARED)
    message(STATUS "  Library         SHARED")
else ()
    message(STATUS "  Library         STATIC")
endif ()

if (GPU_LINUX_WSI_SELECTION)
    message(STATUS "  WSI Backend:  ${GPU_LINUX_WSI_SELECTION}")
endif ()

message(STATUS "  Samples         ${VGPU_SAMPLES}")
message(STATUS "  VGPU Backends:")
if (VGPU_VULKAN_DRIVER)
    message(STATUS "      - Vulkan")
endif()
if (VGPU_D3D12_DRIVER)
    message(STATUS "      - Direct3D12")
endif ()

# Third party
add_subdirectory(third_party)

# Setup library
set(SOURCE_FILES
    include/vgpu.h
    src/vgpu_driver.h
    src/vgpu_driver_d3d.h
    src/vgpu.cpp
    src/vgpu_check.c
    src/vgpu_driver_webgpu.cpp
)

add_library(${PROJECT_NAME} ${SOURCE_FILES})
if (BUILD_SHARED_LIBS)
    target_compile_definitions(${PROJECT_NAME}
        PUBLIC VGPU_SHARED_LIBRARY=1
        PRIVATE VGPU_IMPLEMENTATION=1
    )
endif ()

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/third_party>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

if (MSVC)
    # Set warning level 3
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)

    # Warnings as Errors Enabled
    target_compile_options(${PROJECT_NAME} PRIVATE /WX)

    # Debug information
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/Zi>)

    # Enable full optimization in dev/release
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/Od> $<$<NOT:$<CONFIG:Debug>>:/Ox>)

    # Inline function expansion
	target_compile_options(${PROJECT_NAME} PRIVATE /Ob2)

    # Enable intrinsic functions in dev/release
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<NOT:$<CONFIG:Debug>>:/Oi>)

    # Favor fast code
	target_compile_options(${PROJECT_NAME} PRIVATE /Ot)

    # Enable fiber-safe optimizations in dev/release
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<NOT:$<CONFIG:Debug>>:/GT>)

	# Enable string pooling
	target_compile_options(${PROJECT_NAME} PRIVATE /GF)

    # Use security checks only in debug
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:DEBUG>:/sdl> $<$<NOT:$<CONFIG:DEBUG>>:/sdl->)

	# Enable function-level linking
	target_compile_options(${PROJECT_NAME} PRIVATE /Gy)

    # Use fast floating point model
    target_compile_options(${PROJECT_NAME} PRIVATE /fp:fast "$<$<NOT:$<CONFIG:DEBUG>>:/guard:cf>")
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

if (VGPU_VULKAN_DRIVER)
    target_sources(${PROJECT_NAME} PRIVATE
        src/vgpu_driver_vulkan.cpp
        src/third_party/vk_mem_alloc.h
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE VGPU_VULKAN_DRIVER=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE volk)
endif ()

if (VGPU_D3D12_DRIVER)
    target_sources(${PROJECT_NAME} PRIVATE
        src/vgpu_driver_d3d12.cpp
        src/third_party/D3D12MemAlloc.cpp
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE VGPU_D3D12_DRIVER=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE DirectX-Headers)

    if(WINDOWS_STORE)
        target_link_libraries(${PROJECT_NAME} PRIVATE d3d12 dxgi)
    endif ()
endif ()

if (VGPU_WEBGPU_DRIVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGPU_WEBGPU_DRIVER=1)
endif ()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE UNICODE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN)
endif ()

# Soname
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "${PROJECT_NAME}"
	VERSION ${VGPU_VERSION}
	SOVERSION ${VGPU_VERSION_MAJOR}
)

# Samples
if (VGPU_SAMPLES)
    add_subdirectory(samples)
endif ()

# Install README.md and license
if (VGPU_INSTALL)
    install (FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        DESTINATION .
    )

    install (FILES "include/vgpu.h" DESTINATION DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

    install(TARGETS ${PROJECT_NAME}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif ()

