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
#include <Log.h>
#include <Drm.h>
#include <tangier/TngPrimaryPlane.h>
#include <tangier/TngGrallocBuffer.h>
#include <penwell/PnwPixelFormat.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();

TngPrimaryPlane::TngPrimaryPlane(int index, int disp)
    : TngSpritePlane(index, disp)
{
    log.v("TngPrimaryPlane");
    mType = PLANE_PRIMARY;
}

TngPrimaryPlane::~TngPrimaryPlane()
{
    log.v("~TngPrimaryPlane");
}

void TngPrimaryPlane::setFramebufferTarget(DataBuffer& buf)
{
    log.v("TngPrimaryPlane::setFramebufferTarget");

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

    mContext.ctx.prim_ctx.cntr = PnwPixelFormat::PLANE_PIXEL_FORMAT_BGRA8888;
    mContext.ctx.prim_ctx.cntr |= 0x80000000;
    if (mForceBottom)
        mContext.ctx.prim_ctx.cntr |= 0x00000004;
}

bool TngPrimaryPlane::setDataBuffer(uint32_t handle)
{
    TngGrallocBuffer tmpBuf(handle);
    uint32_t usage;

    log.v("TngPrimaryPlane::setDataBuffer: handle = %d");

    usage = tmpBuf.getUsage();
    if (!handle || (GRALLOC_USAGE_HW_FB & usage)) {
        setFramebufferTarget(tmpBuf);
        return true;
    }

    return DisplayPlane::setDataBuffer(handle);
}

bool TngPrimaryPlane::assignToDevice(int disp)
{
    return true;
}

} // namespace intel
} // namespace android
