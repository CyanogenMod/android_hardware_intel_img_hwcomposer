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
#include <VsyncEventObserver.h>
#include <PhysicalDevice.h>

namespace android {
namespace intel {

VsyncEventObserver::VsyncEventObserver(PhysicalDevice& disp)
    : mLock(),
      mCondition(),
      mDisplayDevice(disp),
      mVsyncControl(NULL),
      mDevice(IDisplayDevice::DEVICE_COUNT),
      mEnabled(false),
      mExitThread(false),
      mInitialized(false)
{
    CTRACE();
}

VsyncEventObserver::~VsyncEventObserver()
{
    WARN_IF_NOT_DEINIT();
}

bool VsyncEventObserver::initialize()
{
    if (mInitialized) {
        WTRACE("object has been initialized");
        return true;
    }

    mExitThread = false;
    mEnabled = false;
    mDevice = mDisplayDevice.getType();
    mVsyncControl = mDisplayDevice.createVsyncControl();
    if (!mVsyncControl || !mVsyncControl->initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to initialize vsync control");
    }

    mThread = new VsyncEventPollThread(this);
    if (!mThread.get()) {
        DEINIT_AND_RETURN_FALSE("failed to create vsync event poll thread.");
    }

    mThread->run("VsyncEventObserver", PRIORITY_URGENT_DISPLAY);

    mInitialized = true;
    return true;
}

void VsyncEventObserver::deinitialize()
{
    if (mEnabled) {
        WTRACE("vsync is still enabled");
        control(false);
    }
    mInitialized = false;
    mExitThread = true;
    mEnabled = false;
    mCondition.signal();

    if (mThread.get()) {
        mThread->requestExitAndWait();
        mThread = NULL;
    }

    DEINIT_AND_DELETE_OBJ(mVsyncControl);
}

bool VsyncEventObserver::control(bool enabled)
{
    ATRACE("enabled = %d on device %d", enabled, mDevice);
    if (enabled == mEnabled) {
        WTRACE("vsync state %d is not changed", enabled);
        return true;
    }

    bool ret = mVsyncControl->control(mDevice, enabled);
    if (!ret) {
        ETRACE("failed to control (%d) vsync on display %d", enabled, mDevice);
        return false;
    }

    Mutex::Autolock _l(mLock);
    mEnabled = enabled;
    mCondition.signal();
    return true;
}

bool VsyncEventObserver::threadLoop()
{
     do {
        // scope for lock
        Mutex::Autolock _l(mLock);
        while (!mEnabled) {
            mCondition.wait(mLock);
            if (mExitThread) {
                ITRACE("exiting thread loop");
                return false;
            }
        }
    } while (0);

    int64_t timestamp;
    bool ret = mVsyncControl->wait(mDevice, timestamp);

    if (ret == false) {
        WTRACE("failed to wait for vsync on display %d, vsync enabled %d", mDevice, mEnabled);
        return true;
    }

    // notify device
    mDisplayDevice.onVsync(timestamp);
    return true;
}

} // namespace intel
} // namesapce android
