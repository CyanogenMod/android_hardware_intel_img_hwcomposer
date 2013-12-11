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
#ifndef VIRTUAL_DEVICE_H
#define VIRTUAL_DEVICE_H

#include <IDisplayDevice.h>
#include "IFrameServer.h"

namespace android {
namespace intel {

class Hwcomposer;
class DisplayPlaneManager;
class IVideoPayloadManager;

class VirtualDevice : public IDisplayDevice, public BnFrameServer {
protected:
    struct CachedBuffer : public android::RefBase {
        CachedBuffer(BufferManager *mgr, uint32_t handle);
        ~CachedBuffer();
        BufferManager *manager;
        BufferMapper *mapper;
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
        bool extendedModeEnabled;
        bool forceNotifyFrameType;
        bool forceNotifyBufferInfo;
    };
    Mutex mConfigLock;
    Configuration mCurrentConfig;
    Configuration mNextConfig;
    size_t mLayerToSend;

    uint32_t mExtLastKhandle;
    int64_t mExtLastTimestamp;

    int64_t mRenderTimestamp;

    // colorspace conversion
    Mutex mCscLock;
    android::List<uint32_t> mAvailableCscBuffers;
    int mCscBuffersToCreate;
    uint32_t mCscWidth;
    uint32_t mCscHeight;

    FrameInfo mLastInputFrameInfo;
    FrameInfo mLastOutputFrameInfo;

    int32_t mVideoFramerate;

    android::KeyedVector<uint32_t, android::sp<CachedBuffer> > mMappedBufferCache;
    android::Mutex mHeldBuffersLock;
    android::KeyedVector<uint32_t, android::sp<android::RefBase> > mHeldBuffers;

private:
    android::sp<CachedBuffer> getMappedBuffer(uint32_t handle);
    void sendToWidi(const hwc_layer_1_t& layer, bool isProtected);

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
    uint32_t mOrigContentWidth;
    uint32_t mOrigContentHeight;
    bool mFirstVideoFrame;
};

}
}

#endif /* VIRTUAL_DEVICE_H */
