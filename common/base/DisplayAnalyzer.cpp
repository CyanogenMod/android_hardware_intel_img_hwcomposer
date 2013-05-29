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
#include <IDisplayDevice.h>
#include <DisplayQuery.h>
#include <BufferManager.h>
#include <DisplayPlaneManager.h>
#include <Hwcomposer.h>
#include <DisplayAnalyzer.h>


namespace android {
namespace intel {

DisplayAnalyzer::DisplayAnalyzer()
    : mInitialized(false),
      mVideoExtendedMode(false),
      mForceCloneMode(false),
      mBlankDevice(false),
      mBlankPending(false),
      mBlankMutex()
{
}

DisplayAnalyzer::~DisplayAnalyzer()
{
}

bool DisplayAnalyzer::initialize()
{
    mVideoExtendedMode = false;
    mForceCloneMode = false;
    mBlankPending = false;
    mBlankDevice = false;
    mInitialized = true;
    return true;
}

void DisplayAnalyzer::uninitialize()
{
    mInitialized = false;
}

void DisplayAnalyzer::analyzeContents(
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    blankSecondaryDevice(numDisplays, displays);
    detectVideoExtendedMode(numDisplays, displays);
    if (mVideoExtendedMode) {
        detectTrickMode(displays[IDisplayDevice::DEVICE_PRIMARY]);
    }
}

void DisplayAnalyzer::detectTrickMode(hwc_display_contents_1_t *list)
{
    if (list == NULL)
        return;

    bool detected = false;
    for (size_t i = 0; i < list->numHwLayers; i++) {
        hwc_layer_1_t *layer = &list->hwLayers[i];
        if (layer && (layer->flags & HWC_TRICK_MODE)) {
            detected = true;
            break;
        }
    }

    if (detected != mForceCloneMode) {
        list->flags |= HWC_GEOMETRY_CHANGED;
        mForceCloneMode = detected;
    }
}

void DisplayAnalyzer::detectVideoExtendedMode(
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    bool geometryChanged = false;
    int activeDisplays = 0;

    hwc_display_contents_1_t *content = NULL;
    for (int i = 0; i < (int)numDisplays; i++) {
        content = displays[i];
        if (content == NULL) {
            continue;
        }
        activeDisplays++;
        if (content->flags & HWC_GEOMETRY_CHANGED) {
            geometryChanged = true;
        }
    }

    if (activeDisplays <= 1) {
        mVideoExtendedMode = false;
        return;
    }

    if (geometryChanged == false) {
        // use previous analysis result
        return;
    }

    // reset status of video extended mode
    mVideoExtendedMode = false;

    // check if there is video layer in the primary device
    content = displays[0];
    if (content == NULL) {
        return;
    }

    uint32_t videoHandle = 0;
    bool videoLayerExist = false;
    // exclude the frame buffer target layer
    for (int j = 0; j < (int)content->numHwLayers - 1; j++) {
        videoLayerExist = isVideoLayer(content->hwLayers[j]);
        if (videoLayerExist == true) {
            videoHandle = (uint32_t)content->hwLayers[j].handle;
            break;
        }
    }

    if (videoLayerExist == false) {
        // no video layer is found in the primary layer
        return;
    }

    // check whether video layer exists in external device or virtual device
    // TODO: video may exist in virtual device but no in external device or vice versa
    // TODO: multiple video layers are not addressed here
    for (int i = 1; i < (int)numDisplays; i++) {
        content = displays[i];
        if (content == NULL) {
            continue;
        }

        // exclude the frame buffer target layer
        for (int j = 0; j < (int)content->numHwLayers - 1; j++) {
            if ((uint32_t)content->hwLayers[j].handle == videoHandle) {
                ITRACE("video layer exists in device %d", i);
                mVideoExtendedMode = true;
                return;
            }
        }
    }
}

bool DisplayAnalyzer::checkVideoExtendedMode()
{
    return mVideoExtendedMode && !mForceCloneMode;
}

bool DisplayAnalyzer::isVideoLayer(hwc_layer_1_t &layer)
{
    bool ret = false;
    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();
    DataBuffer *buffer = bm->get((uint32_t)layer.handle);
     if (!buffer) {
         ETRACE("failed to get buffer");
     } else {
        ret = DisplayQuery::isVideoFormat(buffer->getFormat());
        bm->put(*buffer);
    }
    return ret;
}

bool DisplayAnalyzer::blankSecondaryDevice(bool blank)
{
    ITRACE("Blanking secondary device: %d", blank);
    Mutex::Autolock lock(mBlankMutex);
    mBlankDevice = blank;
    mBlankPending = true;
    Hwcomposer::getInstance().invalidate();
    return true;
}

bool DisplayAnalyzer::blankSecondaryDevice(
    size_t numDisplays,
    hwc_display_contents_1_t** displays)
{
    Mutex::Autolock lock(mBlankMutex);
    if (!mBlankPending && !mBlankDevice) {
        // if device needs to be blanked all layers should be marked as HWC_OVERLAY and skipped.
        // otherwise nothing to do
        return false;
    }

    hwc_display_contents_1_t *content = NULL;
    hwc_layer_1 *layer = NULL;
    for (int i = 0; i < (int)numDisplays; i++) {
        if (i == IDisplayDevice::DEVICE_PRIMARY) {
            continue;
        }
        content = displays[i];
        if (content == NULL) {
            continue;
        }

        if (mBlankPending){
            content->flags = HWC_GEOMETRY_CHANGED;
            mBlankPending = false;
        }

        for (int j = 0; j < (int)content->numHwLayers - 1; j++) {
            layer = &content->hwLayers[j];
            if (!layer) {
                continue;
            }
            if (mBlankDevice) {
                layer->hints |= HWC_HINT_CLEAR_FB;
                layer->flags &= ~HWC_SKIP_LAYER;
                layer->compositionType = HWC_OVERLAY;
            } else {
                layer->hints &= ~HWC_HINT_CLEAR_FB;
                layer->compositionType = HWC_FRAMEBUFFER;
            }
        }
    }
    return true;
}

} // namespace intel
} // namespace android

