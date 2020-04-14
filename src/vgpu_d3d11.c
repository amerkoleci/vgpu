//
// Copyright (c) 2019 Amer Koleci.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "vgpu_backend.h"

typedef struct d3d11_renderer {
    /* Associated vgpu_device */
    vgpu_device* gpu_device;
} d3d11_renderer;

static void d3d11_destroy(vgpu_device* device)
{
    d3d11_renderer* renderer = (d3d11_renderer*)device->renderer;
    VGPU_FREE(renderer);
    VGPU_FREE(device);
}

vgpu_device* d3d11_create_device(const char* app_name, const vgpu_desc* desc) {
    vgpu_device* device;
    d3d11_renderer* renderer;

    device = (vgpu_device*)VGPU_MALLOC(sizeof(vgpu_device));
    ASSIGN_DRIVER(d3d11);

    /* Init the d3d11 renderer */
    renderer = (d3d11_renderer*)VGPU_MALLOC(sizeof(d3d11_renderer));
    memset(renderer, 0, sizeof(d3d11_renderer));

    /* Reference vgpu_device and renderer together. */
    renderer->gpu_device = device;
    device->renderer = (vgpu_renderer*)renderer;

    return device;
}

bool vgpu_d3d11_supported() {
    return true;
}
