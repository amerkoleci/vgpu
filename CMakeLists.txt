#
# Copyright (c) 2019 Amer Koleci.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

cmake_minimum_required(VERSION 3.0)
project(vgpu LANGUAGES CXX C)

set(VGPU_VERSION_MAJOR 0)
set(VGPU_VERSION_MINOR 1)
set(VGPU_VERSION_PATCH 0)
set(VGPU_VERSION ${VGPU_VERSION_MAJOR}.${VGPU_VERSION_MINOR}.${VGPU_VERSION_PATCH})

# Enable folders in IDE (VisualStudio)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Output paths
set(VGPU_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(VGPU_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
mark_as_advanced(VGPU_ROOT_DIR)

set (CMAKE_POSITION_INDEPENDENT_CODE ON)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${VGPU_BUILD_DIR}/bin")
set (CMAKE_PDB_OUTPUT_DIRECTORY "${VGPU_BUILD_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${VGPU_BUILD_DIR}/lib")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${VGPU_BUILD_DIR}/lib")

# Options
if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    #set (VGPU_RENDERER D3D12 CACHE STRING "Select renderer: D3D12 | D3D11 | Vulkan")
    set (VGPU_RENDERER Vulkan CACHE STRING "Select renderer: D3D12 | D3D11 | Vulkan")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "WindowsStore")
    set (VGPU_RENDERER D3D12 CACHE STRING "Select renderer: D3D12 | D3D11")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    if (IOS)
        set (VGPU_RENDERER Metal CACHE STRING "Select renderer: Metal | Vulkan")
    else ()
        set (VGPU_RENDERER Metal CACHE STRING "Select renderer: Metal | Vulkan")
    endif ()
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set (VGPU_RENDERER Vulkan CACHE STRING "Select renderer: Vulkan | OpenGL")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set (VGPU_RENDERER GLES2 CACHE STRING "Select renderer: GLES2 | GLES3")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Durango")
    # XboxOne
    set (VGPU_RENDERER D3D12 CACHE STRING "Select renderer: D3D12 | D3D11")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    set (VGPU_RENDERER GLES2 CACHE STRING "Select renderer: GLES2 | GLES3")
endif ()

string(TOUPPER "${VGPU_RENDERER}" VGPU_RENDERER)
set (VGPU_${VGPU_RENDERER} ON CACHE BOOL "" FORCE)
if (VGPU_GLES2 OR VGPU_GLES3)
    set (VGPU_OPENGL ON CACHE BOOL "" FORCE)
endif ()

option (VGPU_BUILD_SAMPLES "Build the vgpu samples" ON)
option (VGPU_INSTALL "Generate installation target" ON)
option (VGPU_SYSTEM_GLFW "Use the system-provided glfw instead of third_party one" OFF)
option (VGPU_BUILD_SHARED "Build as shared library." OFF)

include(GNUInstallDirs)

# Setup
if (EMSCRIPTEN)
  string(CONCAT VGPU_EMSCRIPTEN_FLAGS
    "-O3 "
    "-s WASM=1 "
    "-s USE_GLFW=3 "
    "-s USE_WEBGL2=1 "
    "-s GL_PREINITIALIZED_CONTEXT=1 "
    "-s USE_ZLIB=1 "
    "-s USE_PTHREADS=0 "
    "-s FULL_ES3=1 "
    "-s FORCE_FILESYSTEM=1 "
    "-s ALLOW_MEMORY_GROWTH=1 "
    "-s \"EXTRA_EXPORTED_RUNTIME_METHODS=['getValue','setValue']\" "
    "--shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/src/resources/vgpu.html\""
  )
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${VGPU_EMSCRIPTEN_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${VGPU_EMSCRIPTEN_FLAGS}")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")

elseif (ANDROID)
elseif (UNIX)
  if (APPLE)
    set (CMAKE_MACOSX_RPATH 1)
  endif ()
  find_package(PkgConfig)
endif ()

# third_party
add_subdirectory(third_party)

# src
add_subdirectory(src)

# samples
if (VGPU_BUILD_SAMPLES)
    add_subdirectory(samples)
endif ()

if (VGPU_INSTALL)
    install(DIRECTORY include/vgpu DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN vgpu.h)
endif ()

# Print current build configuration
message(STATUS "Build Configuration:")
if (VGPU_BUILD_SHARED)
    message(STATUS "  Library         SHARED")
else ()
    message(STATUS "  Library         STATIC")
endif ()
message(STATUS "  Renderer        ${VGPU_RENDERER}")
