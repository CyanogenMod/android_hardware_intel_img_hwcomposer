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
#include <SoftVsyncObserver.h>

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <cutils/properties.h>

#include <sync/sync.h>

#define NUM_CSC_BUFFERS 6

#define QCIF_WIDTH 176
#define QCIF_HEIGHT 144

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
    : mDoBlit(false),
      mDoOnFrameReady(false),
      mCancelOnFrameReady(false),
      mProtectedMode(false),
      mInitialized(false),
      mHwc(hwc),
      mDisplayPlaneManager(dpm),
      mPayloadManager(NULL),
      mVsyncObserver(NULL),
      mOrigContentWidth(0),
      mOrigContentHeight(0),
      mFirstVideoFrame(true),
      mCachedBufferCapcity(16),
      mDScalingEnabled(false)
{
    CTRACE();
    mNextConfig.frameServerActive = false;
    memset(mBlackY.data, 0x10, sizeof(mBlackY.data));
    memset(mBlackUV.data, 0x80, sizeof(mBlackUV.data));
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
        if (mMappedBufferCache.size() > mCachedBufferCapcity)
            mMappedBufferCache.clear();

        cachedBuffer = new CachedBuffer(mHwc.getBufferManager(), handle);
        mMappedBufferCache.add(handle, cachedBuffer);
    } else {
        cachedBuffer = mMappedBufferCache[index];
    }

    return cachedBuffer;
}

bool VirtualDevice::threadLoop()
{
    crop_t cropInfo;
    cropInfo.x = 0;
    cropInfo.y = 0;
    uint32_t srcHandle;
    uint32_t destHandle;
    {
        Mutex::Autolock _l(mCscLock);
        while (!mDoBlit) {
            mRequestQueued.wait(mCscLock);
        }
        cropInfo.w = mBlitCropWidth;
        cropInfo.h = mBlitCropHeight;
        srcHandle = mBlitSrcHandle;
        destHandle = mBlitDestHandle;
    }
    BufferManager* mgr = mHwc.getBufferManager();
    status_t result = OK;
    if (!(mgr->convertRGBToNV12(srcHandle, destHandle, cropInfo, 0))) {
        ETRACE("color space conversion from RGB to NV12 failed");
        result = UNKNOWN_ERROR;
    }

    int64_t renderTimestamp;
    {
        Mutex::Autolock _l(mCscLock);
        mDoBlit = false;
        mRequestProcessed.signal();
        while (!mDoOnFrameReady) {
            mRequestQueued.wait(mCscLock);
        }
        renderTimestamp = mFrameReadyRenderTs;
        if (mCancelOnFrameReady)
            result = UNKNOWN_ERROR;
        mCancelOnFrameReady = false;
    }
    if (result == OK)
        result = mCurrentConfig.frameListener->onFrameReady((int32_t)destHandle, HWC_HANDLE_TYPE_GRALLOC, renderTimestamp, -1);
    if (result != OK) {
        Mutex::Autolock _l(mHeldBuffersLock);
        mHeldBuffers.removeItem(destHandle);
    }
    {
        Mutex::Autolock _l(mCscLock);
        mDoOnFrameReady = false;
        mRequestProcessed.signal();
    }

    return true;
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
        Hwcomposer::getInstance().getDisplayAnalyzer()->isVideoExtModeEnabled();
    MultiDisplayObserver* mds = mHwc.getMultiDisplayObserver();
    if (mds != NULL)
        mds->notifyWidiConnectionStatus(true);
    mVideoFramerate = 0;
    mFirstVideoFrame = true;
    mNextConfig.frameServerActive = true;
    mNextConfig.forceNotifyFrameType = true;
    mNextConfig.forceNotifyBufferInfo = true;
    char prop[PROPERTY_VALUE_MAX];
    if (property_get("widi.video.dscaling.enable", prop, NULL) > 0 &&
            strncmp(prop, "true", PROPERTY_VALUE_MAX) == 0) {
        ITRACE("widi.video.dscaling is enabled, %s", prop);
        mDScalingEnabled = true;
    } else {
        ITRACE("widi.video.dscaling is disabled");
    }

    if (mThread == NULL) {
        mThread = new WidiBlitThread(this);
        mThread->run("WidiBlit", PRIORITY_URGENT_DISPLAY);
    }
    return NO_ERROR;
}

status_t VirtualDevice::stop(bool isConnected)
{
    ITRACE();
    Mutex::Autolock _l(mConfigLock);
    mNextConfig.typeChangeListener = NULL;
    mNextConfig.frameListener = NULL;
    mNextConfig.policy.scaledWidth = 0;
    mNextConfig.policy.scaledHeight = 0;
    mNextConfig.policy.xdpi = 96;
    mNextConfig.policy.ydpi = 96;
    mNextConfig.policy.refresh = 60;
    mNextConfig.frameServerActive = false;
    mNextConfig.extendedModeEnabled = false;
    mNextConfig.forceNotifyFrameType = false;
    mNextConfig.forceNotifyBufferInfo = false;
    {
        Mutex::Autolock _l(mCscLock);
        mCscWidth = 0;
        mCscHeight = 0;
    }
    Hwcomposer::getInstance().getMultiDisplayObserver()->notifyWidiConnectionStatus(false);
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
    // setDownScalingSize(policy.scaledWidth, policy.scaledHeight);
    return NO_ERROR;
}

void VirtualDevice::clearCscBuffers()
{
    if (!mAvailableCscBuffers.empty()) {
        // iterate the list and call freeGraphicBuffer
        for (List<uint32_t>::iterator i = mAvailableCscBuffers.begin(); i != mAvailableCscBuffers.end(); ++i) {
            VTRACE("Deleting the gralloc buffer associated with handle (%d)", (*i));
            mHwc.getBufferManager()->freeGrallocBuffer(*i);
        }
        mAvailableCscBuffers.clear();
    }
}

bool VirtualDevice::prePrepare(hwc_display_contents_1_t *display)
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool VirtualDevice::prepare(hwc_display_contents_1_t *display)
{
    RETURN_FALSE_IF_NOT_INIT();

    mRenderTimestamp = systemTime();
    {
        Mutex::Autolock _l(mConfigLock);
        mCurrentConfig = mNextConfig;
    }

    if (!display) {
        // No image. We're done with any mappings and CSC buffers.
        mMappedBufferCache.clear();
        Mutex::Autolock _l(mCscLock);
        clearCscBuffers();
        return true;
    }

    if (!mCurrentConfig.frameServerActive) {
        {
            // We're done with CSC buffers, since we blit to outbuf in this mode.
            // We want to keep mappings cached, so we don't clear mMappedBufferCache.
            Mutex::Autolock _l(mCscLock);
            clearCscBuffers();
        }
        return vanillaPrepare(display);
    }

    // by default send the FRAMEBUFFER_TARGET layer (composited image)
    mLayerToSend = display->numHwLayers-1;

    DisplayAnalyzer *analyzer = mHwc.getDisplayAnalyzer();

    if (mCurrentConfig.extendedModeEnabled &&
            !analyzer->isOverlayAllowed() && (analyzer->getVideoInstances() <= 1)) {
        if (mCurrentConfig.typeChangeListener->shutdownVideo() != OK) {
            ITRACE("Waiting for prior encoder session to shut down...");
        }
        /* Setting following flag to true will enable us to call bufferInfoChanged() in clone mode. */
        mNextConfig.forceNotifyBufferInfo = true;
        return true;
    }

    if (mCurrentConfig.extendedModeEnabled) {
        if ((display->numHwLayers-1) == 1) {
            hwc_layer_1_t& layer = display->hwLayers[0];
            if (analyzer->isPresentationLayer(layer) && layer.transform == 0 && layer.blending == HWC_BLENDING_NONE) {
                mLayerToSend = 0;
                VTRACE("Layer (%d) is Presentation layer", mLayerToSend);
            }
        }
    }
    // FIXME: It isn't resonable to set downscaling at here
    for (size_t i = 0; i < display->numHwLayers-1; i++) {
        hwc_layer_1_t& layer = display->hwLayers[i];
        if (analyzer->isVideoLayer(layer)) {
            int dswidth  = 1280;//mCurrentConfig.policy.scaledWidth;
            int dsheight = 720; //mCurrentConfig.policy.scaledHeight;
            int offX = 0;
            int offY = 0;
            int bufWidth  = 1920;
            int bufHeight = 1080;
            setDownScaling(dswidth, dsheight, offX, offY, bufWidth, bufHeight);
            break;
        }
    }

    bool protectedLayer = false;
    bool presentationLayer = false;
    if (mCurrentConfig.extendedModeEnabled) {
        for (size_t i = 0; i < display->numHwLayers-1; i++) {
            hwc_layer_1_t& layer = display->hwLayers[i];
            if (analyzer->isVideoLayer(layer)) {
                /* If the resolution of the video layer is less than QCIF, then we are going to play it in clone mode only.*/
                uint32_t vidContentWidth = layer.sourceCropf.right - layer.sourceCropf.left;
                uint32_t vidContentHeight = layer.sourceCropf.bottom - layer.sourceCropf.top;
                if ( (vidContentWidth * vidContentHeight) < (QCIF_WIDTH * QCIF_HEIGHT) ){
                    VTRACE("Identified video layer of resolution < QCIF :playing in clone mode. mLayerToSend = %d", mLayerToSend);
                    break;
                }
                protectedLayer = analyzer->isProtectedLayer(layer);
                presentationLayer = analyzer->isPresentationLayer(layer);
                if (!presentationLayer || protectedLayer) {
                    VTRACE("Layer (%d) is extended video layer", mLayerToSend);
                    mLayerToSend = i;
                    break;
               }
            }
        }
    }

    hwc_layer_1_t& streamingLayer = display->hwLayers[mLayerToSend];

    if (streamingLayer.compositionType == HWC_FRAMEBUFFER_TARGET) {
        VTRACE("Clone mode");
        // TODO: fix this workaround. Once metadeta has correct info.
        mFirstVideoFrame = true;

        if (mCurrentConfig.frameListener != NULL)
            mCurrentConfig.frameListener->onFramePrepare(mRenderTimestamp, -1);
        return true;
    }
    if ((analyzer->getVideoInstances() <= 0) && presentationLayer) {
        VTRACE("No video Instance found, in transition");
        return true;
    }


    VTRACE("Extended mode");

    if (sendToWidi(streamingLayer, protectedLayer)) {
        // if we're streaming one layer (extended mode or background mode), no need to composite
        for (size_t i = 0; i < display->numHwLayers-1; i++) {
            hwc_layer_1_t& layer = display->hwLayers[i];
            layer.compositionType = HWC_OVERLAY;
        }
    } else {
        // if error in playback file , switch to clone mode
        mLayerToSend = display->numHwLayers-1;
        VTRACE("Switch to clone mode");
    }

    return true;
}

bool VirtualDevice::commit(hwc_display_contents_1_t *display, IDisplayContext *context)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (!display)
        return true;

    if (!mCurrentConfig.frameServerActive)
        return vanillaCommit(display);

    DisplayAnalyzer *analyzer = mHwc.getDisplayAnalyzer();

    if (mCurrentConfig.extendedModeEnabled &&
            !analyzer->isOverlayAllowed() && (analyzer->getVideoInstances() <= 1)) {
        if (mCurrentConfig.typeChangeListener->shutdownVideo() != OK) {
            ITRACE("Waiting for prior encoder session to shut down...");
        }
        mNextConfig.forceNotifyBufferInfo = true;
        return true;
    }

    // This is for clone mode
    hwc_layer_1_t& streamingLayer = display->hwLayers[mLayerToSend];
    if (streamingLayer.compositionType == HWC_FRAMEBUFFER_TARGET) {
        sendToWidi(streamingLayer, 0);
    }

    return true;
}

bool VirtualDevice::sendToWidi(const hwc_layer_1_t& layer, bool isProtected)
{
    uint32_t handle = (uint32_t)layer.handle;
    if (handle == 0) {
        ETRACE("layer has no handle set");
        return false;
    }
    HWCBufferHandleType handleType = HWC_HANDLE_TYPE_GRALLOC;
    int64_t mediaTimestamp = -1;

    sp<RefBase> heldBuffer;

    FrameInfo inputFrameInfo;
    memset(&inputFrameInfo, 0, sizeof(inputFrameInfo));
    inputFrameInfo.frameType = HWC_FRAMETYPE_FRAME_BUFFER;
    inputFrameInfo.contentWidth = layer.sourceCropf.right - layer.sourceCropf.left;
    inputFrameInfo.contentHeight = layer.sourceCropf.bottom - layer.sourceCropf.top;
    inputFrameInfo.contentFrameRateN = 0;
    inputFrameInfo.contentFrameRateD = 0;
    inputFrameInfo.isProtected = isProtected;

    // This is to guard encoder if anytime frame is less then one macroblock size.
    if ((inputFrameInfo.contentWidth < 16) || (inputFrameInfo.contentHeight < 16))
        return false;

    FrameInfo outputFrameInfo;
    outputFrameInfo = inputFrameInfo;
    bool queueOnFrameReady = false;

    if (mHwc.getDisplayAnalyzer()->isVideoLayer((hwc_layer_1_t&)layer)) {
        sp<CachedBuffer> cachedBuffer;
        if ((cachedBuffer = getMappedBuffer(handle)) == NULL) {
            ETRACE("Failed to map display buffer");
            return false;
        }

        // TODO: Fix this workaround once we get the content width & height in metadata.
        if (mFirstVideoFrame) {
            mOrigContentWidth = inputFrameInfo.contentWidth;
            mOrigContentHeight = inputFrameInfo.contentHeight;
        }


        // for video mode let 30 fps be the default value.
        inputFrameInfo.contentFrameRateN = 30;
        inputFrameInfo.contentFrameRateD = 1;

        IVideoPayloadManager::MetaData metadata;
        if (mPayloadManager->getMetaData(cachedBuffer->mapper, &metadata)) {
            heldBuffer = new HeldDecoderBuffer(this, cachedBuffer);
            mediaTimestamp = metadata.timestamp;
            inputFrameInfo.contentWidth = metadata.crop_width;
            inputFrameInfo.contentHeight = metadata.crop_height;
            // Use the crop size if something changed derive it again..
            // Only get video source info if frame rate has not been initialized.
            // getVideoSourceInfo() is a fairly expensive operation. This optimization
            // will save us a few milliseconds per frame
            if (mFirstVideoFrame || (mOrigContentWidth != inputFrameInfo.contentWidth) ||
                (mOrigContentHeight != inputFrameInfo.contentHeight)) {
                mVideoFramerate = inputFrameInfo.contentFrameRateN;
                ITRACE("VideoWidth = %d, VideoHeight = %d", metadata.crop_width, metadata.crop_height);
                mOrigContentWidth = inputFrameInfo.contentWidth;
                mOrigContentHeight = inputFrameInfo.contentHeight;

                // For the first video session by default
                int sessionID = Hwcomposer::getInstance().getDisplayAnalyzer()->getFirstVideoInstanceSessionID();
                if (sessionID >= 0) {
                    ITRACE("Session id = %d", sessionID);
                    VideoSourceInfo videoInfo;
                    memset(&videoInfo, 0, sizeof(videoInfo));
                    status_t ret = mHwc.getMultiDisplayObserver()->getVideoSourceInfo(sessionID, &videoInfo);
                    if (ret == NO_ERROR) {
                        ITRACE("width = %d, height = %d, fps = %d", videoInfo.width, videoInfo.height,
                                videoInfo.frameRate);
                        if (videoInfo.frameRate > 0) {
                            mVideoFramerate = videoInfo.frameRate;
                        }
                    }
                }
                mFirstVideoFrame = false;
            }
            inputFrameInfo.frameType = HWC_FRAMETYPE_VIDEO;
            inputFrameInfo.contentFrameRateN = mVideoFramerate;
            inputFrameInfo.contentFrameRateD = 1;


            // skip pading bytes in rotate buffer
            switch (metadata.transform) {
                case HAL_TRANSFORM_ROT_90: {
                    VTRACE("HAL_TRANSFORM_ROT_90");
                    int contentWidth = inputFrameInfo.contentWidth;
                    inputFrameInfo.contentWidth = (contentWidth + 0xf) & ~0xf;
                    inputFrameInfo.cropLeft = inputFrameInfo.contentWidth - contentWidth;
                } break;
                case HAL_TRANSFORM_ROT_180: {
                    VTRACE("HAL_TRANSFORM_ROT_180");
                    int contentWidth = inputFrameInfo.contentWidth;
                    int contentHeight = inputFrameInfo.contentHeight;
                    inputFrameInfo.contentWidth = (contentWidth + 0xf) & ~0xf;
                    inputFrameInfo.contentHeight = (contentHeight + 0xf) & ~0xf;
                    inputFrameInfo.cropLeft = inputFrameInfo.contentWidth - contentWidth;
                    inputFrameInfo.cropTop = inputFrameInfo.contentHeight - contentHeight;
                } break;
                case HAL_TRANSFORM_ROT_270: {
                    VTRACE("HAL_TRANSFORM_ROT_270");
                    int contentHeight = inputFrameInfo.contentHeight;
                    inputFrameInfo.contentHeight = (contentHeight + 0xf) & ~0xf;
                    inputFrameInfo.cropTop = inputFrameInfo.contentHeight - contentHeight;
                } break;
                default:
                  break;
            }
            outputFrameInfo = inputFrameInfo;
            outputFrameInfo.bufferFormat = metadata.format;

            handleType = HWC_HANDLE_TYPE_KBUF;
            bool isRotated = false;
            if (metadata.transform == HAL_TRANSFORM_ROT_90 ||
                    metadata.transform == HAL_TRANSFORM_ROT_180 ||
                    metadata.transform == HAL_TRANSFORM_ROT_270) {
                isRotated = true;
            }
            if (!isRotated && metadata.scaling_khandle) {
                handle = metadata.scaling_khandle;
                outputFrameInfo.bufferWidth = metadata.scaling_width;
                outputFrameInfo.bufferHeight = ((metadata.scaling_height + 0x1f) & (~0x1f));
                outputFrameInfo.lumaUStride = metadata.scaling_luma_stride;
                outputFrameInfo.chromaUStride = metadata.scaling_chroma_u_stride;
                outputFrameInfo.chromaVStride = metadata.scaling_chroma_v_stride;
                outputFrameInfo.contentWidth = metadata.scaling_width;
                outputFrameInfo.contentHeight =((metadata.scaling_height + 0x1f) & (~0x1f));
            } else if (metadata.kHandle != 0) {
                handle = metadata.kHandle;
                outputFrameInfo.bufferWidth = metadata.width;
                outputFrameInfo.bufferHeight = ((metadata.height + 0x1f) & (~0x1f));
                outputFrameInfo.lumaUStride = metadata.lumaStride;
                outputFrameInfo.chromaUStride = metadata.chromaUStride;
                outputFrameInfo.chromaVStride = metadata.chromaVStride;
                if (metadata.scaling_khandle && isRotated) {
                    outputFrameInfo.contentWidth = metadata.scaling_height;
                    outputFrameInfo.contentHeight = ((metadata.scaling_width + 0x1f) & (~0x1f));
                }
            } else {
                ETRACE("Couldn't get any khandle");
                return false;
            }
            if (outputFrameInfo.bufferFormat == 0 ||
                outputFrameInfo.bufferWidth < outputFrameInfo.contentWidth ||
                outputFrameInfo.bufferHeight < outputFrameInfo.contentHeight ||
                outputFrameInfo.contentWidth <= 0 || outputFrameInfo.contentHeight <= 0 ||
                outputFrameInfo.lumaUStride <= 0 ||
                outputFrameInfo.chromaUStride <= 0 || outputFrameInfo.chromaVStride <= 0) {
                ITRACE("Payload cleared or inconsistent info, not sending frame");
                ITRACE("outputFrameInfo.bufferFormat  = %d ", outputFrameInfo.bufferFormat);
                ITRACE("outputFrameInfo.bufferWidth   = %d ", outputFrameInfo.bufferWidth);
                ITRACE("outputFrameInfo.contentWidth  = %d ", outputFrameInfo.contentWidth);
                ITRACE("outputFrameInfo.bufferHeight  = %d ", outputFrameInfo.bufferHeight);
                ITRACE("outputFrameInfo.contentHeight = %d ", outputFrameInfo.contentHeight);
                ITRACE("outputFrameInfo.lumaUStride   = %d ", outputFrameInfo.lumaUStride);
                ITRACE("outputFrameInfo.chromaUStride = %d ", outputFrameInfo.chromaUStride);
                ITRACE("outputFrameInfo.chromaVStride = %d ", outputFrameInfo.chromaVStride);
                return false;
            }
        } else {
            ETRACE("Failed to get metadata");
            return false;
        }
    } else {
        BufferManager* mgr = mHwc.getBufferManager();
        uint32_t grallocHandle = 0;
        {
            Mutex::Autolock _l(mCscLock);
            while (mDoBlit) {
                // wait for previous frame's blit if it hasn't completed
                mRequestProcessed.wait(mCscLock);
            }
            // Blit only support 1:1 so until we get upscale/downsacle ,source & destination will be of same size.
            if ((layer.compositionType == HWC_FRAMEBUFFER_TARGET) &&
                ((mCurrentConfig.policy.scaledWidth != inputFrameInfo.contentWidth) ||
                (mCurrentConfig.policy.scaledHeight != inputFrameInfo.contentHeight))) {
                mCurrentConfig.policy.scaledWidth = inputFrameInfo.contentWidth;
                mCurrentConfig.policy.scaledHeight = inputFrameInfo.contentHeight;
            }

            if (mCscWidth != mCurrentConfig.policy.scaledWidth || mCscHeight != mCurrentConfig.policy.scaledHeight) {
                ITRACE("CSC buffers changing from %dx%d to %dx%d",
                      mCscWidth, mCscHeight, mCurrentConfig.policy.scaledWidth, mCurrentConfig.policy.scaledHeight);
                clearCscBuffers();
                mCscWidth = mCurrentConfig.policy.scaledWidth;
                mCscHeight = mCurrentConfig.policy.scaledHeight;
                mCscBuffersToCreate = NUM_CSC_BUFFERS;
            }

            if (mAvailableCscBuffers.empty()) {
                if (mCscBuffersToCreate <= 0) {
                    WTRACE("Out of CSC buffers, dropping frame");
                    return false;
                }
                uint32_t bufHandle;
                bufHandle = mgr->allocGrallocBuffer(mCurrentConfig.policy.scaledWidth,
                                                    mCurrentConfig.policy.scaledHeight,
                                                    DisplayQuery::queryNV12Format(),
                                                    GRALLOC_USAGE_HW_VIDEO_ENCODER |
                                                    GRALLOC_USAGE_HW_RENDER);
                if (bufHandle == 0){
                    ETRACE("failed to get gralloc buffer handle");
                    return false;
                }
                mCscBuffersToCreate--;
                mAvailableCscBuffers.push_back(bufHandle);
            }
            grallocHandle = *mAvailableCscBuffers.begin();
            mAvailableCscBuffers.erase(mAvailableCscBuffers.begin());

            heldBuffer = new HeldCscBuffer(this, grallocHandle);
            mBlitDestHandle = grallocHandle;
            mBlitSrcHandle = handle;
            mBlitCropWidth = mCurrentConfig.policy.scaledWidth;
            mBlitCropHeight = mCurrentConfig.policy.scaledHeight;
            mDoBlit = true;
            mRequestQueued.signal();
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
        queueOnFrameReady = true;
    }

    {
        Mutex::Autolock _l(mCscLock);
        while (mDoOnFrameReady) {
            mRequestProcessed.wait(mCscLock);
        }
    }

    if (mCurrentConfig.forceNotifyFrameType ||
        memcmp(&inputFrameInfo, &mLastInputFrameInfo, sizeof(inputFrameInfo)) != 0) {
        // something changed, notify type change listener
        mNextConfig.forceNotifyFrameType = false;
        mCurrentConfig.typeChangeListener->frameTypeChanged(inputFrameInfo);
        ITRACE("Notify frameTypeChanged: %dx%d in %dx%d @ %d fps",
            inputFrameInfo.contentWidth, inputFrameInfo.contentHeight,
            inputFrameInfo.bufferWidth, inputFrameInfo.bufferHeight,
            inputFrameInfo.contentFrameRateN);
        mLastInputFrameInfo = inputFrameInfo;
    }

    if (mCurrentConfig.policy.scaledWidth == 0 || mCurrentConfig.policy.scaledHeight == 0) {
        Mutex::Autolock _l(mCscLock);
        mDoOnFrameReady = true;
        mCancelOnFrameReady = true;
        mRequestQueued.signal();
        return false;
    }

    if (mCurrentConfig.forceNotifyBufferInfo ||
        memcmp(&outputFrameInfo, &mLastOutputFrameInfo, sizeof(outputFrameInfo)) != 0) {

        mNextConfig.forceNotifyBufferInfo = false;
        mCurrentConfig.typeChangeListener->bufferInfoChanged(outputFrameInfo);
        ITRACE("Notify bufferInfoChanged: %dx%d in %dx%d @ %d fps",
            outputFrameInfo.contentWidth, outputFrameInfo.contentHeight,
            outputFrameInfo.bufferWidth, outputFrameInfo.bufferHeight,
            outputFrameInfo.contentFrameRateN);
        mLastOutputFrameInfo = outputFrameInfo;

        if (handleType == HWC_HANDLE_TYPE_GRALLOC)
            mMappedBufferCache.clear();
    }
    if (handleType == HWC_HANDLE_TYPE_KBUF &&
        handle == mExtLastKhandle && mediaTimestamp == mExtLastTimestamp) {
        return true; // Do not switch to clone mode here.
    }

    {
        Mutex::Autolock _l(mHeldBuffersLock);
        //Add the heldbuffer to the vector before calling onFrameReady, so that the buffer will be removed
        //from the vector properly even if the notifyBufferReturned call acquires mHeldBuffersLock first.
        mHeldBuffers.add(handle, heldBuffer);
    }
    if (queueOnFrameReady) {
        Mutex::Autolock _l(mCscLock);
        mFrameReadyRenderTs = mRenderTimestamp;
        mDoOnFrameReady = true;
        mRequestQueued.signal();
    }
    else {
        status_t result = mCurrentConfig.frameListener->onFrameReady((int32_t)handle, handleType, mRenderTimestamp, mediaTimestamp);
        if (result != OK) {
            Mutex::Autolock _l(mHeldBuffersLock);
            mHeldBuffers.removeItem(handle);
        }
    }
    if (handleType == HWC_HANDLE_TYPE_KBUF) {
        mExtLastKhandle = handle;
        mExtLastTimestamp = mediaTimestamp;
    }
    return true;
}

void VirtualDevice::fill(uint8_t* ptr, const ied_block& data, size_t len) {
    for (size_t offset = 0; offset < len; offset += 16) {
        *reinterpret_cast<ied_block*>(ptr + offset) = data;
    }
}

void VirtualDevice::copyVideo(buffer_handle_t videoHandle, uint8_t* destPtr, uint32_t destWidth, uint32_t destHeight)
{
    sp<CachedBuffer> videoCachedBuffer = getMappedBuffer((uint32_t) videoHandle);
    if (videoCachedBuffer == NULL) {
        ETRACE("Couldn't map video handle %p", videoHandle);
        return;
    }
    if (videoCachedBuffer->mapper == NULL) {
        ETRACE("Src mapper gone");
        return;
    }
    IVideoPayloadManager::MetaData metadata;
    if (!mPayloadManager->getMetaData(videoCachedBuffer->mapper, &metadata)) {
        ETRACE("Failed to map video payload info");
        return;
    }
    uint8_t* srcPtr = (uint8_t*)videoCachedBuffer->mapper->getCpuAddress(0);

    if (srcPtr == NULL || destPtr == NULL) {
        ETRACE("Unable to map a buffer");
        return;
    }

    uint32_t srcWidth = metadata.lumaStride;
    uint32_t srcHeight = metadata.height;
    uint32_t blitWidth = metadata.crop_width;
    uint32_t blitHeight = metadata.crop_height;

    if (blitWidth > destWidth)
        blitWidth = destWidth;
    if (blitHeight > destHeight)
        blitHeight = destHeight;

    uint32_t outY = (destHeight - blitHeight)/2;
    outY = (outY+1) & ~1;

    // clear top bar
    fill(destPtr, mBlackY, outY*destWidth);
    fill(destPtr + destWidth*destHeight, mBlackUV, outY*destWidth/2);

    // clear bottom bar
    fill(destPtr + (outY+blitHeight)*destWidth, mBlackY, (destHeight-outY-blitHeight)*destWidth);
    fill(destPtr + destWidth*destHeight + (outY+blitHeight)*destWidth/2, mBlackUV, (destHeight-outY-blitHeight)*destWidth/2);

    if (srcWidth == destWidth) {
        // copy whole Y plane
        memcpy(destPtr+outY*destWidth, srcPtr, srcWidth*blitHeight);
        // copy whole UV plane
        memcpy(destPtr+(destHeight+outY/2)*destWidth, srcPtr+srcWidth*srcHeight, srcWidth*blitHeight/2);
    } else {
        uint32_t outX = (destWidth - blitWidth)/2;
        outX = (outX+8) & ~15;
        blitWidth = blitWidth & ~15;

        // clear left and right bars, Y plane
        for (uint32_t y = 0; y < blitHeight; y++) {
            fill(destPtr + (outY + y)*destWidth, mBlackY, outX);
            fill(destPtr + (outY + y)*destWidth+outX+blitWidth, mBlackY, destWidth-outX-blitWidth);
        }

        // clear left and right bars, UV plane
        for (uint32_t y = 0; y < blitHeight/2; y++) {
            fill(destPtr + (destHeight + outY/2 + y)*destWidth, mBlackUV, outX);
            fill(destPtr + (destHeight + outY/2 + y)*destWidth+outX+blitWidth, mBlackUV, destWidth-outX-blitWidth);
        }

        // copy Y plane one row at a time
        for (uint32_t row = 0; row < blitHeight; row++)
            memcpy(destPtr+(row+outY)*destWidth+outX, srcPtr + row*srcWidth, blitWidth);
        // copy UV plane one row at a time
        for (uint32_t row = 0; row < blitHeight/2; row++)
            memcpy(destPtr+(destHeight+row+outY/2)*destWidth+outX, srcPtr+(srcHeight+row)*srcWidth, blitWidth);
    }
}

bool VirtualDevice::vanillaPrepare(hwc_display_contents_1_t *display)
{
    DisplayAnalyzer *analyzer = mHwc.getDisplayAnalyzer();

    // by default send the FRAMEBUFFER_TARGET layer (composited image)
    mLayerToSend = display->numHwLayers-1;
    mProtectedMode = false;

    for (size_t i = 0; i < display->numHwLayers-1; i++) {
        hwc_layer_1_t& layer = display->hwLayers[i];
        if (analyzer->isVideoLayer(layer) && analyzer->isProtectedLayer(layer)) {
            // can't support something below QCIF resolution
            uint32_t vidContentWidth = layer.sourceCropf.right - layer.sourceCropf.left;
            uint32_t vidContentHeight = layer.sourceCropf.bottom - layer.sourceCropf.top;
            if (vidContentWidth >= QCIF_WIDTH && vidContentHeight >= QCIF_HEIGHT) {
                layer.compositionType = HWC_OVERLAY;
                mLayerToSend = i;
                mProtectedMode = true;
                break;
            }
        }
    }

    int32_t compositionType = mProtectedMode ? HWC_OVERLAY : HWC_FRAMEBUFFER;
    for (size_t i = 0; i < display->numHwLayers-1; i++)
        display->hwLayers[i].compositionType = compositionType;

    if (mProtectedMode) {
        // protected content
        sp<CachedBuffer> destCachedBuffer = getMappedBuffer((uint32_t) display->outbuf);
        if (destCachedBuffer == NULL) {
            ETRACE("Couldn't map dest handle %x", (uint32_t) display->outbuf);
            return false;
        }
        if (destCachedBuffer->mapper == NULL) {
            ETRACE("Dest mapper gone");
            return false;
        }
        uint8_t* destPtr = (uint8_t*)destCachedBuffer->mapper->getCpuAddress(0);

        hwc_layer_1_t& targetFb = display->hwLayers[display->numHwLayers-1];
        uint32_t outWidth = targetFb.sourceCropf.right - targetFb.sourceCropf.left;
        uint32_t outHeight = targetFb.sourceCropf.bottom - targetFb.sourceCropf.top;
        copyVideo(display->hwLayers[mLayerToSend].handle, destPtr, outWidth, outHeight);
    } else {
        // We map buffers for protected content.
        // We don't need them anymore when back in clone mode.
        mMappedBufferCache.clear();
    }

    return true;
}

bool VirtualDevice::vanillaCommit(hwc_display_contents_1_t *display)
{
    if (mProtectedMode)
        return true;

    // clone mode
    hwc_layer_1_t& layer = display->hwLayers[mLayerToSend];
    crop_t cropInfo;
    cropInfo.x = 0;
    cropInfo.y = 0;
    cropInfo.w = layer.sourceCropf.right - layer.sourceCropf.left;
    cropInfo.h = layer.sourceCropf.bottom - layer.sourceCropf.top;
    BufferManager* mgr = mHwc.getBufferManager();
    uint32_t srcHandle = (uint32_t) layer.handle;
    uint32_t destHandle = (uint32_t) display->outbuf;
    if (srcHandle == 0 || destHandle == 0)
        return false;
    if (!(mgr->convertRGBToNV12(srcHandle, destHandle, cropInfo, 0))) {
        ETRACE("color space conversion from RGB to NV12 failed");
        return false;
    }

    return true;
}

bool VirtualDevice::vsyncControl(bool enabled)
{
    RETURN_FALSE_IF_NOT_INIT();
    return mVsyncObserver->control(enabled);
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
    mNextConfig.forceNotifyFrameType = false;
    mNextConfig.forceNotifyBufferInfo = false;
    mCurrentConfig = mNextConfig;
    mLayerToSend = 0;

    mCscBuffersToCreate = NUM_CSC_BUFFERS;
    mCscWidth = 0;
    mCscHeight = 0;

    memset(&mLastInputFrameInfo, 0, sizeof(mLastInputFrameInfo));
    memset(&mLastOutputFrameInfo, 0, sizeof(mLastOutputFrameInfo));

    mPayloadManager = createVideoPayloadManager();
    if (!mPayloadManager) {
        DEINIT_AND_RETURN_FALSE("Failed to create payload manager");
    }

    mVsyncObserver = new SoftVsyncObserver(*this);
    if (!mVsyncObserver || !mVsyncObserver->initialize()) {
        DEINIT_AND_RETURN_FALSE("Failed to create Soft Vsync Observer");
    }

    // Publish frame server service with service manager
    status_t ret = defaultServiceManager()->addService(String16("hwc.widi"), this);
    if (ret == NO_ERROR) {
        ProcessState::self()->startThreadPool();
        mInitialized = true;
    } else {
        ETRACE("Could not register hwc.widi with service manager, error = %d", ret);
        deinitialize();
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

void VirtualDevice::onVsync(int64_t timestamp)
{
    mHwc.vsync(DEVICE_VIRTUAL, timestamp);
}

void VirtualDevice::dump(Dump& d)
{
}

void VirtualDevice::deinitialize()
{
    if (mPayloadManager) {
        delete mPayloadManager;
        mPayloadManager = NULL;
    }
    DEINIT_AND_DELETE_OBJ(mVsyncObserver);
    mInitialized = false;
}

// Shold make sure below 4 conditions are met,
// 1: only for video playback case
// 2: only be called ONE time
// 3: is called before video driver begin to decode
// 4: offX & offY are 64 aligned
status_t VirtualDevice::setDownScaling(
        int dswidth, int dsheight,
        int offX, int offY,
        int bufWidth, int bufHeight) {
    status_t ret = UNKNOWN_ERROR;
    if (!mDScalingEnabled)
        return ret;
    int sessionID = mHwc.getDisplayAnalyzer()->getFirstVideoInstanceSessionID();
    if (sessionID >= 0 && mFirstVideoFrame) {
        MultiDisplayObserver* mds = mHwc.getMultiDisplayObserver();
        ret = mds->setDecoderOutputResolution(sessionID, dswidth, dsheight, offX, offY, bufWidth, bufHeight);
        if (ret == NO_ERROR) {
            ITRACE("set video down scaling: %dx%d, %dx%d, %dx%d",
                    dswidth, dsheight, offX, offY, bufWidth, bufHeight);
        } else {
            WTRACE("Failed to set video down scaling");
        }
    }
    return ret;
}

} // namespace intel
} // namespace android
