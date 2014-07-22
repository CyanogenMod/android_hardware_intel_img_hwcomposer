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
#include "OverlayHardware.h"
#include <HwcLayer.h>

#define SPRITE_PLANE_MAX_STRIDE_TILED      16384
//FIXME: need confirmation about this stride
#define SPRITE_PLANE_MAX_STRIDE_LINEAR     8192

#define OVERLAY_PLANE_MAX_STRIDE_PACKED    4096
#define OVERLAY_PLANE_MAX_STRIDE_LINEAR    8192

namespace android {
namespace intel {

bool PlaneCapabilities::isFormatSupported(int planeType, HwcLayer *hwcLayer)
{
    uint32_t format = hwcLayer->getFormat();
    uint32_t trans = hwcLayer->getLayer()->transform;

    if (planeType == DisplayPlane::PLANE_SPRITE || planeType == DisplayPlane::PLANE_PRIMARY) {
        switch (format) {
        case HAL_PIXEL_FORMAT_BGRA_8888:
        case HAL_PIXEL_FORMAT_BGRX_8888:
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_RGB_565:
            return trans ? false : true;
        default:
            VTRACE("unsupported format %#x", format);
            return false;
        }
    } else if (planeType == DisplayPlane::PLANE_OVERLAY) {
        switch (format) {
        case HAL_PIXEL_FORMAT_I420:
        case HAL_PIXEL_FORMAT_NV12:
        case HAL_PIXEL_FORMAT_YUY2:
        case HAL_PIXEL_FORMAT_UYVY:
            // TODO: overlay supports 180 degree rotation
            if (trans == HAL_TRANSFORM_ROT_180) {
                WTRACE("180 degree rotation is not supported yet");
            }
            return trans ? false : true;
        case HAL_PIXEL_FORMAT_YV12:
        case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar:
        case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar_Tiled:
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

bool PlaneCapabilities::isSizeSupported(int planeType, HwcLayer *hwcLayer)
{
    uint32_t format = hwcLayer->getFormat();
    uint32_t w = hwcLayer->getBufferWidth();
    uint32_t h = hwcLayer->getBufferHeight();
    const stride_t& stride = hwcLayer->getBufferStride();

    bool isYUVPacked;
    uint32_t maxStride;

    if (planeType == DisplayPlane::PLANE_SPRITE || planeType == DisplayPlane::PLANE_PRIMARY) {
        switch (format) {
        case HAL_PIXEL_FORMAT_BGRA_8888:
        case HAL_PIXEL_FORMAT_BGRX_8888:
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_RGB_565:
            if (stride.rgb.stride > SPRITE_PLANE_MAX_STRIDE_LINEAR) {
                VTRACE("too large stride %d", stride.rgb.stride);
                return false;
            }
            return true;
        default:
            VTRACE("unsupported format %#x", format);
            return false;
        }
    } else if (planeType == DisplayPlane::PLANE_OVERLAY) {
        switch (format) {
        case HAL_PIXEL_FORMAT_YV12:
        case HAL_PIXEL_FORMAT_I420:
        case HAL_PIXEL_FORMAT_NV12:
        case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar:
        case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar_Tiled:
            isYUVPacked = false;
            break;
        case HAL_PIXEL_FORMAT_YUY2:
        case HAL_PIXEL_FORMAT_UYVY:
            isYUVPacked = true;
            break;
        default:
            VTRACE("unsupported format %#x", format);
            return false;
        }
        // don't use overlay plane if stride is too big
        maxStride = OVERLAY_PLANE_MAX_STRIDE_LINEAR;
        if (isYUVPacked) {
            maxStride = OVERLAY_PLANE_MAX_STRIDE_PACKED;
        }

        if (stride.yuv.yStride > maxStride) {
            VTRACE("stride %d is too large", stride.yuv.yStride);
            return false;
        }
        return true;
    } else {
        ETRACE("invalid plane type %d", planeType);
        return false;
    }
}

bool PlaneCapabilities::isBlendingSupported(int planeType, HwcLayer *hwcLayer)
{
    uint32_t blending = (uint32_t)hwcLayer->getLayer()->blending;
    uint8_t planeAlpha = hwcLayer->getLayer()->planeAlpha;

    if (planeType == DisplayPlane::PLANE_SPRITE || planeType == DisplayPlane::PLANE_PRIMARY) {
        bool ret = false;

        // support premultipled & none blanding
        switch (blending) {
        case DisplayPlane::PLANE_BLENDING_NONE:
            return true;
        case DisplayPlane::PLANE_BLENDING_PREMULT:
            ret = false;
            if ((planeAlpha == 0) || (planeAlpha == 255)) {
                ret = true;
            }
            return ret;
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


bool PlaneCapabilities::isScalingSupported(int planeType, HwcLayer *hwcLayer)
{
    hwc_frect_t& src = hwcLayer->getLayer()->sourceCropf;
    hwc_rect_t& dest = hwcLayer->getLayer()->displayFrame;

    int srcW, srcH;
    int dstW, dstH;

    srcW = (int)src.right - (int)src.left;
    srcH = (int)src.bottom - (int)src.top;
    dstW = dest.right - dest.left;
    dstH = dest.bottom - dest.top;

    if (planeType == DisplayPlane::PLANE_SPRITE || planeType == DisplayPlane::PLANE_PRIMARY) {
        // no scaling is supported
        return ((srcW == dstW) && (srcH == dstH)) ? true : false;

    } else if (planeType == DisplayPlane::PLANE_OVERLAY) {
        // overlay cannot support resolution that bigger than 2047x2047.
        if ((srcW > INTEL_OVERLAY_MAX_WIDTH - 1) || (srcH > INTEL_OVERLAY_MAX_HEIGHT - 1)) {
            return false;
        }

        if (dstW <= 1 || dstH <= 1) {
            // Workaround: Overlay flip when height is 1 causes MIPI stall on TNG
            return false;
        }

        return true;
    } else {
        ETRACE("invalid plane type %d", planeType);
        return false;
    }
}

bool PlaneCapabilities::isTransformSupported(int planeType, HwcLayer *hwcLayer)
{
    uint32_t trans = hwcLayer->getLayer()->transform;

    if (planeType == DisplayPlane::PLANE_OVERLAY)
        return true;
    // don't transform any tranform
    return trans ? false : true;
}

} // namespace intel
} // namespace android

