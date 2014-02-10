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
#ifndef PLATF_DISPLAY_PLANE_MANAGER_H
#define PLATF_DISPLAY_PLANE_MANAGER_H

#include <DisplayPlaneManager.h>
#include <linux/psb_drm.h>

namespace android {
namespace intel {

class PlatfDisplayPlaneManager : public DisplayPlaneManager {
    enum {
        SPRITE_COUNT = 1,
        OVERLAY_COUNT = 2,
        PRIMARY_COUNT = 3,
        EXTRA_SPRITE_COUNT = 2,
    };
private:
    enum {
        REAL_PLANE_SPRITE = 0,
        REAL_PLANE_OVERLAY,
        REAL_PLANE_MAX,
    };
public:
    PlatfDisplayPlaneManager();
    virtual ~PlatfDisplayPlaneManager();

    bool initialize();
    void deinitialize();
    void* getZOrderConfig() const;
    // override get sprite plane
    DisplayPlane* getSpritePlane(int dsp);
    // override put plane
    void putPlane(int dsp, DisplayPlane& plane);
    // override has free sprite
    bool hasFreeSprite(int dsp);
    // override reclaim plane
    void reclaimPlane(int dsp, DisplayPlane& plane);
    // override disable reclaimed planes
    void disableReclaimedPlanes();
    // override
    bool setZOrderConfig(ZOrderConfig& zorderConfig);
protected:
    bool detect(int&spriteCount, int& overlayCount, int& primaryCount);
    DisplayPlane* allocPlane(int index, int type);
    bool isValidZOrderConfig(ZOrderConfig& zorderConfig);
    void* getNativeZOrderConfig();
private:
    DisplayPlane* getRealPlane(int dsp, int type, int slot);
    void reclaimRealPlane(DisplayPlane *plane);
    void disableReclaimedRealPlanes();
    bool primaryPlaneActive(ZOrderConfig& zorderConfig);
private:
    int mExtraSpriteCount;
    Vector<DisplayPlane*> mExtraSprites;
    uint32_t mFreeExtraSprites;
    uint32_t mReclaimedExtraSprites;

    uint32_t mFreeRealPlanes[REAL_PLANE_MAX];
    uint32_t mReclaimedRealPlanes[REAL_PLANE_MAX];
    uint32_t mRealPlaneCounts[REAL_PLANE_MAX];
    Vector<DisplayPlane*> mRealPlanes[REAL_PLANE_MAX];

    struct intel_dc_plane_zorder mZorder;
};

} // namespace intel
} // namespace android


#endif /* PLATF_DISPLAY_PLANE_MANAGER_H */
