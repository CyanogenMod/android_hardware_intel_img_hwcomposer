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
#include <Drm.h>
#include <HwcLayerList.h>
#include <Hwcomposer.h>
#include <GraphicBuffer.h>
#include <IDisplayDevice.h>
#include <PlaneCapabilities.h>
#include <DisplayQuery.h>

namespace android {
namespace intel {

inline bool operator==(const hwc_rect_t& x, const hwc_rect_t& y)
{
    return (x.top == y.top &&
            x.bottom == y.bottom &&
            x.left == y.left &&
            x.right == y.right);
}

inline bool operator !=(const hwc_rect_t& x, const hwc_rect_t& y)
{
    return !operator==(x, y);
}

inline bool operator ==(const hwc_frect_t& x, const hwc_frect_t& y)
{
    return (x.top == y.top &&
            x.bottom == y.bottom &&
            x.left == y.left &&
            x.right == y.right);
}

inline bool operator !=(const hwc_frect_t& x, const hwc_frect_t& y)
{
    return !operator==(x, y);
}

HwcLayer::HwcLayer(int index, hwc_layer_1_t *layer)
    : mIndex(index),
      mLayer(layer),
      mPlane(0),
      mFormat(DataBuffer::FORMAT_INVALID),
      mWidth(0),
      mHeight(0),
      mUsage(0),
      mHandle(0),
      mIsProtected(false),
      mType(LAYER_FB),
      mPriority(0),
      mTransform(0),
      mUpdated(false)
{
    memset(&mSourceCropf, 0, sizeof(mSourceCropf));
    memset(&mDisplayFrame, 0, sizeof(mDisplayFrame));
    memset(&mStride, 0, sizeof(mStride));

    setupAttributes();
}

HwcLayer::~HwcLayer()
{
    if (mPlane) {
        WTRACE("HwcLayer is not cleaned up");
    }

    mLayer = NULL;
    mPlane = NULL;
}

bool HwcLayer::attachPlane(DisplayPlane* plane, int device)
{
    if (mPlane) {
        ETRACE("failed to attach plane, plane exists");
        return false;
    }

    if (!plane) {
        ETRACE("Invalid plane");
        return false;
    }

    // update plane's z order
    // z order = layer's index + 1
    // reserve z order 0 for frame buffer target layer
    plane->setZOrder(mIndex + 1);
    mPlane = plane;
    return true;
}

DisplayPlane* HwcLayer::detachPlane()
{
    // reset plane's z order
    if (mPlane)
        mPlane->setZOrder(-1);
    DisplayPlane *plane = mPlane;
    mPlane = 0;
    return plane;
}

void HwcLayer::setType(uint32_t type)
{
    if (!mLayer)
        return;

    switch (type) {
    case LAYER_OVERLAY:
    case LAYER_SKIPPED:
        mLayer->compositionType = HWC_OVERLAY;
        mLayer->hints |= HWC_HINT_CLEAR_FB;
        break;
    // NOTE: set compositionType to HWC_FRAMEBUFFER here so that we have
    // a chance to submit the primary changes to HW.
    // Upper layer HWComposer will reset the compositionType automatically.
    case LAYER_FRAMEBUFFER_TARGET:
    case LAYER_FB:
    case LAYER_FORCE_FB:
    default:
        mLayer->compositionType = HWC_FRAMEBUFFER;
        break;
    }

    mType = type;
}

uint32_t HwcLayer::getType() const
{
    return mType;
}

void HwcLayer::setCompositionType(int32_t type)
{
    mLayer->compositionType = type;
}

int32_t HwcLayer::getCompositionType() const
{
    return mLayer->compositionType;
}

int HwcLayer::getIndex() const
{
    return mIndex;
}

uint32_t HwcLayer::getFormat() const
{
    return mFormat;
}

uint32_t HwcLayer::getBufferWidth() const
{
    return mWidth;
}

uint32_t HwcLayer::getBufferHeight() const
{
    return mHeight;
}

const stride_t& HwcLayer::getBufferStride() const
{
    return mStride;
}

uint32_t HwcLayer::getUsage() const
{
    return mUsage;
}

uint32_t HwcLayer::getHandle() const
{
    return mHandle;
}

bool HwcLayer::isProtected() const
{
    return mIsProtected;
}

hwc_layer_1_t* HwcLayer::getLayer() const
{
    return mLayer;
}

DisplayPlane* HwcLayer::getPlane() const
{
    return mPlane;
}

void HwcLayer::setPriority(uint32_t priority)
{
    mPriority = priority;
}

uint32_t HwcLayer::getPriority() const
{
    return mPriority;
}

bool HwcLayer::update(hwc_layer_1_t *layer, int dsp)
{
    // update layer
    mLayer = layer;
    setupAttributes();

    // if not a FB layer & a plane was attached update plane's data buffer
    if (mPlane) {
        mPlane->setPosition(layer->displayFrame.left,
                            layer->displayFrame.top,
                            layer->displayFrame.right - layer->displayFrame.left,
                            layer->displayFrame.bottom - layer->displayFrame.top);
        mPlane->setSourceCrop(layer->sourceCropf.left,
                              layer->sourceCropf.top,
                              layer->sourceCropf.right - layer->sourceCropf.left,
                              layer->sourceCropf.bottom - layer->sourceCropf.top);
        mPlane->setTransform(layer->transform);
        mPlane->assignToDevice(dsp);
        bool ret = mPlane->setDataBuffer((uint32_t)layer->handle);
        if (ret == true) {
            return true;
        }
        WTRACE("failed to set data buffer, handle = %#x", (uint32_t)layer->handle);
        if (!mIsProtected) {
            // typical case: rotated buffer is not ready or handle is null
            return false;
        } else {
            // protected video has to be rendered using overlay.
            // if buffer is not ready overlay will still be attached to this layer
            // but rendering needs to be skipped.
            WTRACE("ignoring result of data buffer setting for protected video");
            return true;
        }
    }

    return true;
}

bool HwcLayer::isUpdated()
{
    return mUpdated;
}

void HwcLayer::postFlip()
{
    mUpdated = false;
    if (mPlane) {
        mPlane->postFlip();
    }
}

void HwcLayer::setupAttributes()
{
    if ((mLayer->flags & HWC_SKIP_LAYER) ||
        mTransform != mLayer->transform ||
        mSourceCropf != mLayer->sourceCropf ||
        mDisplayFrame != mLayer->displayFrame ||
        mHandle != (uint32_t)mLayer->handle ||
        DisplayQuery::isVideoFormat(mFormat)) {
        // TODO: same handle does not mean there is always no update
        mUpdated = true;
    }

    // update handle always as it can become "NULL"
    // if the given layer is not ready
    mTransform = mLayer->transform;
    mSourceCropf = mLayer->sourceCropf;
    mDisplayFrame = mLayer->displayFrame;
    mHandle = (uint32_t)mLayer->handle;

    if (mFormat != DataBuffer::FORMAT_INVALID) {
        // other attributes have been set.
        return;
    }

    if (mLayer->handle == NULL) {
        VTRACE("invalid handle");
        return;
    }

    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();
    if (bm == NULL) {
        // TODO: this check is redundant
        return;
    }

    DataBuffer *buffer = bm->lockDataBuffer((uint32_t)mLayer->handle);
     if (!buffer) {
         ETRACE("failed to get buffer");
     } else {
        mFormat = buffer->getFormat();
        mWidth = buffer->getWidth();
        mHeight = buffer->getHeight();
        mStride = buffer->getStride();
        mPriority = (mSourceCropf.right - mSourceCropf.left) * (mSourceCropf.bottom - mSourceCropf.top);
        mPriority <<= LAYER_PRIORITY_SIZE_OFFSET;
        mPriority |= mIndex;
        GraphicBuffer *gBuffer = (GraphicBuffer*)buffer;
        mUsage = gBuffer->getUsage();
        mIsProtected = GraphicBuffer::isProtectedBuffer((GraphicBuffer*)buffer);
        if (mIsProtected) {
            mPriority |= LAYER_PRIORITY_PROTECTED;
        }
        bm->unlockDataBuffer(buffer);
    }
}

//------------------------------------------------------------------------------
HwcLayerList::HwcLayerList(hwc_display_contents_1_t *list,
                            DisplayPlaneManager& dpm,
                            int disp)
    : mList(list),
      mLayerCount(0),
      mLayers(),
      mOverlayLayers(),
      mFBLayers(),
      mZOrderConfig(),
      mDisplayPlaneManager(dpm),
      mDisplayIndex(disp)
{
    if (mList) {
        VTRACE("layer count = %d", list->numHwLayers);
        mLayers.setCapacity(list->numHwLayers);
        mOverlayLayers.setCapacity(list->numHwLayers);
        mSkippedLayers.setCapacity(list->numHwLayers);
        mFBLayers.setCapacity(list->numHwLayers);
        mCandidates.setCapacity(list->numHwLayers);
        mSpriteCandidates.setCapacity(list->numHwLayers);
        mOverlayCandidates.setCapacity(list->numHwLayers);
        mPossiblePrimaryLayers.setCapacity(list->numHwLayers);
        mLayerCount = list->numHwLayers;
        // analyze list from the top layer
        analyze();
    }
}

HwcLayerList::~HwcLayerList()
{
    CTRACE();
}

//------------------------------------------------------------------------------

HwcLayerList::HwcLayerVector::HwcLayerVector()
{

}

int HwcLayerList::HwcLayerVector::do_compare(const void* lhs,
                                              const void* rhs) const
{
    const HwcLayer* l = *(HwcLayer**)lhs;
    const HwcLayer* r = *(HwcLayer**)rhs;

    // sorted from index 0 to n
    return l->getIndex() - r->getIndex();
}

HwcLayerList::PriorityVector::PriorityVector()
{

}

int HwcLayerList::PriorityVector::do_compare(const void* lhs,
                                              const void* rhs) const
{
    const HwcLayer* l = *(HwcLayer**)lhs;
    const HwcLayer* r = *(HwcLayer**)rhs;

    return r->getPriority() - l->getPriority();
}

//------------------------------------------------------------------------------
bool HwcLayerList::checkSupported(int planeType, HwcLayer *hwcLayer)
{
    bool valid = false;
    hwc_layer_1_t& layer = *(hwcLayer->getLayer());

    // if layer was forced to use FB
    if (hwcLayer->getType() == HwcLayer::LAYER_FORCE_FB) {
        VTRACE("layer was forced to use HWC_FRAMEBUFFER");
        return false;
    }

    // check layer flags
    if (layer.flags & HWC_SKIP_LAYER) {
        VTRACE("plane type %d: (skip layer flag was set)", planeType);
        return false;
    }

    if (layer.handle == 0) {
        WTRACE("invalid buffer handle");
        return false;
    }

    // check usage
    if (!hwcLayer->getUsage() & GRALLOC_USAGE_HW_COMPOSER) {
        WTRACE("not a composer layer");
        return false;
    }

    // check buffer format
    valid = PlaneCapabilities::isFormatSupported(planeType,
                                                 hwcLayer->getFormat(),
                                                 layer.transform);
    if (!valid) {
        VTRACE("plane type %d: (bad buffer format)", planeType);
        return false;
    }

    // check buffer size
    valid = PlaneCapabilities::isSizeSupported(planeType,
                                               hwcLayer->getFormat(),
                                               hwcLayer->getBufferWidth(),
                                               hwcLayer->getBufferHeight(),
                                               hwcLayer->getBufferStride());
    if (!valid) {
        VTRACE("plane type %d: (bad buffer size)", planeType);
        return false;
    }

    // check layer blending
    valid = PlaneCapabilities::isBlendingSupported(planeType,
                                                  (uint32_t)layer.blending);
    if (!valid) {
        VTRACE("plane type %d: (bad blending)", planeType);
        return false;
    }

    // check layer scaling
    valid = PlaneCapabilities::isScalingSupported(planeType,
                                                  layer.sourceCropf,
                                                  layer.displayFrame);
    if (!valid) {
        VTRACE("plane type %d: (bad scaling)", planeType);
        return false;
    }

    // TODO: check visible region?
    return true;
}

void HwcLayerList::analyze()
{
    Hwcomposer& hwc = Hwcomposer::getInstance();
    Drm *drm = hwc.getDrm();
    DisplayPlane *plane;

    if (!mList || mLayerCount == 0 || !drm)
        return;

    if (!initialize()) {
        ETRACE("failed to initialize layer list");
        return;
    }

    // go through layer list from top to bottom
    preProccess();

    // assign planes
    assignPlanes();

    // revisit the plane assignments
    revisit();
}

bool HwcLayerList::initialize()
{
    for (size_t i = 0; i < mLayerCount; i++) {
        hwc_layer_1_t *layer = &mList->hwLayers[i];
        if (!layer) {
            // unlikely happen
            ETRACE("layer %d is null", i);
            DEINIT_AND_RETURN_FALSE();
        }

        HwcLayer *hwcLayer = new HwcLayer(i, layer);
        if (!hwcLayer) {
            ETRACE("failed to allocate hwc layer %d", i);
            DEINIT_AND_RETURN_FALSE();
        }

        // by default use GPU for rendering
        if (layer->compositionType == HWC_FRAMEBUFFER_TARGET) {
            hwcLayer->setType(HwcLayer::LAYER_FRAMEBUFFER_TARGET);
        } else if (layer->compositionType == HWC_OVERLAY){
            hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
        } else {
            hwcLayer->setType(HwcLayer::LAYER_FB);
        }

        // add layer to layer list
        mLayers.add(hwcLayer);
    }

    return true;
}

void HwcLayerList::deinitialize()
{
    // reclaim planes
    for (size_t i = 0; i < mLayers.size(); i++) {
        HwcLayer *hwcLayer = mLayers.itemAt(i);
        if (hwcLayer) {
            DisplayPlane *plane = hwcLayer->detachPlane();
            if (plane)
                mDisplayPlaneManager.reclaimPlane(mDisplayIndex, *plane);
        }
        // delete HWC layer
        delete hwcLayer;
    }

    mLayers.clear();
    mOverlayLayers.clear();
    mSkippedLayers.clear();
    mFBLayers.clear();
    mCandidates.clear();
    mOverlayCandidates.clear();
    mSpriteCandidates.clear();
    mPossiblePrimaryLayers.clear();
}

void HwcLayerList::preProccess()
{
    Hwcomposer& hwc = Hwcomposer::getInstance();

    // go through layer list, settle down the candidate layers
    int topLayerIndex = mLayers.size() - 2;
    for (int i = topLayerIndex; i >= 0; i--) {
        HwcLayer *hwcLayer = mLayers.itemAt(i);
        hwc_layer_1_t *layer = hwcLayer->getLayer();

        if (layer->compositionType == HWC_OVERLAY) {
            hwcLayer->setType(HwcLayer::LAYER_SKIPPED);
            mSkippedLayers.add(hwcLayer);
            continue;
        }

        // add layer to FB layer list anyways
        mFBLayers.add(hwcLayer);

        if (checkSupported(DisplayPlane::PLANE_SPRITE, hwcLayer)) {
            // found a sprite candidate, add it to candidate & sprite
            // candidate list
            mCandidates.add(hwcLayer);
            mSpriteCandidates.add(hwcLayer);
            continue;
        }

        if (checkSupported(DisplayPlane::PLANE_OVERLAY, hwcLayer)) {
            // video play back has 'special' cases, do more checks!!!
            // use case #1: overlay is not allowed at the moment, use 3D for it
            if (!(hwc.getDisplayAnalyzer()->isOverlayAllowed())) {
                ITRACE("overlay is not allowed");
                continue;
            }

            // found a overlay candidate, add it to candidate & overlay
            // candidate list
            mCandidates.add(hwcLayer);
            mOverlayCandidates.add(hwcLayer);
            continue;
        }
    }
}

void HwcLayerList::assignPlanes()
{
    // assign overlay planes
    for (size_t idx = 0; idx < mOverlayCandidates.size(); idx++) {
        // break if no free overlay
        if (!mDisplayPlaneManager.hasFreeOverlay(mDisplayIndex)) {
            VTRACE("no free overlay available");
            break;
        }

        // attach plane
        HwcLayer *hwcLayer = mOverlayCandidates.itemAt(idx);
        DisplayPlane *plane = mDisplayPlaneManager.getOverlayPlane(mDisplayIndex);
        if (!plane) {
            WTRACE("failed to get overlay plane for display %d", mDisplayIndex);
            break;
        }
        if (!hwcLayer->attachPlane(plane, mDisplayIndex)) {
            WTRACE("failed to attach plane");
            mDisplayPlaneManager.reclaimPlane(mDisplayIndex, *plane);
            continue;
        }

        mFBLayers.remove(hwcLayer);
        hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
        mOverlayLayers.add(hwcLayer);
    }

    for (size_t idx = 0; idx < mSpriteCandidates.size(); idx++) {
        // break if no free sprite
        if (!mDisplayPlaneManager.hasFreeSprite(mDisplayIndex)) {
            VTRACE("no free sprite available");
            break;
        }

        HwcLayer *hwcLayer = mSpriteCandidates.itemAt(idx);
        DisplayPlane *plane = mDisplayPlaneManager.getSpritePlane(mDisplayIndex);
        if (!plane) {
            ETRACE("sprite plane is null");
            break;
        }
        if (!plane->enable()) {
            ETRACE("sprite plane is not ready");
            mDisplayPlaneManager.putPlane(mDisplayIndex, *plane);
            continue;
        }

        // attach plane to hwc layer
        if (!hwcLayer->attachPlane(plane, mDisplayIndex)) {
            WTRACE("failed to attach plane");
            mDisplayPlaneManager.reclaimPlane(mDisplayIndex, *plane);
            continue;
        }
        mFBLayers.remove(hwcLayer);
        hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
        mOverlayLayers.add(hwcLayer);
    }
}

void HwcLayerList::adjustAssignment()
{
    // find the least priority layer that has been attached a plane
    HwcLayer *target = 0;
    for (int i = mCandidates.size() - 1; i >= 0; i--) {
        HwcLayer *hwcLayer = mCandidates.itemAt(i);
        if (hwcLayer->getPlane() &&
            hwcLayer->getType() == HwcLayer::LAYER_OVERLAY) {
            target = hwcLayer;
            break;
        }
    }

    // it's impossible, print a warning message
    if (!target) {
        WTRACE("failed to find a HWC_OVERLAY layer");
        return;
    }

    // if found a least priority layer detach plane from it and try to attach
    // the reclaimed plane to the same type layer which has lower priority than
    // the old layer

    // set the layer type to LAYER_FORCE_FB
    target->setType(HwcLayer::LAYER_FORCE_FB);
    // remove layer from overlay layer list
    mOverlayLayers.remove(target);
    // add layer to FB layer list
    mFBLayers.add(target);

    // detach plane from the targeted layer
    DisplayPlane *plane = target->detachPlane();
    // try to find next candidate which we can attach the plane to it
    HwcLayer *next = target;
    ssize_t idx;
    do {
        // remove layer from candidates list
        mCandidates.remove(next);

        // remove layer from specific candidates list
        switch (plane->getType()) {
        case DisplayPlane::PLANE_OVERLAY:
            idx = mOverlayCandidates.remove(next);
            if (idx >= 0 && (size_t)idx < mOverlayCandidates.size()) {
                next = mOverlayCandidates.itemAt(idx);
            } else {
                next = NULL;
            }
            break;
        case DisplayPlane::PLANE_SPRITE:
        case DisplayPlane::PLANE_PRIMARY:
            idx = mSpriteCandidates.remove(next);
            if (idx >= 0 && (size_t)idx < mSpriteCandidates.size()) {
                next = mSpriteCandidates.itemAt(idx);
            } else {
                next = NULL;
            }
            break;
        }
    } while(next && !next->attachPlane(plane, mDisplayIndex));

    // if failed to get next candidate, reclaim this plane
    if (!next) {
        VTRACE("reclaimed plane type %d, index %d",
               plane->getType(), plane->getIndex());
        mDisplayPlaneManager.reclaimPlane(mDisplayIndex, *plane);
        return;
    }

    mFBLayers.remove(next);
    next->setType(HwcLayer::LAYER_OVERLAY);
    mOverlayLayers.add(next);
}

void HwcLayerList::revisit()
{
    bool ret = false;

    do {
        // detach primaryPlane
        detachPrimary();

        // calculate possible primary layers
        updatePossiblePrimaryLayers();

        // if failed to find target primary layers, adjust current assignment
        if (mFBLayers.size() && !mPossiblePrimaryLayers.size() ) {
            VTRACE("failed to find primary target layers");
            adjustAssignment();
            continue;
        }

        ret = updateZOrderConfig();
        //if failed to apply a z order configuration, fall back an overlay layer
        if (!ret && mOverlayLayers.size()) {
           VTRACE("failed to set zorder config, adjusting plane assigment...");
           adjustAssignment();
           continue;
        }
    } while (!ret);

    // skip protected layers if we cannot find an appropriate solution to
    // assign overlays.
    // This could barely happen. however, if it happened we want make sure
    // screen content is NOT messed up.
    for (size_t i = 0; i < mLayers.size(); i++) {
        HwcLayer *layer = mLayers.itemAt(i);
        // set a layer to layer overlay and add it back to overlay layer list
        if (layer->getType() != HwcLayer::LAYER_OVERLAY &&
            layer->isProtected()) {
            WTRACE("skip protected layer %d", layer->getIndex());
            layer->setType(HwcLayer::LAYER_SKIPPED);
            // move it from FB layer list to overlay layer list
            mFBLayers.remove(layer);
            mSkippedLayers.add(layer);
        }
    }
}

bool HwcLayerList::updateZOrderConfig()
{
    // acquire primary plane of this display device
    DisplayPlane *primaryPlane =
        mDisplayPlaneManager.getPrimaryPlane(mDisplayIndex);
    if (!primaryPlane) {
        // if primary allocation is failed, it should be a fatal error
        ETRACE("failed to get primary plane for display %d", mDisplayIndex);
        return false;
    }

    if (!primaryPlane->enable()) {
        ETRACE("failed to enable primary plane");
        return false;
    }

    // primary can be used as sprite, setup Z order directly
    if (usePrimaryAsSprite(primaryPlane)) {
        VTRACE("primary is used as sprite");
        return setupZOrderConfig();
    }

    // attach primary to frame buffer target
    if (!usePrimaryAsFramebufferTarget(primaryPlane)) {
        VTRACE("primary is unused");
        mDisplayPlaneManager.reclaimPlane(mDisplayIndex, *primaryPlane);
        return setupZOrderConfig();
    }

    int primaryZOrder = 0;
    // if no possible primary layers, place it at bottom
    if (!mPossiblePrimaryLayers.size()) {
        primaryPlane->setZOrder(primaryZOrder);
        return setupZOrderConfig();
    }
    // try to find out a suitable layer to place primary plane
    bool success = false;
    while (mPossiblePrimaryLayers.size()) {
        HwcLayer *primaryLayer = mPossiblePrimaryLayers.itemAt(0);
        // need update primary plane zorder
        primaryZOrder = primaryLayer->getIndex() + 1;
        primaryPlane->setZOrder(primaryZOrder);

        // try to set z order config, return if setup z order successfully
        success = setupZOrderConfig();
        if (success) {
            VTRACE("primary was attached to framebuffer target");
            break;
        }
        // remove this layer from possible primary layer list
        mPossiblePrimaryLayers.remove(primaryLayer);
    }

    return success;
}

bool HwcLayerList::usePrimaryAsSprite(DisplayPlane *primaryPlane)
{
    // only one FB layer left, it's possible to use primary as sprite
    // if the assignment successes, we are done! update primary z order
    // and attach primary plane to this layer.
    if ((mFBLayers.size() == 1)) {
        HwcLayer *layer = mFBLayers.itemAt(0);
        if (checkSupported(DisplayPlane::PLANE_PRIMARY, layer)) {
            VTRACE("primary check passed for primary layer");
            // attach primary to layer
            if (!layer->attachPlane(primaryPlane, mDisplayIndex)) {
                WTRACE("failed to attach plane");
                mDisplayPlaneManager.reclaimPlane(mDisplayIndex, *primaryPlane);
                return false;
            }
            // set the layer type to overlay
            layer->setType(HwcLayer::LAYER_OVERLAY);
            // remove layer from FB layer list
            mFBLayers.remove(layer);
            // add layer to overlay layers
            mOverlayLayers.add(layer);
            return true;
        }
    }

    return false;
}

void HwcLayerList::detachPrimary()
{
    HwcLayer *framebufferTarget = mLayers.itemAt(mLayers.size() - 1);
    DisplayPlane *primaryPlane = framebufferTarget->getPlane();

    // if primary plane was attached to framebuffer target
    // detach plane
    if (primaryPlane) {
        framebufferTarget->detachPlane();
        // reclaim primary plane
        mDisplayPlaneManager.reclaimPlane(mDisplayIndex, *primaryPlane);
        return;
    }

    // if primary plane was attached to a normal layer
    for (size_t i = 0; i < mLayers.size() - 1; i++) {
        HwcLayer *hwcLayer = mLayers.itemAt(i);
        DisplayPlane *plane = hwcLayer->getPlane();
        if (!plane)
            continue;
        if (plane->getType() == DisplayPlane::PLANE_PRIMARY) {
            // detach plane
            hwcLayer->detachPlane();
            // set layer type to FRAMEBUFFER
            hwcLayer->setType(HwcLayer::LAYER_FB);
            // remove it from overlay list
            mOverlayLayers.remove(hwcLayer);
            // add it to fb layer list
            mFBLayers.add(hwcLayer);
            // reclaim primary plane
            mDisplayPlaneManager.reclaimPlane(mDisplayIndex, *plane);
            break;
        }
    }
}

void HwcLayerList::updatePossiblePrimaryLayers()
{
    mPossiblePrimaryLayers.clear();

    // if no FB layers, clear vector
    if (!mFBLayers.size()) {
        return;
    }

    for (size_t i = 0; i < mFBLayers.size(); i++) {
        HwcLayer *target = mFBLayers.itemAt(i);
        if (mergeFBLayersToLayer(target, i)) {
            mPossiblePrimaryLayers.add(target);
        }
    }
}

bool HwcLayerList::usePrimaryAsFramebufferTarget(DisplayPlane *primaryPlane)
{
    // don't attach primary if
    // 0) no fb layers
    // 1) all overlay layers have been handled
    // NOTE: still need attach primary plane if no fb layers and some layers
    // were skipped, or primary plane would be shut down and we will have no
    // chance to fetch FB data at this point and screen will FREEZE on the last
    // frame.
    if (!mFBLayers.size() && mOverlayLayers.size()) {
        return false;
    }

    // attach primary to frame buffer target
    HwcLayer *layer = mLayers.itemAt(mLayers.size() - 1);

    // invalidate primary plane's data buffer cache
    primaryPlane->invalidateBufferCache();
    // NOTE: calling setType again to trigger glClear() for
    // other overlay layers
    layer->setType(HwcLayer::LAYER_FRAMEBUFFER_TARGET);
    // attach primary plane, it has to be successful
    layer->attachPlane(primaryPlane, mDisplayIndex);

    return true;
}

bool HwcLayerList::calculatePrimaryZOrder(int& zorder)
{
    int primaryZOrder = -1;

    // if no FB layers, move primary to the bottom
    if (!mFBLayers.size()) {
        primaryZOrder = 0;
        return true;
    }

    for (size_t i = 0; i < mFBLayers.size(); i++) {
        HwcLayer *target = mFBLayers.itemAt(i);
        // if all other FB layers can be merged to target layer
        // then it's fine to put primary plane here
        if (mergeFBLayersToLayer(target, i)) {
            primaryZOrder = (target->getIndex() + 1);
            break;
        }
    }

    zorder = primaryZOrder;

    return (primaryZOrder != -1) ? true : false;
}

bool HwcLayerList::mergeFBLayersToLayer(HwcLayer *target, int idx)
{
    // merge all below FB layers to the target layer
    for (int i = 0; i < idx; i++) {
        HwcLayer *below = mFBLayers.itemAt(i);
        if (!mergeToLayer(target, below)) {
            return false;
        }
    }

    // merge all above FB layer to the target layer
    for (size_t i = idx + 1; i < mFBLayers.size(); i++) {
        HwcLayer *above = mFBLayers.itemAt(i);
        if (!mergeToLayer(target, above)) {
            return false;
        }
    }

    return true;
}

bool HwcLayerList::mergeToLayer(HwcLayer* target, HwcLayer* layer)
{
    int targetZOrder = target->getIndex();
    int layerZOrder = layer->getIndex();

    if (targetZOrder == layerZOrder) {
        return true;
    }

    if (targetZOrder < layerZOrder) {
        // layer is above target layer need check intersection with all
        // overlay layers below this layer
        for (int i = layerZOrder - 1; i > targetZOrder; i--) {
            HwcLayer *l = mLayers.itemAt(i);
            if (l->getPlane() && l->getType() == HwcLayer::LAYER_OVERLAY) {
                // check intersection
                if (hasIntersection(l, layer)) {
                    return false;
                }
            }
        }
    } else {
       // layer is under target layer need check intersection with all
       // overlay layers above this layer
       for (int i = layerZOrder + 1; i < targetZOrder; i++) {
           HwcLayer *l = mLayers.itemAt(i);
           if (l->getPlane() && l->getType() == HwcLayer::LAYER_OVERLAY) {
               // check intersection
               if (hasIntersection(l, layer)) {
                   return false;
               }
           }
       }
    }

    return true;
}

bool HwcLayerList::hasIntersection(HwcLayer *la, HwcLayer *lb)
{
    hwc_layer_1_t *a = la->getLayer();
    hwc_layer_1_t *b = lb->getLayer();
    hwc_rect_t *aRect = &a->displayFrame;
    hwc_rect_t *bRect = &b->displayFrame;

    if (bRect->right <= aRect->left ||
        bRect->left >= aRect->right ||
        bRect->top >= aRect->bottom ||
        bRect->bottom <= aRect->top)
        return false;

    return true;
}

bool HwcLayerList::setupZOrderConfig()
{
    ZOrderConfig zorderConfig;
    DisplayPlane *plane;
    HwcLayer *layer;

    zorderConfig.setCapacity(mOverlayLayers.size() + 1);

    // setup display device which this zorder config belongs to
    zorderConfig.setDisplayDevice(mDisplayIndex);

    // add all planes in overlay layer list
    for (size_t i = 0; i < mOverlayLayers.size(); i++) {
        layer = mOverlayLayers.itemAt(i);
        plane = layer->getPlane();
        if (!plane)
            continue;
        zorderConfig.add(plane);
    }

    // add primary plane if it had been assigned to frame buffer target
    layer = mLayers.itemAt(mLayers.size() - 1);
    plane = layer->getPlane();
    if (plane) {
        zorderConfig.add(plane);
    }

    return mDisplayPlaneManager.setZOrderConfig(zorderConfig);
}

void HwcLayerList::setupSmartComposition()
{
    uint32_t compositionType = HWC_OVERLAY;
    HwcLayer *hwcLayer = NULL;

    // setup smart composition only there's no update on all FB layers
    for (size_t i = 0; i < mFBLayers.size(); i++) {
        hwcLayer = mFBLayers.itemAt(i);
        if (hwcLayer->isUpdated()) {
            compositionType = HWC_FRAMEBUFFER;
        }
    }

    VTRACE("smart composition enabled %s",
           (compositionType == HWC_OVERLAY) ? "TRUE" : "FALSE");
    for (size_t i = 0; i < mFBLayers.size(); i++) {
        hwcLayer = mFBLayers.itemAt(i);
        switch (hwcLayer->getType()) {
        case HwcLayer::LAYER_FB:
        case HwcLayer::LAYER_FORCE_FB:
            hwcLayer->setCompositionType(compositionType);
            break;
        default:
            ETRACE("Invalid layer type %d", hwcLayer->getType());
            break;
        }
    }
}

bool HwcLayerList::update(hwc_display_contents_1_t *list)
{
    bool ret;
    bool again = false;

    CTRACE();

    // basic check to make sure the consistance
    if (!list) {
        ETRACE("null layer list");
        return false;
    }

    if (list->numHwLayers != mLayerCount) {
        ETRACE("layer count doesn't match (%d, %d)", list->numHwLayers, mLayerCount);
        return false;
    }

    // update list
    mList = list;

    do {
        again = false;
        // update all layers, call each layer's update()
        for (size_t i = 0; i < mLayerCount; i++) {
            HwcLayer *hwcLayer = mLayers.itemAt(i);
            if (!hwcLayer) {
                ETRACE("no HWC layer for layer %d", i);
                continue;
            }

            ret = hwcLayer->update(&list->hwLayers[i], mDisplayIndex);
            if (ret == false) {
                // layer update failed, fall back to ST and revisit all plane
                // assignment
                WTRACE("failed to update layer %d, count %d, type %d",
                         i, mLayerCount, hwcLayer->getType());
                // if type of layer is LAYER_FB, that layer must have been added to mFBLayers.
                if (hwcLayer->getType() != HwcLayer::LAYER_FB) {
                    // set layer to FB layer
                    hwcLayer->setType(HwcLayer::LAYER_FB);
                    // remove layer from overlay layer list
                    mOverlayLayers.remove(hwcLayer);
                    // add layer to FB layer list
                    mFBLayers.add(hwcLayer);
                    // revisit the overlay assignment.
                    revisit();
                }
            } else if (hwcLayer->getPlane() &&
                        hwcLayer->getType() == HwcLayer::LAYER_FB) {
                // layer update success, if the layer was assigned a plane
                // switch back to overlay and revisit all plane assignment
                ITRACE("updated layer %d, switch back to overlay", i);
                // set layer to overlay layer
                hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
                // remove layer from Fb layer list
                mFBLayers.remove(hwcLayer);
                // add layer to overlay layer list
                mOverlayLayers.add(hwcLayer);
                // revisit plane assignment
                revisit();
                // need update again since we changed the plane assignment
                again = true;
            }
        }
    } while (again && mOverlayLayers.size());

    setupSmartComposition();
    return true;
}

DisplayPlane* HwcLayerList::getPlane(uint32_t index) const
{
    HwcLayer *hwcLayer;

    if (index >= mLayers.size()) {
        ETRACE("invalid layer index %d", index);
        return 0;
    }

    hwcLayer = mLayers.itemAt(index);
    if (!hwcLayer || (hwcLayer->getType() == HwcLayer::LAYER_FB) ||
        (hwcLayer->getType() ==  HwcLayer::LAYER_FORCE_FB))
        return 0;

    if (hwcLayer->getHandle() == 0) {
        WTRACE("plane is attached with invalid handle");
        return 0;
    }

    return hwcLayer->getPlane();
}

bool HwcLayerList::hasProtectedLayer()
{
    for (size_t i = 0; i < mLayers.size(); i++) {
        HwcLayer *hwcLayer = mLayers.itemAt(i);
        if (hwcLayer && hwcLayer->isProtected()) {
            VTRACE("protected layer found, layer index is %d", i);
            return true;
        }
    }
    return false;
}

bool HwcLayerList::hasVisibleLayer()
{
    // excluding framebuffer target layer
    int count = (int)mLayers.size() - 1;
    if (count <= 0) {
        ITRACE("number of layer is %d, visible layer is 0", mLayers.size());
        return false;
    }

    // the last layer is always frambuffer target layer?
    for (size_t i = 0; i < mLayers.size() - 1; i++) {
        HwcLayer *hwcLayer = mLayers.itemAt(i);
        if (hwcLayer == NULL) {
            // TODO: remove this redundant check
            continue;
        }
        if (hwcLayer->getType() == HwcLayer::LAYER_OVERLAY &&
            hwcLayer->getPlane() == NULL) {
            // layer is invisible
            count--;
        }
    }
    ITRACE("number of visible layers %d", count);
    return count != 0;
}

void HwcLayerList::postFlip()
{
    for (size_t i = 0; i < mLayers.size(); i++) {
        HwcLayer *hwcLayer = mLayers.itemAt(i);
        hwcLayer->postFlip();
    }
}

void HwcLayerList::dump(Dump& d)
{
    d.append("Layer list: (number of layers %d):\n", mLayers.size());
    d.append(" LAYER |          TYPE          |   PLANE  | INDEX | Z Order \n");
    d.append("-------+------------------------+----------------------------\n");
    for (size_t i = 0; i < mLayers.size(); i++) {
        HwcLayer *hwcLayer = mLayers.itemAt(i);
        DisplayPlane *plane;
        int planeIndex = -1;
        int zorder = -1;
        const char *type = "HWC_FB";
        const char *planeType = "N/A";

        if (hwcLayer) {
            switch (hwcLayer->getType()) {
            case HwcLayer::LAYER_FB:
            case HwcLayer::LAYER_FORCE_FB:
                type = "HWC_FB";
                break;
            case HwcLayer::LAYER_OVERLAY:
            case HwcLayer::LAYER_SKIPPED:
                type = "HWC_OVERLAY";
                break;
            case HwcLayer::LAYER_FRAMEBUFFER_TARGET:
                type = "HWC_FRAMEBUFFER_TARGET";
                break;
            default:
                type = "Unknown";
            }

            plane = hwcLayer->getPlane();
            if (plane) {
                planeIndex = plane->getIndex();
                zorder = plane->getZOrder();
                switch (plane->getType()) {
                case DisplayPlane::PLANE_OVERLAY:
                    planeType = "OVERLAY";
                    break;
                case DisplayPlane::PLANE_SPRITE:
                    planeType = "SPRITE";
                    break;
                case DisplayPlane::PLANE_PRIMARY:
                    planeType = "PRIMARY";
                    break;
                default:
                    planeType = "Unknown";
                }
            }

            d.append("  %2d   | %22s | %8s | %3D   | %3D \n",
                     i, type, planeType, planeIndex, zorder);
        }
    }
}

} // namespace intel
} // namespace android
