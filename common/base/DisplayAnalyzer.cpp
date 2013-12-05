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
#include <cutils/properties.h>
#include <GraphicBuffer.h>
#include <ExternalDevice.h>

namespace android {
namespace intel {

DisplayAnalyzer::DisplayAnalyzer()
    : mInitialized(false),
      mEnableVideoExtendedMode(true),
      mVideoExtendedMode(false),
      mForceCloneMode(false),
      mBlankDevice(false),
      mOverlayAllowed(true),
      mCachedNumDisplays(0),
      mCachedDisplays(0),
      mPendingEvents(),
      mEventMutex(),
      mEventHandledCondition()
{
}

DisplayAnalyzer::~DisplayAnalyzer()
{
}

bool DisplayAnalyzer::initialize()
{
    // by default video extended mode is enabled
    char prop[PROPERTY_VALUE_MAX];
    if (property_get("hwc.video.extmode.enable", prop, "1") > 0) {
        mEnableVideoExtendedMode = atoi(prop) ? true : false;
    }
    mVideoExtendedMode = false;
    mForceCloneMode = false;
    mBlankDevice = false;
    mOverlayAllowed = true;
    mCachedNumDisplays = 0;
    mCachedDisplays = 0;
    mPendingEvents.clear();
    mVideoStateMap.clear();
    mInitialized = true;

    return true;
}

void DisplayAnalyzer::deinitialize()
{
    mPendingEvents.clear();
    mVideoStateMap.clear();
    mInitialized = false;
}

void DisplayAnalyzer::analyzeContents(
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    // cache and use them only in this context during analysis
    mCachedNumDisplays = numDisplays;
    mCachedDisplays = displays;

    handlePendingEvents();

    if (mEnableVideoExtendedMode) {
        detectVideoExtendedMode();
        if (mVideoExtendedMode) {
            detectTrickMode(mCachedDisplays[IDisplayDevice::DEVICE_PRIMARY]);
        }
    }

    if (mBlankDevice) {
        // this will make sure device is blanked after geometry changes.
        // blank event is only processed once
        blankSecondaryDevice();
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
        resetCompositionType(list);
    }
}

void DisplayAnalyzer::detectVideoExtendedMode()
{
    if (mVideoStateMap.size() != 1) {
        mVideoExtendedMode = false;
        mForceCloneMode = false;
        return;
    }

    bool geometryChanged = false;
    int activeDisplays = 0;

    hwc_display_contents_1_t *content = NULL;
    for (int i = 0; i < (int)mCachedNumDisplays; i++) {
        content = mCachedDisplays[i];
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
    content = mCachedDisplays[0];
    if (content == NULL) {
        return;
    }

    uint32_t videoHandle = 0;
    bool videoLayerExist = false;
    // exclude the frame buffer target layer
    for (int j = 0; j < (int)content->numHwLayers - 1; j++) {
        videoLayerExist = isVideoLayer(content->hwLayers[j]);
        if (videoLayerExist) {
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
    for (int i = 1; i < (int)mCachedNumDisplays; i++) {
        content = mCachedDisplays[i];
        if (content == NULL) {
            continue;
        }

        // exclude the frame buffer target layer
        for (int j = 0; j < (int)content->numHwLayers - 1; j++) {
            if ((uint32_t)content->hwLayers[j].handle == videoHandle) {
                VTRACE("video layer exists in device %d", i);
                mVideoExtendedMode = isVideoFullScreen(i, content->hwLayers[j]);
#ifdef INTEL_WIDI_MERRIFIELD 
                if (i == IDisplayDevice::DEVICE_VIRTUAL) {
                    // WiDi exception: always in video extended mode
                    mVideoExtendedMode = true;
                }
#endif
                return;
            }
        }
    }
}

bool DisplayAnalyzer::checkVideoExtendedMode()
{
    return mVideoExtendedMode && !mForceCloneMode;
}

bool DisplayAnalyzer::isVideoExtendedModeEnabled()
{
#if 1
    // enable it for run-time debugging purpose.
    char prop[PROPERTY_VALUE_MAX];
    if (property_get("hwc.video.extmode.enable", prop, "1") > 0) {
        mEnableVideoExtendedMode = atoi(prop) ? true : false;
    }
    ITRACE("video extended mode enabled: %d", mEnableVideoExtendedMode);
#endif

    return mEnableVideoExtendedMode;
}

bool DisplayAnalyzer::isVideoLayer(hwc_layer_1_t &layer)
{
    bool ret = false;
    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();
    if (!layer.handle) {
        return false;
    }
    DataBuffer *buffer = bm->lockDataBuffer((uint32_t)layer.handle);
     if (!buffer) {
         ETRACE("failed to get buffer");
     } else {
        ret = DisplayQuery::isVideoFormat(buffer->getFormat());
        bm->unlockDataBuffer(buffer);
    }
    return ret;
}

bool DisplayAnalyzer::isVideoFullScreen(int device, hwc_layer_1_t &layer)
{
    IDisplayDevice *displayDevice = Hwcomposer::getInstance().getDisplayDevice(device);
    if (!displayDevice) {
        return false;
    }
    int width = 0, height = 0;
    if (!displayDevice->getDisplaySize(&width, &height)) {
        return false;
    }

    VTRACE("video left %d, right %d, top %d, bottom %d, device width %d, height %d",
        layer.displayFrame.left, layer.displayFrame.right,
        layer.displayFrame.top, layer.displayFrame.bottom,
        width, height);

    int dstW = layer.displayFrame.right - layer.displayFrame.left;
    int dstH = layer.displayFrame.bottom - layer.displayFrame.top;
    if (dstW < width - 1 &&
        dstH < height - 1) {
        VTRACE("video is not full-screen");
        return false;
    }
    return true;
}

bool DisplayAnalyzer::isOverlayAllowed()
{
    return mOverlayAllowed;
}

int DisplayAnalyzer::getVideoInstances()
{
    return (int)mVideoStateMap.size();
}

void DisplayAnalyzer::postHotplugEvent(bool connected)
{
    // TODO: turn on primary display immeidately

    if (!connected) {
        // enable vsync on the primary device immediately
        Hwcomposer::getInstance().getVsyncManager()->resetVsyncSource();
    } else {
        // handle hotplug event (vsync switch) asynchronously
        Event e;
        e.type = HOTPLUG_EVENT;
        e.bValue = connected;
        postEvent(e);
        Hwcomposer::getInstance().invalidate();
    }
}

void DisplayAnalyzer::postVideoEvent(int instanceID, int state)
{
    Event e;
    e.type = VIDEO_EVENT;
    e.videoEvent.instanceID = instanceID;
    e.videoEvent.state = state;
    postEvent(e);

    if ((state == VIDEO_PLAYBACK_STARTING) ||
        (state == VIDEO_PLAYBACK_STOPPING && hasProtectedLayer())) {
        Hwcomposer::getInstance().invalidate();
        Mutex::Autolock lock(mEventMutex);
        // ideally overlay should be disabled in the surface flinger thread, if it is not processed
        // in close to one vsync cycle (50ms)  it will be safely disabled in this thread context
        // there is no threading issue
        status_t err = mEventHandledCondition.waitRelative(mEventMutex, milliseconds(50));
        if (err == -ETIMEDOUT) {
            WTRACE("timeout waiting for event handling");
            Hwcomposer::getInstance().getPlaneManager()->disableOverlayPlanes();
        }
    }
}

void DisplayAnalyzer::postBlankEvent(bool blank)
{
    Event e;
    e.type = BLANK_EVENT;
    e.bValue = blank;
    postEvent(e);
    Hwcomposer::getInstance().invalidate();
}

void DisplayAnalyzer::postEvent(Event& e)
{
    Mutex::Autolock lock(mEventMutex);
    mPendingEvents.add(e);
}

bool DisplayAnalyzer::getEvent(Event& e)
{
    Mutex::Autolock lock(mEventMutex);
    if (mPendingEvents.size() == 0) {
        return false;
    }
    e = mPendingEvents[0];
    mPendingEvents.removeAt(0);
    return true;
}

void DisplayAnalyzer::handlePendingEvents()
{
    // handle one event per analysis to avoid blocking surface flinger
    // some event may take lengthy time to process
    Event e;
    if (!getEvent(e)) {
        return;
    }

    switch (e.type) {
    case HOTPLUG_EVENT:
        handleHotplugEvent(e.bValue);
        break;
    case BLANK_EVENT:
        handleBlankEvent(e.bValue);
        break;
    case VIDEO_EVENT:
        handleVideoEvent(e.videoEvent.instanceID, e.videoEvent.state);
        break;
    case TIMING_EVENT:
        handleTimingEvent();
        break;
    }
}

void DisplayAnalyzer::handleHotplugEvent(bool connected)
{
    Hwcomposer::getInstance().getVsyncManager()->resetVsyncSource();
}

void DisplayAnalyzer::handleBlankEvent(bool blank)
{
    mBlankDevice = blank;
    // force geometry changed in the secondary device to reset layer composition type
    for (int i = 0; i < (int)mCachedNumDisplays; i++) {
        if (i == IDisplayDevice::DEVICE_PRIMARY) {
            continue;
        }
        if (mCachedDisplays[i]) {
            mCachedDisplays[i]->flags |= HWC_GEOMETRY_CHANGED;
        }
    }
    blankSecondaryDevice();
}

void DisplayAnalyzer::handleTimingEvent()
{
    // check whether external device is connected, reset refresh rate to match video frame rate
    // if video is in playing state or reset refresh rate to default preferred one if video is not
    // at playing state
    Hwcomposer *hwc = &Hwcomposer::getInstance();
    ExternalDevice *dev = NULL;
    dev = (ExternalDevice *)hwc->getDisplayDevice(IDisplayDevice::DEVICE_EXTERNAL);
    if (!dev) {
        return;
    }

    if (!dev->isConnected()) {
        return;
    }

    if (hwc->getMultiDisplayObserver()->isExternalDeviceTimingFixed()) {
        VTRACE("Timing of external device is fixed.");
        return;
    }

    int hz = 0;
    if (mVideoStateMap.size() == 1) {
        VideoSourceInfo info;
        int instanceID = mVideoStateMap.keyAt(0);
        status_t err = hwc->getMultiDisplayObserver()->getVideoSourceInfo(
                instanceID, &info);
        if (err == NO_ERROR) {
            hz = info.frameRate;
        }
    }

    dev->setRefreshRate(hz);
}

void DisplayAnalyzer::handleVideoEvent(int instanceID, int state)
{
    mVideoStateMap.removeItem(instanceID);
    if (state != VIDEO_PLAYBACK_STOPPED) {
        mVideoStateMap.add(instanceID, state);
    }

    Hwcomposer *hwc = &Hwcomposer::getInstance();

    // sanity check
    if (hwc->getMultiDisplayObserver()->getVideoSessionNumber() !=
        (int)mVideoStateMap.size()) {
        WTRACE("session number does not match!!");
        mVideoStateMap.clear();
        if (state != VIDEO_PLAYBACK_STOPPED) {
            mVideoStateMap.add(instanceID, state);
        }
    }

    if ((state == VIDEO_PLAYBACK_STARTING) ||
        (state == VIDEO_PLAYBACK_STOPPING && hasProtectedLayer())) {
        hwc_display_contents_1_t *content = NULL;
        for (int i = 0; i < (int)mCachedNumDisplays; i++) {
            content = mCachedDisplays[i];
            if (content == NULL) {
                continue;
            }
            content->flags |= HWC_GEOMETRY_CHANGED;
            resetCompositionType(content);
        }
        // if video is in starting or stopping stage, overlay use is temporarily not allowed to
        // avoid scrambed RGB overlay if video is protected.
        mOverlayAllowed = false;

        // disable overlay plane and acknolwdge the waiting thread
        hwc->getPlaneManager()->disableOverlayPlanes();
        mEventHandledCondition.signal();
    } else {
        mOverlayAllowed = true;
    }

    // delay changing timing as it is a lengthy operation
    if (state == VIDEO_PLAYBACK_STARTED ||
        state == VIDEO_PLAYBACK_STOPPED) {
        Event e;
        e.type = TIMING_EVENT;
        postEvent(e);
    }
}

void DisplayAnalyzer::blankSecondaryDevice()
{
    hwc_display_contents_1_t *content = NULL;
    hwc_layer_1 *layer = NULL;
    for (int i = 0; i < (int)mCachedNumDisplays; i++) {
        if (i == IDisplayDevice::DEVICE_PRIMARY) {
            continue;
        }
        content = mCachedDisplays[i];
        if (content == NULL) {
            continue;
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
}

bool DisplayAnalyzer::isPresentationLayer(hwc_layer_1_t &layer)
{
    if (mCachedDisplays == NULL) {
        return false;
    }
    // check if the given layer exists in the primary device
    hwc_display_contents_1_t *content = mCachedDisplays[0];
    if (content == NULL) {
        return false;
    }
    for (size_t i = 0; i < content->numHwLayers - 1; i++) {
        if ((uint32_t)content->hwLayers[i].handle == (uint32_t)layer.handle) {
            VTRACE("Layer exists for Primary device");
            return false;
        }
    }
    return true;
}

bool DisplayAnalyzer::hasProtectedLayer()
{
    DataBuffer * buffer = NULL;
    hwc_display_contents_1_t *content = NULL;
    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();

    if (bm == NULL){
        return false;
    }

    if (mCachedDisplays == NULL) {
        return false;
    }
    // check if the given layer exists in the primary device
    for (int index = 0; index < (int)mCachedNumDisplays; index++) {
        content = mCachedDisplays[index];
        if (content == NULL) {
            continue;
        }

        for (size_t i = 0; i < content->numHwLayers - 1; i++) {
            if (isProtectedLayer(content->hwLayers[i]))
                return true;
        }
    }

    return false;
}

// reset the composition type of all layers to default
void DisplayAnalyzer::resetCompositionType(hwc_display_contents_1_t *display)
{
    if (display == NULL)
        return;

    for (size_t i = 0; i < display->numHwLayers - 1; i++) {
        hwc_layer_1_t *layer = &display->hwLayers[i];
        if (layer) layer->compositionType = HWC_FRAMEBUFFER;
    }
}

bool DisplayAnalyzer::isProtectedLayer(hwc_layer_1_t &layer)
{
    bool ret = false;
    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();
    if (!layer.handle) {
        return ret;
    }
    DataBuffer *buffer = bm->lockDataBuffer((uint32_t)layer.handle);
    if (!buffer) {
        ETRACE("failed to get buffer");
    } else {
        ret = GraphicBuffer::isProtectedBuffer((GraphicBuffer*)buffer);
        bm->unlockDataBuffer(buffer);
    }
    return ret;
}

} // namespace intel
} // namespace android

