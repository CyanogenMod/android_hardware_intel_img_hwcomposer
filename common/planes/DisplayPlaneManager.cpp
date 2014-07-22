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
#include <IDisplayDevice.h>
#include <DisplayPlaneManager.h>

namespace android {
namespace intel {

DisplayPlaneManager::DisplayPlaneManager()
    : mTotalPlaneCount(0),
      mNativeZOrderConfig(0),
      mInitialized(false)
{
    int i;

    for (i = 0; i < DisplayPlane::PLANE_MAX; i++) {
        mPlaneCount[i] = 0;
        mFreePlanes[i] = 0;
        mReclaimedPlanes[i] = 0;
    }
}

DisplayPlaneManager::~DisplayPlaneManager()
{
    WARN_IF_NOT_DEINIT();
}

void DisplayPlaneManager::deinitialize()
{
    int i;
    size_t j;

    for (i = 0; i < DisplayPlane::PLANE_MAX; i++) {
        for (j = 0; j < mPlanes[i].size(); j++) {
            // reset plane
            DisplayPlane *plane = mPlanes[i].itemAt(j);
            plane->reset();

            DEINIT_AND_DELETE_OBJ(plane);
        }
        mPlanes[i].clear();
    }

    mNativeZOrderConfig = 0;
}

bool DisplayPlaneManager::initialize()
{
    int i, j;
    int spriteCount = 0;
    int overlayCount = 0;
    int primaryCount = 0;

    CTRACE();

    if (mInitialized) {
        WTRACE("object has been initialized");
        return true;
    }

    // detect display plane usage. Hopefully throw DRM ioctl
    if (!detect(spriteCount, overlayCount, primaryCount)) {
        ETRACE("failed to detect planes");
        return false;
    }

    // calculate total plane number and free plane bitmaps
    mPlaneCount[DisplayPlane::PLANE_SPRITE] = spriteCount;
    mPlaneCount[DisplayPlane::PLANE_OVERLAY] = overlayCount;
    mPlaneCount[DisplayPlane::PLANE_PRIMARY] = primaryCount;

    mTotalPlaneCount = spriteCount + overlayCount + primaryCount;
    for (i = 0; i < DisplayPlane::PLANE_MAX; i++) {
        mFreePlanes[i] = ((1 << mPlaneCount[i]) - 1);
    }

    // allocate plane pools
    for (i = 0; i < DisplayPlane::PLANE_MAX; i++) {
        if (mPlaneCount[i]) {
            mPlanes[i].setCapacity(mPlaneCount[i]);

            for (j = 0; j < mPlaneCount[i]; j++) {
                DisplayPlane* plane = allocPlane(j, i);
                if (!plane) {
                    ETRACE("failed to allocate plane %d, type %d", j, i);
                    DEINIT_AND_RETURN_FALSE();
                }
                mPlanes[i].push_back(plane);
            }
        }
    }

    // get native z order config
    mNativeZOrderConfig = getNativeZOrderConfig();
    if (!mNativeZOrderConfig) {
        ETRACE("failed to get native z order config");
        DEINIT_AND_RETURN_FALSE();
    }

    mInitialized = true;
    return true;
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
        WTRACE("bit %d was set", index);
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

DisplayPlane* DisplayPlaneManager::getPlane(int type, int dsp)
{
    int freePlaneIndex;

    RETURN_NULL_IF_NOT_INIT();

    if (dsp < 0 || dsp > IDisplayDevice::DEVICE_EXTERNAL) {
        ETRACE("Invalid display device %d", dsp);
        return 0;
    }

    if (type < 0 || type >= DisplayPlane::PLANE_MAX) {
        ETRACE("Invalid plane type %d", type);
        return 0;
    }

    // try to get free plane from reclaimed planes
    if (type == DisplayPlane::PLANE_PRIMARY)
        // primary planes are attached to specific displays
        freePlaneIndex = getPlane(mReclaimedPlanes[type], dsp);
    else
        freePlaneIndex = getPlane(mReclaimedPlanes[type]);

    // if has free plane, return it
    if (freePlaneIndex >= 0)
        return mPlanes[type].itemAt(freePlaneIndex);

    // failed to get a free plane from reclaimed planes, try it on free planes
    if (type == DisplayPlane::PLANE_PRIMARY)
        freePlaneIndex = getPlane(mFreePlanes[type], dsp);
    else
        freePlaneIndex = getPlane(mFreePlanes[type]);

    // found plane on free planes
    if (freePlaneIndex >= 0)
        return mPlanes[type].itemAt(freePlaneIndex);

    VTRACE("failed to get a plane, type %d", type);
    return 0;
}

void DisplayPlaneManager::putPlane(DisplayPlane& plane)
{
    int index;
    int type;

    RETURN_VOID_IF_NOT_INIT();

    index = plane.getIndex();
    type = plane.getType();

    if (type < 0 || type >= DisplayPlane::PLANE_MAX) {
        ETRACE("Invalid plane type %d", type);
        return;
    }

    putPlane(index, mFreePlanes[type]);
}

bool DisplayPlaneManager::hasFreePlanes(int type, int dsp)
{
    uint32_t freePlanes = 0;

    RETURN_FALSE_IF_NOT_INIT();

    if (dsp < 0 || dsp > IDisplayDevice::DEVICE_EXTERNAL) {
        ETRACE("Invalid display device %d", dsp);
        return 0;
    }

    if (type < 0 || type >= DisplayPlane::PLANE_MAX) {
        ETRACE("Invalid plane type %d", type);
        return 0;
    }

    if (type == DisplayPlane::PLANE_PRIMARY) {
        freePlanes = (1 << dsp) & (mFreePlanes[type] | mReclaimedPlanes[type]);
    } else {
        freePlanes = (mFreePlanes[type] | mReclaimedPlanes[type]);
    }

    return freePlanes ? true : false;
}

DisplayPlane* DisplayPlaneManager::getSpritePlane()
{
    return getPlane((int)DisplayPlane::PLANE_SPRITE);
}

DisplayPlane* DisplayPlaneManager::getOverlayPlane(int dsp)
{
    return getPlane((int)DisplayPlane::PLANE_OVERLAY, dsp);
}

DisplayPlane* DisplayPlaneManager::getPrimaryPlane(int dsp)
{
    return getPlane((int)DisplayPlane::PLANE_PRIMARY, dsp);
}

bool DisplayPlaneManager::hasFreeSprite()
{
    return hasFreePlanes((int)DisplayPlane::PLANE_SPRITE);
}

bool DisplayPlaneManager::hasFreeOverlay()
{
    return hasFreePlanes((int)DisplayPlane::PLANE_OVERLAY);
}

bool DisplayPlaneManager::hasFreePrimary(int dsp)
{
    return hasFreePlanes((int)DisplayPlane::PLANE_PRIMARY, dsp);
}

void DisplayPlaneManager::reclaimPlane(DisplayPlane& plane)
{
    RETURN_VOID_IF_NOT_INIT();

    int index = plane.getIndex();
    int type = plane.getType();

    ATRACE("reclaimPlane = %d, type = %d", index, plane.getType());

    if (type < 0 || type >= DisplayPlane::PLANE_MAX) {
        ETRACE("Invalid plane type %d", type);
        return;
    }

    putPlane(index, mReclaimedPlanes[type]);

    // NOTE: don't invalidate plane's data cache here because the reclaimed
    // plane might be re-assigned to the same layer later
}

void DisplayPlaneManager::disableReclaimedPlanes()
{
    int i, j;
    bool ret;

    RETURN_VOID_IF_NOT_INIT();

    for (i = 0; i < DisplayPlane::PLANE_MAX; i++) {
        // disable reclaimed planes
        if (mReclaimedPlanes[i]) {
            for (j = 0; j < mPlaneCount[i]; j++) {
                int bit = (1 << j);
                if (mReclaimedPlanes[i] & bit) {
                    DisplayPlane* plane = mPlanes[i].itemAt(j);
                    // disable plane first
                    ret = plane->disable();
                    // reset plane
                    if (ret)
                        ret = plane->reset();
                    if (ret) {
                        // only merge into free bitmap if it is successfully disabled and reset
                        // otherwise, plane will be disabled and reset again.
                        mFreePlanes[i] |=bit;
                        mReclaimedPlanes[i] &= ~bit;
                    }
                }
            }
        }
    }
}

void DisplayPlaneManager::disableOverlayPlanes()
{
    for (int i = 0; i < DisplayPlane::PLANE_MAX; i++) {
        for (int j = 0; j < mPlaneCount[i]; j++) {
            DisplayPlane* plane = (DisplayPlane *)mPlanes[i][j];
            if (plane && plane->getType() == DisplayPlane::PLANE_OVERLAY) {
                plane->disable();
            }
        }
    }
}

bool DisplayPlaneManager::setZOrderConfig(ZOrderConfig& zorderConfig)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (!zorderConfig.size()) {
        WTRACE("No zorder config, should NOT happen");
        return false;
    }

    if (!isValidZOrderConfig(zorderConfig)) {
        VTRACE("Invalid z order config");
        return false;
    }

    // setup plane's z order
    for (size_t i = 0; i < zorderConfig.size(); i++) {
        DisplayPlane *plane = zorderConfig.itemAt(i);
        plane->setZOrderConfig(zorderConfig, mNativeZOrderConfig);
    }

    return true;
}

void* DisplayPlaneManager::getZOrderConfig() const
{
    RETURN_NULL_IF_NOT_INIT();

    return mNativeZOrderConfig;
}

void DisplayPlaneManager::dump(Dump& d)
{
    d.append("Display Plane Manager state:\n");
    d.append("-------------------------------------------------------------\n");
    d.append(" PLANE TYPE | COUNT |   FREE   | RECLAIMED \n");
    d.append("------------+-------+----------+-----------\n");
    d.append("    SPRITE  |  %2d   | %08x | %08x\n",
             mPlaneCount[DisplayPlane::PLANE_SPRITE],
             mFreePlanes[DisplayPlane::PLANE_SPRITE],
             mReclaimedPlanes[DisplayPlane::PLANE_SPRITE]);
    d.append("   OVERLAY  |  %2d   | %08x | %08x\n",
             mPlaneCount[DisplayPlane::PLANE_OVERLAY],
             mFreePlanes[DisplayPlane::PLANE_OVERLAY],
             mReclaimedPlanes[DisplayPlane::PLANE_OVERLAY]);
    d.append("   PRIMARY  |  %2d   | %08x | %08x\n",
             mPlaneCount[DisplayPlane::PLANE_PRIMARY],
             mFreePlanes[DisplayPlane::PLANE_PRIMARY],
             mReclaimedPlanes[DisplayPlane::PLANE_PRIMARY]);
}

} // namespace intel
} // namespace android


