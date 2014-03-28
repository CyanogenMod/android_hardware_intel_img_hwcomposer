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
#include <HwcLayer.h>
#include <utils/Vector.h>

namespace android {
namespace intel {

struct ZOrderLayer
{
    ZOrderLayer() {
        memset(this, 0, sizeof(ZOrderLayer));
    }

    inline bool operator<(const ZOrderLayer& rhs) const {
        return zorder < rhs.zorder;
    }

    int planeType;
    int zorder;
    DisplayPlane *plane;
    HwcLayer *hwcLayer;
};

class ZOrderConfig : public SortedVector<ZOrderLayer*> {
public:
    ZOrderConfig() {}

    int do_compare(const void* lhs, const void* rhs) const {
        const ZOrderLayer *l = *(ZOrderLayer**)lhs;
        const ZOrderLayer *r = *(ZOrderLayer**)rhs;

        // sorted from z order 0 to n
        return l->zorder - r->zorder;
    }
};


class DisplayPlaneManager {
public:
    DisplayPlaneManager();
    virtual ~DisplayPlaneManager();

public:
    virtual bool initialize();
    virtual void deinitialize();

    virtual bool isValidZOrder(int dsp, ZOrderConfig& config) = 0;
    virtual bool assignPlanes(int dsp, ZOrderConfig& config) = 0;
    // TODO: remove this API
    virtual void* getZOrderConfig() const = 0;
    virtual int getFreePlanes(int dsp, int type);
    virtual void reclaimPlane(int dsp, DisplayPlane& plane);
    virtual void disableReclaimedPlanes();
    virtual void disableOverlayPlanes();
    // dump interface
    virtual void dump(Dump& d);

protected:
    // plane allocation & free
    int getPlane(uint32_t& mask);
    int getPlane(uint32_t& mask, int index);
    DisplayPlane* getPlane(int type, int index);
    DisplayPlane* getAnyPlane(int type);
    void putPlane(int index, uint32_t& mask);
    void putPlane(int dsp, DisplayPlane& plane);
    bool isFreePlane(int type, int index);
    virtual DisplayPlane* allocPlane(int index, int type) = 0;

protected:
    int mPlaneCount[DisplayPlane::PLANE_MAX];
    int mTotalPlaneCount;
    int mPrimaryPlaneCount;
    int mSpritePlaneCount;
    int mOverlayPlaneCount;

    Vector<DisplayPlane*> mPlanes[DisplayPlane::PLANE_MAX];

    // Bitmap of free planes. Bit0 - plane A, bit 1 - plane B, etc.
    uint32_t mFreePlanes[DisplayPlane::PLANE_MAX];
    uint32_t mReclaimedPlanes[DisplayPlane::PLANE_MAX];

    bool mInitialized;

enum {
    DEFAULT_PRIMARY_PLANE_COUNT = 3
};
};

} // namespace intel
} // namespace android

#endif /* DISPLAYPLANEMANAGER_H_ */
