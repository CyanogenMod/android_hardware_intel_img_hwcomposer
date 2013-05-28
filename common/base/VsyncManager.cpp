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
#include <IDisplayDevice.h>
#include <DisplayQuery.h>
#include <BufferManager.h>
#include <DisplayPlaneManager.h>
#include <Hwcomposer.h>
#include <VsyncManager.h>


namespace android {
namespace intel {

VsyncManager::VsyncManager(Vector<IDisplayDevice*>& devices)
    : mDevices(devices),
      mMutex(),
      mInitialized(false),
      mEnabled(false),
      mVsyncSource(IDisplayDevice::DEVICE_COUNT)
{
}

VsyncManager::~VsyncManager()
{
}

bool VsyncManager::initialize()
{
    mEnabled = false;
    mVsyncSource = IDisplayDevice::DEVICE_COUNT;
    mInitialized = true;
    return true;
}

void VsyncManager::deinitialize()
{
    mEnabled = false;
    mVsyncSource = IDisplayDevice::DEVICE_COUNT;
    mInitialized = false;
}


bool VsyncManager::handleVsyncControl(int disp, int enabled)
{
    Mutex::Autolock lock(&mMutex);

    if (disp < 0 || disp >= IDisplayDevice::DEVICE_COUNT) {
        ETRACE("invalid disp %d", disp);
        return false;
    }

    if (disp != IDisplayDevice::DEVICE_PRIMARY) {
        WTRACE("Vsync control on non-primary device %d", disp);
        return true;
    }

    if (mEnabled == enabled) {
        return true;
    }

    mEnabled = enabled;

    bool ret = disableVsync();
    if (mEnabled) {
        ret = enableVsync();
    }
    return ret;
}

void VsyncManager::handleHotplugEvent(int disp, int connected)
{
    Mutex::Autolock lock(&mMutex);

    if (disp != IDisplayDevice::DEVICE_EXTERNAL) {
        WTRACE("hotplug event on non-external device %d", disp);
        return;
    }

    if (!mEnabled) {
        ITRACE("vsync is disabled");
        return;
    }

    // When HDMI is connected, HDMI vsync should be the drive source for screen refresh
    // as it is the main focus.
    if (connected) {
        static bool warnOnce = false;
        if (!warnOnce) {
            WTRACE("========================================================");
            WTRACE("vsync from external display will be the drive source");
            WTRACE("for screen refresh. However, when reporting vsync event");
            WTRACE("to surface flinger, primary display will be assumed as");
            WTRACE("vsync source, see Hwcomposer::vsync for more workaround");
            WTRACE("========================================================");
            warnOnce = true;
        }
    }
    disableVsync();
    enableVsync();
}

int VsyncManager::getVsyncSource()
{
    return mVsyncSource;
}

bool VsyncManager::enableVsync()
{
    // TODO: extension for WiDi, no vsync control from WiDi yet.

    IDisplayDevice *device = mDevices.itemAt(IDisplayDevice::DEVICE_EXTERNAL);
    if (device && device->isConnected()) {
        //ITRACE("enable vsync on external device");
        mVsyncSource = IDisplayDevice::DEVICE_EXTERNAL;
        return device->vsyncControl(1);
    }

    device = mDevices.itemAt(IDisplayDevice::DEVICE_PRIMARY);
    if (device) {
        mVsyncSource = IDisplayDevice::DEVICE_PRIMARY;
        return device->vsyncControl(1);
    }

    return false;
}

bool VsyncManager::disableVsync()
{
    if (mVsyncSource == IDisplayDevice::DEVICE_COUNT) {
        return true;
    }

    bool ret = true;
    IDisplayDevice *device = mDevices.itemAt(mVsyncSource);
    if (device) {
        ret = device->vsyncControl(0);
    }
    mVsyncSource = IDisplayDevice::DEVICE_COUNT;
    return ret;
}

} // namespace intel
} // namespace android

