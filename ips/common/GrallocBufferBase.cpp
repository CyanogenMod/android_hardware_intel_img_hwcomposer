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
#include <HwcTrace.h>
#include <common/GrallocBufferBase.h>

namespace android {
namespace intel {

GrallocBufferBase::GrallocBufferBase(uint32_t handle)
    :DataBuffer(handle)
{
    ATRACE("handle = %#x", handle);
}

void GrallocBufferBase::initialize()
{
    int yStride, uvStride;

    // setup stride
    switch (mFormat) {
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_I420:
        yStride = align_to(align_to(mWidth, 32), 64);
        uvStride = align_to(yStride >> 1, 64);
        mStride.yuv.yStride = yStride;
        mStride.yuv.uvStride = uvStride;
        break;
    case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar:
        yStride = align_to(align_to(mWidth, 32), 64);
        uvStride = yStride;
        mStride.yuv.yStride = yStride;
        mStride.yuv.uvStride = uvStride;
        break;
    case HAL_PIXEL_FORMAT_YUY2:
    case HAL_PIXEL_FORMAT_UYVY:
        yStride = align_to((align_to(mWidth, 32) << 1), 64);
        uvStride = 0;
        mStride.yuv.yStride = yStride;
        mStride.yuv.uvStride = uvStride;
        break;
    default:
        mStride.rgb.stride = align_to(((mBpp >> 3) * align_to(mWidth, 32)), 64);
        break;
    }
}

}
}
