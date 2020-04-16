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

# CMake version
cmake_minimum_required(VERSION 3.10)
project(vgpu)

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
    option(VGPU_BACKEND_D3D11 "Include Direct3D12 backend" ON)
    option(VGPU_BACKEND_D3D12 "Include Direct3D12 backend" ON)
    option(VGPU_BACKEND_OPENGL "Include OpenGL backend" ON)
    option(VGPU_BACKEND_VULKAN "Include Vulkan backend" ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "WindowsStore")
    option(VGPU_BACKEND_D3D11 "Include Direct3D12 backend" ON)
    option(VGPU_BACKEND_D3D12 "Include Direct3D12 backend" ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Durango")
    # XboxOne
    option(VGPU_BACKEND_D3D11 "Include Direct3D12 backend" ON)
    option(VGPU_BACKEND_D3D12 "Include Direct3D12 backend" ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(VGPU_BACKEND_VULKAN ON CACHE BOOL "Include Vulkan renderer")
else()
    set(VGPU_BACKEND_OPENGL ON CACHE BOOL "Include OpenGL renderer")
    set(VGPU_BACKEND_OPENGL ON CACHE BOOL "Include OpenGL renderer")
endif ()

option (VGPU_BUILD_SAMPLES "Build the vgpu samples" ON)
option (VGPU_INSTALL "Generate installation target" ON)
option(BUILD_SHARED_LIBS "Build shared library" ON)

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

# GLFW
if (VGPU_BUILD_SAMPLES AND NOT (EMSCRIPTEN OR ANDROID))
    set (GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set (GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set (GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set (GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
    #set (GLFW_USE_WAYLAND ${VGPU_USE_WAYLAND} CACHE BOOL "" FORCE)
    add_subdirectory(third_party/glfw)
    set_property(TARGET "glfw" PROPERTY FOLDER "third_party")
    unset (LIB_SUFFIX CACHE)
endif ()


# vgpu
set(SOURCE_FILES 
    include/vgpu/vgpu.h
    src/stb_ds.h
    src/vgpu_backend.h
    src/vgpu.c
)

add_library (vgpu ${SOURCE_FILES})
target_compile_definitions(vgpu PRIVATE VGPU_IMPLEMENTATION)

if (BUILD_SHARED_LIBS)
    target_compile_definitions(vgpu PRIVATE VGPU_SHARED_LIBRARY)
endif ()

if (VGPU_BACKEND_D3D11)
    target_sources(vgpu PRIVATE
        src/vgpu_d3d.h
        src/vgpu_d3d11.c
    )

    target_compile_definitions(vgpu PRIVATE VGPU_D3D11)
endif ()

if (VGPU_BACKEND_VULKAN)
    target_sources(vgpu PRIVATE
        src/vk/vkbind.h
        src/vk/vk_mem_alloc.h
        src/vk/vk_mem_alloc.cpp
        src/vgpu_vk.c
    )

    if (MSVC)
        target_sources(vgpu PRIVATE
            src/vk/vk_mem_alloc.natvis
        )
    endif ()

    target_compile_definitions(vgpu PRIVATE VGPU_VULKAN)
    #target_link_libraries(vgpu PRIVATE vulkan)
endif ()

set_target_properties(vgpu PROPERTIES
    VERSION ${VGPU_VERSION}
    SOVERSION ${VGPU_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 11
    C_STANDARD 99
)

target_include_directories(vgpu PUBLIC
    "$<BUILD_INTERFACE:${VGPU_ROOT_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_FULL_INCLUDEDIR}>"
)

if (MSVC)
    # Set warning level 3 and treat all other warnings as errors
	target_compile_options(vgpu PRIVATE
        /W3
        /WX
    )
endif ()

if (VGPU_INSTALL)
    install(TARGETS vgpu
        EXPORT vgpu
        RUNTIME DESTINATION "bin"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
endif()

# samples
if (VGPU_BUILD_SAMPLES)
    add_subdirectory(samples)
endif ()

if (VGPU_INSTALL)
    install(DIRECTORY include/vgpu DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN vgpu.h)
endif ()

# Print current build configuration
message(STATUS "Build Configuration:")
if (BUILD_SHARED_LIBS)
    message(STATUS "  Library         SHARED")
else ()
    message(STATUS "  Library         STATIC")
endif ()
message(STATUS "  Renderer        ${VGPU_RENDERER}")
