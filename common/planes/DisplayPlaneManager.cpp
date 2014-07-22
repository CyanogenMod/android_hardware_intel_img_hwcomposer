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

#include <DisplayPlaneManager.h>

namespace android {
namespace intel {

DisplayPlaneManager::DisplayPlaneManager()
    : mSpritePlaneCount(0),
      mPrimaryPlaneCount(0),
      mOverlayPlaneCount(0),
      mTotalPlaneCount(0),
      mFreeSpritePlanes(0),
      mFreePrimaryPlanes(0),
      mFreeOverlayPlanes(0),
      mReclaimedSpritePlanes(0),
      mReclaimedPrimaryPlanes(0),
      mReclaimedOverlayPlanes(0),
      mInitialized(false)
{

}

DisplayPlaneManager::~DisplayPlaneManager()
{
    size_t i;

    for (i = 0; i < mOverlayPlanes.size(); i++)
        delete mOverlayPlanes.itemAt(i);
    mOverlayPlanes.clear();

    for (i = 0; i < mSpritePlanes.size(); i++)
        delete mSpritePlanes.itemAt(i);
    mSpritePlanes.clear();

    for (i = 0; i < mPrimaryPlanes.size(); i++)
        delete mPrimaryPlanes.itemAt(i);
    mPrimaryPlanes.clear();
}

bool DisplayPlaneManager::initialize()
{
    int i;
    size_t j;

    LOGV("DisplayPlaneManager::initialize");

    // detect display plane usage. Hopefully throw DRM ioctl
    detect();

    // allocate primary plane pool
    if (mPrimaryPlaneCount) {
        mPrimaryPlanes.setCapacity(mPrimaryPlaneCount);

        for (i = 0; i < mPrimaryPlaneCount; i++) {
            DisplayPlane* plane = allocPlane(i, DisplayPlane::PLANE_PRIMARY);
            if (!plane) {
                LOGE("initialize: failed to allocate primary plane %d", i);
                goto primary_err;
            }
            // reset overlay plane
            plane->reset();
            // insert plane
            mPrimaryPlanes.push_back(plane);
        }
    }

    // allocate sprite plane pool
    if (mSpritePlaneCount) {
        mSpritePlanes.setCapacity(mSpritePlaneCount);

        for (i = 0; i < mSpritePlaneCount; i++) {
            DisplayPlane* plane = allocPlane(i, DisplayPlane::PLANE_SPRITE);
            if (!plane) {
                LOGE("initialize: failed to allocate sprite plane %d", i);
                goto sprite_err;
            }
            // reset overlay plane
            plane->reset();
            // insert plane
            mSpritePlanes.push_back(plane);
        }
    }

    if (mOverlayPlaneCount) {
        // allocate overlay plane pool
        mOverlayPlanes.setCapacity(mOverlayPlaneCount);
        for (i = 0; i < mOverlayPlaneCount; i++) {
            DisplayPlane* plane = allocPlane(i, DisplayPlane::PLANE_OVERLAY);
            if (!plane) {
                LOGE("initialize: failed to allocate sprite plane %d", i);
                goto overlay_err;
            }
            // reset overlay plane
            plane->reset();
            // insert plane
            mOverlayPlanes.push_back(plane);
        }
    }

    mInitialized = true;
    return true;
overlay_err:
    for (j = 0; j < mOverlayPlanes.size(); j++)
        delete mOverlayPlanes.itemAt(j);
    mOverlayPlanes.clear();
sprite_err:
    for (j = 0; j < mSpritePlanes.size(); j++)
        delete mSpritePlanes.itemAt(j);
    mSpritePlanes.clear();
primary_err:
    for (j = 0; j < mPrimaryPlanes.size(); j++)
        delete mPrimaryPlanes.itemAt(j);
    mPrimaryPlanes.clear();
    mInitialized = false;
    return false;
}

int DisplayPlaneManager::getPlane(uint32_t& mask)
{
    if (!mask)
        return -1;

    for (int i = 0; i < 32; i++) {
        int bit = (1 << i);
        if (bit & mask) {
            mask &= ~bit;
            return i;
        }
    }

    return -1;
}

void DisplayPlaneManager::putPlane(int index, uint32_t& mask)
{
    if (index < 0 || index >= 32)
        return;

    int bit = (1 << index);

    if (bit & mask) {
        LOGW("putPlane: bit %d was set", index);
        return;
    }

    mask |= bit;
}

int DisplayPlaneManager::getPlane(uint32_t& mask, int index)
{
    if (!mask || index < 0 || index > mTotalPlaneCount)
        return -1;

    int bit = (1 << index);
    if (bit & mask) {
        mask &= ~bit;
        return index;
    }

    return -1;
}

DisplayPlane* DisplayPlaneManager::getSpritePlane()
{
    if (!initCheck()) {
        LOGE("getSpritePlane: plane manager was not initialized\n");
        return 0;
    }

    int freePlaneIndex;

    // check reclaimed sprite planes
    freePlaneIndex = getPlane(mReclaimedSpritePlanes);
    if (freePlaneIndex >= 0)
        return mSpritePlanes.itemAt(freePlaneIndex);

    // check free sprite planes
    freePlaneIndex = getPlane(mFreeSpritePlanes);
    if (freePlaneIndex >= 0)
        return mSpritePlanes.itemAt(freePlaneIndex);
    LOGE("getSpritePlane: failed to get a sprite plane\n");
    return 0;
}

DisplayPlane* DisplayPlaneManager::getPrimaryPlane(int pipe)
{
    if (!initCheck()) {
        LOGE("getSpritePlane: plane manager was not initialized\n");
        return 0;
    }

    int freePlaneIndex;

    // check reclaimed primary planes
    freePlaneIndex = getPlane(mReclaimedPrimaryPlanes, pipe);
    if (freePlaneIndex >= 0)
        return mPrimaryPlanes.itemAt(freePlaneIndex);

    // check free primary planes
    freePlaneIndex = getPlane(mFreePrimaryPlanes, pipe);
    if (freePlaneIndex >= 0)
        return mPrimaryPlanes.itemAt(freePlaneIndex);
    LOGE("getPrimaryPlane: failed to get a primary plane\n");
    return 0;
}

DisplayPlane* DisplayPlaneManager::getOverlayPlane()
{
    if (!initCheck()) {
        LOGE("getOverlayPlane: plane manager was not initialized\n");
        return 0;
    }

    int freePlaneIndex;

    // check reclaimed overlay planes
    freePlaneIndex = getPlane(mReclaimedOverlayPlanes);
    if (freePlaneIndex < 0) {
       // check free overlay planes
       freePlaneIndex = getPlane(mFreeOverlayPlanes);
    }

    if (freePlaneIndex < 0) {
       LOGE("getOverlayPlane: failed to get a overlay plane\n");
       return 0;
    }

    return mOverlayPlanes.itemAt(freePlaneIndex);
}

void DisplayPlaneManager::putSpritePlane(DisplayPlane& plane)
{
    int index = plane.getIndex();

    if (plane.getType() == DisplayPlane::PLANE_SPRITE)
        putPlane(index, mFreeSpritePlanes);
}

void DisplayPlaneManager::putOverlayPlane(DisplayPlane& plane)
{
    int index = plane.getIndex();
    if (plane.getType() == DisplayPlane::PLANE_OVERLAY)
        putPlane(index, mFreeOverlayPlanes);
}

bool DisplayPlaneManager::hasFreeSprites()
{
    if (!initCheck())
        return false;

    return (mFreeSpritePlanes || mReclaimedSpritePlanes) ? true : false;
}

bool DisplayPlaneManager::hasFreeOverlays()
{
    if (!initCheck())
        return false;

    return (mFreeOverlayPlanes || mReclaimedOverlayPlanes) ? true : false;
}

int DisplayPlaneManager::getFreeSpriteCount() const
{
    if (!initCheck())
        return 0;

    uint32_t availableSprites = mFreeSpritePlanes;
    availableSprites |= mReclaimedSpritePlanes;
    int count = 0;
    for (int i = 0; i < 32; i++) {
        int bit = (1 << i);
        if (bit & availableSprites)
            count++;
    }

    return count;
}

int DisplayPlaneManager::getFreeOverlayCount() const
{
    if (!initCheck())
        return 0;

    uint32_t availableOverlays = mFreeOverlayPlanes;
    availableOverlays |= mReclaimedOverlayPlanes;
    int count = 0;
    for (int i = 0; i < 32; i++) {
        int bit = (1 << i);
        if (bit & availableOverlays)
            count++;
    }

    return count;
}

bool DisplayPlaneManager::hasReclaimedOverlays()
{
    if (!initCheck())
        return false;

    return (mReclaimedOverlayPlanes) ? true : false;
}

bool DisplayPlaneManager::primaryAvailable(int pipe)
{
    if (!initCheck())
        return false;

    return ((mFreePrimaryPlanes & (1 << pipe)) ||
            (mReclaimedPrimaryPlanes & (1 << pipe))) ? true : false;
}

void DisplayPlaneManager::reclaimPlane(DisplayPlane& plane)
{
    if (!initCheck()) {
        LOGE("reclaimPlane: plane manager is not initialized\n");
        return;
    }

    int index = plane.getIndex();

    LOGV("reclaimPlane: reclaimPlane %d, type %d\n", index, plane.getType());

    if (plane.getType() == DisplayPlane::PLANE_OVERLAY)
        putPlane(index, mReclaimedOverlayPlanes);
    else if (plane.getType() == DisplayPlane::PLANE_SPRITE)
        putPlane(index, mReclaimedSpritePlanes);
    else if (plane.getType() == DisplayPlane::PLANE_PRIMARY)
        putPlane(index, mReclaimedPrimaryPlanes);
    else
        LOGE("reclaimPlane: invalid plane type %d", plane.getType());
}

void DisplayPlaneManager::disableReclaimedPlanes()
{
    if (!initCheck()) {
        LOGE("disableReclaimedPlanes: plane manager is not initialized");
        return;
    }

    LOGV("DisplayPlaneManager::disableReclaimedPlanes: "
          "sprite %d, reclaimed 0x%x"
          "primary %d, reclaimed 0x%x"
          "overlay %d, reclaimed 0x%x",
          mSpritePlanes.size(), mReclaimedSpritePlanes,
          mPrimaryPlanes.size(), mReclaimedPrimaryPlanes,
          mOverlayPlanes.size(), mReclaimedOverlayPlanes);

    // disable reclaimed sprite planes
    if (mSpritePlanes.size() && mReclaimedSpritePlanes) {
        for (int i = 0; i < mSpritePlaneCount; i++) {
            int bit = (1 << i);
            if (mReclaimedSpritePlanes & bit) {
                DisplayPlane* plane = mSpritePlanes.itemAt(i);
                // disable plane
                plane->disable();
                // invalidate plane's data buffer cache
                plane->invalidateBufferCache();
            }
        }
        // merge into free sprite bitmap
        mFreeSpritePlanes |= mReclaimedSpritePlanes;
        mReclaimedSpritePlanes = 0;
    }

    // disable reclaimed primary planes
    if (mPrimaryPlanes.size() && mReclaimedPrimaryPlanes) {
        for (int i = 0; i < mPrimaryPlaneCount; i++) {
            int bit = (1 << i);
            if (mReclaimedPrimaryPlanes & bit) {
                DisplayPlane* plane = mPrimaryPlanes.itemAt(i);
                // disable plane
                plane->disable();
                // invalidate plane's data buffer cache
                plane->invalidateBufferCache();
            }
        }
        // merge into free sprite bitmap
        mFreePrimaryPlanes |= mReclaimedPrimaryPlanes;
        mReclaimedPrimaryPlanes = 0;
    }

    // disable reclaimed overlay planes
    if (mOverlayPlanes.size() && mReclaimedOverlayPlanes) {
        for (int i = 0; i < mOverlayPlaneCount; i++) {
            int bit = (1 << i);
            if (mReclaimedOverlayPlanes & bit) {
                DisplayPlane* plane = mOverlayPlanes.itemAt(i);
                // disable plane
                plane->disable();
                // invalidate plane's data buffer cache
                plane->invalidateBufferCache();
            }
        }
        // merge into free overlay bitmap
        mFreeOverlayPlanes |= mReclaimedOverlayPlanes;
        mReclaimedOverlayPlanes = 0;
    }
}

void DisplayPlaneManager::dump(Dump& d)
{
    d.append("Display Plane Manager state:\n");
    d.append("-------------------------------------------------------------\n");
    d.append(" PLANE TYPE | COUNT |   FREE   | RECLAIMED \n");
    d.append("------------+-------+----------+-----------\n");
    d.append("    SPRITE  |  %2d   | %08x | %08x\n",
             mSpritePlaneCount,
             mFreeSpritePlanes,
             mReclaimedSpritePlanes);
    d.append("   OVERLAY  |  %2d   | %08x | %08x\n",
             mOverlayPlaneCount,
             mFreeOverlayPlanes,
             mReclaimedOverlayPlanes);
    d.append("   PRIMARY  |  %2d   | %08x | %08x\n",
             mPrimaryPlaneCount,
             mFreePrimaryPlanes,
             mReclaimedPrimaryPlanes);
}

} // namespace intel
} // namespace android


