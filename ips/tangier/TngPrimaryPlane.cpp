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
#include <Drm.h>
#include <tangier/TngPrimaryPlane.h>
#include <tangier/TngGrallocBuffer.h>
#include <common/PixelFormat.h>

namespace android {
namespace intel {

TngPrimaryPlane::TngPrimaryPlane(int index, int disp)
    : TngSpritePlane(index, disp)
{
    CTRACE();
    mType = PLANE_PRIMARY;
    mForceBottom = true;
    mAbovePrimary = false;
}

TngPrimaryPlane::~TngPrimaryPlane()
{
    CTRACE();
}

void TngPrimaryPlane::setFramebufferTarget(uint32_t handle)
{
    CTRACE();

    // do not need to update the buffer handle
    if (mCurrentDataBuffer != handle)
        mUpdateMasks |= PLANE_BUFFER_CHANGED;
    else
        mUpdateMasks &= ~PLANE_BUFFER_CHANGED;

    // if no update then do Not need set data buffer
    if (!mUpdateMasks)
        return;

    // don't need to map data buffer for primary plane
    mContext.type = DC_PRIMARY_PLANE;
    mContext.ctx.prim_ctx.update_mask = SPRITE_UPDATE_ALL;
    mContext.ctx.prim_ctx.index = mIndex;
    mContext.ctx.prim_ctx.pipe = mDevice;
    mContext.ctx.prim_ctx.linoff = 0;
    mContext.ctx.prim_ctx.stride = align_to((4 * align_to(mPosition.w, 32)), 64);
    mContext.ctx.prim_ctx.pos = 0;
    mContext.ctx.prim_ctx.size =
        ((mPosition.h - 1) & 0xfff) << 16 | ((mPosition.w - 1) & 0xfff);
    mContext.ctx.prim_ctx.surf = 0;

    mContext.ctx.prim_ctx.cntr = PixelFormat::PLANE_PIXEL_FORMAT_BGRA8888;
    mContext.ctx.prim_ctx.cntr |= 0x80000000;

    mCurrentDataBuffer = handle;
}

bool TngPrimaryPlane::setDataBuffer(uint32_t handle)
{
    if (!handle) {
        setFramebufferTarget(handle);
        return true;
    }

    TngGrallocBuffer tmpBuf(handle);
    uint32_t usage;
    bool ret;

    ATRACE("handle = %#x", handle);

    usage = tmpBuf.getUsage();
    if (GRALLOC_USAGE_HW_FB & usage) {
        setFramebufferTarget(handle);
        return true;
    }

    // use primary as a sprite
    ret = DisplayPlane::setDataBuffer(handle);
    if (ret == false) {
        ETRACE("failed to set data buffer");
        return ret;
    }

    mContext.type = DC_PRIMARY_PLANE;
    return true;
}

void TngPrimaryPlane::setZOrderConfig(ZOrderConfig& zorderConfig,
                                           void *nativeConfig)
{
    if (!nativeConfig) {
        ETRACE("Invalid parameter, no native config");
        return;
    }

    mForceBottom = false;

    int primaryIndex = -1;
    int overlayIndex = -1;
    // only consider force bottom when overlay is active
    for (size_t i = 0; i < zorderConfig.size(); i++) {
        DisplayPlane *plane = zorderConfig.itemAt(i);
        if (plane->getType() == DisplayPlane::PLANE_PRIMARY)
            primaryIndex = i;
        if (plane->getType() == DisplayPlane::PLANE_OVERLAY) {
            overlayIndex = i;
        }
    }

    // if has overlay plane which is below primary plane
    if (overlayIndex > primaryIndex) {
        mForceBottom = true;
    }

    struct intel_dc_plane_zorder *zorder =
        (struct intel_dc_plane_zorder *)nativeConfig;
    zorder->forceBottom[mIndex] = mForceBottom ? 1 : 0;
}

bool TngPrimaryPlane::assignToDevice(int disp)
{
    return true;
}

// override disable
bool TngPrimaryPlane::disable()
{
    CTRACE();
    return true;
}

} // namespace intel
} // namespace android
