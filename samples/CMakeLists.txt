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

function(add_sample name)
    set (target_name "${name}")
    add_executable(${target_name} ${CMAKE_CURRENT_SOURCE_DIR}/${name}.c)
    if (MSVC)
        set_target_properties(${target_name} PROPERTIES FOLDER "samples")
    endif()

    target_link_libraries(${target_name} PRIVATE vgpu)

    if (WIN32)
        target_link_libraries(${target_name} PRIVATE vgpu_shader)
    endif ()

    if (NOT (EMSCRIPTEN OR ANDROID))
        target_link_libraries(${target_name} PRIVATE glfw)
    endif ()
endfunction()

add_sample(00_clear)
add_sample(01_triangle)
