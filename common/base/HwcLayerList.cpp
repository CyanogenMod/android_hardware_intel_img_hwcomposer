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
#include <cutils/log.h>

#include <Drm.h>
#include <Hwcomposer.h>
#include <HwcConfig.h>
#include <HwcLayerList.h>
#include <IDisplayDevice.h>
#include <PlaneCapabilities.h>

namespace android {
namespace intel {

HwcLayer::HwcLayer(int index, hwc_layer_1_t *layer)
    : mIndex(index),
      mLayer(layer),
      mPlane(0),
      mType(LAYER_FB)
{

}

bool HwcLayer::attachPlane(DisplayPlane* plane)
{
    if (mPlane) {
        LOGE("attachPlane: failed to attach plane, plane exists");
        return false;
    }

    mPlane = plane;
    return true;
}

DisplayPlane* HwcLayer::detachPlane()
{
    DisplayPlane *plane = mPlane;
    mPlane = 0;
    return plane;
}

void HwcLayer::setType(uint32_t type)
{
    if (!mLayer)
        return;

    switch (type) {
    case LAYER_SPRITE:
    case LAYER_OVERLAY:
        mLayer->compositionType = HWC_OVERLAY;
        break;
    // NOTE: set compositionType to HWC_FRAMEBUFFER here so that we have
    // a chance to submit the primary changes to HW.
    // Upper layer HWComposer will reset the compositionType automatically.
    case LAYER_PRIMARY:
    case LAYER_FB:
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
    bool ret = true;

    // if not a FB layer & a plane was attached update plane's data buffer
    if ((mType != LAYER_FB) && mPlane) {
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
        ret = mPlane->setDataBuffer((uint32_t)layer->handle);
    }

    // update layer
    mLayer = layer;

    return ret;
}

//------------------------------------------------------------------------------
HwcLayerList::HwcLayerList(hwc_display_contents_1_t *list,
                            DisplayPlaneManager& dpm,
                            DisplayPlane* primary,
                            int disp)
    : mList(list),
      mLayerCount(0),
      mDisplayPlaneManager(dpm),
      mPrimaryPlane(primary),
      mFramebufferTarget(0),
      mDisplayIndex(disp)
{
    LOGV("HwcLayerList: layer count = %d", list->numHwLayers);

    if (mList) {
        mLayers.setCapacity(list->numHwLayers);
        mOverlayLayers.setCapacity(list->numHwLayers);
        mFBLayers.setCapacity(list->numHwLayers);
        mLayerCount = list->numHwLayers;
        // analysis list from the top layer
        analyze(mLayerCount - 1);
    }
}

HwcLayerList::~HwcLayerList()
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
bool HwcLayerList::check(int planeType, hwc_layer_1_t& layer)
{
    bool valid = false;
    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();
    DataBuffer *buffer;
    uint32_t format;
    bool ret = false;

    if (!bm) {
        LOGE("HwcLayerList::check: failed to get buffer manager");
        return false;
    }

    // get buffer source format
    buffer = bm->get((uint32_t)layer.handle);
    if (!buffer) {
        LOGE("HwcLayerList::check:failed to get buffer");
        return false;
    }
    format = buffer->getFormat();
    bm->put(*buffer);

    // check layer flags
    if (layer.flags & HWC_SKIP_LAYER) {
        LOGV("plane type %d: (skip layer flag was set)", planeType);
        goto check_out;
    }

    // check buffer format
    valid = PlaneCapabilities::isFormatSupported(planeType, format);
    if (!valid) {
        LOGV("plane type %d: (bad buffer format)", planeType);
        goto check_out;
    }

    valid = PlaneCapabilities::isTransformSupported(planeType,
                                                    layer.transform);
    if (!valid) {
        LOGV("plane type $d: (bad transform)", planeType);
        goto check_out;
    }

    // check layer blending
    valid = PlaneCapabilities::isBlendingSupported(planeType,
                                                  (uint32_t)layer.blending);
    if (!valid) {
        LOGV("plane type %d: (bad blending)", planeType);
        goto check_out;
    }

    // check layer scaling
    valid = PlaneCapabilities::isScalingSupported(planeType,
                                                  layer.sourceCrop,
                                                  layer.displayFrame);
    if (!valid) {
        LOGV("plane type %d: (bad scaling)", planeType);
        goto check_out;
    }

    // check visible region?
check_out:
    return valid;
}

void HwcLayerList::setZOrder()
{
    ZOrderConfig zorder;
    int primaryIndex;
    int overlayCount;
    int planeCount;
    bool primaryAvailable;

    // set the primary to bottom by default;
    primaryIndex = -1;
    overlayCount = 0;
    planeCount = 0;
    primaryAvailable = true;
    for (int i = mOverlayLayers.size() - 1; i >= 0; i--) {
        HwcLayer *hwcLayer = mOverlayLayers.itemAt(i);
        if (!hwcLayer)
            continue;
        DisplayPlane *plane = hwcLayer->getPlane();
        if (!plane)
            continue;

        planeCount++;

        switch (plane->getType()) {
        case DisplayPlane::PLANE_SPRITE:
            break;
        case DisplayPlane::PLANE_OVERLAY:
            zorder.overlayIndexes[overlayCount++] = i;
            break;
        case DisplayPlane::PLANE_PRIMARY:
            primaryIndex = i;
            primaryAvailable = false;
            break;
        }
    }

    // primary wasn't found, set primary plane to the bottom
    if (primaryAvailable)
        primaryIndex = 0;

    // generate final z order config and pass it to all active planes
    zorder.layerCount = mLayers.size();
    zorder.planeCount = planeCount;
    zorder.overlayCount = overlayCount;
    zorder.primaryIndex = primaryIndex;

    for (int i = mOverlayLayers.size() - 1; i >= 0; i--) {
        HwcLayer *hwcLayer = mOverlayLayers.itemAt(i);
        if (!hwcLayer)
            continue;
        DisplayPlane *plane = hwcLayer->getPlane();
        if (!plane)
            continue;
        plane->setZOrderConfig(zorder);
    }
}

// This function takes following actions:
// 1) re-check plane assignment, adjust the assignment to meet
//    display controller requirement.
// 2) after re-checking, try to attach primary a layer as much as possible.
// 3) generate a final plane z-order configure for current layer list.
// NOTE: current implementation will treat overlay Layer as higher priority.
void HwcLayerList::revisit()
{
    bool primaryPlaneUsed = false;

    if (!mPrimaryPlane) {
        LOGW("HwcLayerList::revisit: no primary plane");
        return;
    }

    // detach primaryPlane
    // FIXME: make it more efficient
    for (size_t i = 0; i < mLayers.size(); i++) {
        HwcLayer *hwcLayer = mLayers.itemAt(i);
        if (!hwcLayer) {
            LOGW("revisit: no HWC layer for layer %d", i);
            continue;
        }
        // detach primary plane
        if (hwcLayer->getPlane() == mPrimaryPlane) {
            hwcLayer->detachPlane();
            hwcLayer->setType(HwcLayer::LAYER_FB);
            // remove it from overlay list
            mOverlayLayers.remove(hwcLayer);
            // add it to fb layer list
            mFBLayers.add(hwcLayer);
        }
    }

    // check whether we can take over the layer by using primary
    // we can use primary plane only when:
    // 0) Be able to be accepted by primary plane which this list layer
    //    attached to.
    // 1) all the other layers have been set to OVERLAY layer.
    if ((mFBLayers.size() == 1)) {
        HwcLayer *hwcLayer = mFBLayers.itemAt(0);
        if (check(DisplayPlane::PLANE_PRIMARY, *(hwcLayer->getLayer()))) {
            LOGV("primary check passed for primary layer");
            // attach primary to hwc layer
            hwcLayer->attachPlane(mPrimaryPlane);
            // set the layer type to sprite, since primary was treated as sprite
            hwcLayer->setType(HwcLayer::LAYER_SPRITE);
            // remove layer from FBLayers
            mFBLayers.remove(hwcLayer);
            // add layer to overlay layers
            mOverlayLayers.add(hwcLayer);

            primaryPlaneUsed = true;
        }
    }

    // if there is still FB layers, attach frame buffer target
    if (mFramebufferTarget && !primaryPlaneUsed) {
        LOGV("analyzeFrom: using frame buffer target");
        // attach primary plane
        mFramebufferTarget->attachPlane(mPrimaryPlane);
        mFramebufferTarget->setType(HwcLayer::LAYER_PRIMARY);
        // still add it to overlay list
        mOverlayLayers.add(mFramebufferTarget);
    }

    // generate z order config
    setZOrder();
}

void HwcLayerList::analyze(uint32_t index)
{
    int freeSpriteCount = 0;
    int freeOverlayCount = 0;
    int supportExtendVideo = 0;
    DisplayPlane *plane;
    Drm *drm = Hwcomposer::getInstance().getDrm();

    if (!mList || index >= mLayerCount)
        return;

    // load prop
    HwcConfig::getInstance().extendVideo(supportExtendVideo);

    freeSpriteCount = mDisplayPlaneManager.getFreeSpriteCount();
    freeOverlayCount = mDisplayPlaneManager.getFreeOverlayCount();

    // go through layer list from top to bottom
    for (int i = index; i >= 0; i--) {
        hwc_layer_1_t *layer = &mList->hwLayers[i];
        if (!layer)
            continue;

        // new hwc layer
        HwcLayer *hwcLayer = new HwcLayer(i, layer);
        if (!hwcLayer) {
            LOGE("failed to allocate hwc layer");
            continue;
        }

        // insert layer to layers
        mLayers.add(hwcLayer);

        // if a HWC_FRAMEBUFFER_TARGET layer, save it to the last
        if (layer->compositionType & HWC_FRAMEBUFFER_TARGET) {
            mFramebufferTarget = hwcLayer;
            continue;
        }

        // check whether the layer can be handled by sprite plane
        if (freeSpriteCount) {
            if (check(DisplayPlane::PLANE_SPRITE, *layer)) {
                LOGV("sprite check passed for layer %d", i);
                plane = mDisplayPlaneManager.getSpritePlane();
                if (plane) {
                    // attach plane to hwc layer
                    hwcLayer->attachPlane(plane);
                    // set the layer type to sprite
                    hwcLayer->setType(HwcLayer::LAYER_SPRITE);
                    // clear fb
                    layer->hints |=  HWC_HINT_CLEAR_FB;
                }
            }
        }

        // check whether the layer can be handled by overlay plane
        if (freeOverlayCount) {
            if (check(DisplayPlane::PLANE_OVERLAY, *layer)) {
                LOGV("overlay check passed for layer %d", i);
                plane = mDisplayPlaneManager.getOverlayPlane();
                if (plane) {
                    // attach plane to hwc layer
                    hwcLayer->attachPlane(plane);
                    // set the layer type to overlay
                    hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
                    // clear fb
                    layer->hints |=  HWC_HINT_CLEAR_FB;
                }

                // check wheter we are supporting extend video mode
                if (drm && supportExtendVideo) {
                    bool extConnected =
                        drm->outputConnected(Drm::OUTPUT_EXTERNAL);
                    if (extConnected && plane && !mDisplayIndex) {
                        hwcLayer->detachPlane();
                        mDisplayPlaneManager.putOverlayPlane(*plane);
                    }
                    hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
                }
            }
        }

        // if still FB layer
        if (!hwcLayer->getType())
            mFBLayers.add(hwcLayer);
        else
            mOverlayLayers.add(hwcLayer);
    } // for (ssize_t i = index; i >= 0; i--)

    // revisit the plane assignments
    revisit();
}

bool HwcLayerList::update(hwc_display_contents_1_t *list)
{
    bool ret;
    bool updateError = false;

    LOGV("HwcLayerList::update");

    // basic check to make sure the consistance
    if (!list) {
        LOGE("update: null layer list");
        return false;
    }

    if (list->numHwLayers != mLayerCount) {
        LOGE("update: update layer count doesn't match (%d, %d)",
              list->numHwLayers, mLayerCount);
        return false;
    }

    // update list
    mList = list;

    do {
        updateError = false;
        // update all layers, call each layer's update()
        for (size_t i = 0; i < mLayers.size(); i++) {
            HwcLayer *hwcLayer = mLayers.itemAt(i);
            if (!hwcLayer) {
                LOGE("update: no HWC layer for layer %d", i);
                continue;
            }

            ret = hwcLayer->update(&list->hwLayers[i], mDisplayIndex);
            if (ret == false) {
                LOGI("update: failed to update layer %d", i);
                updateError = true;
                // set layer to FB layer
                hwcLayer->setType(HwcLayer::LAYER_FB);
                // remove layer from overlay layer list
                mOverlayLayers.remove(hwcLayer);
                // add layer to FB layer list
                mFBLayers.add(hwcLayer);
                // revisit the overlay assignment.
                revisit();
            }
        }
    } while (updateError && mOverlayLayers.size());

    return true;
}

DisplayPlane* HwcLayerList::getPlane(uint32_t index) const
{
    HwcLayer *hwcLayer;

    if (index >= mLayers.size()) {
        LOGE("HwcLayerList::getPlane: invalid layer index %d", index);
        return 0;
    }

    hwcLayer = mLayers.itemAt(index);
    if (!hwcLayer || (hwcLayer->getType() == HwcLayer::LAYER_FB))
        return 0;

    return hwcLayer->getPlane();
}

void HwcLayerList::dump(Dump& d)
{
    d.append("Layer list: (number of layers %d):\n", mLayers.size());
    d.append(" LAYER |    TYPE    |   PLANE INDEX  \n");
    d.append("-------+------------+----------------\n");
    for (size_t i = 0; i < mLayers.size(); i++) {
        HwcLayer *hwcLayer = mLayers.itemAt(i);
        DisplayPlane *plane;
        int planeIndex = -1;
        const char *type;

        if (hwcLayer) {
            switch (hwcLayer->getType()) {
            case HwcLayer::LAYER_FB:
                type = "FB";
                break;
            case HwcLayer::LAYER_SPRITE:
                type = "Sprite";
                break;
            case HwcLayer::LAYER_OVERLAY:
                type = "Overlay";
                break;
            case HwcLayer::LAYER_PRIMARY:
                type = "Primary";
                break;
            default:
                type = "Unknown";
            }

            plane = hwcLayer->getPlane();
            if (plane)
                planeIndex = plane->getIndex();


            d.append("  %2d   | %8s   |%10D  \n", i, type, planeIndex);
        }
    }
}


} // namespace intel
} // namespace android
