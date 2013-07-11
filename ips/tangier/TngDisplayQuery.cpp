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
 */

#include <HwcTrace.h>
#include <DisplayPlane.h>
#include <hal_public.h>
#include <OMX_IVCommon.h>
#include <DisplayQuery.h>


namespace android {
namespace intel {

bool DisplayQuery::isVideoFormat(uint32_t format)
{
    switch (format) {
    case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar:
    case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar_Tiled:
        return true;
    default:
        return false;
    }
}

int DisplayQuery::getOverlayLumaStrideAlignment(uint32_t format)
{
    // both luma and chroma stride need to be 64-byte aligned for overlay
    switch (format) {
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_I420:
        // for these two formats, chroma stride is calculated as half of luma stride
        // so luma stride needs to be 128-byte aligned.
        return 128;
    default:
        return 64;
    }
}

uint32_t DisplayQuery::queryNV12Format()
{
    return HAL_PIXEL_FORMAT_NV12;
}

} // namespace intel
} // namespace android

