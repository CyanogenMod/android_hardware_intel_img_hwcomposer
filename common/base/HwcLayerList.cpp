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

HwcLayer::HwcLayer(int index, hwc_layer_1_t *layer)
    : mIndex(index),
      mLayer(layer),
      mPlane(0),
      mFormat(DataBuffer::FORMAT_INVALID),
      mUsage(0),
      mHandle(0),
      mIsProtected(false),
      mType(LAYER_FB)
{
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

bool HwcLayer::attachPlane(DisplayPlane* plane)
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

int HwcLayer::getIndex() const
{
    return mIndex;
}

uint32_t HwcLayer::getFormat() const
{
    return mFormat;
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

bool HwcLayer::update(hwc_layer_1_t *layer, int disp)
{
    // update layer
    mLayer = layer;
    setupAttributes();

    // if not a FB layer & a plane was attached update plane's data buffer
    if (mPlane) {
        mPlane->assignToDevice(disp);
        mPlane->setPosition(layer->displayFrame.left,
                            layer->displayFrame.top,
                            layer->displayFrame.right - layer->displayFrame.left,
                            layer->displayFrame.bottom - layer->displayFrame.top);
        mPlane->setSourceCrop(layer->sourceCrop.left,
                              layer->sourceCrop.top,
                              layer->sourceCrop.right - layer->sourceCrop.left,
                              layer->sourceCrop.bottom - layer->sourceCrop.top);
        mPlane->setTransform(layer->transform);
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

void HwcLayer::setupAttributes()
{
    // update handle always as it can become "NULL"
    // if the given layer is not ready
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
        GraphicBuffer *gBuffer = (GraphicBuffer*)buffer;
        mUsage = gBuffer->getUsage();
        mIsProtected = GraphicBuffer::isProtectedBuffer((GraphicBuffer*)buffer);
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
        mFBLayers.setCapacity(list->numHwLayers);
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

    // check layer blending
    valid = PlaneCapabilities::isBlendingSupported(planeType,
                                                  (uint32_t)layer.blending);
    if (!valid) {
        VTRACE("plane type %d: (bad blending)", planeType);
        return false;
    }

    // check layer scaling
    valid = PlaneCapabilities::isScalingSupported(planeType,
                                                  layer.sourceCrop,
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
                mDisplayPlaneManager.reclaimPlane(*plane);
        }
        // delete HWC layer
        delete hwcLayer;
    }

    mLayers.clear();
    mOverlayLayers.clear();
    mFBLayers.clear();
}

void HwcLayerList::preProccess()
{
    Hwcomposer& hwc = Hwcomposer::getInstance();
    //Drm *drm = hwc.getDrm();
    DisplayPlane *plane;

    // skip frame buffer target
    int topLayerIndex = mLayers.size() - 2;

    for (int i = topLayerIndex; i >= 0; i--) {
        HwcLayer *hwcLayer = mLayers.itemAt(i);
        hwc_layer_1_t *layer = hwcLayer->getLayer();

        if (layer->compositionType == HWC_OVERLAY) {
            // layer has been preprocessed
            hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
            mOverlayLayers.add(hwcLayer);
            continue;
        }

        // go through the layer list from top to bottom
        // assign a display planes to layers which can be blended by DC
        // TODO: need further optimization.

        // check whether the layer can be handled by sprite plane
        if (mDisplayPlaneManager.hasFreeSprite() &&
            checkSupported(DisplayPlane::PLANE_SPRITE, hwcLayer)) {
            VTRACE("sprite check passed for layer %d", i);
            plane = mDisplayPlaneManager.getSpritePlane();
            if (plane) {
                // enable plane
                if (!plane->enable()) {
                    ETRACE("sprite plane is not ready");
                    mDisplayPlaneManager.putPlane(*plane);
                    mFBLayers.add(hwcLayer);
                    continue;
                }
                // attach plane to hwc layer
                hwcLayer->attachPlane(plane);
                // set the layer type to overlay
                hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
                mOverlayLayers.add(hwcLayer);
                continue;
            } else {
                VTRACE("sprite plane is not available for layer %d", i);
            }
        }

        // check whether the layer can be handled by overlay plane
        // Have to go through this check even overlay plane may not be available
        // as protected video layer needs to be skipped if overlay is not available
        if (checkSupported(DisplayPlane::PLANE_OVERLAY, hwcLayer)) {
            VTRACE("overlay check passed for layer %d", i);
            if (mDisplayIndex == IDisplayDevice::DEVICE_PRIMARY) {
                // check if HWC is in video extended mode
                if (DisplayQuery::isVideoFormat(hwcLayer->getFormat()) &&
                    hwc.getDisplayAnalyzer()->checkVideoExtendedMode()) {
                    ITRACE("video is skipped in extended mode");
                    hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
                    mOverlayLayers.add(hwcLayer);
                    continue;
                }
            }

            if (hwc.getDisplayAnalyzer()->isOverlayAllowed()) {
                plane = mDisplayPlaneManager.getOverlayPlane();
            } else {
                WTRACE("overlay use is not allowed.");
                plane = NULL;
            }

            if (plane) {
                hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
                hwcLayer->attachPlane(plane);
                mOverlayLayers.add(hwcLayer);
                continue;
            } else if (hwcLayer->isProtected()) {
                // TODO: detach overlay from non-protected layers
                WTRACE("protected layer is skipped");
                hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
                mOverlayLayers.add(hwcLayer);
                continue;
            } else {
                WTRACE("overlay plane is not available for video layer %d", i);
            }
        }

        // if still FB layer
        if (hwcLayer->getType() == HwcLayer::LAYER_FB) {
            mFBLayers.add(hwcLayer);
        }
    }
}

void HwcLayerList::revisit()
{
    bool ret = false;

    // try to attach primary plane & setup z order configuration
    // till we can place primary plane to an appropriate layer in the
    // layer list AND found a platform supported plane z order configure
    // NOTE: we will eventually finish these two steps setting successfully
    // since the worst situation is that we fall back all assigned overlay
    // layers and use GPU composition completely.
    //
    do {
        // detach primaryPlane
        detachPrimary();

        // try to attach primary plane
        ret = attachPrimary();
        //if failed to attach primary plane, fall back an overlay layer
        if (!ret && mOverlayLayers.size()) {
            ITRACE("failed to attach primary plane");
            fallbackOverlayLayer();
            continue;
        }

        // try to set z order config
        ret = setupZOrderConfig();
        //if failed to apply a z order configuration, fall back an overlay layer
        if (!ret && mOverlayLayers.size()) {
           ITRACE("failed to set zorder config");
           fallbackOverlayLayer();
           continue;
        }
    } while (!ret);

    // blank protected layers if we cannot find an appropriate solution to
    // assign overlays.
    // This could barely happen. however, if it happened we want make sure
    // screen content is NOT messed up.
    for (size_t i = 0; i < mLayers.size(); i++) {
        HwcLayer *layer = mLayers.itemAt(i);
        // set a layer to layer overlay and add it back to overlay layer list
        if (layer->getType() != HwcLayer::LAYER_OVERLAY &&
            layer->isProtected()) {
            layer->setType(HwcLayer::LAYER_OVERLAY);
            // move it from FB layer list to overlay layer list
            mFBLayers.remove(layer);
            mOverlayLayers.add(layer);
        }
    }
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
        mDisplayPlaneManager.reclaimPlane(*primaryPlane);
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
            mDisplayPlaneManager.reclaimPlane(*plane);
            break;
        }
    }
}

bool HwcLayerList::attachPrimary()
{
    // acquire primary plane of this display device
    DisplayPlane *primaryPlane =
        mDisplayPlaneManager.getPrimaryPlane(mDisplayIndex);
    if (!primaryPlane) {
        // if primary allocation is failed, it should be a fatal error
        ETRACE("failed to get primary plane for display %d", mDisplayIndex);
        return false;
    }

    // only one FB layer left, it's possible to use primary as sprite
    // if the assignment successes, we are done! update primary z order
    // and attach primary plane to this layer.
    if ((mFBLayers.size() == 1)) {
        HwcLayer *layer = mFBLayers.itemAt(0);
        if (checkSupported(DisplayPlane::PLANE_PRIMARY, layer)) {
            VTRACE("primary check passed for primary layer");
            // attach primary to layer
            layer->attachPlane(primaryPlane);
            // set the layer type to overlay
            layer->setType(HwcLayer::LAYER_OVERLAY);
            // remove layer from FB layer list
            mFBLayers.remove(layer);
            // add layer to overlay layers
            mOverlayLayers.add(layer);
            return true;
        }
    }

    // attach primary to frame buffer target
    HwcLayer *layer = mLayers.itemAt(mLayers.size() - 1);
    // need update primary plane zorder
    int primaryZOrder = -1;
    if (!calculatePrimaryZOrder(primaryZOrder)) {
        ITRACE("failed to determine primary z order");
        // reclaim primary plane
        mDisplayPlaneManager.reclaimPlane(*primaryPlane);
        return false;
    }

    // invalidate primary plane's data buffer cache
    primaryPlane->invalidateBufferCache();
    // NOTE: calling setType again to trigger glClear() for
    // other overlay layers
    layer->setType(HwcLayer::LAYER_FRAMEBUFFER_TARGET);
    // attach primary plane
    layer->attachPlane(primaryPlane);
    // update primary plane zorder
    primaryPlane->setZOrder(primaryZOrder);

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

void HwcLayerList::fallbackOverlayLayer()
{
    if (!mOverlayLayers.size()) {
        WTRACE("try to fallback overlay layer while there's no overlay layers");
        return;
    }

    // TODO: more optimization is needed to kick back a HWC_OVERLAY layer
    // e.g. select a layer based on its display frame size, whether it's
    // a protected layer and then try to attach the reclaimed plane to other
    // candidates if possible. This will requires more cpu time! currently,
    // only select the HWC_OVERLAY which is on the top of the layer stack
    // and force it back to GPU rendering.

    // get the first overlay which has been attach a plane
    HwcLayer *target = 0;
    for (int i = mOverlayLayers.size() - 1; i >= 0; i--) {
        HwcLayer *layer = mOverlayLayers.itemAt(i);
        // detach assigned plane
        DisplayPlane *plane = layer->detachPlane();
        if (plane) {
            VTRACE("falling back plane type = %d, index = %d",
                   plane->getType(), plane->getIndex());
            mDisplayPlaneManager.reclaimPlane(*plane);

            // found the first overlay layer
            target = layer;
            break;
        }
    }

    if (!target) {
        WTRACE("Couldn't find overlay layer to be fell back");
        return;
    }

    // set the layer type to LAYER_FORCE_FB
    target->setType(HwcLayer::LAYER_FORCE_FB);
    // remove layer from overlay layer list
    mOverlayLayers.remove(target);
    // add layer to FB layer list
    mFBLayers.add(target);
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
    if (!hwcLayer || (hwcLayer->getType() == HwcLayer::LAYER_FB))
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
            ITRACE("protected layer found, layer index is %d", i);
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

void HwcLayerList::dump(Dump& d)
{
    d.append("Layer list: (number of layers %d):\n", mLayers.size());
    d.append(" LAYER |          TYPE          |   PLANE  | INDEX  \n");
    d.append("-------+------------------------+------------------ \n");
    for (size_t i = 0; i < mLayers.size(); i++) {
        HwcLayer *hwcLayer = mLayers.itemAt(i);
        DisplayPlane *plane;
        int planeIndex = -1;
        const char *type = "HWC_FB";
        const char *planeType = "N/A";

        if (hwcLayer) {
            switch (hwcLayer->getType()) {
            case HwcLayer::LAYER_FB:
                type = "HWC_FB";
                break;
            case HwcLayer::LAYER_OVERLAY:
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

            d.append("  %2d   | %22s | %8s | %3D \n",
                     i, type, planeType, planeIndex);
        }
    }
}

} // namespace intel
} // namespace android
