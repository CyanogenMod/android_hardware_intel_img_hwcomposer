/*
// Copyright (c) 2014 Intel Corporation 
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#ifndef VIRTUAL_DEVICE_H
#define VIRTUAL_DEVICE_H

#include <IDisplayDevice.h>
#include <common/base/SimpleThread.h>
#include <IVideoPayloadManager.h>
#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/Vector.h>

#include "IFrameServer.h"

#include <va/va.h>
#include <va/va_vpp.h>

namespace android {
namespace intel {

class Hwcomposer;
class DisplayPlaneManager;
class IVideoPayloadManager;
class SoftVsyncObserver;

class VirtualDevice : public IDisplayDevice, public BnFrameServer {
protected:
    class VAMappedHandle;
    struct CachedBuffer : public android::RefBase {
        CachedBuffer(BufferManager *mgr, uint32_t handle);
        ~CachedBuffer();
        BufferManager *manager;
        BufferMapper *mapper;
        VAMappedHandle *vaMappedHandle;
    };
    struct HeldCscBuffer : public android::RefBase {
        HeldCscBuffer(const android::sp<VirtualDevice>& vd, uint32_t handle);
        virtual ~HeldCscBuffer();
        android::sp<VirtualDevice> vd;
        uint32_t handle;
    };
    struct HeldDecoderBuffer : public android::RefBase {
        HeldDecoderBuffer(const sp<VirtualDevice>& vd, const android::sp<CachedBuffer>& cachedBuffer);
        virtual ~HeldDecoderBuffer();
        android::sp<VirtualDevice> vd;
        android::sp<CachedBuffer> cachedBuffer;
    };
    struct Configuration {
        sp<IFrameTypeChangeListener> typeChangeListener;
        sp<IFrameListener> frameListener;
        FrameProcessingPolicy policy;
        bool frameServerActive;
        bool extendedModeEnabled;
        bool forceNotifyFrameType;
        bool forceNotifyBufferInfo;
    };
    struct Task;
    struct RenderTask;
    struct ComposeTask;
    struct DisableVspTask;
    struct BlitTask;
    struct FrameTypeChangedTask;
    struct BufferInfoChangedTask;
    struct OnFrameReadyTask;
    struct ied_block {
        uint8_t data[16];
    };
    ied_block mBlackY;
    ied_block mBlackUV;

    Mutex mConfigLock;
    Configuration mCurrentConfig;
    Configuration mNextConfig;
    ssize_t mRgbLayer;
    ssize_t mYuvLayer;
    bool mProtectedMode;

    uint32_t mExtLastKhandle;
    int64_t mExtLastTimestamp;

    int64_t mRenderTimestamp;

    // colorspace conversion
    Mutex mCscLock;
    android::List<uint32_t> mAvailableCscBuffers;
    int mCscBuffersToCreate;
    uint32_t mCscWidth;
    uint32_t mCscHeight;

    // fence info
    int mSyncTimelineFd;
    unsigned mNextSyncPoint;
    bool mExpectAcquireFences;

    // async blit info
    DECLARE_THREAD(WidiBlitThread, VirtualDevice);
    Condition mRequestQueued;
    Condition mRequestDequeued;
    Vector< sp<Task> > mTasks;

    FrameInfo mLastInputFrameInfo;
    FrameInfo mLastOutputFrameInfo;

    int32_t mVideoFramerate;

    android::KeyedVector<uint32_t, android::sp<CachedBuffer> > mMappedBufferCache;
    android::Mutex mHeldBuffersLock;
    android::KeyedVector<uint32_t, android::sp<android::RefBase> > mHeldBuffers;

    // VSP
    bool mVspInUse;
    bool mVspEnabled;
    VADisplay va_dpy;
    VAConfigID va_config;
    VAContextID va_context;
    VASurfaceID va_video_in;

    // scaling info
    uint32_t mLastScaleWidth;
    uint32_t mLastScaleHeight;

private:
    uint32_t getCscBuffer(uint32_t width, uint32_t height);
    void clearCscBuffers();
    android::sp<CachedBuffer> getMappedBuffer(uint32_t handle);

    bool sendToWidi(hwc_display_contents_1_t *display);
    bool queueCompose(hwc_display_contents_1_t *display);
    bool queueColorConvert(hwc_display_contents_1_t *display);
    bool handleExtendedMode(hwc_display_contents_1_t *display);
    bool queueVideoCopy(hwc_display_contents_1_t *display);

    void queueFrameTypeInfo(const FrameInfo& inputFrameInfo);
    void queueBufferInfo(const FrameInfo& outputFrameInfo);

    static void fill(uint8_t* ptr, const ied_block& data, size_t len);
    void copyVideo(uint8_t* srcPtr, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcStride,
                   uint8_t* destPtr, uint32_t destWidth, uint32_t destHeight,
                   uint32_t outX, uint32_t outY, uint32_t width, uint32_t height);
    void colorSwap(buffer_handle_t src, buffer_handle_t dest, uint32_t pixelCount);
    void vspEnable(uint32_t width, uint32_t height);
    void vspDisable();
    void vspCompose(VASurfaceID videoIn, VASurfaceID rgbIn, VASurfaceID videoOut);

public:
    VirtualDevice(Hwcomposer& hwc, DisplayPlaneManager& dpm);
    virtual ~VirtualDevice();

public:
    virtual bool prePrepare(hwc_display_contents_1_t *display);
    virtual bool prepare(hwc_display_contents_1_t *display);
    virtual bool commit(hwc_display_contents_1_t *display,
                          IDisplayContext *context);

    virtual bool vsyncControl(bool enabled);
    virtual bool blank(bool blank);
    virtual bool getDisplaySize(int *width, int *height);
    virtual bool getDisplayConfigs(uint32_t *configs,
                                       size_t *numConfigs);
    virtual bool getDisplayAttributes(uint32_t config,
                                          const uint32_t *attributes,
                                          int32_t *values);
    virtual bool compositionComplete();
    virtual bool initialize();
    virtual void deinitialize();
    virtual bool isConnected() const;
    virtual const char* getName() const;
    virtual int getType() const;
    virtual void onVsync(int64_t timestamp);
    virtual void dump(Dump& d);

    // IFrameServer methods
    virtual android::status_t start(sp<IFrameTypeChangeListener> frameTypeChangeListener);
    virtual android::status_t stop(bool isConnected);
    virtual android::status_t notifyBufferReturned(int index);
    virtual android::status_t setResolution(const FrameProcessingPolicy& policy, android::sp<IFrameListener> listener);
protected:
    virtual IVideoPayloadManager* createVideoPayloadManager() = 0;

protected:
    bool mInitialized;
    Hwcomposer& mHwc;
    DisplayPlaneManager& mDisplayPlaneManager;
    IVideoPayloadManager *mPayloadManager;
    SoftVsyncObserver *mVsyncObserver;
    uint32_t mOrigContentWidth;
    uint32_t mOrigContentHeight;
    bool mFirstVideoFrame;
    bool mLastConnectionStatus;
    uint32_t mCachedBufferCapcity;
};

}
}

#endif /* VIRTUAL_DEVICE_H */
