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
#include <Hwcomposer.h>
#include <DisplayPlaneManager.h>
#include <DisplayQuery.h>
#include <VirtualDevice.h>
#include <IVideoPayloadManager.h>

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

namespace android {
namespace intel {

VirtualDevice::CachedBuffer::CachedBuffer(BufferManager *mgr, buffer_handle_t handle)
    : manager(mgr),
      mapper(NULL)
{
    DataBuffer *buffer = manager->lockDataBuffer((uint32_t) handle);
    mapper = manager->map(*buffer);
    manager->unlockDataBuffer(buffer);
}

VirtualDevice::CachedBuffer::~CachedBuffer()
{
    manager->unmap(mapper);
}

VirtualDevice::VirtualDevice(Hwcomposer& hwc, DisplayPlaneManager& dpm)
    : mInitialized(false),
      mHwc(hwc),
      mDisplayPlaneManager(dpm)
{
    CTRACE();
}

VirtualDevice::~VirtualDevice()
{
    WARN_IF_NOT_DEINIT();
}

sp<VirtualDevice::CachedBuffer> VirtualDevice::getDisplayBuffer(buffer_handle_t handle)
{
    ssize_t index = mDisplayBufferCache.indexOfKey(handle);
    sp<CachedBuffer> cachedBuffer;
    if (index == NAME_NOT_FOUND) {
        cachedBuffer = new CachedBuffer(mHwc.getBufferManager(), handle);
        mDisplayBufferCache.add(handle, cachedBuffer);
    } else {
        cachedBuffer = mDisplayBufferCache[index];
    }
    return cachedBuffer;
}

status_t VirtualDevice::start(sp<IFrameTypeChangeListener> typeChangeListener, bool disableExtVideoMode)
{
    ITRACE();
    Mutex::Autolock _l(mConfigLock);
    mNextConfig.typeChangeListener = typeChangeListener;
    mNextConfig.policy.scaledWidth = 0;
    mNextConfig.policy.scaledHeight = 0;
    mNextConfig.policy.xdpi = 96;
    mNextConfig.policy.ydpi = 96;
    mNextConfig.policy.refresh = 60;
    mNextConfig.extendedModeEnabled =
        Hwcomposer::getInstance().getDisplayAnalyzer()->isVideoExtendedModeEnabled();
    mNextConfig.forceNotify = true;
    return NO_ERROR;
}

status_t VirtualDevice::stop(bool isConnected)
{
    ITRACE();
    Mutex::Autolock _l(mConfigLock);
    mNextConfig.typeChangeListener = NULL;
    mNextConfig.policy.scaledWidth = 0;
    mNextConfig.policy.scaledHeight = 0;
    mNextConfig.policy.xdpi = 96;
    mNextConfig.policy.ydpi = 96;
    mNextConfig.policy.refresh = 60;
    mNextConfig.extendedModeEnabled = false;
    mNextConfig.forceNotify = false;
    return NO_ERROR;
}

status_t VirtualDevice::notifyBufferReturned(int khandle)
{
    CTRACE();
    Mutex::Autolock _l(mHeldBuffersLock);
    ssize_t index = mHeldBuffers.indexOfKey(khandle);
    if (index == NAME_NOT_FOUND) {
        ETRACE("Couldn't find returned khandle %x", khandle);
    } else {
        sp<CachedBuffer> cachedBuffer = mHeldBuffers.valueAt(index);
        if (!mPayloadManager->setRenderStatus(cachedBuffer->mapper, false)) {
            ETRACE("Failed to set render status");
        }
        mHeldBuffers.removeItemsAt(index, 1);
    }
    return NO_ERROR;
}

status_t VirtualDevice::setResolution(const FrameProcessingPolicy& policy, sp<IFrameListener> listener)
{
    CTRACE();
    Mutex::Autolock _l(mConfigLock);
    mNextConfig.frameListener = listener;
    mNextConfig.policy = policy;
    return NO_ERROR;
}

bool VirtualDevice::prePrepare(hwc_display_contents_1_t *display)
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool VirtualDevice::prepare(hwc_display_contents_1_t *display)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (!display) {
        return true;
    }

    {
        Mutex::Autolock _l(mConfigLock);
        mCurrentConfig = mNextConfig;
        mNextConfig.forceNotify = false;
    }

    buffer_handle_t videoFrame = NULL;
    hwc_layer_1_t* videoLayer = NULL;

    if (mCurrentConfig.extendedModeEnabled) {
        DisplayAnalyzer *analyzer = mHwc.getDisplayAnalyzer();
        if (analyzer->checkVideoExtendedMode()) {
            for (size_t i = 0; i < display->numHwLayers-1; i++) {
                hwc_layer_1_t& layer = display->hwLayers[i];
                if (analyzer->isVideoLayer(layer)) {
                    videoFrame = layer.handle;
                    videoLayer = &layer;
                    break;
                }
            }
        }
    }

    bool extActive = false;
    if (mCurrentConfig.typeChangeListener != NULL) {
        FrameInfo frameInfo;

        if (videoFrame != NULL) {
            sp<CachedBuffer> cachedBuffer;
            if ((cachedBuffer = getDisplayBuffer(videoFrame)) == NULL) {
                ETRACE("Failed to map display buffer");
            } else {
                memset(&frameInfo, 0, sizeof(frameInfo));

                IVideoPayloadManager::MetaData metadata;
                if (mPayloadManager->getMetaData(cachedBuffer->mapper, &metadata)) {

                    frameInfo.frameType = HWC_FRAMETYPE_VIDEO;
                    frameInfo.bufferFormat = metadata.format;

                    hwc_layer_1_t& layer = *videoLayer;
                    if ((metadata.transform & HAL_TRANSFORM_ROT_90) == 0) {
                        frameInfo.contentWidth = layer.sourceCrop.right - layer.sourceCrop.left;
                        frameInfo.contentHeight = layer.sourceCrop.bottom - layer.sourceCrop.top;
                    } else {
                        frameInfo.contentWidth = layer.sourceCrop.bottom - layer.sourceCrop.top;
                        frameInfo.contentHeight = layer.sourceCrop.right - layer.sourceCrop.left;
                    }

                    frameInfo.bufferWidth = metadata.width;
                    frameInfo.bufferHeight = metadata.height;
                    frameInfo.lumaUStride = metadata.lumaStride;
                    frameInfo.chromaUStride = metadata.chromaUStride;
                    frameInfo.chromaVStride = metadata.chromaVStride;

                    // TODO: Need to get framerate from HWC when available (for now indicate default with zero)
                    frameInfo.contentFrameRateN = 0;
                    frameInfo.contentFrameRateD = 1;

                    if (frameInfo.bufferFormat != 0 &&
                            frameInfo.bufferWidth >= frameInfo.contentWidth &&
                            frameInfo.bufferHeight >= frameInfo.contentHeight &&
                            frameInfo.contentWidth > 0 && frameInfo.contentHeight > 0 &&
                            frameInfo.lumaUStride > 0 &&
                            frameInfo.chromaUStride > 0 && frameInfo.chromaVStride > 0) {
                        extActive = true;
                    } else {
                        ITRACE("Payload cleared or inconsistent info, aborting extended mode");
                    }
                } else {
                    ETRACE("Failed to get metadata");
                }
            }
        }

        if (!extActive) {
            memset(&frameInfo, 0, sizeof(frameInfo));
            frameInfo.frameType = HWC_FRAMETYPE_NOTHING;
        }

        if (mCurrentConfig.forceNotify || memcmp(&frameInfo, &mLastFrameInfo, sizeof(frameInfo)) != 0) {
            // something changed, notify type change listener
            mCurrentConfig.typeChangeListener->frameTypeChanged(frameInfo);
            mCurrentConfig.typeChangeListener->bufferInfoChanged(frameInfo);

            mExtLastTimestamp = 0;
            mExtLastKhandle = 0;

            if (frameInfo.frameType == HWC_FRAMETYPE_NOTHING) {
                ITRACE("Clone mode");
                mDisplayBufferCache.clear();
            } else {
                ITRACE("Extended mode: %dx%d in %dx%d @ %d fps",
                      frameInfo.contentWidth, frameInfo.contentHeight,
                      frameInfo.bufferWidth, frameInfo.bufferHeight,
                      frameInfo.contentFrameRateN);
            }
            mLastFrameInfo = frameInfo;
        }
    }

    if (extActive) {
        // tell surfaceflinger to not render the layers if we're
        // in extended video mode
        for (size_t i = 0; i < display->numHwLayers-1; i++) {
            hwc_layer_1_t& layer = display->hwLayers[i];
            if (layer.compositionType != HWC_BACKGROUND) {
                layer.compositionType = HWC_OVERLAY;
                layer.flags |= HWC_HINT_DISABLE_ANIMATION;
            }
        }

        if (mCurrentConfig.frameListener != NULL) {
            sp<CachedBuffer> cachedBuffer = getDisplayBuffer(videoFrame);
            if (cachedBuffer == NULL) {
                ETRACE("Failed to map display buffer");
                return true;
            }

            IVideoPayloadManager::MetaData metadata;
            if (mPayloadManager->getMetaData(cachedBuffer->mapper, &metadata)) {

                if (metadata.timestamp == mExtLastTimestamp && metadata.kHandle == mExtLastKhandle)
                    return true;

                mExtLastTimestamp = metadata.timestamp;
                mExtLastKhandle = metadata.kHandle;

                status_t status = mCurrentConfig.frameListener->onFrameReady(
                        metadata.kHandle, HWC_HANDLE_TYPE_KBUF, systemTime(), metadata.timestamp);
                if (status == OK) {
                    if (!mPayloadManager->setRenderStatus(cachedBuffer->mapper, true)) {
                        ETRACE("Failed to set render status");
                    }
                    Mutex::Autolock _l(mHeldBuffersLock);
                    mHeldBuffers.add(metadata.kHandle, cachedBuffer);
                }
            } else {
                ETRACE("Failed to get metadata");
            }
        }
    }

    return true;
}

bool VirtualDevice::commit(hwc_display_contents_1_t *display, IDisplayContext *context)
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool VirtualDevice::vsyncControl(bool enabled)
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool VirtualDevice::blank(bool blank)
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool VirtualDevice::getDisplaySize(int *width, int *height)
{
    RETURN_FALSE_IF_NOT_INIT();
    if (!width || !height) {
        ETRACE("invalid parameters");
        return false;
    }

    // TODO: make this platform specifc
    *width = 1280;
    *height = 720;
    return true;
}

bool VirtualDevice::getDisplayConfigs(uint32_t *configs,
                                         size_t *numConfigs)
{
    RETURN_FALSE_IF_NOT_INIT();
    if (!configs || !numConfigs) {
        ETRACE("invalid parameters");
        return false;
    }

    *configs = 0;
    *numConfigs = 1;

    return true;
}

bool VirtualDevice::getDisplayAttributes(uint32_t configs,
                                            const uint32_t *attributes,
                                            int32_t *values)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (!attributes || !values) {
        ETRACE("invalid parameters");
        return false;
    }

    int i = 0;
    while (attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE) {
        switch (attributes[i]) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            values[i] = 1e9 / 60;
            break;
        case HWC_DISPLAY_WIDTH:
            values[i] = 1280;
            break;
        case HWC_DISPLAY_HEIGHT:
            values[i] = 720;
            break;
        case HWC_DISPLAY_DPI_X:
            values[i] = 0;
            break;
        case HWC_DISPLAY_DPI_Y:
            values[i] = 0;
            break;
        default:
            ETRACE("unknown attribute %d", attributes[i]);
            break;
        }
        i++;
    }

    return true;
}

bool VirtualDevice::compositionComplete()
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool VirtualDevice::initialize()
{
    // Add initialization codes here. If init fails, invoke DEINIT_AND_RETURN_FALSE();
    mNextConfig.typeChangeListener = NULL;
    mNextConfig.policy.scaledWidth = 0;
    mNextConfig.policy.scaledHeight = 0;
    mNextConfig.policy.xdpi = 96;
    mNextConfig.policy.ydpi = 96;
    mNextConfig.policy.refresh = 60;
    mNextConfig.extendedModeEnabled = false;
    mNextConfig.forceNotify = false;
    mCurrentConfig = mNextConfig;

    memset(&mLastFrameInfo, 0, sizeof(mLastFrameInfo));

    mPayloadManager = createVideoPayloadManager();
    if (!mPayloadManager) {
        ETRACE("Failed to create payload manager");
        return false;
    }

    // Publish frame server service with service manager
    status_t ret = defaultServiceManager()->addService(String16("hwc.widi"), this);
    if (ret == NO_ERROR) {
        ProcessState::self()->startThreadPool();
        mInitialized = true;
    } else {
        ETRACE("Could not register hwc.widi with service manager, error = %d", ret);
        delete mPayloadManager;
        mPayloadManager = NULL;
    }

    return mInitialized;
}

bool VirtualDevice::isConnected() const
{
    return true;
}

const char* VirtualDevice::getName() const
{
    return "Virtual";
}

int VirtualDevice::getType() const
{
    return DEVICE_VIRTUAL;
}

void VirtualDevice::dump(Dump& d)
{
}

void VirtualDevice::deinitialize()
{
    mInitialized = false;
    if (mPayloadManager) {
        delete mPayloadManager;
        mPayloadManager = NULL;
    }
}

} // namespace intel
} // namespace android
