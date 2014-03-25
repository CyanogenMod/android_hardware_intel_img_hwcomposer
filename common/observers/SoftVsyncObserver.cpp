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
#include <SoftVsyncObserver.h>
#include <IDisplayDevice.h>

extern "C" int clock_nanosleep(clockid_t clock_id, int flags,
                           const struct timespec *request,
                           struct timespec *remain);


namespace android {
namespace intel {

SoftVsyncObserver::SoftVsyncObserver(IDisplayDevice& disp)
    : mDisplayDevice(disp),
      mDevice(IDisplayDevice::DEVICE_COUNT),
      mEnabled(false),
      mRefreshRate(60), // default 60 frames per second
      mRefreshPeriod(0),
      mLock(),
      mCondition(),
      mNextFakeVSync(0),
      mExitThread(false),
      mInitialized(false)
{
}

SoftVsyncObserver::~SoftVsyncObserver()
{
    WARN_IF_NOT_DEINIT();
}

bool SoftVsyncObserver::initialize()
{
    if (mInitialized) {
        WTRACE("object has been initialized");
        return true;
    }

    mExitThread = false;
    mEnabled = false;
    mRefreshRate = 60;
    mDevice = mDisplayDevice.getType();
    mThread = new VsyncEventPollThread(this);
    if (!mThread.get()) {
        DEINIT_AND_RETURN_FALSE("failed to create vsync event poll thread.");
    }
    mThread->run("SoftVsyncObserver", PRIORITY_URGENT_DISPLAY);
    mInitialized = true;
    return true;
}

void SoftVsyncObserver::deinitialize()
{
    if (mEnabled) {
        WTRACE("soft vsync is still enabled");
        control(false);
    }

    mExitThread = true;
    mCondition.signal();

    if (mThread.get()) {
        mThread->requestExitAndWait();
        mThread = NULL;
    }
    mInitialized = false;
}

void SoftVsyncObserver::setRefreshRate(int rate)
{
    if (mEnabled) {
        WTRACE("too late to set refresh rate");
    } else if (rate < 1 || rate > 120) {
        WTRACE("invalid refresh rate %d", rate);
    } else {
        mRefreshRate = rate;
    }
}

bool SoftVsyncObserver::control(bool enabled)
{
    if (enabled == mEnabled) {
        WTRACE("vsync state %d is not changed", enabled);
        return true;
    }

    if (enabled) {
        mRefreshPeriod = nsecs_t(1e9 / mRefreshRate);
        mNextFakeVSync = systemTime(CLOCK_MONOTONIC) + mRefreshPeriod;
    }
    mEnabled = enabled;
    mCondition.signal();
    return true;
}

bool SoftVsyncObserver::threadLoop()
{
    { // scope for lock
        Mutex::Autolock _l(mLock);
        while (!mEnabled) {
            mCondition.wait(mLock);
            if (mExitThread) {
                ITRACE("exiting thread loop");
                return false;
            }
        }
    }


    const nsecs_t period = mRefreshPeriod;
    const nsecs_t now = systemTime(CLOCK_MONOTONIC);
    nsecs_t next_vsync = mNextFakeVSync;
    nsecs_t sleep = next_vsync - now;
    if (sleep < 0) {
        // we missed, find where the next vsync should be
        sleep = (period - ((now - next_vsync) % period));
        next_vsync = now + sleep;
    }
    mNextFakeVSync = next_vsync + period;

    struct timespec spec;
    spec.tv_sec  = next_vsync / 1000000000;
    spec.tv_nsec = next_vsync % 1000000000;

    int err;
    do {
        err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &spec, NULL);
    } while (err < 0 && errno == EINTR);


    if (err == 0) {
        mDisplayDevice.onVsync(next_vsync);
    }

    return true;
}

} // namespace intel
} // namesapce android

