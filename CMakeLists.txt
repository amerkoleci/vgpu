# CMake version
cmake_minimum_required (VERSION 3.14)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds are not supported. Please choose a different binary directory.")
	return()
endif()

# Build version
set (VGPU_VERSION_MAJOR 1)
set (VGPU_VERSION_MINOR 0)
set (VGPU_VERSION_PATCH 0)
set (VGPU_VERSION ${VGPU_VERSION_MAJOR}.${VGPU_VERSION_MINOR}.${VGPU_VERSION_PATCH})

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

# Project declaration
project (vgpu
    VERSION ${VGPU_VERSION}
    DESCRIPTION "Cross platform low-level libraries"
    HOMEPAGE_URL "https://github.com/amerkoleci/vgpu"
    LANGUAGES CXX C
)

# Configure CMake global variables
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)

# Configure CMake global variables
set(CMAKE_INSTALL_MESSAGE LAZY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for OS X")

# Use solution folders to organize projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Build Type
if(NOT CMAKE_BUILD_TYPE)
	# By default, we use Release
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE "Release" CACHE
		STRING "Choose the type of build." FORCE
	)

	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
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

# Options
if (IOS OR EMSCRIPTEN)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable shared library on (IOS, WEB)" FORCE)
else()
    option(BUILD_SHARED_LIBS "Build shared library" ON)
endif ()

option(VGPU_INSTALL "Generate the install target" ${VGPU_MASTER_PROJECT})
set(VGPU_WSI_SELECTION "XCB" CACHE STRING "Select WSI target (XCB, XLIB, WAYLAND, D2D)")

# Graphics backends
if(EMSCRIPTEN)
    option(VGPU_GL_DRIVER "Enable OpenGL (WebGL 2.0) backend" ON)
    option(VGPU_WEBGPU_DRIVER "Enable WebGPU backend" ON)
elseif(IOS OR APPLE)
    set(VGPU_METAL_DRIVER ON CACHE BOOL "Use Metal backend" FORCE)
elseif(WINDOWS_STORE)
    option(VGPU_D3D12_DRIVER "Enable Direct3D12 backend" ON)
    option(VGPU_D3D11_DRIVER "Enable Direct3D12 backend" ON)
elseif(WIN32)
    option(VGPU_VULKAN_DRIVER "Enable Vulkan backend" ON)
    option(VGPU_D3D12_DRIVER "Enable Direct3D12 backend" ON)
    option(VGPU_D3D11_DRIVER "Enable Direct3D12 backend" ON)

    #option(VGPU_D3D12_DRIVER_AGILITY_SDK "Use Direct3D12 Agility SDK" OFF)
else()
    set(VGPU_VULKAN_DRIVER ON CACHE BOOL "Use Vulkan backend" FORCE)
endif ()

if(EMSCRIPTEN OR WINDOWS_STORE)
    set(VGPU_SAMPLES OFF CACHE BOOL "Disable samples" FORCE)
else()
    option(VGPU_SAMPLES "Enable samples" ${VGPU_MASTER_PROJECT})
endif()

if(VGPU_INSTALL)
    include(GNUInstallDirs)
endif ()

# Print current build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Version ${VGPU_VERSION}")
message(STATUS "GIT Revision ID: ${VGPU_REVISION_ID}")
message(STATUS "GIT Short ChangeSet: ${VGPU_SHORT_CHANGE_SET}")
message(STATUS "GIT ChangeSet: ${VGPU_CHANGE_SET}")
message(STATUS "GIT ChangeSet Date: ${VGPU_CHANGE_SET_DATE}")

if (BUILD_SHARED_LIBS)
    message(STATUS "  Library         SHARED")
else ()
    message(STATUS "  Library         STATIC")
endif ()

message(STATUS "  WSI Backend: ${VGPU_WSI_SELECTION}")
message(STATUS "  Samples         ${VGPU_SAMPLES}")
message(STATUS "  VGPU Backends:")
if (VGPU_VULKAN_DRIVER)
    message(STATUS "      - Vulkan")
endif()
if (VGPU_D3D11_DRIVER)
    message(STATUS "      - Direct3D11")
endif ()
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
    src/vgpu_driver_vulkan.cpp
    src/vgpu_driver_webgpu.cpp
    src/vgpu_driver_d3d11.cpp
    src/vgpu_driver_d3d12.cpp
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
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()

if (VGPU_D3D11_DRIVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGPU_D3D11_DRIVER=1)

    if(WINDOWS_STORE)
        target_link_libraries(${PROJECT_NAME} PRIVATE d3d11 dxgi)
    endif ()
endif ()

if (VGPU_D3D12_DRIVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGPU_D3D12_DRIVER=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE DirectX-Headers D3D12MemoryAllocator)

    if(WINDOWS_STORE)
        target_link_libraries(${PROJECT_NAME} PRIVATE d3d12 dxgi)
    endif ()
endif ()

if (VGPU_VULKAN_DRIVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGPU_VULKAN_DRIVER=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE volk vma)
endif ()

if (VGPU_WEBGPU_DRIVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGPU_WEBGPU_DRIVER=1)
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

