# CMake version
cmake_minimum_required (VERSION 3.16)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds are not supported. Please choose a different binary directory.")
	return()
endif()

# Build version
set (LIB_VERSION_MAJOR 1)
set (LIB_VERSION_MINOR 0)
set (LIB_VERSION_PATCH 0)
set (LIB_VERSION ${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH})

# Project declaration
project (vlibs
    VERSION ${LIB_VERSION}
    DESCRIPTION "Cross platform low-level libraries"
    HOMEPAGE_URL "https://github.com/amerkoleci/vlibs"
    LANGUAGES CXX C
)

# Configure CMake global variables
set(CMAKE_CXX_STANDARD 11)
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
if (NOT DEFINED VLIBS_MASTER_PROJECT)
  set(VLIBS_MASTER_PROJECT OFF)
  if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(VLIBS_MASTER_PROJECT ON)
    message(STATUS "CMake version: ${CMAKE_VERSION}")
  endif ()
endif ()

# Options
include(GNUInstallDirs)
include(CMakeDependentOption)
if (IOS OR EMSCRIPTEN)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable shared library on (IOS, WEB)" FORCE)
else()
    option(BUILD_SHARED_LIBS "Build shared library" ON)
endif ()

option(VLIBS_INSTALL "Generate install rules for vgpu" ON)
cmake_dependent_option(VGPU_VULKAN_DRIVER "Build the Vulkan backend" ON "NOT EMSCRIPTEN;NOT WINDOWS_STORE" OFF)
cmake_dependent_option(VGPU_D3D11_DRIVER "Build the D3D11 backend" ON "WIN32" OFF)
cmake_dependent_option(VGPU_D3D12_DRIVER "Build the D3D12 backend" ON "WIN32" OFF)
cmake_dependent_option(VGPU_WEBGPU_DRIVER "Build the WebGPU backend" ON "EMSCRIPTEN" OFF)
cmake_dependent_option(VLIBS_SAMPLES "Build samples" ${VLIBS_MASTER_PROJECT} "NOT WINDOWS_STORE" OFF)

# Third party
add_subdirectory(third_party)

# Source
add_subdirectory(src)

# Samples
if (VLIBS_SAMPLES)
    add_subdirectory(samples)
endif ()

# Install README.md and license
if (VLIBS_INSTALL)
    install (FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        DESTINATION ${CMAKE_INSTALL_PREFIX}
    )
endif ()

execute_process(COMMAND git rev-list --count HEAD
    OUTPUT_VARIABLE VLIBS_REVISION_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if ("${VLIBS_REVISION_ID}" STREQUAL "")
    # We don't have any source control so just hard-code all of this information to empty
    set (VLIBS_REVISION_ID 0)
    set (VLIBS_SHORT_CHANGE_SET 0)
    set (VLIBS_CHANGE_SET 0)
    set (VLIBS_CHANGE_SET_DATE "")
else ()
    execute_process(COMMAND git rev-parse --short HEAD
        OUTPUT_VARIABLE VLIBS_SHORT_CHANGE_SET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(COMMAND git rev-parse HEAD
        OUTPUT_VARIABLE VLIBS_CHANGE_SET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(COMMAND git log -1 --abbrev=12 --date=format:%Y-%m-%d --pretty=format:%cd
        OUTPUT_VARIABLE VLIBS_CHANGE_SET_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif ()

# Print current build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Version ${LIB_VERSION}")
message(STATUS "GIT Revision ID: ${VLIBS_REVISION_ID}")
message(STATUS "GIT Short ChangeSet: ${VLIBS_SHORT_CHANGE_SET}")
message(STATUS "GIT ChangeSet: ${VLIBS_CHANGE_SET}")
message(STATUS "GIT ChangeSet Date: ${VLIBS_CHANGE_SET_DATE}")

if (BUILD_SHARED_LIBS)
    message(STATUS "  Library         SHARED")
else ()
    message(STATUS "  Library         STATIC")
endif ()

message(STATUS "  Samples         ${VLIBS_SAMPLES}")
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

