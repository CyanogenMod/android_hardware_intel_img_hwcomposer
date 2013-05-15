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
#include <display/IMultiDisplayCallback.h>
#include <display/IMultiDisplayListener.h>
#include <display/MultiDisplayType.h>
#include <display/MultiDisplayClient.h>
#include <utils/threads.h>
#endif

namespace android {
namespace intel {

#ifdef TARGET_HAS_MULTIPLE_DISPLAY

class MultiDisplayObserver;

class MultiDisplayCallback : public BnMultiDisplayCallback {
public:
    MultiDisplayCallback(MultiDisplayObserver *observer);
    virtual ~MultiDisplayCallback();

    status_t setPhoneState(MDS_PHONE_STATE state);
    status_t setVideoState(MDS_VIDEO_STATE state);
    status_t setDisplayTiming(MDS_DISPLAY_ID dpyId, MDSDisplayTiming *timing);
    status_t setDisplayState(MDS_DISPLAY_ID dpyId, MDS_DISPLAY_STATE state);
    status_t setScalingType(MDS_DISPLAY_ID dpyId, MDS_SCALING_TYPE type);
    status_t setOverscan(MDS_DISPLAY_ID dpyId, int hValue, int vValue);

private:
    MultiDisplayObserver *mDispObserver;
    MDS_PHONE_STATE mPhoneState;
    MDS_VIDEO_STATE mVideoState;
};

class MultiDisplayObserver {
public:
    MultiDisplayObserver();
    virtual ~MultiDisplayObserver();

public:
    bool initialize();
    void deinitialize();
    status_t notifyHotPlug(int disp, int connected);

private:
    class MultiDisplayObserverThread : public Thread {
     public:
         MultiDisplayObserverThread(MultiDisplayObserver *owner) {
             mOwner = owner;
         }
         ~MultiDisplayObserverThread() {
             mOwner = NULL;
          }

     private:
         virtual bool threadLoop() {
             return mOwner->threadLoop();
         }

     private:
         MultiDisplayObserver *mOwner;
     };

     friend class MultiDisplayObserverThread;

private:
     bool threadLoop();
     bool isMDSRunning();
     bool initMDSClient();
     void deinitMDSClient();

private:
    enum {
        THREAD_LOOP_DELAY = 10, // 10 ms
        THREAD_LOOP_BOUND = 500,// 5s
    };

private:
    MultiDisplayClient *mMDSClient;
    sp<MultiDisplayCallback> mMDSCallback;
    sp<MultiDisplayObserverThread> mThread;
    mutable Mutex mLock;
    Condition mCondition;
    int mThreadLoopCount;
    bool mInitialized;
};

#else

// dummy declaration and implementation of MultiDisplayObserver
class MultiDisplayObserver {
public:
    MultiDisplayObserver() {}
    virtual ~MultiDisplayObserver() {}

    bool initialize() { return true; }
    void deinitialize() {}
    status_t notifyHotPlug(int disp, int connected) { return NO_ERROR; }
};

#endif //TARGET_HAS_MULTIPLE_DISPLAY

} // namespace intel
} // namespace android

#endif /* __MULTIMultiDisplayObserver_H_ */
