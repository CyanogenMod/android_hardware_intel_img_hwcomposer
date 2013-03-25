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
#include <math.h>

#include <Drm.h>
#include <Hwcomposer.h>

#include <tangier/TngOverlayPlane.h>
#include <tangier/TngGrallocBuffer.h>

namespace android {
namespace intel {

TngOverlayPlane::TngOverlayPlane(int index, int disp)
    : OverlayPlaneBase(index, disp)
{
    LOGV("TngOverlayPlane");

    memset(&mContext, 0, sizeof(mContext));
}

TngOverlayPlane::~TngOverlayPlane()
{
    LOGV("~TngOverlayPlane");
}

bool TngOverlayPlane::flip()
{
    LOGV("TngOverlayPlane:flip");

    if (!initCheck()) {
        LOGE("TngOverlayPlane::setDataBuffer: overlay wasn't initialized");
        return false;
    }

    mContext.type = DC_OVERLAY_PLANE;
    mContext.ctx.ov_ctx.ovadd = 0x0;
    mContext.ctx.ov_ctx.ovadd = (mBackBuffer->gttOffsetInPage << 12);
    mContext.ctx.ov_ctx.index = mIndex;
    mContext.ctx.ov_ctx.pipe = mPipeConfig;
    mContext.ctx.ov_ctx.ovadd |= 0x1;

    LOGV("TngOverlayPlane::flip: ovadd = 0x%x, index = %d, pipe = %d",
          mContext.ctx.ov_ctx.ovadd,
          mIndex,
          mPipe);

    return true;
}

void* TngOverlayPlane::getContext() const
{
    LOGV("TngOverlayPlane::getContext");
    return (void *)&mContext;
}

} // namespace intel
} // namespace android
