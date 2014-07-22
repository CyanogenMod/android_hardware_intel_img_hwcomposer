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
#include <PlatfDisplayPlaneManager.h>
#include <tangier/TngPrimaryPlane.h>
#include <tangier/TngSpritePlane.h>
#include <tangier/TngOverlayPlane.h>

namespace android {
namespace intel {

PlatfDisplayPlaneManager::PlatfDisplayPlaneManager()
    : DisplayPlaneManager()
{
    CTRACE();
}

PlatfDisplayPlaneManager::~PlatfDisplayPlaneManager()
{

}

void PlatfDisplayPlaneManager::detect()
{
    CTRACE();

    mSpritePlaneCount = 1;
    mPrimaryPlaneCount = 3;
    mOverlayPlaneCount = 2;
    mTotalPlaneCount = mSpritePlaneCount + mPrimaryPlaneCount + mOverlayPlaneCount;

    mFreeSpritePlanes = 0x1;
    // plane A, B & C
    mFreePrimaryPlanes = 0x7;
    // both overlay A & C
    mFreeOverlayPlanes = 0x3;
}

DisplayPlane* PlatfDisplayPlaneManager::allocPlane(int index, int type)
{
    DisplayPlane *plane = 0;
    ATRACE("index = %d, type = %d", index, type);

    switch (type) {
    case DisplayPlane::PLANE_PRIMARY:
        plane = new TngPrimaryPlane(index, index);
        break;
    case DisplayPlane::PLANE_SPRITE:
        plane = new TngSpritePlane(index, 0);
        break;
    case DisplayPlane::PLANE_OVERLAY:
        plane = new TngOverlayPlane(index, 0);
        break;
    default:
        ETRACE("unsupported type %d", type);
        break;
    }
    if (plane && !plane->initialize(DisplayPlane::DEFAULT_DATA_BUFFER_COUNT)) {
        ETRACE("failed to initialize plane.");
        delete plane;
        return NULL;
    }

    return plane;
}

} // namespace intel
} // namespace android

