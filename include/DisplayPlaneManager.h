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
#ifndef DISPLAYPLANEMANAGER_H_
#define DISPLAYPLANEMANAGER_H_

#include <Dump.h>
#include <DisplayPlane.h>
#include <utils/Vector.h>

namespace android {
namespace intel {

class DisplayPlaneManager {
    enum {
        PLANE_ON_RECLAIMED_LIST = 1,
        PLANE_ON_FREE_LIST,
    };
public:
    DisplayPlaneManager();
    virtual ~DisplayPlaneManager();

    bool initCheck() const { return mInitialized; }
    virtual bool initialize();
    virtual void deinitialize();

    // plane allocation & free
    DisplayPlane* getSpritePlane();
    DisplayPlane* getPrimaryPlane(int pipe);
    DisplayPlane* getOverlayPlane();
    void putSpritePlane(DisplayPlane& plane);
    void putOverlayPlane(DisplayPlane& plane);

    bool hasFreeSprites();
    bool hasFreeOverlays();
    int getFreeSpriteCount() const;
    int getFreeOverlayCount() const;

    bool hasReclaimedOverlays();
    bool primaryAvailable(int index);

    void reclaimPlane(DisplayPlane& plane);
    void disableReclaimedPlanes();

    // dump interface
    void dump(Dump& d);
protected:
    int getPlane(uint32_t& mask);
    int getPlane(uint32_t& mask, int index);
    void putPlane(int index, uint32_t& mask);

    // sub-classes need implement follow functions
    virtual void detect() = 0;
    virtual DisplayPlane* allocPlane(int index, int type) = 0;
protected:
    int mSpritePlaneCount;
    int mPrimaryPlaneCount;
    int mOverlayPlaneCount;
    int mTotalPlaneCount;

    Vector<DisplayPlane*> mSpritePlanes;
    Vector<DisplayPlane*> mPrimaryPlanes;
    Vector<DisplayPlane*> mOverlayPlanes;

    // Bitmap of free planes. Bit0 - plane A, bit 1 - plane B, etc.
    uint32_t mFreeSpritePlanes;
    uint32_t mFreePrimaryPlanes;
    uint32_t mFreeOverlayPlanes;
    uint32_t mReclaimedSpritePlanes;
    uint32_t mReclaimedPrimaryPlanes;
    uint32_t mReclaimedOverlayPlanes;

    bool mInitialized;
};

} // namespace intel
} // namespace android

#endif /* DISPLAYPLANEMANAGER_H_ */
