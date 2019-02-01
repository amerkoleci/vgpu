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

# GLFW
if (VGPU_BUILD_SAMPLES AND NOT (EMSCRIPTEN OR ANDROID))
    if (VGPU_SYSTEM_GLFW)
        pkg_search_module(GLFW REQUIRED glfw3)
        include_directories(${GLFW_INCLUDE_DIRS})
    else()
        set (GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set (GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set (GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set (GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        #set (GLFW_USE_WAYLAND ${VGPU_USE_WAYLAND} CACHE BOOL "" FORCE)
        add_subdirectory(glfw)
        set_property(TARGET "glfw" PROPERTY FOLDER "third_party")
    endif ()
    unset (LIB_SUFFIX CACHE)
endif ()

if (VGPU_VULKAN)
    add_subdirectory(volk)
    add_subdirectory(vk_mem_alloc)

    set_property(TARGET "volk" PROPERTY FOLDER "third_party")
    set_property(TARGET "vk_mem_alloc" PROPERTY FOLDER "third_party")
endif ()
