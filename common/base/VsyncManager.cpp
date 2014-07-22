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
    mInitialized = false;
}

bool VsyncManager::handleVsyncControl(int disp, bool enabled)
{
    Mutex::Autolock l(mLock);

    if (!mEnableDynamicVsync) {
        IDisplayDevice *device = mDevices.itemAt(disp);
        if (device) {
            bool ret = device->vsyncControl(enabled);
            if (ret) {
                // useless to track mEnabled if vsync is controlled on a secondary display
                mEnabled = enabled;
            }
            return ret;
        } else {
            ETRACE("invalid display %d", disp);
            return false;
        }
    }

    if (disp != IDisplayDevice::DEVICE_PRIMARY) {
        WTRACE("vsync control on non-primary device %d", disp);
        return false;
    }

    if (mEnabled == enabled) {
        WTRACE("vsync state %d is not changed", enabled);
        return true;
    }

    bool ret;
    if (!enabled) {
        ret = disableVsync();
    } else {
        ret = enableVsync();
    }

    if (ret) {
        // commit the enabling state
        mEnabled = enabled;
    }
    return ret;
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

    // When HDMI is connected, HDMI vsync should be the drive source for screen refresh
    // as it is the main focus.
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

    disableVsync();
    enableVsync();
}

int VsyncManager::getVsyncSource()
{
    return mVsyncSource;
}

void VsyncManager::enableDynamicVsync(bool enable)
{
    Mutex::Autolock l(mLock);
    mEnableDynamicVsync = enable;

    if (!mEnabled) {
        ITRACE("has been disabled");
        mVsyncSource = IDisplayDevice::DEVICE_COUNT;
        return;
    }

    disableVsync();
    enableVsync();
}

bool VsyncManager::enableVsync()
{
    // TODO: extension for WiDi, no vsync control from WiDi yet.
    if (mVsyncSource != IDisplayDevice::DEVICE_COUNT) {
        ETRACE("vsync has been enabled. %d", mVsyncSource);
        return false;
    }

    IDisplayDevice *device = mDevices.itemAt(IDisplayDevice::DEVICE_EXTERNAL);
    if (mEnableDynamicVsync && device && device->isConnected()) {
        // When HDMI is connected, HDMI vsync should be the drive source for screen refresh
        // as it is the main focus.
        if (device->vsyncControl(true)) {
            mVsyncSource = IDisplayDevice::DEVICE_EXTERNAL;
            return true;
        }
        WTRACE("failed to enable vsync on the external display");
        // fall through to enable vsync on the primary display
    }

    device = mDevices.itemAt(IDisplayDevice::DEVICE_PRIMARY);
    if (device) {
        if (device->vsyncControl(true)) {
            mVsyncSource = IDisplayDevice::DEVICE_PRIMARY;
            return true;
        }
        ETRACE("failed to enable vsync on the primary display");
        return false;
    }

    return false;
}

bool VsyncManager::disableVsync()
{
    if (mVsyncSource == IDisplayDevice::DEVICE_COUNT) {
        ETRACE("vsync has been disabled");
        return false;
    }

    IDisplayDevice *device = mDevices.itemAt(mVsyncSource);
    if (device) {
        if (device->vsyncControl(false)) {
            mVsyncSource = IDisplayDevice::DEVICE_COUNT;
            return true;
        } else {
            ETRACE("failed to disable vsync on device %d", mVsyncSource);
            return false;
        }
    }
    return false;
}

} // namespace intel
} // namespace android

