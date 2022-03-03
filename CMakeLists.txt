# CMake version
cmake_minimum_required (VERSION 3.14)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds are not supported. Please choose a different binary directory.")
	return()
endif()

# Use solution folders to organize projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Configure CMake global variables
set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

# Project declaration
project (vgfx C CXX)

# Build Type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	# By default, we use Release
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE "Release" CACHE
		STRING "Choose the type of build." FORCE
	)

	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif()

# Platform Flags
if(APPLE)
	set(CMAKE_MACOSX_RPATH ON)
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)
	set(LOBJC "objc")
endif()

# Options
include(CMakeDependentOption)

option(VGFX_BUILD_SHARED "Build vgfx as a shared library (DLL or .so)" ON)
option(VGFX_INSTALL "Generate install rules for vgfx" ON)
option(VGFX_VULKAN_DRIVER "Build the Vulkan backend" ON)
cmake_dependent_option(VGFX_D3D12_DRIVER "Build the D3D12 backend" ON "WIN32" OFF)
option(VGFX_SAMPLES "Build samples" ON)

if (VGFX_BUILD_SHARED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Build version
set (VGPU_VERSION_MAJOR 0)
set (VGPU_VERSION_MINOR 1)
set (VGPU_VERSION_PATCH 0)
set (VGPU_VERSION ${VGPU_VERSION_MAJOR}.${VGPU_VERSION_MINOR}.${VGPU_VERSION_PATCH})

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

# Install directories
set (INSTALL_BASE_INCLUDE_DIR include)
set (INSTALL_INCLUDE_DIR ${INSTALL_BASE_INCLUDE_DIR}/vgfx)

# Setup library
set(SOURCE_FILES
    include/vgfx.h
    src/vgfx_driver.h
    src/vgfx.c
    src/vgfx_driver_vulkan.cpp
    src/vgfx_driver_d3d12.cpp
)

if (VGFX_BUILD_SHARED)
    add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE VGFX_SHARED_LIBRARY_BUILD=1
        PUBLIC VGFX_SHARED_LIBRARY_INCLUDE=1
    )
else()
    add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
endif ()

# Build flags
if(NOT MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY COMPILE_FLAGS "-std=gnu99 -Wall -Wno-strict-aliasing -pedantic")
endif()

# MinGW builds should statically link libgcc
if(MINGW)
    target_link_libraries(${PROJECT_NAME} PRIVATE -static-libgcc)
endif()

if(MSVC)
    # Multiprocessor compiler
    target_compile_options(${PROJECT_NAME} PRIVATE /MP)
endif ()

if (VGFX_VULKAN_DRIVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGFX_VULKAN_DRIVER=1)
endif ()

if (VGFX_D3D12_DRIVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGFX_D3D12_DRIVER=1)
endif ()

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${INSTALL_BASE_INCLUDE_DIR}>
)

# Soname
set_target_properties(vgfx PROPERTIES
    OUTPUT_NAME "vgfx"
	VERSION ${VGPU_VERSION}
	SOVERSION ${VGPU_VERSION_MAJOR}
)

# Print current build configuration
message(STATUS "VGPU version ${VGPU_VERSION}")
message(STATUS "GIT Revision ID: ${VGPU_REVISION_ID}")
message(STATUS "GIT Short ChangeSet: ${VGPU_SHORT_CHANGE_SET}")
message(STATUS "GIT ChangeSet: ${VGPU_CHANGE_SET}")
message(STATUS "GIT ChangeSet Date: ${VGPU_CHANGE_SET_DATE}")

if (VGFX_BUILD_SHARED)
  message(STATUS "  Library         SHARED")
else ()
  message(STATUS "  Library         STATIC")
endif ()

# Install
if (VGFX_INSTALL)
    # README.md and license
    install (FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        DESTINATION ${CMAKE_INSTALL_PREFIX}
    )

    install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include DESTINATION ${CMAKE_INSTALL_PREFIX}/include FILES_MATCHING PATTERN *.h)

    install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endif ()
