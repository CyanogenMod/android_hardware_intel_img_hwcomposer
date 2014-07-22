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

class ZOrderConfig : public SortedVector<DisplayPlane*> {
public:
    int do_compare(const void* lhs, const void* rhs) const {
        const DisplayPlane *l = *(DisplayPlane**)lhs;
        const DisplayPlane *r = *(DisplayPlane**)rhs;

        // sorted from z order 0 to n
        return l->getZOrder() - r->getZOrder();
    }
};

class DisplayPlaneManager {
    enum {
        PLANE_ON_RECLAIMED_LIST = 1,
        PLANE_ON_FREE_LIST,
    };
public:
    DisplayPlaneManager();
    virtual ~DisplayPlaneManager();

    bool initCheck() const { return mInitialized; }

    // sub-class can override initialize & deinitialize
    virtual bool initialize();
    virtual void deinitialize();

    // plane allocation & free
    DisplayPlane* getSpritePlane();
    DisplayPlane* getOverlayPlane(int dsp);
    DisplayPlane* getPrimaryPlane(int dsp);
    void putPlane(DisplayPlane& plane);

    bool hasFreeSprite();
    bool hasFreeOverlay();
    bool hasFreePrimary(int dsp);

    void reclaimPlane(DisplayPlane& plane);
    void disableReclaimedPlanes();
    void disableOverlayPlanes();

    // z order config
    bool setZOrderConfig(ZOrderConfig& zorderConfig);
    void* getZOrderConfig() const;

    // dump interface
    void dump(Dump& d);

private:
    int getPlane(uint32_t& mask);
    int getPlane(uint32_t& mask, int index);
    void putPlane(int index, uint32_t& mask);

    inline DisplayPlane* getPlane(int type, int dsp = 0);
    inline bool hasFreePlanes(int type, int dsp = 0);
protected:
    // sub-classes need implement follow functions
    virtual bool detect(int& spriteCount,
                          int& overlayCount,
                          int& primaryCount) = 0;
    virtual DisplayPlane* allocPlane(int index, int type) = 0;
    virtual bool isValidZOrderConfig(ZOrderConfig& zorderConfig) = 0;
    virtual void* getNativeZOrderConfig() = 0;
private:
    int mPlaneCount[DisplayPlane::PLANE_MAX];
    int mTotalPlaneCount;

    Vector<DisplayPlane*> mPlanes[DisplayPlane::PLANE_MAX];

    // Bitmap of free planes. Bit0 - plane A, bit 1 - plane B, etc.
    uint32_t mFreePlanes[DisplayPlane::PLANE_MAX];
    uint32_t mReclaimedPlanes[DisplayPlane::PLANE_MAX];

    void *mNativeZOrderConfig;
protected:
    bool mInitialized;
};

} // namespace intel
} // namespace android

#endif /* DISPLAYPLANEMANAGER_H_ */
