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

set (VGPU_HEADER_FILES ${VGPU_ROOT_DIR}/include/vgpu/vgpu.h)
set (VGPU_SOURCE_FILES ${CMAKE_CURRENT_LIST_DIR}/vgpu.cpp)

if (VGPU_VULKAN)
    list (APPEND VGPU_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/vgpu_vk.cpp)
elseif (VGPU_D3D12)
    list (APPEND VGPU_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/vgpu_d3d12.cpp)
endif()

if (VGPU_BUILD_SHARED)
    add_library(vgpu SHARED ${VGPU_HEADER_FILES} ${VGPU_SOURCE_FILES})
    target_compile_definitions(vgpu PRIVATE VGPU_BUILD_SHARED_LIBRARY)
else()
    add_library (vgpu STATIC ${VGPU_HEADER_FILES} ${VGPU_SOURCE_FILES})
endif ()

set_target_properties(vgpu PROPERTIES
    VERSION ${VGPU_VERSION}
    SOVERSION ${VGPU_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
    FOLDER "vgpu"
)

target_include_directories(vgpu PUBLIC
    "$<BUILD_INTERFACE:${VGPU_ROOT_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_FULL_INCLUDEDIR}>"
)

if (VGPU_VULKAN)
    target_link_libraries(vgpu PRIVATE volk vk_mem_alloc)
endif ()
