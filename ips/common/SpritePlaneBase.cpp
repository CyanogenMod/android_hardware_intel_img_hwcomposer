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
#include <Hwcomposer.h>
#include <BufferManager.h>
#include <common/SpritePlaneBase.h>
#include <common/PixelFormat.h>

namespace android {
namespace intel {

SpritePlaneBase::SpritePlaneBase(int index, int disp)
    : DisplayPlane(index, PLANE_SPRITE, disp),
      mForceBottom(false),
      mAbovePrimary(true)
{
    CTRACE();
}

SpritePlaneBase::~SpritePlaneBase()
{
    CTRACE();
}

bool SpritePlaneBase::reset()
{
    CTRACE();
    return true;
}

bool SpritePlaneBase::flip(void *ctx)
{
    CTRACE();
    return DisplayPlane::flip(ctx);
}

bool SpritePlaneBase::enable()
{
    return enablePlane(true);
}

bool SpritePlaneBase::disable()
{
    return enablePlane(false);
}

void SpritePlaneBase::setZOrderConfig(ZOrderConfig& config)
{
    ATRACE("overlay count = %d", config.overlayCount);

    if (config.overlayCount)
        mForceBottom = false;
    else
        mForceBottom = true;

    // set sprite to be above primary by default
    mAbovePrimary = true;

    // configure sprite z order
    if (config.spriteCount) {
        if (!config.primaryIndex) {
            // if frame buffer target is active
            if (config.spriteIndexes[0] == 0)
                mAbovePrimary = false;
            else if (config.spriteIndexes[0] == (config.layerCount - 1))
                mAbovePrimary = true;
            else
                WTRACE("unsupported z order config, will use default");
        } else if (config.spriteIndexes[0] < config.primaryIndex)
            // if primary was used as sprite
            mAbovePrimary = false;
    }
}

} // namespace intel
} // namespace android
