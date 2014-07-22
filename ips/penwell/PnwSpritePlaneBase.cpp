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
#include <Hwcomposer.h>
#include <BufferManager.h>
#include <penwell/PnwSpritePlaneBase.h>
#include <penwell/PnwPlaneCapabilities.h>
#include <penwell/PnwPixelFormat.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();

PnwSpritePlaneBase::PnwSpritePlaneBase(int index, int disp)
    : DisplayPlane(index, PLANE_SPRITE, disp),
      mForceBottom(true)
{
    log.v("PnwSpritePlaneBase");
}

PnwSpritePlaneBase::~PnwSpritePlaneBase()
{
    log.v("~PnwSpritePlaneBase");
}

bool PnwSpritePlaneBase::isValidTransform(uint32_t trans)
{
    log.v("PnwSpritePlaneBase::isValidTransform: trans %d", trans);

    if (!initCheck()) {
        log.e("PnwSpritePlaneBase::isValidTransform: plane hasn't been initialized");
        return false;
    }

    return trans ? false : true;
}

bool PnwSpritePlaneBase::isValidBuffer(uint32_t handle)
{
    BufferManager& bm(Hwcomposer::getInstance().getBufferManager());
    DataBuffer *buffer;
    uint32_t format;
    bool ret = false;

    log.v("PnwSpritePlaneBase::isValidBuffer: handle = 0x%x", handle);

    buffer = bm.get(handle);
    if (!buffer) {
        log.e("PnwSpritePlaneBase::failed to get buffer");
        return false;
    }

    format = buffer->getFormat();
    bm.put(*buffer);
    ret = PnwPlaneCapabilities::isFormatSupported(mType, format);

    if (!ret) {
        log.v("PnwSpritePlaneBase::isValidBuffer: unsupported format 0x%x",
              format);
        return false;
    }
    return true;
}

bool PnwSpritePlaneBase::isValidBlending(uint32_t blending)
{
    log.v("PnwSpritePlaneBase::isValidBlending: blending = 0x%x", blending);

    if (!initCheck()) {
        log.e("PnwSpritePlaneBase::isValidBlending: plane hasn't been initialized");
        return false;
    }

    if (!PnwPlaneCapabilities::isBlendingSupported(mType, blending)) {
        log.v("PnwSpritePlaneBase::isValidBlending: unsupported blending 0x%x",
              blending);
        return false;
    }

    return true;
}

bool PnwSpritePlaneBase::isValidScaling(hwc_rect_t& src, hwc_rect_t& dest)
{
    log.v("PnwSpritePlaneBase::isValidScaling");

    if (!initCheck()) {
        log.e("PnwSpritePlaneBase::isValidScaling: plane hasn't been initialized");
        return false;
    }

    return PnwPlaneCapabilities::isScalingSupported(mType, src, dest);
}

void PnwSpritePlaneBase::checkPosition(int& x, int& y, int& w, int& h)
{

}

bool PnwSpritePlaneBase::reset()
{
    log.v("PnwSpritePlaneBase::reset");
    return true;
}

bool PnwSpritePlaneBase::flip()
{
    log.v("PnwSpritePlaneBase::flip");
    return true;
}

bool PnwSpritePlaneBase::enable()
{
    log.v("PnwSpritePlaneBase::enable");
    return true;
}

bool PnwSpritePlaneBase::disable()
{
    log.v("PnwSpritePlaneBase::disable");
    return true;
}

void PnwSpritePlaneBase::setZOrderConfig(ZOrderConfig& config)
{
    log.v("PnwSpritePlaneBase::setZOrderConfig, overlay count %d",
          config.overlayCount);

    if (config.overlayCount)
        mForceBottom = false;
    else
        mForceBottom = true;
}

bool PnwSpritePlaneBase::initialize()
{
    return DisplayPlane::initialize();
}

} // namespace intel
} // namespace android
