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
#ifndef __MULTIDISPLAY_OBSERVER_H
#define __MULTIDISPLAY_OBSERVER_H

#ifdef TARGET_HAS_MULTIPLE_DISPLAY
#include <display/MultiDisplayService.h>
#include <SimpleThread.h>
#else
#include <utils/Errors.h>
#endif

namespace android {
namespace intel {

struct VideoSourceInfo {
    int width;
    int height;
    int frameRate;
};


#ifdef TARGET_HAS_MULTIPLE_DISPLAY

class MultiDisplayObserver;

class MultiDisplayCallback : public BnMultiDisplayCallback {
public:
    MultiDisplayCallback(MultiDisplayObserver *observer);
    virtual ~MultiDisplayCallback();

    status_t blankSecondaryDisplay(bool blank);
    status_t updateVideoState(int sessionId, MDS_VIDEO_STATE state);
    status_t setHdmiTiming(const MDSHdmiTiming& timing);
    status_t setHdmiScalingType(MDS_SCALING_TYPE type);
    status_t setHdmiOverscan(int hValue, int vValue);
    status_t updateInputState(bool state);

private:
    MultiDisplayObserver *mDispObserver;
    MDS_VIDEO_STATE mVideoState;
};

class MultiDisplayObserver {
public:
    MultiDisplayObserver();
    virtual ~MultiDisplayObserver();

public:
    bool initialize();
    void deinitialize();
    status_t notifyHotPlug(bool connected);
    status_t getVideoSourceInfo(int sessionID, VideoSourceInfo* info);
    int  getVideoSessionNumber();
    bool isExternalDeviceTimingFixed() const;
    status_t notifyWidiConnectionStatus(bool connected);
    status_t setDecoderOutputResolution(int sessionID,
            int32_t width, int32_t height,
            int32_t offX,  int32_t offY,
            int32_t bufWidth, int32_t bufHeight);

private:
    bool isMDSRunning();
    bool initMDSClient();
    bool initMDSClientAsync();
    void deinitMDSClient();
    status_t blankSecondaryDisplay(bool blank);
    status_t updateVideoState(int sessionId, MDS_VIDEO_STATE state);
    status_t setHdmiTiming(const MDSHdmiTiming& timing);
    status_t updateInputState(bool active);
    friend class MultiDisplayCallback;

private:
    enum {
        THREAD_LOOP_DELAY = 10, // 10 ms
        THREAD_LOOP_BOUND = 2000, // 20s
    };

private:
    sp<IMultiDisplayCallbackRegistrar> mMDSCbRegistrar;
    sp<IMultiDisplayInfoProvider> mMDSInfoProvider;
    sp<IMultiDisplayConnectionObserver> mMDSConnObserver;
    sp<IMultiDisplayDecoderConfig> mMDSDecoderConfig;
    sp<MultiDisplayCallback> mMDSCallback;
    mutable Mutex mLock;
    Condition mCondition;
    int mThreadLoopCount;
    bool mDeviceConnected;
    // indicate external devices's timing is set
    bool mExternalHdmiTiming;
    bool mInitialized;

private:
    DECLARE_THREAD(MDSClientInitThread, MultiDisplayObserver);
};

#else

// dummy declaration and implementation of MultiDisplayObserver
class MultiDisplayObserver {
public:
    MultiDisplayObserver() {}
    virtual ~MultiDisplayObserver() {}

    bool initialize() { return true; }
    void deinitialize() {}
    status_t notifyHotPlug(bool connected) { return NO_ERROR; }
    status_t getVideoSourceInfo(int sessionID, VideoSourceInfo* info) { return INVALID_OPERATION; }
    int  getVideoSessionNumber() { return 0; }
    bool isExternalDeviceTimingFixed() const { return false; }
    status_t notifyWidiConnectionStatus(bool connected) { return NO_ERROR; }
    status_t setDecoderOutputResolution(
            int sessionID,
            int32_t width, int32_t height,
            int32_t, int32_t, int32_t, int32_t) { return NO_ERROR; }
};

#endif //TARGET_HAS_MULTIPLE_DISPLAY

} // namespace intel
} // namespace android

#endif /* __MULTIMultiDisplayObserver_H_ */
