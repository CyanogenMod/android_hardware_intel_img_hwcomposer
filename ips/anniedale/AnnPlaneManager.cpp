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
#include <utils/String8.h>
#include <anniedale/AnnPlaneManager.h>
#include <anniedale/AnnRGBPlane.h>
#include <anniedale/AnnOverlayPlane.h>
#include <PlaneCapabilities.h>

namespace android {
namespace intel {


struct PlaneDescription {
    char nickname;
    int type;
    int index;
};


static PlaneDescription PLANE_DESC[] =
{
    // nickname must be continous and start with 'A',
    // it is used to fast locate plane index and type
    {'A', DisplayPlane::PLANE_PRIMARY, 0},
    {'B', DisplayPlane::PLANE_PRIMARY, 1},
    {'C', DisplayPlane::PLANE_PRIMARY, 2},
    {'D', DisplayPlane::PLANE_SPRITE,  0},
    {'E', DisplayPlane::PLANE_SPRITE,  1},
    {'F', DisplayPlane::PLANE_SPRITE,  2},
    {'G', DisplayPlane::PLANE_OVERLAY, 0},  // nickname for Overlay A
    {'H', DisplayPlane::PLANE_OVERLAY, 1}   // nickname for Overlay C
};


struct ZOrderDescription {
    int index;  // based on overlay position
    const char *zorder;
};

// If overlay is in the bottom of Z order, two legitimate combinations are Oa, D, E, F
// and Oc, D, E, F. However, plane A has to be part of the blending chain as it can't
//  be disabled [HW bug]. The only legitimate combinations including overlay and plane A is:
// A, Oa, E, F
// A, Oc, E, F
#define OVERLAY_HW_WORKAROUND


static ZOrderDescription PIPE_A_ZORDER_DESC[] =
{
    {0, "ADEF"},  // no overlay
#ifndef OVERLAY_HW_WORKAROUND
    {1, "GDEF"},  // overlay A at bottom (1 << 0)
    {1, "HDEF"},  // overlay C at bottom (1 << 0)
#else
    {1, "GEF"},  // overlay A at bottom (1 << 0)
    {1, "HEF"},  // overlay C at bottom (1 << 0)
#endif
    {2, "AGEF"},  // overlay A at next to bottom (1 << 1)
    {2, "AHEF"},  // overlay C at next to bottom (1 << 1)
#ifndef OVERLAY_HW_WORKAROUND
    {3, "GHEF"},  // overlay A, C at bottom
#else
    {3, "GHF"},   // overlay A, C at bottom
#endif
    {4, "ADGF"},  // overlay A at next to top (1 << 2)
    {4, "ADHF"},  // overlay C at next to top (1 << 2)
    {6, "AGHF"},  // overlay A, C in between
    {8, "ADEG"},  // overlay A at top (1 << 3)
    {8, "ADEH"},  // overlay C at top (1 <<3)
    {12, "ADGH"}  // overlay A, C at top
};

// use overlay C over overlay A if possible on pipe B
// workaround: use only overlay C on pipe B
static ZOrderDescription PIPE_B_ZORDER_DESC[] =
{
    {0, "BD"},    // no overlay
    {1, "HBD"},   // overlay C at bottom (1 << 0)
//    {1, "GBD"},   // overlay A at bottom (1 << 0)
    {2, "BHD"},   // overlay C at middle (1 << 1)
//   {2, "BGD"},   // overlay A at middle (1 << 1)
    {3, "GHBD"},  // overlay A and C at bottom ( 1 << 0 + 1 << 1)
    {4, "BDH"},   // overlay C at top (1 << 2)
    {4, "BDG"},   // overlay A at top (1 << 2)
    {6, "BGHD"},  // overlay A/C at middle  1 << 1 + 1 << 2)
    {12, "BDGH"}  // overlay A/C at top (1 << 2 + 1 << 3)
};

static const int PIPE_A_ZORDER_COMBINATIONS =
        sizeof(PIPE_A_ZORDER_DESC)/sizeof(ZOrderDescription);
static const int PIPE_B_ZORDER_COMBINATIONS =
        sizeof(PIPE_B_ZORDER_DESC)/sizeof(ZOrderDescription);

AnnPlaneManager::AnnPlaneManager()
    : DisplayPlaneManager()
{
}

AnnPlaneManager::~AnnPlaneManager()
{
}

bool AnnPlaneManager::initialize()
{
    mSpritePlaneCount = 3;  // Sprite D, E, F
    mOverlayPlaneCount = 2; // Overlay A, C
    mPrimaryPlaneCount = 3; // Primary A, B, C

    return DisplayPlaneManager::initialize();
}

void AnnPlaneManager::deinitialize()
{
    DisplayPlaneManager::deinitialize();
}

DisplayPlane* AnnPlaneManager::allocPlane(int index, int type)
{
    DisplayPlane *plane = NULL;

    switch (type) {
    case DisplayPlane::PLANE_PRIMARY:
        plane = new AnnRGBPlane(index, DisplayPlane::PLANE_PRIMARY, index/*disp*/);
        break;
    case DisplayPlane::PLANE_SPRITE:
        plane = new AnnRGBPlane(index, DisplayPlane::PLANE_SPRITE, 0/*disp*/);
        break;
    case DisplayPlane::PLANE_OVERLAY:
        plane = new AnnOverlayPlane(index, 0/*disp*/);
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

bool AnnPlaneManager::isValidZOrder(int dsp, ZOrderConfig& config)
{
    int size = (int)config.size();

    if (size == 0 || size > 4) {
        VTRACE("invalid z order config size %d", size);
        return false;
    }

    if (dsp == IDisplayDevice::DEVICE_PRIMARY) {
        int firstOverlay = -1;
        for (int i = 0; i < size; i++) {
            if (config[i]->planeType == DisplayPlane::PLANE_OVERLAY) {
                firstOverlay = i;
                break;
            }
        }
#ifdef OVERLAY_HW_WORKAROUND
        if (firstOverlay == 0 && size > 3) {
            VTRACE("not capable to support 3 sprite layers on top of overlay");
            return false;
        }
#endif
    } else if (dsp == IDisplayDevice::DEVICE_EXTERNAL) {
        int sprites = 0;
        for (int i = 0; i < size; i++) {
            if (config[i]->planeType != DisplayPlane::PLANE_OVERLAY) {
                sprites++;
            }
        }
        if (sprites > 2) {
            ETRACE("number of sprite: %d, maximum 1 sprite and 1 primary supported on pipe 1", sprites);
            return false;
        }
    } else {
        ETRACE("invalid display device %d", dsp);
        return false;
    }
    return true;
}

bool AnnPlaneManager::assignPlanes(int dsp, ZOrderConfig& config)
{
    if (dsp < 0 || dsp > IDisplayDevice::DEVICE_EXTERNAL) {
        ETRACE("invalid display device %d", dsp);
        return false;
    }

    int size = (int)config.size();

    // calculate index based on overlay Z order position
    int index = 0;
    for (int i = 0; i < size; i++) {
        if (config[i]->planeType == DisplayPlane::PLANE_OVERLAY) {
            index += (1 << i);
        }
    }

    int combinations = 0;
    if (dsp == IDisplayDevice::DEVICE_PRIMARY)
        combinations = PIPE_A_ZORDER_COMBINATIONS;
    else
        combinations = PIPE_B_ZORDER_COMBINATIONS;

    ZOrderDescription *zorderDesc = NULL;
    for (int i = 0; i < combinations; i++) {
        if (dsp == IDisplayDevice::DEVICE_PRIMARY)
            zorderDesc = &PIPE_A_ZORDER_DESC[i];
        else
            zorderDesc = &PIPE_B_ZORDER_DESC[i];

        if (zorderDesc->index != index)
            continue;

        if (assignPlanes(dsp, config, zorderDesc->zorder)) {
            VTRACE("zorder assigned %s", zorderDesc->zorder);
            return true;
        }
    }
    return false;
}

bool AnnPlaneManager::assignPlanes(int dsp, ZOrderConfig& config, const char *zorder)
{
    int size = (int)config.size();

    if (zorder == NULL || size > (int)strlen(zorder)) {
        //DTRACE("invalid zorder or ZOrder config.");
        return false;
    }

    // test if plane is avalable
    for (int i = 0; i < size; i++) {
        char id = *(zorder + i);
        PlaneDescription& desc = PLANE_DESC[id - 'A'];
        if (!isFreePlane(desc.type, desc.index)) {
            DTRACE("plane type %d index %d is not available", desc.type, desc.index);
            return false;
        }

#if 0
        // plane type check
        if (config[i]->planeType == DisplayPlane::PLANE_OVERLAY &&
            desc.type != DisplayPlane::PLANE_OVERLAY) {
            ETRACE("invalid plane type %d, expected %d", desc.type, config[i]->planeType);
            return false;
        }

        if (config[i]->planeType != DisplayPlane::PLANE_OVERLAY) {
            if (config[i]->planeType != DisplayPlane::PLANE_PRIMARY &&
                config[i]->planeType != DisplayPlane::PLANE_SPRITE) {
                ETRACE("invalid plane type %d,", config[i]->planeType);
                return false;
            }
            if (desc.type != DisplayPlane::PLANE_PRIMARY &&
                desc.type != DisplayPlane::PLANE_SPRITE) {
                ETRACE("invalid plane type %d, expected %d", desc.type, config[i]->planeType);
                return false;
            }
        }
#endif

        if  (desc.type == DisplayPlane::PLANE_OVERLAY && desc.index == 1 &&
             config[i]->hwcLayer->getTransform() != 0) {
            DTRACE("overlay C does not support transform");
            return false;
        }
    }

    bool primaryPlaneActive = false;
    // allocate planes
    for (int i = 0; i < size; i++) {
        char id = *(zorder + i);
        PlaneDescription& desc = PLANE_DESC[id - 'A'];
        ZOrderLayer *zLayer = config.itemAt(i);
        zLayer->plane = getPlane(desc.type, desc.index);
        if (zLayer->plane == NULL) {
            ETRACE("failed to get plane, should never happen!");
        }
        // override type
        zLayer->planeType = desc.type;
        if (desc.type == DisplayPlane::PLANE_PRIMARY) {
            primaryPlaneActive = true;
        }
    }

    // setup Z order
    int slot = 0;
    for (int i = 0; i < size; i++) {
        slot = i;

#ifdef OVERLAY_HW_WORKAROUND
        if (!primaryPlaneActive && config[i]->planeType == DisplayPlane::PLANE_OVERLAY) {
            slot += 1;
        }
#endif

        config[i]->plane->setZOrderConfig(config, (void *)slot);
        config[i]->plane->enable();
    }

#if 0
    DTRACE("config size %d, zorder %s", size, zorder);
    for (int i = 0; i < size; i++) {
        const ZOrderLayer *l = config.itemAt(i);
        ITRACE("%d: plane type %d, index %d, zorder %d",
            i, l->planeType, l->plane->getIndex(), l->zorder);
    }
#endif

    return true;
}

void* AnnPlaneManager::getZOrderConfig() const
{
    return NULL;
}

int AnnPlaneManager::getFreePlanes(int dsp, int type)
{
    RETURN_NULL_IF_NOT_INIT();

    if (type != DisplayPlane::PLANE_SPRITE) {
        return DisplayPlaneManager::getFreePlanes(dsp, type);
    }

    if (dsp < 0 || dsp > IDisplayDevice::DEVICE_EXTERNAL) {
        ETRACE("invalid display device %d", dsp);
        return 0;
    }

    uint32_t freePlanes = mFreePlanes[type] | mReclaimedPlanes[type];
    int start = 0;
    int stop = mSpritePlaneCount;
    if (dsp == IDisplayDevice::DEVICE_EXTERNAL) {
        // only Sprite D (index 0) can be assigned to pipe 1
        // Sprites E/F (index 1, 2) are fixed on pipe 0
        stop = 1;
    }
    int count = 0;
    for (int i = start; i < stop; i++) {
        if ((1 << i) & freePlanes) {
            count++;
        }
    }
    return count;
}

} // namespace intel
} // namespace android

