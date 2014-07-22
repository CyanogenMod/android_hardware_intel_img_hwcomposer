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
#include <common/SpritePlaneBase.h>
//#include <common/PlaneCapabilities.h>
#include <common/PixelFormat.h>

namespace android {
namespace intel {

SpritePlaneBase::SpritePlaneBase(int index, int disp)
    : DisplayPlane(index, PLANE_SPRITE, disp),
      mForceBottom(true)
{
    LOGV("SpritePlaneBase");
}

SpritePlaneBase::~SpritePlaneBase()
{
    LOGV("~SpritePlaneBase");
}

void SpritePlaneBase::checkPosition(int& x, int& y, int& w, int& h)
{

}

bool SpritePlaneBase::reset()
{
    LOGV("SpritePlaneBase::reset");
    return true;
}

bool SpritePlaneBase::flip()
{
    LOGV("SpritePlaneBase::flip");
    return true;
}

bool SpritePlaneBase::enable()
{
    LOGV("SpritePlaneBase::enable");
    return true;
}

bool SpritePlaneBase::disable()
{
    LOGV("SpritePlaneBase::disable");
    return true;
}

void SpritePlaneBase::setZOrderConfig(ZOrderConfig& config)
{
    LOGV("SpritePlaneBase::setZOrderConfig, overlay count %d",
          config.overlayCount);

    if (config.overlayCount)
        mForceBottom = false;
    else
        mForceBottom = true;
}

} // namespace intel
} // namespace android
