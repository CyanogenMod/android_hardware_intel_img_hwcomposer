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
#include <math.h>
#include <HwcTrace.h>
#include <Drm.h>
#include <Hwcomposer.h>
#include <tangier/TngOverlayPlane.h>
#include <tangier/TngGrallocBuffer.h>

namespace android {
namespace intel {

TngOverlayPlane::TngOverlayPlane(int index, int disp)
    : OverlayPlaneBase(index, disp)
{
    CTRACE();

    memset(&mContext, 0, sizeof(mContext));
}

TngOverlayPlane::~TngOverlayPlane()
{
    CTRACE();
}

bool TngOverlayPlane::flip(void *ctx)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (!DisplayPlane::flip(ctx))
        return false;

    mContext.type = DC_OVERLAY_PLANE;
    mContext.ctx.ov_ctx.ovadd = 0x0;
    mContext.ctx.ov_ctx.ovadd = (mBackBuffer->gttOffsetInPage << 12);
    mContext.ctx.ov_ctx.index = mIndex;
    mContext.ctx.ov_ctx.pipe = mPipeConfig;
    mContext.ctx.ov_ctx.ovadd |= 0x1;

    VTRACE("ovadd = %#x, index = %d, device = %d",
          mContext.ctx.ov_ctx.ovadd,
          mIndex,
          mDevice);

    return true;
}

void* TngOverlayPlane::getContext() const
{
    CTRACE();
    return (void *)&mContext;
}

bool TngOverlayPlane::setDataBuffer(BufferMapper& mapper)
{
    if (OverlayPlaneBase::setDataBuffer(mapper) == false) {
        return false;
    }

    if (mIsProtectedBuffer) {
        // Bit 0: Decryption request, only allowed to change on a synchronous flip
        // This request will be qualified with the separate decryption enable bit for OV
        mBackBuffer->buf->OSTART_0Y |= 0x1;
        mBackBuffer->buf->OSTART_1Y |= 0x1;
    }
    return true;
}

bool TngOverlayPlane::flush(uint32_t flags)
{
    RETURN_FALSE_IF_NOT_INIT();
    ATRACE("flags = %#x, type = %d, index = %d", flags, mType, mIndex);

    if (!(flags & PLANE_ENABLE) && !(flags & PLANE_DISABLE))
        return false;

    struct drm_psb_register_rw_arg arg;
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));

    if (flags & PLANE_DISABLE)
        arg.plane_disable_mask = 1;
    else if (flags & PLANE_ENABLE)
        arg.plane_enable_mask = 1;

    arg.plane.type = DC_OVERLAY_PLANE;
    arg.plane.index = mIndex;
    arg.plane.ctx = (mBackBuffer->gttOffsetInPage << 12);
    // pipe select
    arg.plane.ctx |= mPipeConfig;

    // issue ioctl
    Drm *drm = Hwcomposer::getInstance().getDrm();
    bool ret = drm->writeReadIoctl(DRM_PSB_REGISTER_RW, &arg, sizeof(arg));
    if (ret == false) {
        WTRACE("overlay update failed with error code %d", ret);
        return false;
    }

    return true;
}

} // namespace intel
} // namespace android
