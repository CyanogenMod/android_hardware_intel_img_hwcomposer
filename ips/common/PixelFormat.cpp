/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <cutils/log.h>

#include <hal_public.h>

#include <common/PixelFormat.h>

namespace android {
namespace intel {

bool PixelFormat::convertFormat(uint32_t grallocFormat, uint32_t& spriteFormat, int& bpp)
{
    switch (grallocFormat) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
        spriteFormat = PLANE_PIXEL_FORMAT_RGBA8888;
        bpp = 4;
        break;
    case HAL_PIXEL_FORMAT_RGBX_8888:
        spriteFormat = PLANE_PIXEL_FORMAT_BGRX8888;
        bpp = 4;
        break;
    case HAL_PIXEL_FORMAT_BGRX_8888:
        spriteFormat = PLANE_PIXEL_FORMAT_BGRX8888;
        bpp = 4;
        break;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        spriteFormat = PLANE_PIXEL_FORMAT_BGRA8888;
        bpp = 4;
        break;
    case HAL_PIXEL_FORMAT_RGB_565:
        spriteFormat = PLANE_PIXEL_FORMAT_BGRX565;
        bpp = 2;
        break;
    default:
        return false;
    }

    return true;
}


} // namespace intel
} // namespace android
