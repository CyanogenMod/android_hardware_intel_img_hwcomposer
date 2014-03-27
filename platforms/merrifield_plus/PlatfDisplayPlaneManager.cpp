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
#include <anniedale/AnnRGBPlane.h>
#include <anniedale/AnnDisplayPlane.h>
#include <anniedale/AnnOverlayPlane.h>

namespace android {
namespace intel {

// hardware z order configs
static const uint32_t PRIMARY_ZORDER_CONFIG[2][4] = {
    {1 << 0, 1 << 3, 1 << 4,  1 << 5},
    {1 << 0, 1 << 0, 1 << 0,  1 << 0},
};

static const uint32_t EXTERNAL_ZORDER_CONFIG[2][4] = {
    {1 << 1, (1 << 1) | (1 << 3), (1 << 1) | (1 << 3),  1 << 3},
    {1 << 1, 1 << 1, 1 << 1,  1 << 1},
};

PlatfDisplayPlaneManager::PlatfDisplayPlaneManager()
    : DisplayPlaneManager(),
      mExtraSpriteCount(EXTRA_SPRITE_COUNT),
      mExtraSprites(),
      mFreeExtraSprites(0),
      mReclaimedExtraSprites(0)
{
    CTRACE();
    memset(&mZorder, 1, sizeof(mZorder));

    for (int i = 0; i < REAL_PLANE_MAX; i++) {
        mFreeRealPlanes[i] = 0;
        mReclaimedRealPlanes[i] = 0;
        mRealPlaneCounts[i] = 0;
    }
}

PlatfDisplayPlaneManager::~PlatfDisplayPlaneManager()
{

}

bool PlatfDisplayPlaneManager::initialize()
{
    bool ret;
    int i;

    ret = DisplayPlaneManager::initialize();
    if (ret == false) {
        ETRACE("failed to initialize DisplayPlaneManager");
        return false;
    }

    // init extra sprites
    mFreeExtraSprites = ((1 << mExtraSpriteCount) - 1);
    mExtraSprites.setCapacity(mExtraSpriteCount);

    // init free real planes;
    mRealPlaneCounts[REAL_PLANE_SPRITE] =
        PRIMARY_COUNT + SPRITE_COUNT + EXTRA_SPRITE_COUNT;
    mRealPlaneCounts[REAL_PLANE_OVERLAY] = OVERLAY_COUNT;

    // allocate and reset extra sprites
    for (i = 0; i < mExtraSpriteCount; i++) {
        // allocate extra sprites, plane indexes start from SPRITE_COUNT
        DisplayPlane* plane = allocPlane((SPRITE_COUNT + i),
                                         DisplayPlane::PLANE_SPRITE);
        if (!plane) {
                ETRACE("failed to allocate extra sprite %d", (SPRITE_COUNT + i));
                DEINIT_AND_RETURN_FALSE();
            }
            // reset plane
            plane->reset();
            mExtraSprites.push_back(plane);
    }

    // create real planes
    for (i = 0; i < REAL_PLANE_MAX; i++) {
        mFreeRealPlanes[i] = ((1 << mRealPlaneCounts[i]) - 1);
        mRealPlanes[i].setCapacity(mRealPlaneCounts[i]);
    }

    for (i = 0; i < PRIMARY_COUNT; i++) {
        DisplayPlane *plane = new AnnRGBPlane(i, DisplayPlane::PLANE_PRIMARY, i);
        if (!plane->initialize(DisplayPlane::MIN_DATA_BUFFER_COUNT)) {
            ETRACE("failed to initialize primary plane %d", i);
            DEINIT_AND_RETURN_FALSE();
        }
        mRealPlanes[REAL_PLANE_SPRITE].push_back(plane);
    }

    for (i = 0; i < SPRITE_COUNT + EXTRA_SPRITE_COUNT; i++) {
        DisplayPlane *plane = new AnnRGBPlane(i, DisplayPlane::PLANE_SPRITE, 0);
        if (!plane->initialize(DisplayPlane::MIN_DATA_BUFFER_COUNT)) {
            ETRACE("failed to initialize sprite plane %d", i);
            DEINIT_AND_RETURN_FALSE();
        }
        mRealPlanes[REAL_PLANE_SPRITE].push_back(plane);
    }

    for (i = 0; i < OVERLAY_COUNT; i++) {
        DisplayPlane *plane = new AnnOverlayPlane(i, 0);
        if (!plane->initialize(DisplayPlane::MIN_DATA_BUFFER_COUNT)) {
            ETRACE("failed to initialize overlay plane %d", i);
            DEINIT_AND_RETURN_FALSE();
        }
        mRealPlanes[REAL_PLANE_OVERLAY].push_back(plane);
    }

    return true;
}

void PlatfDisplayPlaneManager::deinitialize()
{
    for (size_t i = 0; i < mExtraSprites.size(); i++) {
        // reset plane
        DisplayPlane *plane = mExtraSprites.itemAt(i);
        plane->reset();
        DEINIT_AND_DELETE_OBJ(plane);
    }
    mExtraSprites.clear();

    DisplayPlaneManager::deinitialize();
}

void* PlatfDisplayPlaneManager::getZOrderConfig() const
{
    // HACK: on Annidale, Z order config is not needed by kernel driver
    return NULL;
}

DisplayPlane* PlatfDisplayPlaneManager::getSpritePlane(int dsp)
{
    DisplayPlane *plane = DisplayPlaneManager::getSpritePlane(dsp);
    if (plane) {
        VTRACE("Get sprite plane");
        return plane;
    }

    if (dsp != IDisplayDevice::DEVICE_PRIMARY) {
        return NULL;
    }

    // for primary display we have extra sprites
    int freePlaneIndex = -1;

    freePlaneIndex = getPlane(mReclaimedExtraSprites);
    if (freePlaneIndex >= 0) {
        VTRACE("Get extra sprite plane on reclaimed plane list");
        return mExtraSprites.itemAt(freePlaneIndex);
    }

    freePlaneIndex = getPlane(mFreeExtraSprites);
    if (freePlaneIndex >= 0) {
        VTRACE("Get extra sprite plane on free plane list");
        return mExtraSprites.itemAt(freePlaneIndex);
    }

    VTRACE("failed to get extra sprite");

    return NULL;
}

void PlatfDisplayPlaneManager::putPlane(int dsp, DisplayPlane& plane)
{
    int index;
    int type;

    RETURN_VOID_IF_NOT_INIT();

    index = plane.getIndex();
    type = plane.getType();

    if (type != DisplayPlane::PLANE_SPRITE || index < SPRITE_COUNT) {
        DisplayPlaneManager::putPlane(dsp, plane);
        return;
    }

    if (dsp != IDisplayDevice::DEVICE_PRIMARY)
        WTRACE("putting a plane which doesn't belong to device %d", dsp);

    // put it back into free extra sprites
    index -= SPRITE_COUNT;
    DisplayPlaneManager::putPlane(index, mFreeExtraSprites);
}

bool PlatfDisplayPlaneManager::hasFreeSprite(int dsp)
{
    bool ret = DisplayPlaneManager::hasFreeSprite(dsp);
    if (ret == true) {
        VTRACE("DisplayPlaneManager has free sprite");
        return ret;
    }

    if (dsp != IDisplayDevice::DEVICE_PRIMARY) {
        return false;
    }

    // extra sprites is available for primary device
    uint32_t freePlanes = (mFreeExtraSprites | mReclaimedExtraSprites);
    return freePlanes ? true : false;
}

void PlatfDisplayPlaneManager::reclaimPlane(int dsp, DisplayPlane& plane)
{
    AnnDisplayPlane *annPlane;
    int index;
    int type;

    RETURN_VOID_IF_NOT_INIT();

    index = plane.getIndex();
    type = plane.getType();

    // reclaim real plane
    annPlane = reinterpret_cast<AnnDisplayPlane *>(&plane);
    reclaimRealPlane(annPlane->getRealPlane());

    // clear this slot
    annPlane->setRealPlane(0);

    if (type != DisplayPlane::PLANE_SPRITE || index < SPRITE_COUNT) {
        DisplayPlaneManager::reclaimPlane(dsp, plane);
        return;
    }

    if (dsp != IDisplayDevice::DEVICE_PRIMARY)
        WTRACE("reclaiming a plane which doesn't belong to device %d", dsp);

    index -= SPRITE_COUNT;
    DisplayPlaneManager::putPlane(index, mReclaimedExtraSprites);
}

void PlatfDisplayPlaneManager::disableReclaimedPlanes()
{
    int i, j;
    bool ret;

    RETURN_VOID_IF_NOT_INIT();

    // merge into free bitmap
    for (i = 0; i < DisplayPlane::PLANE_MAX; i++) {
        // disable reclaimed planes
        if (mReclaimedPlanes[i]) {
            mFreePlanes[i] |= mReclaimedPlanes[i];
            mReclaimedPlanes[i] = 0;
        }
    }

    if (mReclaimedExtraSprites) {
        mFreeExtraSprites |= mReclaimedExtraSprites;
        mReclaimedExtraSprites = 0;
    }

    // disable reclaimed real planes
    disableReclaimedRealPlanes();
}

// WA for HW issue
bool PlatfDisplayPlaneManager::primaryPlaneActive(ZOrderConfig& zorderConfig)
{
    for (size_t i = 0; i < zorderConfig.size(); i++) {
        DisplayPlane *plane = zorderConfig.itemAt(i);
        if (!plane) {
            continue;
        }

        if (plane->getType() == DisplayPlane::PLANE_PRIMARY) {
            return true;
        }
    }
    return false;
}

bool PlatfDisplayPlaneManager::setZOrderConfig(ZOrderConfig& zorderConfig)
{
    AnnDisplayPlane *annPlane;

    RETURN_FALSE_IF_NOT_INIT();

    if (!zorderConfig.size()) {
        WTRACE("No zorder config, should NOT happen");
        return false;
    }

    if (!isValidZOrderConfig(zorderConfig)) {
        VTRACE("Invalid z order config");
        return false;
    }

    for (size_t i = 0; i < zorderConfig.size(); i++) {
        DisplayPlane *plane = zorderConfig.itemAt(i);
        if (!plane) {
            ETRACE("no plane found in zorder config");
            return false;
        }

        // reclaim plane's existing realPlane
        annPlane = reinterpret_cast<AnnDisplayPlane*>(plane);
        if (annPlane->getRealPlane()) {
            reclaimRealPlane(annPlane->getRealPlane());
        }

        int slot = i;
        // WA for HW issue
        if (!primaryPlaneActive(zorderConfig)) {
            ITRACE("primary plane is NOT active");
            slot += 1;
        }

        DisplayPlane *realPlane = getRealPlane(zorderConfig.getDisplayDevice(),
                                               plane->getType(), slot);
        if (!realPlane) {
            ETRACE("failed to get real plane");
            return false;
        }

        realPlane->setZOrderConfig(zorderConfig, (void *)i);

        // TODO: check enable failure
        realPlane->enable();

        annPlane->setRealPlane(realPlane);
    }

    VTRACE("dump start:");
    for (size_t i = 0; i < zorderConfig.size(); i++) {
        DisplayPlane *plane = zorderConfig.itemAt(i);
        AnnDisplayPlane *annPlane = reinterpret_cast<AnnDisplayPlane*>(plane);
        DisplayPlane *realPlane = annPlane->getRealPlane();
        VTRACE("slot %d, plane (type = %d, idx = %d)", i, realPlane->getType(), realPlane->getIndex());
    }
    VTRACE("dump end!");
    return true;
}

bool PlatfDisplayPlaneManager::detect(int&spriteCount,
                                           int& overlayCount,
                                           int& primaryCount)
{
    CTRACE();

    spriteCount = SPRITE_COUNT;
    overlayCount = OVERLAY_COUNT;
    primaryCount = PRIMARY_COUNT;

    return true;
}

DisplayPlane* PlatfDisplayPlaneManager::allocPlane(int index, int type)
{
    DisplayPlane *plane = 0;
    ATRACE("index = %d, type = %d", index, type);

    switch (type) {
    case DisplayPlane::PLANE_PRIMARY:
        plane = new AnnDisplayPlane(index, DisplayPlane::PLANE_PRIMARY, index);
        break;
    case DisplayPlane::PLANE_SPRITE:
        plane = new AnnDisplayPlane(index, DisplayPlane::PLANE_SPRITE, 0);
        break;
    case DisplayPlane::PLANE_OVERLAY:
        plane = new AnnDisplayPlane(index, DisplayPlane::PLANE_OVERLAY, 0);
        break;
    default:
        ETRACE("unsupported type %d", type);
        break;
    }

    if (plane && !plane->initialize(DisplayPlane::MIN_DATA_BUFFER_COUNT)) {
        ETRACE("failed to initialize plane.");
        DEINIT_AND_DELETE_OBJ(plane);
    }

    return plane;
}

bool PlatfDisplayPlaneManager::isValidZOrderConfig(ZOrderConfig& config)
{
    // check whether it's a supported z order config
    if (!config.size() || config.size() > 4) {
        ETRACE("Invalid z order config. size %d", config.size());
        return false;
    }

    return true;
}

void* PlatfDisplayPlaneManager::getNativeZOrderConfig()
{
    return &mZorder;
}

DisplayPlane* PlatfDisplayPlaneManager::getRealPlane(int dsp,
                                                     int type, int slot)
{
    uint32_t *reclaimedRealPlanes;
    uint32_t *freeRealPlanes;
    uint32_t possiblePlanes;
    int realType;
    int index;

    RETURN_NULL_IF_NOT_INIT();

    if (dsp < 0 || dsp > IDisplayDevice::DEVICE_EXTERNAL) {
        ETRACE("Invalid display device %d", dsp);
        return 0;
    }

    if (type < 0 || type >= DisplayPlane::PLANE_MAX) {
        ETRACE("Invalid plane type %d", type);
        return 0;
    }

    switch (type) {
    case DisplayPlane::PLANE_OVERLAY:
        realType = REAL_PLANE_OVERLAY;
        break;
    case DisplayPlane::PLANE_PRIMARY:
    case DisplayPlane::PLANE_SPRITE:
        realType = REAL_PLANE_SPRITE;
        break;
    default:
        ETRACE("Invalid plane type %d", type);
        return 0;
    }

    // get the possible plane candidates for this slot
    switch (dsp) {
    case IDisplayDevice::DEVICE_PRIMARY:
        possiblePlanes = PRIMARY_ZORDER_CONFIG[realType][slot];
        break;
    case IDisplayDevice::DEVICE_EXTERNAL:
        possiblePlanes = EXTERNAL_ZORDER_CONFIG[realType][slot];
        break;
    default:
        ETRACE("Unsupported display device %d", dsp);
        return 0;
    }

    reclaimedRealPlanes = &mReclaimedRealPlanes[realType];
    freeRealPlanes = &mFreeRealPlanes[realType];

    // allocate plane for this slot following an order from bottom to top
    for (index = 0; index < 32; index++) {
        int bit = (1 << index);
        if (!(possiblePlanes & bit))
            continue;

        // try to get it from reclaimed planes
        if (*reclaimedRealPlanes & bit) {
            // got plane from reclaimed planes
            VTRACE("acquired plane (type = %d) for slot %d on reclaimed planes "
                   "real index %d", type, slot, index);
            *reclaimedRealPlanes &= ~bit;
            return mRealPlanes[realType].itemAt(index);
        }

        // try to get it from free planes
        if (*freeRealPlanes & bit) {
            VTRACE("acquired plane (type = %d) for slot %d on free planes "
                   "real index %d", type, slot, index);
            *freeRealPlanes &= ~bit;
            return mRealPlanes[realType].itemAt(index);
        }
    }

    WTRACE("failed to acquire plane (type = %d) for slot %d", type, slot);
    return 0;
}

void PlatfDisplayPlaneManager::reclaimRealPlane(DisplayPlane *plane)
{
    uint32_t *reclaimedRealPlanes;
    int index;
    int type, realType;

    RETURN_VOID_IF_NOT_INIT();

    if (!plane) {
        VTRACE("Invalid plane");
        return;
    }

    index = plane->getIndex();
    type = plane->getType();

    switch (type) {
    case DisplayPlane::PLANE_OVERLAY:
        realType = REAL_PLANE_OVERLAY;
        break;
    case DisplayPlane::PLANE_PRIMARY:
        realType = REAL_PLANE_SPRITE;
        break;
    case DisplayPlane::PLANE_SPRITE:
        index += PRIMARY_COUNT;
        realType = REAL_PLANE_SPRITE;
        break;
    default:
        ETRACE("Invalid plane type %d", type);
        return;
    }

    reclaimedRealPlanes = &mReclaimedRealPlanes[realType];
    *reclaimedRealPlanes |= (1 << index);

    VTRACE("reclaimed real plane type %d, index %d", type, index);
}

void PlatfDisplayPlaneManager::disableReclaimedRealPlanes()
{
    uint32_t i, j;
    bool ret = false;

    RETURN_VOID_IF_NOT_INIT();

    for (i = 0; i < REAL_PLANE_MAX; i++) {
        if (mReclaimedRealPlanes[i]) {
            for (j = 0; j < mRealPlaneCounts[i]; j++) {
                int bit = (1 << j);

                if (mReclaimedRealPlanes[i] & bit) {
                    DisplayPlane* plane = mRealPlanes[i].itemAt(j);
                    // check plane state first
                    ret = plane->isDisabled();
                    // reset plane
                    if (ret)
                        ret = plane->reset();
                }
            }

            if (ret) {
                // merge into free bitmap
                mFreeRealPlanes[i] |= mReclaimedRealPlanes[i];
                mReclaimedRealPlanes[i] = 0;
            }
        }
    }
}

} // namespace intel
} // namespace android

