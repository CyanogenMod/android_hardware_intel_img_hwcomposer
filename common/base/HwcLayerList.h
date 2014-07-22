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
#ifndef HWC_LAYER_LIST_H
#define HWC_LAYER_LIST_H

#include <Dump.h>
#include <hardware/hwcomposer.h>
#include <utils/SortedVector.h>
#include <DataBuffer.h>
#include <DisplayPlane.h>
#include <DisplayPlaneManager.h>
#include <HwcLayer.h>

namespace android {
namespace intel {


class HwcLayerList {
public:
    HwcLayerList(hwc_display_contents_1_t *list, int disp);
    virtual ~HwcLayerList();

public:
    virtual bool initialize();
    virtual void deinitialize();

    virtual bool update(hwc_display_contents_1_t *list);
    virtual DisplayPlane* getPlane(uint32_t index) const;

    void postFlip();

    // dump interface
    virtual void dump(Dump& d);


private:
    bool checkSupported(int planeType, HwcLayer *hwcLayer);
    bool allocatePlanesV1();
    bool allocatePlanesV2();
    bool assignOverlayPlanes();
    bool assignOverlayPlanes(int index, int planeNumber);
    bool assignSpritePlanes();
    bool assignSpritePlanes(int index, int planeNumber);
    bool assignPrimaryPlane();
    bool assignPrimaryPlaneHelper(HwcLayer *hwcLayer, int zorder = -1);
    bool attachPlanes();
    bool useAsFrameBufferTarget(HwcLayer *target);
    bool hasIntersection(HwcLayer *la, HwcLayer *lb);
    ZOrderLayer* addZOrderLayer(int type, HwcLayer *hwcLayer, int zorder = -1);
    void removeZOrderLayer(ZOrderLayer *layer);
    void setupSmartComposition();
    void dump();

private:
    class HwcLayerVector : public SortedVector<HwcLayer*> {
    public:
        HwcLayerVector() {}
        virtual int do_compare(const void* lhs, const void* rhs) const {
            const HwcLayer* l = *(HwcLayer**)lhs;
            const HwcLayer* r = *(HwcLayer**)rhs;
            // sorted from index 0 to n
            return l->getIndex() - r->getIndex();
        }
    };

    class PriorityVector : public SortedVector<HwcLayer*> {
    public:
        PriorityVector() {}
        virtual int do_compare(const void* lhs, const void* rhs) const {
            const HwcLayer* l = *(HwcLayer**)lhs;
            const HwcLayer* r = *(HwcLayer**)rhs;
            return r->getPriority() - l->getPriority();
        }
    };

    hwc_display_contents_1_t *mList;
    int mLayerCount;

    HwcLayerVector mLayers;
    HwcLayerVector mFBLayers;
    PriorityVector mSpriteCandidates;
    PriorityVector mOverlayCandidates;
    ZOrderConfig mZOrderConfig;
    HwcLayer *mFrameBufferTarget;
    int mDisplayIndex;
};

} // namespace intel
} // namespace android


#endif /* HWC_LAYER_LIST_H */
