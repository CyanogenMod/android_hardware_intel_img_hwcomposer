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
#include <HotplugEventObserver.h>
#include <ExternalDevice.h>

namespace android {
namespace intel {

HotplugEventObserver::HotplugEventObserver(ExternalDevice& disp)
    : mDisplayDevice(disp),
      mHotplugControl(NULL),
      mExitThread(false),
      mInitialized(false)
{
    CTRACE();
}

HotplugEventObserver::~HotplugEventObserver()
{
    WARN_IF_NOT_DEINIT();
}

bool HotplugEventObserver::initialize()
{
    if (mInitialized) {
        WTRACE("object has been initialized");
        return true;
    }

    mExitThread = false;
    mHotplugControl = mDisplayDevice.createHotplugControl();
    if (!mHotplugControl || !mHotplugControl->initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to create hotplug control");
    }

    mThread = new HotplugEventPollThread(this);
    if (!mThread.get()) {
        DEINIT_AND_RETURN_FALSE("failed to create hotplug event poll thread.");
    }

    mInitialized = true;
    mThread->run("HotplugEventObserver", PRIORITY_URGENT_DISPLAY);
    return true;
}

void HotplugEventObserver::deinitialize()
{
    mExitThread = true;

    // deinitialize hotplug control to break thread loop
    if (mHotplugControl) {
        mHotplugControl->deinitialize();
    }

    if (mThread.get()) {
        mThread->requestExitAndWait();
        mThread = NULL;
    }

    if (mHotplugControl) {
        delete mHotplugControl;
        mHotplugControl = NULL;
    }

    mInitialized = false;
}

bool HotplugEventObserver::threadLoop()
{
    if (mExitThread) {
        ITRACE("exiting hotplug event thread");
        return false;
    }

    if (mHotplugControl->waitForEvent()) {
        mDisplayDevice.onHotplug();
    }
    return true;
}

} // namespace intel
} // namespace android
