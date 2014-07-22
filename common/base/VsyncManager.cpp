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
      mInitialized(false),
      mEnableDynamicVsync(true),
      mEnabled(false),
      mVsyncSource(IDisplayDevice::DEVICE_COUNT),
      mLock()
{
}

VsyncManager::~VsyncManager()
{
    WARN_IF_NOT_DEINIT();
}

bool VsyncManager::initialize()
{
    mEnabled = false;
    mVsyncSource = IDisplayDevice::DEVICE_COUNT;
    mEnableDynamicVsync = !scUsePrimaryVsyncOnly;
    mInitialized = true;
    return true;
}

void VsyncManager::deinitialize()
{
    if (mEnabled) {
        WTRACE("vsync is still enabled");
    }

    mVsyncSource = IDisplayDevice::DEVICE_COUNT;
    mEnabled = false;
    mEnableDynamicVsync = !scUsePrimaryVsyncOnly;
    mInitialized = false;
}

bool VsyncManager::handleVsyncControl(int disp, bool enabled)
{
    Mutex::Autolock l(mLock);

    if (disp != IDisplayDevice::DEVICE_PRIMARY) {
        WTRACE("vsync control on non-primary device %d", disp);
        return false;
    }

    if (mEnabled == enabled) {
        WTRACE("vsync state %d is not changed", enabled);
        return true;
    }

    if (!enabled) {
        disableVsync();
        mEnabled = false;
        return true;
    } else {
        mEnabled = enableVsync(getCandidate());
        return mEnabled;
    }

    return false;
}

void VsyncManager::resetVsyncSource()
{
    Mutex::Autolock l(mLock);

    if (!mEnableDynamicVsync) {
        ITRACE("dynamic vsync source switch is not supported");
        return;
    }

    if (!mEnabled) {
        return;
    }

    int vsyncSource = getCandidate();
    if (vsyncSource == mVsyncSource) {
        return;
    }

    disableVsync();
    enableVsync(vsyncSource);
}

int VsyncManager::getVsyncSource()
{
    return mVsyncSource;
}

void VsyncManager::enableDynamicVsync(bool enable)
{
    Mutex::Autolock l(mLock);
    if (scUsePrimaryVsyncOnly) {
        WTRACE("dynamic vsync is not supported");
        return;
    }

    mEnableDynamicVsync = enable;

    if (!mEnabled) {
        return;
    }

    int vsyncSource = getCandidate();
    if (vsyncSource == mVsyncSource) {
        return;
    }

    disableVsync();
    enableVsync(vsyncSource);
}

int VsyncManager::getCandidate()
{
    if (!mEnableDynamicVsync) {
        return IDisplayDevice::DEVICE_PRIMARY;
    }

    IDisplayDevice *device = NULL;
    // use HDMI vsync when connected
    device = mDevices.itemAt(IDisplayDevice::DEVICE_EXTERNAL);
    if (device && device->isConnected()) {
        return IDisplayDevice::DEVICE_EXTERNAL;
    }

#ifdef INTEL_WIDI_MERRIFIELD
    // use vsync from virtual display when video extended mode is entered
    if (Hwcomposer::getInstance().getDisplayAnalyzer()->isVideoExtModeActive()) {
        device = mDevices.itemAt(IDisplayDevice::DEVICE_VIRTUAL);
        if (device && device->isConnected()) {
            return IDisplayDevice::DEVICE_VIRTUAL;
        }
        WTRACE("Could not use vsync from secondary device");
    }
#endif
    return IDisplayDevice::DEVICE_PRIMARY;
}

bool VsyncManager::enableVsync(int candidate)
{
    if (mVsyncSource != IDisplayDevice::DEVICE_COUNT) {
        WTRACE("vsync has been enabled on %d", mVsyncSource);
        return true;
    }

    IDisplayDevice *device = mDevices.itemAt(candidate);
    if (!device) {
        ETRACE("invalid vsync source candidate %d", candidate);
        return false;
    }

    if (device->vsyncControl(true)) {
        mVsyncSource = candidate;
        return true;
    }

    if (candidate != IDisplayDevice::DEVICE_PRIMARY) {
        WTRACE("failed to enable vsync on display %d, fall back to primary", candidate);
        device = mDevices.itemAt(IDisplayDevice::DEVICE_PRIMARY);
        if (device && device->vsyncControl(true)) {
            mVsyncSource = IDisplayDevice::DEVICE_PRIMARY;
            return true;
        }
    }
    ETRACE("failed to enable vsync on the primary display");
    return false;
}

void VsyncManager::disableVsync()
{
    if (mVsyncSource == IDisplayDevice::DEVICE_COUNT) {
        WTRACE("vsync has been disabled");
        return;
    }

    IDisplayDevice *device = mDevices.itemAt(mVsyncSource);
    if (device && !device->vsyncControl(false)) {
        WTRACE("failed to disable vsync on device %d", mVsyncSource);
    }
    mVsyncSource = IDisplayDevice::DEVICE_COUNT;
}

} // namespace intel
} // namespace android

