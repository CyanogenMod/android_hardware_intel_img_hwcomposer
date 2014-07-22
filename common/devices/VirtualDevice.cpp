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

#define NUM_CSC_BUFFERS 4

namespace android {
namespace intel {

VirtualDevice::CachedBuffer::CachedBuffer(BufferManager *mgr, uint32_t handle)
    : manager(mgr),
      mapper(NULL)
{
    DataBuffer *buffer = manager->lockDataBuffer(handle);
    mapper = manager->map(*buffer);
    manager->unlockDataBuffer(buffer);
}

VirtualDevice::CachedBuffer::~CachedBuffer()
{
    manager->unmap(mapper);
}

VirtualDevice::HeldCscBuffer::HeldCscBuffer(const sp<VirtualDevice>& vd, uint32_t grallocHandle)
    : vd(vd),
      handle(grallocHandle)
{
}

VirtualDevice::HeldCscBuffer::~HeldCscBuffer()
{
    Mutex::Autolock _l(vd->mCscLock);
    BufferManager* mgr = vd->mHwc.getBufferManager();
    DataBuffer* dataBuf = mgr->lockDataBuffer(handle);
    uint32_t bufWidth = dataBuf->getWidth();
    uint32_t bufHeight = dataBuf->getHeight();
    mgr->unlockDataBuffer(dataBuf);
    if (bufWidth == vd->mCscWidth && bufHeight == vd->mCscHeight) {
        VTRACE("Pushing back the handle %d to mAvailableCscBuffers", handle);
        vd->mAvailableCscBuffers.push_back(handle);
    } else {
        VTRACE("Deleting the gralloc buffer associated with handle (%d)", handle);
        mgr->freeGrallocBuffer(handle);
        vd->mCscBuffersToCreate++;
    }
}

VirtualDevice::HeldDecoderBuffer::HeldDecoderBuffer(const sp<VirtualDevice>& vd, const android::sp<CachedBuffer>& cachedBuffer)
    : vd(vd),
      cachedBuffer(cachedBuffer)
{
    if (!vd->mPayloadManager->setRenderStatus(cachedBuffer->mapper, true)) {
        ETRACE("Failed to set render status");
    }
}

VirtualDevice::HeldDecoderBuffer::~HeldDecoderBuffer()
{
    if (!vd->mPayloadManager->setRenderStatus(cachedBuffer->mapper, false)) {
        ETRACE("Failed to set render status");
    }
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

sp<VirtualDevice::CachedBuffer> VirtualDevice::getMappedBuffer(uint32_t handle)
{
    ssize_t index = mMappedBufferCache.indexOfKey(handle);
    sp<CachedBuffer> cachedBuffer;
    if (index == NAME_NOT_FOUND) {
        cachedBuffer = new CachedBuffer(mHwc.getBufferManager(), handle);
        mMappedBufferCache.add(handle, cachedBuffer);
    } else {
        cachedBuffer = mMappedBufferCache[index];
    }
    return cachedBuffer;
}

status_t VirtualDevice::start(sp<IFrameTypeChangeListener> typeChangeListener)
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
    {
        Mutex::Autolock _l(mCscLock);
        mCscWidth = 0;
        mCscHeight = 0;
    }
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
        VTRACE("Removing heldBuffer associated with handle (%d)", khandle);
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

    mRenderTimestamp = systemTime();
    {
        Mutex::Autolock _l(mConfigLock);
        mCurrentConfig = mNextConfig;
        mNextConfig.forceNotify = false;
    }

    if (mCurrentConfig.typeChangeListener == NULL) {
        //clear the buffer queues if any from the previous Widi session
        mMappedBufferCache.clear();
        {
            Mutex::Autolock _l(mCscLock);
            if (!mAvailableCscBuffers.empty()) {
                for (List<uint32_t>::iterator i = mAvailableCscBuffers.begin(); i != mAvailableCscBuffers.end(); ++i) {
                    VTRACE("Deleting the gralloc buffer associated with handle (%d)", (*i));
                    mHwc.getBufferManager()->freeGrallocBuffer(*i);
                }
                mAvailableCscBuffers.clear();
            }
        }
        return false;
    }

    // by default send the FRAMEBUFFER_TARGET layer (composited image)
    mLayerToSend = display->numHwLayers-1;

    DisplayAnalyzer *analyzer = mHwc.getDisplayAnalyzer();
    if ((display->numHwLayers-1) == 1) {
        hwc_layer_1_t& layer = display->hwLayers[0];
        if (analyzer->isPresentationLayer(layer) && layer.transform == 0 && layer.blending == HWC_BLENDING_NONE) {
            mLayerToSend = 0;
            VTRACE("Layer (%d) is Presentation layer", mLayerToSend);
        }
    }

    if (mCurrentConfig.extendedModeEnabled) {
        if (analyzer->checkVideoExtendedMode()) {
            for (size_t i = 0; i < display->numHwLayers-1; i++) {
                hwc_layer_1_t& layer = display->hwLayers[i];
                if (analyzer->isVideoLayer(layer)) {
                    VTRACE("Layer (%d) is extended video layer", mLayerToSend);
                    mLayerToSend = i;
                    break;
                }
            }
        }
    }

    hwc_layer_1_t& streamingLayer = display->hwLayers[mLayerToSend];

    // if we're streaming the target framebuffer, just notify widi stack and return
    if (streamingLayer.compositionType == HWC_FRAMEBUFFER_TARGET) {
        FrameInfo frameInfo;
        memset(&frameInfo, 0, sizeof(frameInfo));
        frameInfo.frameType = HWC_FRAMETYPE_NOTHING;
        VTRACE("Clone mode");
        if (mCurrentConfig.forceNotify || memcmp(&frameInfo, &mLastInputFrameInfo, sizeof(frameInfo)) != 0) {
            // something changed, notify type change listener
            mCurrentConfig.typeChangeListener->frameTypeChanged(frameInfo);
            mCurrentConfig.typeChangeListener->bufferInfoChanged(frameInfo);

            mExtLastTimestamp = 0;
            mExtLastKhandle = 0;

            mMappedBufferCache.clear();
            mLastInputFrameInfo = frameInfo;
            mLastOutputFrameInfo = frameInfo;
        }
        return true;
    }

    // if we're streaming one layer (extended mode or background mode), no need to composite
    for (size_t i = 0; i < display->numHwLayers-1; i++) {
        hwc_layer_1_t& layer = display->hwLayers[i];
        layer.compositionType = HWC_OVERLAY;
        layer.flags |= HWC_HINT_DISABLE_ANIMATION;
    }

    sendToWidi(streamingLayer);
    return true;
}

bool VirtualDevice::commit(hwc_display_contents_1_t *display, IDisplayContext *context)
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

void VirtualDevice::sendToWidi(const hwc_layer_1_t& layer)
{
    uint32_t handle = (uint32_t)layer.handle;
    if (handle == 0) {
        ETRACE("layer has no handle set");
        return;
    }
    HWCBufferHandleType handleType = HWC_HANDLE_TYPE_GRALLOC;
    int64_t mediaTimestamp = -1;

    sp<RefBase> heldBuffer;

    FrameInfo inputFrameInfo;
    memset(&inputFrameInfo, 0, sizeof(inputFrameInfo));
    inputFrameInfo.frameType = HWC_FRAMETYPE_FRAME_BUFFER;
    inputFrameInfo.contentWidth = layer.sourceCrop.right - layer.sourceCrop.left;
    inputFrameInfo.contentHeight = layer.sourceCrop.bottom - layer.sourceCrop.top;
    inputFrameInfo.contentFrameRateN = 60;
    inputFrameInfo.contentFrameRateD = 1;

    FrameInfo outputFrameInfo;
    outputFrameInfo = inputFrameInfo;

    if (mHwc.getDisplayAnalyzer()->isVideoLayer((hwc_layer_1_t&)layer)) {
        sp<CachedBuffer> cachedBuffer;
        if ((cachedBuffer = getMappedBuffer(handle)) == NULL) {
            ETRACE("Failed to map display buffer");
            return;
        }

        IVideoPayloadManager::MetaData metadata;
        if (mPayloadManager->getMetaData(cachedBuffer->mapper, &metadata)) {
            heldBuffer = new HeldDecoderBuffer(this, cachedBuffer);
            mediaTimestamp = metadata.timestamp;

            int sessionID = -1;
            if (layer.flags & HWC_HAS_VIDEO_SESSION_ID)
                sessionID = ((layer.flags & GRALLOC_USAGE_MDS_SESSION_ID_MASK) >> 24);
            if (sessionID >= 0) {
                VTRACE("Session id = %d", sessionID);
                VideoSourceInfo videoInfo;
                memset(&videoInfo, 0, sizeof(videoInfo));
                status_t ret = mHwc.getMultiDisplayObserver()->getVideoSourceInfo(sessionID, &videoInfo);
                if (ret == NO_ERROR) {
                    VTRACE("width = %d, height = %d, fps = %d", videoInfo.width, videoInfo.height,
                            videoInfo.frameRate);
                    if (videoInfo.frameRate > 0) {
                        inputFrameInfo.contentFrameRateN = videoInfo.frameRate;
                    }
                    inputFrameInfo.contentFrameRateD = 1;
                }
            }

            if (metadata.transform & HAL_TRANSFORM_ROT_90) {
                inputFrameInfo.contentWidth = layer.sourceCrop.bottom - layer.sourceCrop.top;
                inputFrameInfo.contentHeight = layer.sourceCrop.right - layer.sourceCrop.left;
            }

            outputFrameInfo = inputFrameInfo;
            outputFrameInfo.bufferFormat = metadata.format;

            handleType = HWC_HANDLE_TYPE_KBUF;
            if (metadata.kHandle != 0) {
                handle = metadata.kHandle;
                outputFrameInfo.bufferWidth = metadata.width;
                outputFrameInfo.bufferHeight = ((metadata.height + 0x1f) & (~0x1f));
                outputFrameInfo.lumaUStride = metadata.lumaStride;
                outputFrameInfo.chromaUStride = metadata.chromaUStride;
                outputFrameInfo.chromaVStride = metadata.chromaVStride;
            } else {
                ETRACE("Couldn't get any khandle");
                return;
            }

            if (outputFrameInfo.bufferFormat == 0 ||
                outputFrameInfo.bufferWidth < outputFrameInfo.contentWidth ||
                outputFrameInfo.bufferHeight < outputFrameInfo.contentHeight ||
                outputFrameInfo.contentWidth <= 0 || outputFrameInfo.contentHeight <= 0 ||
                outputFrameInfo.lumaUStride <= 0 ||
                outputFrameInfo.chromaUStride <= 0 || outputFrameInfo.chromaVStride <= 0) {
                ITRACE("Payload cleared or inconsistent info, not sending frame");
                return;
            }
        } else {
            ETRACE("Failed to get metadata");
            return;
        }
    } else {
        BufferManager* mgr = mHwc.getBufferManager();
        uint32_t grallocHandle = 0;
        {
            Mutex::Autolock _l(mCscLock);
            if (mCscWidth != mCurrentConfig.policy.scaledWidth || mCscHeight != mCurrentConfig.policy.scaledHeight) {
                ITRACE("CSC buffers changing from %dx%d to %dx%d",
                      mCscWidth, mCscHeight, mCurrentConfig.policy.scaledWidth, mCurrentConfig.policy.scaledHeight);
                // iterate the list and call freeGraphicBuffer
                for (List<uint32_t>::iterator i = mAvailableCscBuffers.begin(); i != mAvailableCscBuffers.end(); ++i) {
                    VTRACE("Deleting the gralloc buffer associated with handle (%d)", (*i));
                    mgr->freeGrallocBuffer(*i);
                }
                mAvailableCscBuffers.clear();
                mCscWidth = mCurrentConfig.policy.scaledWidth;
                mCscHeight = mCurrentConfig.policy.scaledHeight;
                mCscBuffersToCreate = NUM_CSC_BUFFERS;
            }

            if (mAvailableCscBuffers.empty()) {
                if (mCscBuffersToCreate <= 0) {
                    WTRACE("Out of CSC buffers, dropping frame");
                    return;
                }
                uint32_t bufHandle;
                bufHandle = mgr->allocGrallocBuffer(mCurrentConfig.policy.scaledWidth,
                                                    mCurrentConfig.policy.scaledHeight,
                                                    DisplayQuery::queryNV12Format(),
                                                    GRALLOC_USAGE_HW_VIDEO_ENCODER |
                                                    GRALLOC_USAGE_HW_RENDER);
                if (bufHandle == 0){
                    ETRACE("failed to get gralloc buffer handle");
                    return;
                }
                mCscBuffersToCreate--;
                mAvailableCscBuffers.push_back(bufHandle);
            }
            grallocHandle = *mAvailableCscBuffers.begin();
            mAvailableCscBuffers.erase(mAvailableCscBuffers.begin());
        }
        heldBuffer = new HeldCscBuffer(this, grallocHandle);
        crop_t cropInfo;
        cropInfo.w = mCurrentConfig.policy.scaledWidth;
        cropInfo.h = mCurrentConfig.policy.scaledHeight;
        cropInfo.x = 0;
        cropInfo.y = 0;
        if (!(mgr->convertRGBToNV12(handle, grallocHandle, cropInfo, 0))) {
            ETRACE("color space conversion from RGB to NV12 failed");
            return;
        }
        handle = grallocHandle;
        outputFrameInfo.contentWidth = mCurrentConfig.policy.scaledWidth;
        outputFrameInfo.contentHeight = mCurrentConfig.policy.scaledHeight;

        DataBuffer* dataBuf = mgr->lockDataBuffer(handle);
        outputFrameInfo.bufferWidth = dataBuf->getWidth();
        outputFrameInfo.bufferHeight = dataBuf->getHeight();
        outputFrameInfo.lumaUStride = dataBuf->getWidth();
        outputFrameInfo.chromaUStride = dataBuf->getWidth();
        outputFrameInfo.chromaVStride = dataBuf->getWidth();
        mgr->unlockDataBuffer(dataBuf);
    }

    if (mCurrentConfig.forceNotify ||
        memcmp(&inputFrameInfo, &mLastInputFrameInfo, sizeof(inputFrameInfo)) != 0) {
        // something changed, notify type change listener
        mCurrentConfig.typeChangeListener->frameTypeChanged(inputFrameInfo);
        mLastInputFrameInfo = inputFrameInfo;
    }

    if (mCurrentConfig.policy.scaledWidth == 0 || mCurrentConfig.policy.scaledHeight == 0)
        return;

    if (mCurrentConfig.forceNotify ||
        memcmp(&outputFrameInfo, &mLastOutputFrameInfo, sizeof(outputFrameInfo)) != 0) {
        mCurrentConfig.typeChangeListener->bufferInfoChanged(outputFrameInfo);
        mLastOutputFrameInfo = outputFrameInfo;

        if (handleType == HWC_HANDLE_TYPE_GRALLOC)
            mMappedBufferCache.clear();
    }

    if (handleType == HWC_HANDLE_TYPE_KBUF &&
        handle == mExtLastKhandle && mediaTimestamp == mExtLastTimestamp) {
        return;
    }

    {
        Mutex::Autolock _l(mHeldBuffersLock);
        //Add the heldbuffer to the vector before calling onFrameReady, so that the buffer will be removed
        //from the vector properly even if the notifyBufferReturned call acquires mHeldBuffersLock first.
        mHeldBuffers.add(handle, heldBuffer);
    }
    status_t result = mCurrentConfig.frameListener->onFrameReady((int32_t)handle, handleType, mRenderTimestamp, mediaTimestamp);
    if (result != OK) {
        Mutex::Autolock _l(mHeldBuffersLock);
        mHeldBuffers.removeItem(handle);
    }
    if (handleType == HWC_HANDLE_TYPE_KBUF) {
        mExtLastKhandle = handle;
        mExtLastTimestamp = mediaTimestamp;
    }
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
    mLayerToSend = 0;

    mCscBuffersToCreate = NUM_CSC_BUFFERS;
    mCscWidth = 0;
    mCscHeight = 0;

    memset(&mLastInputFrameInfo, 0, sizeof(mLastInputFrameInfo));
    memset(&mLastOutputFrameInfo, 0, sizeof(mLastOutputFrameInfo));

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
