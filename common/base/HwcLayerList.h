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
#ifndef HWCLAYERLIST_H_
#define HWCLAYERLIST_H_

#include <Dump.h>
#include <hardware/hwcomposer.h>
#include <utils/SortedVector.h>
#include <DataBuffer.h>
#include <DisplayPlane.h>
#include <DisplayPlaneManager.h>

namespace android {
namespace intel {

class HwcLayer {
public:
    enum {
        // LAYER_FB layers are marked as HWC_FRAMEBUFFER.
        // And a LAYER_FB can become HWC_OVERLAY layers during
        // revisiting layer list.
        LAYER_FB = 0,
        // LAYER_FORCE_FB layers are marked as HWC_FRAMEBUFFER.
        // And a LAYER_FORCE_FB can never become HWC_OVERLAY layers during
        // revisiting layer list.
        LAYER_FORCE_FB,
        // LAYER_OVERLAY layers are marked as HWC_OVERLAY
        LAYER_OVERLAY,
        // LAYER_SKIPPED layers are marked as HWC_OVERLAY with no plane attached
        LAYER_SKIPPED,
        // LAYER_FRAMEBUFFER_TARGET layers are marked as HWC_FRAMEBUFFER_TARGET
        LAYER_FRAMEBUFFER_TARGET,
    };

    enum {
        LAYER_PRIORITY_PROTECTED = 0x70000000UL,
        LAYER_PRIORITY_SIZE_OFFSET = 4,
    };
public:
    HwcLayer(int index, hwc_layer_1_t *layer);
    virtual ~HwcLayer();

    // plane operations
    bool attachPlane(DisplayPlane *plane, int device);
    DisplayPlane* detachPlane();

    void setType(uint32_t type);
    uint32_t getType() const;
    int32_t getCompositionType() const;
    void setCompositionType(int32_t type);

    int getIndex() const;
    uint32_t getFormat() const;
    uint32_t getBufferWidth() const;
    uint32_t getBufferHeight() const;
    const stride_t& getBufferStride() const;
    uint32_t getUsage() const;
    uint32_t getHandle() const;
    bool isProtected() const;
    hwc_layer_1_t* getLayer() const;
    DisplayPlane* getPlane() const;

    void setPriority(uint32_t priority);
    uint32_t getPriority() const;

    bool update(hwc_layer_1_t *layer);
    void postFlip();
    bool isUpdated();

private:
    void setupAttributes();

private:
    const int mIndex;
    hwc_layer_1_t *mLayer;
    DisplayPlane *mPlane;
    uint32_t mFormat;
    uint32_t mWidth;
    uint32_t mHeight;
    stride_t mStride;
    uint32_t mUsage;
    uint32_t mHandle;
    bool mIsProtected;
    uint32_t mType;
    uint32_t mPriority;

    // for smart composition
    uint32_t mTransform;
    hwc_rect_t mSourceCrop;
    hwc_rect_t mDisplayFrame;
    bool mUpdated;
};

class HwcLayerList {
public:
    HwcLayerList(hwc_display_contents_1_t *list, DisplayPlaneManager& dpm,
                  int disp);
    virtual ~HwcLayerList();

    virtual void deinitialize();

    virtual bool update(hwc_display_contents_1_t *list);
    virtual DisplayPlane* getPlane(uint32_t index) const;

    bool hasProtectedLayer();
    bool hasVisibleLayer();
    void postFlip();

    // dump interface
    virtual void dump(Dump& d);
protected:
    class HwcLayerVector : public SortedVector<HwcLayer*> {
    public:
        HwcLayerVector();
        virtual int do_compare(const void* lhs, const void* rhs) const;
    };

    class PriorityVector : public SortedVector<HwcLayer*> {
    public:
        PriorityVector();
        virtual int do_compare(const void* lhs, const void* rhs) const;
    };

    virtual bool initialize();
    virtual void revisit();
    virtual bool checkSupported(int planeType, HwcLayer *hwcLayer);
    virtual void analyze();
private:
    void assignPlanes();
    void adjustAssignment();
    void preProccess();
    void detachPrimary();
    bool usePrimaryAsSprite(DisplayPlane *primaryPlane);
    bool usePrimaryAsFramebufferTarget(DisplayPlane *primaryPlane);
    bool updateZOrderConfig();
    void updatePossiblePrimaryLayers();
    bool calculatePrimaryZOrder(int& zorder);
    bool mergeFBLayersToLayer(HwcLayer *target, int idx);
    bool mergeToLayer(HwcLayer* target, HwcLayer* layer);
    bool hasIntersection(HwcLayer *la, HwcLayer *lb);
    bool setupZOrderConfig();
    void setupSmartComposition();
private:
    hwc_display_contents_1_t *mList;
    uint32_t mLayerCount;
    HwcLayerVector mLayers;
    HwcLayerVector mOverlayLayers;
    HwcLayerVector mSkippedLayers;
    HwcLayerVector mFBLayers;
    ZOrderConfig mZOrderConfig;
    // need a display plane manager to get display plane info;
    DisplayPlaneManager& mDisplayPlaneManager;
    int mDisplayIndex;

    PriorityVector mCandidates;
    PriorityVector mSpriteCandidates;
    PriorityVector mOverlayCandidates;
    PriorityVector mPossiblePrimaryLayers;
};

} // namespace intel
} // namespace android


#endif /* HWCLAYERLIST_H_ */
