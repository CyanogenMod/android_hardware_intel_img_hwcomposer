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
#include <PlaneCapabilities.h>


namespace android {
namespace intel {

bool PlaneCapabilities::isFormatSupported(int planeType, uint32_t format)
{
    if (planeType == DisplayPlane::PLANE_SPRITE || planeType == DisplayPlane::PLANE_PRIMARY) {
        switch (format) {
        case HAL_PIXEL_FORMAT_BGRA_8888:
        case HAL_PIXEL_FORMAT_BGRX_8888:
        case HAL_PIXEL_FORMAT_RGB_565:
            return true;
        default:
            VTRACE("unsupported format %#x", format);
            return false;
        }
    } else if (planeType == DisplayPlane::PLANE_OVERLAY) {
        switch (format) {
        case HAL_PIXEL_FORMAT_YV12:
        case HAL_PIXEL_FORMAT_I420:
        case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar:
        case HAL_PIXEL_FORMAT_YUY2:
        case HAL_PIXEL_FORMAT_UYVY:
            return true;
        default:
            VTRACE("unsupported format %#x", format);
            return false;
        }
    } else {
        ETRACE("invalid plane type %d", planeType);
        return false;
    }
}

bool PlaneCapabilities::isBlendingSupported(int planeType, uint32_t blending)
{
    if (planeType == DisplayPlane::PLANE_SPRITE || planeType == DisplayPlane::PLANE_PRIMARY) {
        // support premultipled & none blanding
        switch (blending) {
        case DisplayPlane::PLANE_BLENDING_NONE:
        case DisplayPlane::PLANE_BLENDING_PREMULT:
            return true;
        default:
            VTRACE("unsupported blending %#x", blending);
            return false;
        }
    } else if (planeType == DisplayPlane::PLANE_OVERLAY) {
        // overlay doesn't support blending
        return (blending == DisplayPlane::PLANE_BLENDING_NONE) ? true : false;
    } else {
        ETRACE("invalid plane type %d", planeType);
        return false;
    }
}

bool PlaneCapabilities::isScalingSupported(int planeType, hwc_rect_t& src, hwc_rect_t& dest)
{
    if (planeType == DisplayPlane::PLANE_SPRITE || planeType == DisplayPlane::PLANE_PRIMARY) {
        int srcW, srcH;
        int dstW, dstH;

        srcW = src.right - src.left;
        srcH = src.bottom - src.top;
        dstW = dest.right - dest.left;
        dstH = dest.bottom - dest.top;
        // no scaling is supported
        return ((srcW == dstW) && (srcH == dstH)) ? true : false;

    } else if (planeType == DisplayPlane::PLANE_OVERLAY) {
        // TODO:  check overlay scaling support
        return true;
    } else {
        ETRACE("invalid plane type %d", planeType);
        return false;
    }
}

bool PlaneCapabilities::isTransformSupported(int planeType, uint32_t trans)
{
    if (planeType == DisplayPlane::PLANE_OVERLAY)
        return true;
    // don't transform any tranform
    return trans ? false : true;
}

} // namespace intel
} // namespace android

