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

#include <Hwcomposer.h>
#include <BufferManager.h>
#include <tangier/TngSpritePlane.h>
#include <common/PixelFormat.h>

namespace android {
namespace intel {

TngSpritePlane::TngSpritePlane(int index, int disp)
    : SpritePlaneBase(index, disp)
{
    LOGV("TngSpritePlane");

    memset(&mContext, 0, sizeof(mContext));
}

TngSpritePlane::~TngSpritePlane()
{
    LOGV("~TngSpritePlane");
}

bool TngSpritePlane::setDataBuffer(BufferMapper& mapper)
{
    int bpp;
    int srcX, srcY;
    int dstX, dstY, dstW, dstH;
    uint32_t spriteFormat;
    uint32_t stride;
    uint32_t linoff;

    LOGV("TngSpritePlane::setDataBuffer");

    // setup plane position
    dstX = mPosition.x;
    dstY = mPosition.y;
    dstW = mPosition.w;
    dstH = mPosition.h;

    checkPosition(dstX, dstY, dstW, dstH);

    // setup plane format
    if (!PixelFormat::convertFormat(mapper.getFormat(), spriteFormat, bpp)) {
        LOGE("TngSpritePlane::setDataBuffer: unsupported format 0x%x",
              mapper.getFormat());
        return false;
    }

    // setup stride and source buffer crop
    srcX = mapper.getCrop().x;
    srcY = mapper.getCrop().y;
    stride = mapper.getStride().rgb.stride;
    linoff = srcY * stride + srcX * bpp;

    // unlikely happen, but still we need make sure linoff is valid
    if (linoff > (stride * mapper.getHeight())) {
        LOGE("TngSpritePlane::setDataBuffer: invalid source crop");
        return false;
    }

    // update context
    mContext.type = DC_SPRITE_PLANE;
    mContext.ctx.sp_ctx.index = 0;
    mContext.ctx.sp_ctx.pipe = 0;
    mContext.ctx.sp_ctx.cntr = spriteFormat | 0x80000000;
    mContext.ctx.sp_ctx.linoff = linoff;
    mContext.ctx.sp_ctx.stride = stride;
    mContext.ctx.sp_ctx.surf = mapper.getGttOffsetInPage(0) << 12;
    mContext.ctx.sp_ctx.pos = (dstY & 0xfff) << 16 | (dstX & 0xfff);
    mContext.ctx.sp_ctx.size =
        ((dstH - 1) & 0xfff) << 16 | ((dstW - 1) & 0xfff);
    mContext.ctx.sp_ctx.update_mask = SPRITE_UPDATE_ALL;
    if (mForceBottom)
        mContext.ctx.sp_ctx.cntr  |= 0x00000004;

    LOGV("TngSpritePlane::setDataBuffer: cntr = 0x%x, linoff = 0x%x, stride = 0x%x,"
          "surf = 0x%x, pos = 0x%x, size = 0x%x\n",
          mContext.ctx.sp_ctx.cntr,
          mContext.ctx.sp_ctx.linoff,
          mContext.ctx.sp_ctx.stride,
          mContext.ctx.sp_ctx.surf,
          mContext.ctx.sp_ctx.pos,
          mContext.ctx.sp_ctx.size);
    return true;
}

void* TngSpritePlane::getContext() const
{
    LOGV("TngSpritePlane::getContext");
    return (void *)&mContext;
}

} // namespace intel
} // namespace android
