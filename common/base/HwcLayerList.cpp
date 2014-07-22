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
      mIsProtected(false),
      mType(LAYER_FB)
{
    setupAttributes();
}

bool HwcLayer::attachPlane(DisplayPlane* plane)
{
    if (mPlane) {
        ETRACE("failed to attach plane, plane exists");
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
    case LAYER_OVERLAY:
        mLayer->compositionType = HWC_OVERLAY;
        mLayer->hints |= HWC_HINT_CLEAR_FB;
        break;
    // NOTE: set compositionType to HWC_FRAMEBUFFER here so that we have
    // a chance to submit the primary changes to HW.
    // Upper layer HWComposer will reset the compositionType automatically.
    case LAYER_FRAMEBUFFER_TARGET:
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

uint32_t HwcLayer::getFormat() const
{
    return mFormat;
}

uint32_t HwcLayer::getUsage() const
{
    return mUsage;
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
    bool ret;

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
        ret = mPlane->setDataBuffer((uint32_t)layer->handle);
        if (ret == true) {
            return true;
        }
        ETRACE("failed to set data buffer");
        if (!mIsProtected) {
            return false;
        } else {
            // protected video has to be rendered using overlay.
            // if buffer is not ready overlay will still be attached to this layer
            // but rendering needs to be skipped.
            ETRACE("ignoring result of data buffer setting for protected video");
            return true;
        }
    }

    return true;
}

void HwcLayer::setupAttributes()
{
    if (mFormat != DataBuffer::FORMAT_INVALID) {
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

    DataBuffer *buffer = bm->get((uint32_t)mLayer->handle);
     if (!buffer) {
         ETRACE("failed to get buffer");
     } else {
        mFormat = buffer->getFormat();
        GraphicBuffer *gBuffer = (GraphicBuffer*)buffer;
        mUsage = gBuffer->getUsage();
        mIsProtected = GraphicBuffer::isProtectedBuffer((GraphicBuffer*)buffer);
        bm->put(*buffer);
    }
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
    if (mList) {
        VTRACE("layer count = %d", list->numHwLayers);
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
    mLayers.clear();
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
    valid = PlaneCapabilities::isFormatSupported(planeType, hwcLayer->getFormat());
    if (!valid) {
        VTRACE("plane type %d: (bad buffer format)", planeType);
        goto check_out;
    }

    valid = PlaneCapabilities::isTransformSupported(planeType,
                                                    layer.transform);
    if (!valid) {
        VTRACE("plane type %d: (bad transform)", planeType);
        goto check_out;
    }

    // check layer blending
    valid = PlaneCapabilities::isBlendingSupported(planeType,
                                                  (uint32_t)layer.blending);
    if (!valid) {
        VTRACE("plane type %d: (bad blending)", planeType);
        goto check_out;
    }

    // check layer scaling
    valid = PlaneCapabilities::isScalingSupported(planeType,
                                                  layer.sourceCrop,
                                                  layer.displayFrame);
    if (!valid) {
        VTRACE("plane type %d: (bad scaling)", planeType);
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
    int spriteCount;
    int planeCount;
    bool primaryAvailable;

    // set the primary to bottom by default;
    primaryIndex = -1;
    overlayCount = 0;
    spriteCount = 0;
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
            zorder.spriteIndexes[spriteCount++] = hwcLayer->getIndex();
            break;
        case DisplayPlane::PLANE_OVERLAY:
            zorder.overlayIndexes[overlayCount++] = hwcLayer->getIndex();
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
    zorder.spriteCount = spriteCount;
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
        WTRACE("no primary plane");
        return;
    }

    // detach primaryPlane
    // FIXME: make it more efficient
    for (size_t i = 0; i < mLayers.size(); i++) {
        HwcLayer *hwcLayer = mLayers.itemAt(i);
        if (!hwcLayer) {
            WTRACE("no HWC layer for layer %d", i);
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
        if (checkSupported(DisplayPlane::PLANE_PRIMARY, hwcLayer)) {
            VTRACE("primary check passed for primary layer");
            // attach primary to hwc layer
            hwcLayer->attachPlane(mPrimaryPlane);
            // set the layer type to overlay
            hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
            // remove layer from FBLayers
            mFBLayers.remove(hwcLayer);
            // add layer to overlay layers
            mOverlayLayers.add(hwcLayer);

            primaryPlaneUsed = true;
        }
    }

    // if there is still FB layers, attach frame buffer target
    if (mFramebufferTarget && !primaryPlaneUsed) {
        VTRACE("using frame buffer target");
        // attach primary plane
        mFramebufferTarget->attachPlane(mPrimaryPlane);
        mFramebufferTarget->setType(HwcLayer::LAYER_FRAMEBUFFER_TARGET);
        // still add it to overlay list
        mOverlayLayers.add(mFramebufferTarget);
    }

    // generate z order config
    setZOrder();
}

void HwcLayerList::analyze(uint32_t index)
{
    Hwcomposer& hwc = Hwcomposer::getInstance();
    Drm *drm = hwc.getDrm();
    DisplayPlane *plane;

    if (!mList || index >= mLayerCount || !drm)
        return;

    // go through layer list from top to bottom
    for (int i = index; i >= 0; i--) {
        hwc_layer_1_t *layer = &mList->hwLayers[i];
        if (!layer) {
            // this will cause index and plane out of sync, see getPlane API
            ETRACE("layer %d is null", i);
            continue;
        }

        // new hwc layer
        HwcLayer *hwcLayer = new HwcLayer(i, layer);
        if (!hwcLayer) {
            ETRACE("failed to allocate hwc layer");
            continue;
        }

        // insert layer to layers
        mLayers.add(hwcLayer);

        if (layer->compositionType == HWC_OVERLAY) {
            // layer has been preprocessed
            hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
            mOverlayLayers.add(hwcLayer);
            continue;
        }

        // if a HWC_FRAMEBUFFER_TARGET layer, save it to the last
        if (layer->compositionType == HWC_FRAMEBUFFER_TARGET) {
            mFramebufferTarget = hwcLayer;
            continue;
        }

        if (layer->handle == NULL) {
            VTRACE("null buffer handle");
            mFBLayers.add(hwcLayer);
            continue;
        }

        // check whether the layer can be handled by sprite plane
        if (checkSupported(DisplayPlane::PLANE_SPRITE, hwcLayer)) {
            VTRACE("sprite check passed for layer %d", i);
            plane = mDisplayPlaneManager.getSpritePlane();
            if (plane) {
                // enable plane
                if (!plane->enable()) {
                    ETRACE("sprite plane is not ready");
                    mDisplayPlaneManager.putSpritePlane(*plane);
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

            plane = mDisplayPlaneManager.getOverlayPlane();
            if (plane) {
                hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
                hwcLayer->attachPlane(plane);
                mOverlayLayers.add(hwcLayer);
                continue;
            } else if (hwcLayer->isProtected()) {
                // TODO: we need to detach overlay from non-protected layers
                WTRACE("protected layer is skipped");
                hwcLayer->setType(HwcLayer::LAYER_OVERLAY);
                mOverlayLayers.add(hwcLayer);
                continue;
            } else {
                // fall back to GPU rendering
                hwcLayer->setType(HwcLayer::LAYER_FB);
                ITRACE("overlay plane is not available for video layer %d", i);
            }
        }

        // if still FB layer
        if (hwcLayer->getType() == HwcLayer::LAYER_FB) {
            mFBLayers.add(hwcLayer);
        }
    } // for (ssize_t i = index; i >= 0; i--)

    // revisit the plane assignments
    revisit();
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
        for (size_t i = 0; i < mLayers.size(); i++) {
            HwcLayer *hwcLayer = mLayers.itemAt(i);
            if (!hwcLayer) {
                ETRACE("no HWC layer for layer %d", i);
                continue;
            }

            ret = hwcLayer->update(&list->hwLayers[i], mDisplayIndex);
            if (ret == false) {
                // layer update failed, fall back to ST and revisit all plane
                // assignment
                ITRACE("failed to update layer %d", i);
                // set layer to FB layer
                hwcLayer->setType(HwcLayer::LAYER_FB);
                // remove layer from overlay layer list
                mOverlayLayers.remove(hwcLayer);
                // add layer to FB layer list
                mFBLayers.add(hwcLayer);
                // revisit the overlay assignment.
                revisit();

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
