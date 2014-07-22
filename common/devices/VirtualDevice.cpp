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
#include <SoftVsyncObserver.h>

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

#include <hal_public.h>
#include <sync/sw_sync.h>
#include <sync/sync.h>

#include <va/va_android.h>
#include <va/va_vpp.h>
#include <va/va_tpi.h>

#define NUM_CSC_BUFFERS 6

#define QCIF_WIDTH 176
#define QCIF_HEIGHT 144

namespace android {
namespace intel {

static const bool VSP_FOR_CLEAR_VIDEO = false; // debug feature

static void my_close_fence(const char* func, const char* fenceName, int& fenceFd)
{
    if (fenceFd != -1) {
        VTRACE("%s: closing fence %s (fd=%d)", func, fenceName, fenceFd);
        int err = close(fenceFd);
        if (err < 0) {
            ETRACE("%s: fence %s close error %d: %s", func, fenceName, err, strerror(errno));
        }
        fenceFd = -1;
    }
}

static void my_sync_wait_and_close(const char* func, const char* fenceName, int& fenceFd)
{
    if (fenceFd != -1) {
        VTRACE("%s: waiting on fence %s (fd=%d)", func, fenceName, fenceFd);
        int err = sync_wait(fenceFd, 300);
        if (err < 0) {
            ETRACE("%s: fence %s sync_wait error %d: %s", func, fenceName, err, strerror(errno));
        }
        my_close_fence(func, fenceName, fenceFd);
    }
}

static void my_timeline_inc(const char* func, const char* timelineName, int& syncTimelineFd)
{
    if (syncTimelineFd != -1) {
        VTRACE("%s: incrementing timeline %s (fd=%d)", func, timelineName, syncTimelineFd);
        int err = sw_sync_timeline_inc(syncTimelineFd, 1);
        if (err < 0)
            ETRACE("%s sync timeline %s increment error %d: %s", func, timelineName, errno, strerror(errno));
        syncTimelineFd = -1;
    }
}

#define CLOSE_FENCE(fenceName)          my_close_fence(__func__, #fenceName, fenceName)
#define SYNC_WAIT_AND_CLOSE(fenceName)  my_sync_wait_and_close(__func__, #fenceName, fenceName)
#define TIMELINE_INC(timelineName)      my_timeline_inc(__func__, #timelineName, timelineName)

class MappedSurface {
public:
    MappedSurface(VADisplay dpy, VASurfaceID surf)
        : va_dpy(dpy),
          ptr(NULL)
    {
        VAStatus va_status;
        va_status = vaDeriveImage(va_dpy, surf, &image);
        if (va_status != VA_STATUS_SUCCESS) {
            ETRACE("vaDeriveImage returns %08x", va_status);
            return;
        }
        va_status = vaMapBuffer(va_dpy, image.buf, (void**)&ptr);
        if (va_status != VA_STATUS_SUCCESS) {
            ETRACE("vaMapBuffer returns %08x", va_status);
            vaDestroyImage(va_dpy, image.image_id);
            return;
        }
    }
    ~MappedSurface() {
        if (ptr == NULL)
            return;

        VAStatus va_status;

        va_status = vaUnmapBuffer(va_dpy, image.buf);
        if (va_status != VA_STATUS_SUCCESS) ETRACE("vaUnmapBuffer returns %08x", va_status);

        va_status = vaDestroyImage(va_dpy, image.image_id);
        if (va_status != VA_STATUS_SUCCESS) ETRACE("vaDestroyImage returns %08x", va_status);
    }
    bool valid() { return ptr != NULL; }
    uint8_t* getPtr() { return ptr; }
private:
    VADisplay va_dpy;
    VAImage image;
    uint8_t* ptr;
};

class VirtualDevice::VAMappedHandle {
public:
    VAMappedHandle(VADisplay dpy, buffer_handle_t handle, uint32_t stride, uint32_t height, bool rgb)
        : va_dpy(dpy),
          surface(0)
    {
        int format;
        VASurfaceAttributeTPI attribTpi;
        memset(&attribTpi, 0, sizeof(attribTpi));
        VTRACE("Map gralloc %p size=%ux%u", handle, stride, height);
        attribTpi.type = VAExternalMemoryAndroidGrallocBuffer;
        attribTpi.width = stride;
        attribTpi.height = height;
        if (rgb) {
            attribTpi.size = stride*height*4;
            attribTpi.pixel_format = VA_FOURCC_RGBA;
            attribTpi.luma_stride = stride;
            attribTpi.chroma_u_stride = 0;
            attribTpi.chroma_v_stride = 0;
            attribTpi.luma_offset = 0;
            attribTpi.chroma_u_offset = 0;
            attribTpi.chroma_v_offset = 0;
            format = VA_RT_FORMAT_RGB32;
        }
        else {
            attribTpi.size = stride*height*3/2;
            attribTpi.pixel_format = VA_FOURCC_NV12;
            attribTpi.luma_stride = stride;
            attribTpi.chroma_u_stride = stride;
            attribTpi.chroma_v_stride = stride;
            attribTpi.luma_offset = 0;
            attribTpi.chroma_u_offset = stride*height;
            attribTpi.chroma_v_offset = stride*height+1;
            format = VA_RT_FORMAT_YUV420;
        }
        attribTpi.count = 1;
        attribTpi.buffers = (long unsigned int*) &handle;

        VAStatus va_status;
        va_status = vaCreateSurfacesWithAttribute(va_dpy,
                    stride,
                    height,
                    format,
                    1,
                    &surface,
                    &attribTpi);
        if (va_status != VA_STATUS_SUCCESS) {
            ETRACE("vaCreateSurfacesWithAttribute returns %08x, surface = %x", va_status, surface);
            surface = 0;
        }
    }
    VAMappedHandle(VADisplay dpy, uint32_t khandle, uint32_t stride, uint32_t height)
        : va_dpy(dpy),
          surface(0)
    {
        int format;
        VASurfaceAttributeTPI attribTpi;
        memset(&attribTpi, 0, sizeof(attribTpi));
        VTRACE("Map khandle 0x%x size=%ux%u", khandle, stride, height);
        attribTpi.type = VAExternalMemoryKernelDRMBufffer;
        attribTpi.width = stride;
        attribTpi.height = height;
        attribTpi.size = stride*height*3/2;
        attribTpi.pixel_format = VA_FOURCC_NV12;
        attribTpi.luma_stride = stride;
        attribTpi.chroma_u_stride = stride;
        attribTpi.chroma_v_stride = stride;
        attribTpi.luma_offset = 0;
        attribTpi.chroma_u_offset = stride*height;
        attribTpi.chroma_v_offset = stride*height+1;
        format = VA_RT_FORMAT_YUV420;
        attribTpi.count = 1;
        attribTpi.buffers = (long unsigned int*) &khandle;

        VAStatus va_status;
        va_status = vaCreateSurfacesWithAttribute(va_dpy,
                    stride,
                    height,
                    format,
                    1,
                    &surface,
                    &attribTpi);
        if (va_status != VA_STATUS_SUCCESS) {
            ETRACE("vaCreateSurfacesWithAttribute returns %08x", va_status);
            surface = 0;
        }
    }
    ~VAMappedHandle()
    {
        if (surface == 0)
            return;
        VAStatus va_status;
        va_status = vaDestroySurfaces(va_dpy, &surface, 1);
        if (va_status != VA_STATUS_SUCCESS) ETRACE("vaDestroySurfaces returns %08x", va_status);
    }
private:
    VADisplay va_dpy;
public:
    VASurfaceID surface;
};

VirtualDevice::CachedBuffer::CachedBuffer(BufferManager *mgr, uint32_t handle)
    : manager(mgr),
      mapper(NULL),
      vaMappedHandle(NULL)
{
    DataBuffer *buffer = manager->lockDataBuffer(handle);
    mapper = manager->map(*buffer);
    manager->unlockDataBuffer(buffer);
}

VirtualDevice::CachedBuffer::~CachedBuffer()
{
    if (vaMappedHandle != NULL)
        delete vaMappedHandle;
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

struct VirtualDevice::Task : public RefBase {
    virtual void run(VirtualDevice& vd) = 0;
    virtual ~Task() {}
};

struct VirtualDevice::RenderTask : public VirtualDevice::Task {
    RenderTask() : successful(false) { }
    virtual void run(VirtualDevice& vd) = 0;
    bool successful;
};

struct VirtualDevice::ComposeTask : public VirtualDevice::RenderTask {
    ComposeTask()
        : yuvAcquireFenceFd(-1),
          rgbAcquireFenceFd(-1),
          outbufAcquireFenceFd(-1),
          syncTimelineFd(-1) { }

    virtual ~ComposeTask() {
        // If queueCompose() creates this object and sets up fences,
        // but aborts before enqueuing the task, or if the task runs
        // but errors out, make sure our acquire fences get closed
        // and any release fences get signaled.
        CLOSE_FENCE(yuvAcquireFenceFd);
        CLOSE_FENCE(rgbAcquireFenceFd);
        CLOSE_FENCE(outbufAcquireFenceFd);
        TIMELINE_INC(syncTimelineFd);
    }

    virtual void run(VirtualDevice& vd) {
        VASurfaceID videoInSurface;

        if (vd.va_context == 0 || vd.va_video_in == 0)
            vd.vspEnable(outWidth, outHeight);

        uint32_t videoKhandle;
        uint32_t videoWidth;
        uint32_t videoHeight;
        uint32_t videoStride;
        uint32_t videoBufHeight;

        if (videoMetadata.scaling_khandle != 0) {
            videoKhandle = videoMetadata.scaling_khandle;
            videoWidth = videoMetadata.scaling_width;
            videoHeight = videoMetadata.scaling_height;
            videoStride = videoMetadata.scaling_luma_stride;
            videoBufHeight = (videoHeight+0x1f) & ~0x1f;
        }
        else {
            videoKhandle = videoMetadata.kHandle;
            videoWidth = videoMetadata.crop_width;
            videoHeight = videoMetadata.crop_height;
            videoStride = videoMetadata.lumaStride;
            videoBufHeight = videoMetadata.height;
        }

        SYNC_WAIT_AND_CLOSE(yuvAcquireFenceFd);

        if (displayFrame.left == 0 && displayFrame.top == 0 &&
            displayFrame.right == outWidth && displayFrame.bottom == outHeight &&
            videoStride == (uint32_t)outWidth && videoHeight == (uint32_t)outHeight) {
            // direct use is possible
            if (videoCachedBuffer->vaMappedHandle == NULL) {
                videoCachedBuffer->vaMappedHandle = new VAMappedHandle(vd.va_dpy, videoKhandle, videoStride, videoBufHeight);
            }
            videoInSurface = videoCachedBuffer->vaMappedHandle->surface;
        }
        else {
            // must copy video into the displayFrame rect of a blank buffer
            // with the same dimensions as the RGB buffer
            {
                VAMappedHandle originalInputVideoHandle(vd.va_dpy, videoKhandle, videoStride, videoBufHeight);
                MappedSurface origianlInputVideoSurface(vd.va_dpy, originalInputVideoHandle.surface);
                uint8_t* srcPtr = origianlInputVideoSurface.getPtr();
                if (srcPtr == NULL) {
                    ETRACE("Couldn't map input video");
                    return;
                }
                MappedSurface tmpInputVideoHandle(vd.va_dpy, vd.va_video_in);
                uint8_t* destPtr = tmpInputVideoHandle.getPtr();
                if (destPtr == NULL) {
                    ETRACE("Couldn't map temp video surface");
                    return;
                }
                vd.copyVideo(srcPtr, videoWidth, videoBufHeight, videoStride,
                             destPtr, outWidth, outHeight,
                             displayFrame.left, displayFrame.top,
                             displayFrame.right - displayFrame.left, displayFrame.bottom - displayFrame.top);
            }
            vaSyncSurface(vd.va_dpy, vd.va_video_in);
            videoInSurface = vd.va_video_in;
        }
        if (videoInSurface == 0) {
            ETRACE("Couldn't map video");
            return;
        }
        VTRACE("handle=%p khandle=%x va_surface=%x size=%ux%u crop=%ux%u",
              videoHandle, videoMetadata.kHandle, videoInSurface,
              videoMetadata.lumaStride, videoMetadata.height,
              videoMetadata.crop_width, videoMetadata.crop_height);
        VTRACE("scaling_khandle=%x size=%ux%u crop=%ux%u",
              videoMetadata.scaling_khandle,
              videoMetadata.scaling_luma_stride, videoMetadata.scaling_height,
              videoMetadata.scaling_width, videoMetadata.scaling_height);

        SYNC_WAIT_AND_CLOSE(rgbAcquireFenceFd);
        SYNC_WAIT_AND_CLOSE(outbufAcquireFenceFd);

        VAMappedHandle mappedRgbIn(vd.va_dpy, rgbHandle, outWidth, outHeight, true);
        if (mappedRgbIn.surface == 0) {
            ETRACE("Unable to map RGB surface");
            return;
        }
        VAMappedHandle mappedVideoOut(vd.va_dpy, outputHandle, outWidth, outHeight, false);
        if (mappedVideoOut.surface == 0) {
            ETRACE("Unable to map outbuf");
            return;
        }

        vd.vspCompose(videoInSurface, mappedRgbIn.surface, mappedVideoOut.surface);
        TIMELINE_INC(syncTimelineFd);
        successful = true;
    }
    buffer_handle_t videoHandle;
    hwc_rect_t displayFrame;
    buffer_handle_t rgbHandle;
    buffer_handle_t outputHandle;
    int32_t outWidth;
    int32_t outHeight;
    sp<CachedBuffer> videoCachedBuffer;
    sp<RefBase> heldVideoBuffer;
    IVideoPayloadManager::MetaData videoMetadata;
    int yuvAcquireFenceFd;
    int rgbAcquireFenceFd;
    int outbufAcquireFenceFd;
    int syncTimelineFd;
};

struct VirtualDevice::DisableVspTask : public VirtualDevice::Task {
    virtual void run(VirtualDevice& vd) {
        vd.vspDisable();
    }
};

struct VirtualDevice::BlitTask : public VirtualDevice::RenderTask {
    BlitTask()
        : srcAcquireFenceFd(-1),
          destAcquireFenceFd(-1) { }

    virtual ~BlitTask()
    {
        // If queueColorConvert() creates this object and sets up fences,
        // but aborts before enqueuing the task, or if the task runs
        // but errors out, make sure our acquire fences get closed
        // and any release fences get signaled.
        CLOSE_FENCE(srcAcquireFenceFd);
        CLOSE_FENCE(destAcquireFenceFd);
        TIMELINE_INC(syncTimelineFd);
    }

    virtual void run(VirtualDevice& vd) {
        SYNC_WAIT_AND_CLOSE(srcAcquireFenceFd);
        SYNC_WAIT_AND_CLOSE(destAcquireFenceFd);
        BufferManager* mgr = vd.mHwc.getBufferManager();
        if (!(mgr->convertRGBToNV12((uint32_t)srcHandle, (uint32_t)destHandle, cropInfo, 0))) {
            ETRACE("color space conversion from RGB to NV12 failed");
        }
        else
            successful = true;
        TIMELINE_INC(syncTimelineFd);
    }
    buffer_handle_t srcHandle;
    buffer_handle_t destHandle;
    int srcAcquireFenceFd;
    int destAcquireFenceFd;
    int syncTimelineFd;
    crop_t cropInfo;
};

struct VirtualDevice::FrameTypeChangedTask : public VirtualDevice::Task {
    virtual void run(VirtualDevice& vd) {
        typeChangeListener->frameTypeChanged(inputFrameInfo);
        ITRACE("Notify frameTypeChanged: %dx%d in %dx%d @ %d fps",
            inputFrameInfo.contentWidth, inputFrameInfo.contentHeight,
            inputFrameInfo.bufferWidth, inputFrameInfo.bufferHeight,
            inputFrameInfo.contentFrameRateN);
    }
    sp<IFrameTypeChangeListener> typeChangeListener;
    FrameInfo inputFrameInfo;
};

struct VirtualDevice::BufferInfoChangedTask : public VirtualDevice::Task {
    virtual void run(VirtualDevice& vd) {
        typeChangeListener->bufferInfoChanged(outputFrameInfo);
        ITRACE("Notify bufferInfoChanged: %dx%d in %dx%d @ %d fps",
            outputFrameInfo.contentWidth, outputFrameInfo.contentHeight,
            outputFrameInfo.bufferWidth, outputFrameInfo.bufferHeight,
            outputFrameInfo.contentFrameRateN);
    }
    sp<IFrameTypeChangeListener> typeChangeListener;
    FrameInfo outputFrameInfo;
};

struct VirtualDevice::OnFrameReadyTask : public VirtualDevice::Task {
    virtual void run(VirtualDevice& vd) {
        if (renderTask != NULL && !renderTask->successful)
            return;

        {
            Mutex::Autolock _l(vd.mHeldBuffersLock);
            //Add the heldbuffer to the vector before calling onFrameReady, so that the buffer will be removed
            //from the vector properly even if the notifyBufferReturned call acquires mHeldBuffersLock first.
            vd.mHeldBuffers.add(handle, heldBuffer);
        }

        status_t result = frameListener->onFrameReady(handle, handleType, renderTimestamp, mediaTimestamp);
        if (result != OK) {
            Mutex::Autolock _l(vd.mHeldBuffersLock);
            vd.mHeldBuffers.removeItem(handle);
        }
    }
    sp<RenderTask> renderTask;
    sp<RefBase> heldBuffer;
    sp<IFrameListener> frameListener;
    uint32_t handle;
    HWCBufferHandleType handleType;
    int64_t renderTimestamp;
    int64_t mediaTimestamp;
};

VirtualDevice::VirtualDevice(Hwcomposer& hwc, DisplayPlaneManager& dpm)
    : mProtectedMode(false),
      mInitialized(false),
      mHwc(hwc),
      mDisplayPlaneManager(dpm),
      mPayloadManager(NULL),
      mVsyncObserver(NULL),
      mOrigContentWidth(0),
      mOrigContentHeight(0),
      mFirstVideoFrame(true),
      mLastConnectionStatus(false),
      mCachedBufferCapcity(16)
{
    CTRACE();
    mNextConfig.frameServerActive = false;
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
    sp<Task> task;
    {
        Mutex::Autolock _l(mCscLock);
        while (mTasks.empty()) {
            mRequestQueued.wait(mCscLock);
        }
        task = *mTasks.begin();
        mTasks.erase(mTasks.begin());
        mRequestDequeued.signal();
    }
    task->run(*this);

    return true;
}

status_t VirtualDevice::start(sp<IFrameTypeChangeListener> typeChangeListener)
{
    ITRACE();
    Mutex::Autolock _l(mConfigLock);
    mNextConfig.typeChangeListener = typeChangeListener;
    mNextConfig.frameListener = NULL;
    mNextConfig.policy.scaledWidth = 0;
    mNextConfig.policy.scaledHeight = 0;
    mNextConfig.policy.xdpi = 96;
    mNextConfig.policy.ydpi = 96;
    mNextConfig.policy.refresh = 60;
    mNextConfig.extendedModeEnabled =
        Hwcomposer::getInstance().getDisplayAnalyzer()->isVideoExtModeEnabled();
    mVideoFramerate = 0;
    mFirstVideoFrame = true;
    mNextConfig.frameServerActive = true;
    mNextConfig.forceNotifyFrameType = true;
    mNextConfig.forceNotifyBufferInfo = true;

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

uint32_t VirtualDevice::getCscBuffer(uint32_t width, uint32_t height)
{
    if (mCscWidth != width || mCscHeight != height) {
        ITRACE("CSC buffers changing from %dx%d to %dx%d",
                mCscWidth, mCscHeight, width, height);
        clearCscBuffers();
        mCscWidth = width;
        mCscHeight = height;
        mCscBuffersToCreate = NUM_CSC_BUFFERS;
    }

    uint32_t bufHandle;
    if (mAvailableCscBuffers.empty()) {
        if (mCscBuffersToCreate <= 0) {
            WTRACE("Out of CSC buffers, dropping frame");
            return 0;
        }
        BufferManager* mgr = mHwc.getBufferManager();
        bufHandle = mgr->allocGrallocBuffer(width,
                                            height,
                                            DisplayQuery::queryNV12Format(),
                                            GRALLOC_USAGE_HW_VIDEO_ENCODER |
                                            GRALLOC_USAGE_HW_RENDER);
        if (bufHandle == 0){
            ETRACE("failed to get gralloc buffer handle");
            return 0;
        }
        mCscBuffersToCreate--;
        return bufHandle;
    }
    else {
        bufHandle = *mAvailableCscBuffers.begin();
        mAvailableCscBuffers.erase(mAvailableCscBuffers.begin());
    }
    return bufHandle;
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
    mVspInUse = false;
    mExpectAcquireFences = false;

    {
        Mutex::Autolock _l(mConfigLock);
        mCurrentConfig = mNextConfig;
    }

    bool shouldBeConnected = (display != NULL && (!mCurrentConfig.frameServerActive ||
                                                  mCurrentConfig.extendedModeEnabled));
    if (shouldBeConnected != mLastConnectionStatus) {
        // calling this will reload the property 'hwc.video.extmode.enable'
        Hwcomposer::getInstance().getDisplayAnalyzer()->isVideoExtModeEnabled();

        Hwcomposer::getInstance().getMultiDisplayObserver()->notifyWidiConnectionStatus(shouldBeConnected);
        mLastConnectionStatus = shouldBeConnected;
    }

    if (!display) {
        // No image. We're done with any mappings and CSC buffers.
        mMappedBufferCache.clear();
        Mutex::Autolock _l(mCscLock);
        clearCscBuffers();
        return true;
    }

    if (!mCurrentConfig.frameServerActive) {
        // We're done with CSC buffers, since we blit to outbuf in this mode.
        // We want to keep mappings cached, so we don't clear mMappedBufferCache.
        Mutex::Autolock _l(mCscLock);
        clearCscBuffers();
    }

    // by default send the FRAMEBUFFER_TARGET layer (composited image)
    mRgbLayer = display->numHwLayers-1;
    mYuvLayer = -1;

    DisplayAnalyzer *analyzer = mHwc.getDisplayAnalyzer();

    mProtectedMode = false;

    if (mCurrentConfig.typeChangeListener != NULL &&
        !analyzer->isOverlayAllowed() &&
        analyzer->getVideoInstances() <= 1) {
        if (mCurrentConfig.typeChangeListener->shutdownVideo() != OK) {
            ITRACE("Waiting for prior encoder session to shut down...");
        }
        /* Setting following flag to true will enable us to call bufferInfoChanged() in clone mode. */
        mNextConfig.forceNotifyBufferInfo = true;
        mRgbLayer = -1;
        mYuvLayer = -1;
        goto finish;
    }

#if 0
    // TODO: Bring back this optimization
    if (display->numHwLayers-1 == 1) {
        hwc_layer_1_t& layer = display->hwLayers[0];
        if (analyzer->isPresentationLayer(layer) && layer.transform == 0 && layer.blending == HWC_BLENDING_NONE) {
            if (analyzer->isVideoLayer(layer))
                mYuvLayer = 0;
            else
                mRgbLayer = 0;
            VTRACE("Presentation fast path");
        }
        goto finish;
    }
#endif

    for (size_t i = 0; i < display->numHwLayers-1; i++) {
        hwc_layer_1_t& layer = display->hwLayers[i];
        if (analyzer->isVideoLayer(layer) && (mCurrentConfig.extendedModeEnabled || VSP_FOR_CLEAR_VIDEO || analyzer->isProtectedLayer(layer))) {
            if (mCurrentConfig.extendedModeEnabled && mCurrentConfig.frameServerActive) {
                // If composed in surface flinger, then stream fbtarget.
                if (layer.flags & HWC_SKIP_LAYER) continue;

                /* If the resolution of the video layer is less than QCIF, then we are going to play it in clone mode only.*/
                uint32_t vidContentWidth = layer.sourceCropf.right - layer.sourceCropf.left;
                uint32_t vidContentHeight = layer.sourceCropf.bottom - layer.sourceCropf.top;
                if (vidContentWidth < QCIF_WIDTH || vidContentHeight < QCIF_HEIGHT) {
                    VTRACE("Ingoring layer %d which is too small for extended mode", i);
                    continue;
                }
            }
            mYuvLayer = i;
            mProtectedMode = analyzer->isProtectedLayer(layer);
            if (mCurrentConfig.extendedModeEnabled)
                mRgbLayer = -1;
            break;
        }
    }

finish:
    for (size_t i = 0; i < display->numHwLayers-1; i++) {
        hwc_layer_1_t& layer = display->hwLayers[i];
        if ((size_t)mRgbLayer == display->numHwLayers-1)
            layer.compositionType = HWC_FRAMEBUFFER;
        else
            layer.compositionType = HWC_OVERLAY;
    }
    if (mYuvLayer != -1)
        display->hwLayers[mYuvLayer].compositionType = HWC_OVERLAY;

    if (mYuvLayer == -1) {
        VTRACE("Clone mode");
        // TODO: fix this workaround. Once metadeta has correct info.
        mFirstVideoFrame = true;
    }

    if ((size_t)mRgbLayer == display->numHwLayers-1) {
        // we're streaming fbtarget, so send onFramePrepare and wait for composition to happen
        if (mCurrentConfig.frameListener != NULL)
            mCurrentConfig.frameListener->onFramePrepare(mRenderTimestamp, -1);
        return true;
    }

    // we don't need fbtarget, so send the frame now
    bool result = sendToWidi(display);
    if (result) {
        mRgbLayer = -1;
        mYuvLayer = -1;
        // Extended mode is successful.
        // Fences aren't set in prepare, and we don't need them here, but they'll
        // be set later and we have to close them. Don't log a warning in this case.
        mExpectAcquireFences = true;
    } else {
        // if error in playback file , switch to clone mode
        WTRACE("Error, falling back to clone mode");
        mRgbLayer = display->numHwLayers-1;
        mYuvLayer = -1;
        for (size_t i = 0; i < display->numHwLayers-1; i++) {
            hwc_layer_1_t& layer = display->hwLayers[i];
            layer.compositionType = HWC_FRAMEBUFFER;
        }
    }

    return true;
}

bool VirtualDevice::commit(hwc_display_contents_1_t *display, IDisplayContext *context)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (display != NULL && (mRgbLayer != -1 || mYuvLayer != -1))
        sendToWidi(display);

    if (mVspEnabled && !mVspInUse) {
        sp<DisableVspTask> disableVsp = new DisableVspTask();
        Mutex::Autolock _l(mCscLock);
        mTasks.push(disableVsp);
        mVspEnabled = false;
    }

    if (display != NULL) {
        // All acquire fences should be copied somewhere else or closed by now
        // and set to -1 in these structs except in the case of extended mode.
        // Make sure the fences are closed and log a warning if not in extended mode.
        if (display->outbufAcquireFenceFd != -1) {
            if (!mExpectAcquireFences)
                WTRACE("outbuf acquire fence (fd=%d) not yet saved or closed", display->outbufAcquireFenceFd);
            CLOSE_FENCE(display->outbufAcquireFenceFd);
        }
        for (size_t i = 0; i < display->numHwLayers; i++) {
            hwc_layer_1_t& layer = display->hwLayers[i];
            if (layer.acquireFenceFd != -1) {
                if (!mExpectAcquireFences)
                    WTRACE("layer %d acquire fence (fd=%d) not yet saved or closed", i, layer.acquireFenceFd);
                CLOSE_FENCE(layer.acquireFenceFd);
            }
        }
    }

    return true;
}

bool VirtualDevice::sendToWidi(hwc_display_contents_1_t *display)
{
    VTRACE("RGB=%d, YUV=%d", mRgbLayer, mYuvLayer);

    if (mYuvLayer != -1 && mRgbLayer != -1) {
        mVspEnabled = true;
        mVspInUse = true;
        if (queueCompose(display))
            return true;

        return queueColorConvert(display);
    }

    if (mRgbLayer != -1)
        return queueColorConvert(display);

    if (mYuvLayer != -1) {
        if (mCurrentConfig.frameServerActive)
            return handleExtendedMode(display);
        return queueVideoCopy(display);
    }

    return true;
}

bool VirtualDevice::queueCompose(hwc_display_contents_1_t *display)
{
    sp<ComposeTask> composeTask = new ComposeTask();
    sp<HeldCscBuffer> heldBuffer;

    composeTask->syncTimelineFd = -1;

    composeTask->videoHandle = display->hwLayers[mYuvLayer].handle;
    if (composeTask->videoHandle == NULL) {
        ETRACE("No video handle");
        return false;
    }
    composeTask->displayFrame = display->hwLayers[mYuvLayer].displayFrame;

    composeTask->rgbHandle = display->hwLayers[mRgbLayer].handle;
    if (composeTask->rgbHandle == NULL) {
        ETRACE("No RGB handle");
        return false;
    }

    {
        hwc_layer_1_t& rgbLayer = display->hwLayers[mRgbLayer];
        composeTask->outWidth = rgbLayer.sourceCropf.right - rgbLayer.sourceCropf.left;
        composeTask->outHeight = rgbLayer.sourceCropf.bottom - rgbLayer.sourceCropf.top;
    }

    Mutex::Autolock _l(mCscLock);

    if (mCurrentConfig.frameServerActive) {
        composeTask->outputHandle = (buffer_handle_t) getCscBuffer(composeTask->outWidth, composeTask->outHeight);
        heldBuffer = new HeldCscBuffer(this, (uint32_t) composeTask->outputHandle);
    } else {
        composeTask->outputHandle = display->outbuf;
    }

    if (composeTask->outputHandle == NULL) {
        ETRACE("No outbuf");
        return false;
    }
    composeTask->videoCachedBuffer = getMappedBuffer((uint32_t) composeTask->videoHandle);
    if (composeTask->videoCachedBuffer == NULL) {
        ETRACE("Couldn't map video handle %p", composeTask->videoHandle);
        return false;
    }
    if (composeTask->videoCachedBuffer->mapper == NULL) {
        ETRACE("Src mapper gone");
        return false;
    }
    composeTask->heldVideoBuffer = new HeldDecoderBuffer(this, composeTask->videoCachedBuffer);
    if (!mPayloadManager->getMetaData(composeTask->videoCachedBuffer->mapper, &composeTask->videoMetadata)) {
        ETRACE("Failed to map video payload info");
        return false;
    }
    if (composeTask->videoMetadata.width == 0 || composeTask->videoMetadata.height == 0) {
        ETRACE("Bad video metadata for handle %p", composeTask->videoHandle);
        return false;
    }
    if (composeTask->videoMetadata.kHandle == 0) {
        ETRACE("Bad khandle");
        return false;
    }
    uint32_t scaleWidth = composeTask->displayFrame.right - composeTask->displayFrame.left;
    uint32_t scaleHeight = composeTask->displayFrame.bottom - composeTask->displayFrame.top;

    // no upscaling exists, so don't try
    if (scaleWidth > composeTask->videoMetadata.width)
        scaleWidth = composeTask->videoMetadata.width;
    if (scaleHeight > composeTask->videoMetadata.height)
        scaleHeight = composeTask->videoMetadata.height;

    scaleWidth &= ~1;
    scaleHeight &= ~1;

    if (mFirstVideoFrame || scaleWidth != mLastScaleWidth || scaleHeight != mLastScaleHeight) {
        int sessionID = mHwc.getDisplayAnalyzer()->getFirstVideoInstanceSessionID();
        if (sessionID >= 0) {
            MultiDisplayObserver* mds = mHwc.getMultiDisplayObserver();
            status_t ret = mds->setDecoderOutputResolution(sessionID, scaleWidth, scaleHeight, 0, 0, scaleWidth, scaleHeight);
            if (ret == NO_ERROR) {
                mLastScaleWidth = scaleWidth;
                mLastScaleHeight = scaleHeight;
                mFirstVideoFrame = false;
                ITRACE("Set scaling to %ux%u", scaleWidth, scaleHeight);
            }
            else
                ETRACE("Failed to set scaling to %ux%u: %x", scaleWidth, scaleHeight, ret);
        }
    }

    composeTask->yuvAcquireFenceFd = display->hwLayers[mYuvLayer].acquireFenceFd;
    display->hwLayers[mYuvLayer].acquireFenceFd = -1;

    composeTask->rgbAcquireFenceFd = display->hwLayers[mRgbLayer].acquireFenceFd;
    display->hwLayers[mRgbLayer].acquireFenceFd = -1;

    composeTask->outbufAcquireFenceFd = display->outbufAcquireFenceFd;
    display->outbufAcquireFenceFd = -1;

    int retireFd = sw_sync_fence_create(mSyncTimelineFd, "widi_compose_retire", mNextSyncPoint);
    display->hwLayers[mRgbLayer].releaseFenceFd = retireFd;
    display->hwLayers[mYuvLayer].releaseFenceFd = dup(retireFd);
    display->retireFenceFd = dup(retireFd);
    mNextSyncPoint++;
    composeTask->syncTimelineFd = mSyncTimelineFd;

    mTasks.push_back(composeTask);
    mRequestQueued.signal();

    if (mCurrentConfig.frameServerActive) {
        hwc_layer_1_t& layer = display->hwLayers[mRgbLayer];
        FrameInfo inputFrameInfo;
        memset(&inputFrameInfo, 0, sizeof(inputFrameInfo));
        inputFrameInfo.isProtected = mProtectedMode;
        inputFrameInfo.frameType = HWC_FRAMETYPE_FRAME_BUFFER;
        inputFrameInfo.contentWidth = layer.sourceCropf.right - layer.sourceCropf.left;
        inputFrameInfo.contentHeight = layer.sourceCropf.bottom - layer.sourceCropf.top;
        inputFrameInfo.contentFrameRateN = 0;
        inputFrameInfo.contentFrameRateD = 0;
        FrameInfo outputFrameInfo = inputFrameInfo;

        BufferManager* mgr = mHwc.getBufferManager();
        DataBuffer* dataBuf = mgr->lockDataBuffer((uint32_t) composeTask->outputHandle);
        outputFrameInfo.bufferWidth = dataBuf->getWidth();
        outputFrameInfo.bufferHeight = dataBuf->getHeight();
        outputFrameInfo.lumaUStride = dataBuf->getWidth();
        outputFrameInfo.chromaUStride = dataBuf->getWidth();
        outputFrameInfo.chromaVStride = dataBuf->getWidth();
        mgr->unlockDataBuffer(dataBuf);

        queueFrameTypeInfo(inputFrameInfo);
        if (mCurrentConfig.policy.scaledWidth == 0 || mCurrentConfig.policy.scaledHeight == 0)
            return true; // This isn't a failure, WiDi just doesn't want frames right now.
        queueBufferInfo(outputFrameInfo);

        sp<OnFrameReadyTask> frameReadyTask = new OnFrameReadyTask();
        frameReadyTask->renderTask = composeTask;
        frameReadyTask->heldBuffer = heldBuffer;
        frameReadyTask->frameListener = mCurrentConfig.frameListener;
        frameReadyTask->handle = (uint32_t) composeTask->outputHandle;
        frameReadyTask->handleType = HWC_HANDLE_TYPE_GRALLOC;
        frameReadyTask->renderTimestamp = mRenderTimestamp;
        frameReadyTask->mediaTimestamp = -1;
        if (frameReadyTask->frameListener != NULL)
            mTasks.push_back(frameReadyTask);
    }

    return true;
}

bool VirtualDevice::queueColorConvert(hwc_display_contents_1_t *display)
{
    sp<RefBase> heldBuffer;

    hwc_layer_1_t& layer = display->hwLayers[mRgbLayer];
    if (layer.handle == NULL) {
        ETRACE("RGB layer has no handle set");
        return false;
    }

    {
        const IMG_native_handle_t* nativeSrcHandle = reinterpret_cast<const IMG_native_handle_t*>(layer.handle);
        const IMG_native_handle_t* nativeDestHandle = reinterpret_cast<const IMG_native_handle_t*>(display->outbuf);

        if ((nativeSrcHandle->iFormat == HAL_PIXEL_FORMAT_RGBA_8888 &&
            nativeDestHandle->iFormat == HAL_PIXEL_FORMAT_BGRA_8888) ||
            (nativeSrcHandle->iFormat == HAL_PIXEL_FORMAT_BGRA_8888 &&
            nativeDestHandle->iFormat == HAL_PIXEL_FORMAT_RGBA_8888))
        {
            SYNC_WAIT_AND_CLOSE(layer.acquireFenceFd);
            SYNC_WAIT_AND_CLOSE(display->outbufAcquireFenceFd);
            display->retireFenceFd = -1;

            // synchronous in this case
            colorSwap(layer.handle, display->outbuf, ((nativeSrcHandle->iWidth+31)&~31)*nativeSrcHandle->iHeight);
            // Workaround: Don't keep cached buffers. If the VirtualDisplaySurface gets destroyed,
            //             these would be unmapped on the next frame, after the buffers are destroyed,
            //             which is causing heap corruption, probably due to a double-free somewhere.
            mMappedBufferCache.clear();
            return true;
        }
    }

    sp<BlitTask> blitTask = new BlitTask();
    blitTask->cropInfo.x = 0;
    blitTask->cropInfo.y = 0;
    blitTask->cropInfo.w = layer.sourceCropf.right - layer.sourceCropf.left;
    blitTask->cropInfo.h = layer.sourceCropf.bottom - layer.sourceCropf.top;
    blitTask->srcHandle = layer.handle;

    Mutex::Autolock _l(mCscLock);

    blitTask->srcAcquireFenceFd = layer.acquireFenceFd;
    layer.acquireFenceFd = -1;

    blitTask->syncTimelineFd = mSyncTimelineFd;
    // Framebuffer after BlitTask::run() calls sw_sync_timeline_inc().
    layer.releaseFenceFd = sw_sync_fence_create(mSyncTimelineFd, "widi_blit_retire", mNextSyncPoint);

    if (mCurrentConfig.frameServerActive) {
        blitTask->destHandle = (buffer_handle_t)getCscBuffer(blitTask->cropInfo.w, blitTask->cropInfo.h);
        blitTask->destAcquireFenceFd = -1;

        // we use our own buffer, so just close this fence without a wait
        CLOSE_FENCE(display->outbufAcquireFenceFd);
    }
    else {
        blitTask->destHandle = display->outbuf;
        blitTask->destAcquireFenceFd = display->outbufAcquireFenceFd;
        // don't let TngDisplayContext::commitEnd() close this
        display->outbufAcquireFenceFd = -1;
        display->retireFenceFd = dup(layer.releaseFenceFd);
        mNextSyncPoint++;
    }

    if (blitTask->destHandle == NULL)
        return false;

    if (mCurrentConfig.frameServerActive)
        heldBuffer = new HeldCscBuffer(this, (uint32_t)blitTask->destHandle);

    mTasks.push_back(blitTask);
    mRequestQueued.signal();

    if (mCurrentConfig.frameServerActive) {
        FrameInfo inputFrameInfo;
        memset(&inputFrameInfo, 0, sizeof(inputFrameInfo));
        inputFrameInfo.isProtected = mProtectedMode;
        FrameInfo outputFrameInfo;

        inputFrameInfo.frameType = HWC_FRAMETYPE_FRAME_BUFFER;
        inputFrameInfo.contentWidth = blitTask->cropInfo.w;
        inputFrameInfo.contentHeight = blitTask->cropInfo.h;
        inputFrameInfo.contentFrameRateN = 0;
        inputFrameInfo.contentFrameRateD = 0;
        outputFrameInfo = inputFrameInfo;

        BufferManager* mgr = mHwc.getBufferManager();
        DataBuffer* dataBuf = mgr->lockDataBuffer((uint32_t)blitTask->destHandle);
        outputFrameInfo.bufferWidth = dataBuf->getWidth();
        outputFrameInfo.bufferHeight = dataBuf->getHeight();
        outputFrameInfo.lumaUStride = dataBuf->getWidth();
        outputFrameInfo.chromaUStride = dataBuf->getWidth();
        outputFrameInfo.chromaVStride = dataBuf->getWidth();
        mgr->unlockDataBuffer(dataBuf);

        queueFrameTypeInfo(inputFrameInfo);
        if (mCurrentConfig.policy.scaledWidth == 0 || mCurrentConfig.policy.scaledHeight == 0)
            return true; // This isn't a failure, WiDi just doesn't want frames right now.
        queueBufferInfo(outputFrameInfo);

        sp<OnFrameReadyTask> frameReadyTask = new OnFrameReadyTask();
        frameReadyTask->renderTask = blitTask;
        frameReadyTask->heldBuffer = heldBuffer;
        frameReadyTask->frameListener = mCurrentConfig.frameListener;
        frameReadyTask->handle = (uint32_t) blitTask->destHandle;
        frameReadyTask->handleType = HWC_HANDLE_TYPE_GRALLOC;
        frameReadyTask->renderTimestamp = mRenderTimestamp;
        frameReadyTask->mediaTimestamp = -1;
        if (frameReadyTask->frameListener != NULL)
            mTasks.push_back(frameReadyTask);
    }

    return true;
}

bool VirtualDevice::handleExtendedMode(hwc_display_contents_1_t *display)
{
    FrameInfo inputFrameInfo;
    memset(&inputFrameInfo, 0, sizeof(inputFrameInfo));
    inputFrameInfo.isProtected = mProtectedMode;
    FrameInfo outputFrameInfo;

    hwc_layer_1_t& layer = display->hwLayers[mYuvLayer];
    uint32_t handle = (uint32_t)layer.handle;
    if (handle == 0) {
        ETRACE("video layer has no handle set");
        return false;
    }
    sp<CachedBuffer> cachedBuffer;
    if ((cachedBuffer = getMappedBuffer(handle)) == NULL) {
        ETRACE("Failed to map display buffer");
        return false;
    }

    inputFrameInfo.frameType = HWC_FRAMETYPE_VIDEO;
    // for video mode let 30 fps be the default value.
    inputFrameInfo.contentFrameRateN = 30;
    inputFrameInfo.contentFrameRateD = 1;
    //handleType = HWC_HANDLE_TYPE_KBUF;

    IVideoPayloadManager::MetaData metadata;
    if (!mPayloadManager->getMetaData(cachedBuffer->mapper, &metadata)) {
        ETRACE("Failed to get metadata");
        return false;
    }

    sp<RefBase> heldBuffer = new HeldDecoderBuffer(this, cachedBuffer);
    int64_t mediaTimestamp = metadata.timestamp;
    inputFrameInfo.contentWidth = metadata.crop_width;
    inputFrameInfo.contentHeight = metadata.crop_height;
    // Use the crop size if something changed derive it again..
    // Only get video source info if frame rate has not been initialized.
    // getVideoSourceInfo() is a fairly expensive operation. This optimization
    // will save us a few milliseconds per frame
    if (mFirstVideoFrame || (mOrigContentWidth != inputFrameInfo.contentWidth) ||
        (mOrigContentHeight != inputFrameInfo.contentHeight)) {
        mVideoFramerate = inputFrameInfo.contentFrameRateN;
        VTRACE("VideoWidth = %d, VideoHeight = %d", metadata.crop_width, metadata.crop_height);
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

    if (metadata.kHandle == 0) {
        ETRACE("Couldn't get any khandle");
        return false;
    }
    handle = metadata.kHandle;
    outputFrameInfo.bufferWidth = metadata.width;
    outputFrameInfo.bufferHeight = ((metadata.height + 0x1f) & (~0x1f));
    outputFrameInfo.lumaUStride = metadata.lumaStride;
    outputFrameInfo.chromaUStride = metadata.chromaUStride;
    outputFrameInfo.chromaVStride = metadata.chromaVStride;
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

    queueFrameTypeInfo(inputFrameInfo);
    if (mCurrentConfig.policy.scaledWidth == 0 || mCurrentConfig.policy.scaledHeight == 0)
        return true; // This isn't a failure, WiDi just doesn't want frames right now.
    queueBufferInfo(outputFrameInfo);

    if (handle == mExtLastKhandle && mediaTimestamp == mExtLastTimestamp) {
        // Same frame again. We don't send a frame, but we return true because
        // this isn't an error.
        return true;
    }
    mExtLastKhandle = handle;
    mExtLastTimestamp = mediaTimestamp;

    sp<OnFrameReadyTask> frameReadyTask = new OnFrameReadyTask;
    frameReadyTask->renderTask = NULL;
    frameReadyTask->heldBuffer = heldBuffer;
    frameReadyTask->frameListener = mCurrentConfig.frameListener;
    frameReadyTask->handle = handle;
    frameReadyTask->handleType = HWC_HANDLE_TYPE_KBUF;
    frameReadyTask->renderTimestamp = mRenderTimestamp;
    frameReadyTask->mediaTimestamp = mediaTimestamp;

    Mutex::Autolock _l(mCscLock);
    if (frameReadyTask->frameListener != NULL)
        mTasks.push_back(frameReadyTask);
    mRequestQueued.signal();

    return true;
}

bool VirtualDevice::queueVideoCopy(hwc_display_contents_1_t *display)
{
    // TODO: Add path for video but no RGB layer and make this reachable
    //       Could compose with a blank RGB layer, or copy the video to
    //       outbuf using the CPU.
    return true;
}

void VirtualDevice::queueFrameTypeInfo(const FrameInfo& inputFrameInfo)
{
    if (mCurrentConfig.forceNotifyFrameType ||
        memcmp(&inputFrameInfo, &mLastInputFrameInfo, sizeof(inputFrameInfo)) != 0) {
        // something changed, notify type change listener
        mNextConfig.forceNotifyFrameType = false;
        mLastInputFrameInfo = inputFrameInfo;

        sp<FrameTypeChangedTask> notifyTask = new FrameTypeChangedTask;
        notifyTask->typeChangeListener = mCurrentConfig.typeChangeListener;
        notifyTask->inputFrameInfo = inputFrameInfo;
        mTasks.push_back(notifyTask);
    }
}

void VirtualDevice::queueBufferInfo(const FrameInfo& outputFrameInfo)
{
    if (mCurrentConfig.forceNotifyBufferInfo ||
        memcmp(&outputFrameInfo, &mLastOutputFrameInfo, sizeof(outputFrameInfo)) != 0) {
        mNextConfig.forceNotifyBufferInfo = false;
        mLastOutputFrameInfo = outputFrameInfo;

        sp<BufferInfoChangedTask> notifyTask = new BufferInfoChangedTask;
        notifyTask->typeChangeListener = mCurrentConfig.typeChangeListener;
        notifyTask->outputFrameInfo = outputFrameInfo;

        //if (handleType == HWC_HANDLE_TYPE_GRALLOC)
        //    mMappedBufferCache.clear(); // !
        mTasks.push_back(notifyTask);
    }
}

void VirtualDevice::fill(uint8_t* ptr, const ied_block& data, size_t len) {
    for (size_t offset = 0; offset < len; offset += 16) {
        *reinterpret_cast<ied_block*>(ptr + offset) = data;
    }
}

void VirtualDevice::copyVideo(uint8_t* srcPtr, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcStride,
                              uint8_t* destPtr, uint32_t destWidth, uint32_t destHeight,
                              uint32_t blitX, uint32_t blitY, uint32_t blitWidth, uint32_t blitHeight)
{
    if (srcPtr == NULL || destPtr == NULL) {
        ETRACE("copyVideo: a pointer is NULL");
        return;
    }

    if (blitWidth > srcWidth) {
        // no upscaling yet, so center it instead
        blitX += (blitWidth - srcWidth)/2;
        blitWidth = srcWidth;
    }
    if (blitHeight > srcHeight) {
        // no upscaling yet, so center it instead
        blitY += (blitHeight - srcHeight)/2;
        blitHeight = srcHeight;
    }

    blitX = (blitX+8) & ~15;
    blitY = (blitY+1) & ~1;
    blitWidth = blitWidth & ~15;

    if (blitX + blitWidth > destWidth)
        blitWidth = destWidth - blitX;
    if (blitY + blitHeight > destHeight)
        blitHeight = destHeight - blitY;

    VTRACE("Copy %p (%ux%u stride=%u) -> %p (%ux%u), @%ux%u %ux%u",
          srcPtr, srcWidth, srcHeight, srcStride,
          destPtr, destWidth, destHeight,
          blitX, blitY, blitWidth, blitHeight);
    // clear top bar
    fill(destPtr, mBlackY, blitY*destWidth);
    fill(destPtr + destWidth*destHeight, mBlackUV, blitY*destWidth/2);

    // clear bottom bar
    fill(destPtr + (blitY+blitHeight)*destWidth, mBlackY, (destHeight-blitY-blitHeight)*destWidth);
    fill(destPtr + destWidth*destHeight + (blitY+blitHeight)*destWidth/2, mBlackUV, (destHeight-blitY-blitHeight)*destWidth/2);

    if (blitX == 0 && srcStride == destWidth) {
        // copy whole Y plane
        memcpy(destPtr+blitY*destWidth, srcPtr, srcStride*blitHeight);
        // copy whole UV plane
        memcpy(destPtr+(destHeight+blitY/2)*destWidth, srcPtr+srcStride*srcHeight, srcStride*blitHeight/2);
    } else {
        // clear left and right bars, Y plane
        for (uint32_t y = 0; y < blitHeight; y++) {
            fill(destPtr + (blitY + y)*destWidth, mBlackY, blitX);
            fill(destPtr + (blitY + y)*destWidth+blitX+blitWidth, mBlackY, destWidth-blitX-blitWidth);
        }

        // clear left and right bars, UV plane
        for (uint32_t y = 0; y < blitHeight/2; y++) {
            fill(destPtr + (destHeight + blitY/2 + y)*destWidth, mBlackUV, blitX);
            fill(destPtr + (destHeight + blitY/2 + y)*destWidth+blitX+blitWidth, mBlackUV, destWidth-blitX-blitWidth);
        }

        // copy Y plane one row at a time
        for (uint32_t row = 0; row < blitHeight; row++)
            memcpy(destPtr+(row+blitY)*destWidth+blitX, srcPtr + row*srcStride, blitWidth);
        // copy UV plane one row at a time
        for (uint32_t row = 0; row < blitHeight/2; row++)
            memcpy(destPtr+(destHeight+row+blitY/2)*destWidth+blitX, srcPtr+(srcHeight+row)*srcStride, blitWidth);
    }
}

void VirtualDevice::colorSwap(buffer_handle_t src, buffer_handle_t dest, uint32_t pixelCount)
{
    sp<CachedBuffer> srcCachedBuffer;
    sp<CachedBuffer> destCachedBuffer;

    {
        srcCachedBuffer = getMappedBuffer((uint32_t)src);
        if (srcCachedBuffer == NULL || srcCachedBuffer->mapper == NULL)
            return;
        destCachedBuffer = getMappedBuffer((uint32_t)dest);
        if (destCachedBuffer == NULL || destCachedBuffer->mapper == NULL)
            return;
    }

    uint8_t* srcPtr = static_cast<uint8_t*>(srcCachedBuffer->mapper->getCpuAddress(0));
    uint8_t* destPtr = static_cast<uint8_t*>(destCachedBuffer->mapper->getCpuAddress(0));
    if (srcPtr == NULL || destPtr == NULL)
        return;
    while (pixelCount > 0) {
        destPtr[0] = srcPtr[2];
        destPtr[1] = srcPtr[1];
        destPtr[2] = srcPtr[0];
        destPtr[3] = srcPtr[3];
        srcPtr += 4;
        destPtr += 4;
        pixelCount--;
    }
}

void VirtualDevice::vspEnable(uint32_t width, uint32_t height)
{
    ITRACE("Start VSP");

    VAStatus va_status;
    va_status = vaCreateSurfaces(
                va_dpy,
                VA_RT_FORMAT_YUV420,
                width,
                height,
                &va_video_in,
                1,
                NULL,
                0);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaCreateSurfaces (video in) returns %08x", va_status);

    va_status = vaCreateContext(
                va_dpy,
                va_config,
                width,
                height,
                0,
                &va_video_in /* not used by VSP, but libva checks for it */,
                1,
                &va_context);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaCreateContext returns %08x", va_status);
}

void VirtualDevice::vspDisable()
{
    ITRACE("Shut down VSP");

    if (va_context == 0 && va_video_in == 0) {
        ITRACE("Already shut down");
        return;
    }

    VABufferID pipeline_param_id;
    VAStatus va_status;
    va_status = vaCreateBuffer(va_dpy,
                va_context,
                VAProcPipelineParameterBufferType,
                sizeof(VAProcPipelineParameterBuffer),
                1,
                NULL,
                &pipeline_param_id);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaCreateBuffer returns %08x", va_status);

    VABlendState blend_state;
    VAProcPipelineParameterBuffer *pipeline_param;
    va_status = vaMapBuffer(va_dpy,
                pipeline_param_id,
                (void **)&pipeline_param);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaMapBuffer returns %08x", va_status);

    memset(pipeline_param, 0, sizeof(VAProcPipelineParameterBuffer));
    pipeline_param->pipeline_flags = VA_PIPELINE_FLAG_END;
    pipeline_param->num_filters = 0;
    pipeline_param->blend_state = &blend_state;

    va_status = vaUnmapBuffer(va_dpy, pipeline_param_id);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaUnmapBuffer returns %08x", va_status);

    va_status = vaBeginPicture(va_dpy, va_context, va_video_in /* just need some valid surface */);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaBeginPicture returns %08x", va_status);

    va_status = vaRenderPicture(va_dpy, va_context, &pipeline_param_id, 1);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaRenderPicture returns %08x", va_status);

    va_status = vaEndPicture(va_dpy, va_context);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaEndPicture returns %08x", va_status);

    va_status = vaDestroyContext(va_dpy, va_context);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaDestroyContext returns %08x", va_status);
    va_context = 0;

    va_status = vaDestroySurfaces(va_dpy, &va_video_in, 1);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaDestroySurfaces returns %08x", va_status);
    va_video_in = 0;
}

void VirtualDevice::vspCompose(VASurfaceID videoIn, VASurfaceID rgbIn, VASurfaceID videoOut)
{
    VAStatus va_status;

    VABufferID pipeline_param_id;
    va_status = vaCreateBuffer(va_dpy,
                va_context,
                VAProcPipelineParameterBufferType,
                sizeof(VAProcPipelineParameterBuffer),
                1,
                NULL,
                &pipeline_param_id);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaCreateBuffer returns %08x", va_status);

    VABlendState blend_state;

    VAProcPipelineParameterBuffer *pipeline_param;
    va_status = vaMapBuffer(va_dpy,
                pipeline_param_id,
                (void **)&pipeline_param);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaMapBuffer returns %08x", va_status);

    memset(pipeline_param, 0, sizeof(VAProcPipelineParameterBuffer));
    pipeline_param->surface = videoIn;
    pipeline_param->pipeline_flags = 0;
    pipeline_param->num_filters = 0;
    pipeline_param->blend_state = &blend_state;
    pipeline_param->num_additional_outputs = 1;
    pipeline_param->additional_outputs = &rgbIn;
    pipeline_param->surface_region = NULL;
    pipeline_param->output_region = NULL;

    va_status = vaUnmapBuffer(va_dpy, pipeline_param_id);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaUnmapBuffer returns %08x", va_status);

    va_status = vaBeginPicture(va_dpy, va_context, videoOut);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaBeginPicture returns %08x", va_status);

    va_status = vaRenderPicture(va_dpy, va_context, &pipeline_param_id, 1);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaRenderPicture returns %08x", va_status);

    va_status = vaEndPicture(va_dpy, va_context);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaEndPicture returns %08x", va_status);

    va_status = vaSyncSurface(va_dpy, videoOut);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaSyncSurface returns %08x", va_status);
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
    mRgbLayer = -1;
    mYuvLayer = -1;

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

    mSyncTimelineFd = sw_sync_timeline_create();
    mNextSyncPoint = 1;
    mExpectAcquireFences = false;

    mThread = new WidiBlitThread(this);
    mThread->run("WidiBlit", PRIORITY_URGENT_DISPLAY);

    // Publish frame server service with service manager
    status_t ret = defaultServiceManager()->addService(String16("hwc.widi"), this);
    if (ret == NO_ERROR) {
        ProcessState::self()->startThreadPool();
        mInitialized = true;
    } else {
        ETRACE("Could not register hwc.widi with service manager, error = %d", ret);
        deinitialize();
    }

    mLastScaleWidth = 0;
    mLastScaleHeight = 0;

    mVspEnabled = false;
    mVspInUse = false;
    int display = 0;
    int major_ver, minor_ver;
    va_dpy = vaGetDisplay(&display);
    VAStatus va_status = vaInitialize(va_dpy, &major_ver, &minor_ver);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaInitialize returns %08x", va_status);

    VAConfigAttrib va_attr;
    va_attr.type = VAConfigAttribRTFormat;
    va_status = vaGetConfigAttributes(va_dpy,
                VAProfileNone,
                VAEntrypointVideoProc,
                &va_attr,
                1);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaGetConfigAttributes returns %08x", va_status);

    va_status = vaCreateConfig(
                va_dpy,
                VAProfileNone,
                VAEntrypointVideoProc,
                &(va_attr),
                1,
                &va_config
                );
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaCreateConfig returns %08x", va_status);

    VADisplayAttribute attr;
    attr.type = VADisplayAttribRenderMode;
    attr.value = VA_RENDER_MODE_LOCAL_OVERLAY;
    va_status = vaSetDisplayAttributes(va_dpy, &attr, 1);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaSetDisplayAttributes returns %08x", va_status);

    va_context = 0;
    va_video_in = 0;

    // TODO: get encrypted black when IED is armed
    memset(mBlackY.data, 0, sizeof(mBlackY.data));
    memset(mBlackUV.data, 0, sizeof(mBlackUV.data));

    ITRACE("Init done.");

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
    VAStatus va_status;
    va_status = vaDestroyConfig(va_dpy, va_config);
    if (va_status != VA_STATUS_SUCCESS) ETRACE("vaDestroyConfig returns %08x", va_status);
    va_config = 0;

    va_status = vaTerminate(va_dpy);
    va_dpy = 0;

    if (mPayloadManager) {
        delete mPayloadManager;
        mPayloadManager = NULL;
    }
    DEINIT_AND_DELETE_OBJ(mVsyncObserver);
    mInitialized = false;
}

} // namespace intel
} // namespace android
