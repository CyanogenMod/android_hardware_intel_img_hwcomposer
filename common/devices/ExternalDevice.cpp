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
#include <Drm.h>
#include <Hwcomposer.h>
#include <ExternalDevice.h>

namespace android {
namespace intel {

ExternalDevice::ExternalDevice(Hwcomposer& hwc, DisplayPlaneManager& dpm)
    : PhysicalDevice(DEVICE_EXTERNAL, hwc, dpm),
      mHdcpControl(NULL),
      mHotplugObserver(NULL),
      mAbortModeSettingCond(),
      mPendingDrmMode(),
      mHotplugEventPending(false)
{
    CTRACE();
}

ExternalDevice::~ExternalDevice()
{
    CTRACE();
}

bool ExternalDevice::initialize()
{
    if (!PhysicalDevice::initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to initialize physical device");
    }

    mHdcpControl = createHdcpControl();
    if (!mHdcpControl) {
        DEINIT_AND_RETURN_FALSE("failed to create HDCP control");
    }

    mHotplugEventPending = false;
    if (mConnected) {
        mHdcpControl->startHdcpAsync(HdcpLinkStatusListener, this);
    }

    // create hotplug observer
    mHotplugObserver = new HotplugEventObserver(*this);
    if (!mHotplugObserver || !mHotplugObserver->initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to create hotplug observer");
    }
    return true;
}

void ExternalDevice::deinitialize()
{
    // abort mode settings if it is in the middle
    mAbortModeSettingCond.signal();
    if (mThread.get()) {
        mThread->join();
        mThread = NULL;
    }

    DEINIT_AND_DELETE_OBJ(mHotplugObserver);

    if (mHdcpControl) {
        mHdcpControl->stopHdcp();
        delete mHdcpControl;
        mHdcpControl = 0;
    }

    mHotplugEventPending = false;
    PhysicalDevice::deinitialize();
}

bool ExternalDevice::setDrmMode(drmModeModeInfo& value)
{
    if (!mConnected) {
        WTRACE("external device is not connected");
        return false;
    }

    if (mThread.get()) {
        mThread->join();
        mThread = NULL;
    }

    Drm *drm = Hwcomposer::getInstance().getDrm();
    drmModeModeInfo mode;
    drm->getModeInfo(mType, mode);
    if (drm->isSameDrmMode(&value, &mode))
        return true;

    // any issue here by faking connection status?
    mConnected = false;
    mPendingDrmMode = value;

    // setting mode in a working thread
    mThread = new ModeSettingThread(this);
    if (!mThread.get()) {
        ETRACE("failed to create mode settings thread");
        return false;
    }

    mThread->run("ModeSettingsThread", PRIORITY_URGENT_DISPLAY);
    return true;
}

bool ExternalDevice::threadLoop()
{
    // one-time execution
    setDrmMode();
    return false;
}

void ExternalDevice::setDrmMode()
{
    ITRACE("start mode setting...");

    Drm *drm = Hwcomposer::getInstance().getDrm();

    mConnected = false;
    mHwc.hotplug(mType, false);

    {
        Mutex::Autolock lock(mLock);
        // TODO: make timeout value flexible, or wait until surface flinger
        // acknowledges hot unplug event.
        status_t err = mAbortModeSettingCond.waitRelative(mLock, milliseconds(20));
        if (err != -ETIMEDOUT) {
            ITRACE("Mode settings is interrupted");
            return;
        }
    }

    // TODO: potential threading issue with onHotplug callback
    mHdcpControl->stopHdcp();
    if (!drm->setDrmMode(mType, mPendingDrmMode)) {
        ETRACE("failed to set Drm mode");
        return;
    }

    if (!PhysicalDevice::updateDisplayConfigs()) {
        ETRACE("failed to update display configs");
        return;
    }
    mConnected = true;
    mHotplugEventPending = true;
    // delay sending hotplug event until HDCP is authenticated
    mHdcpControl->startHdcpAsync(HdcpLinkStatusListener, this);
}


void ExternalDevice::HdcpLinkStatusListener(bool success, void *userData)
{
    if (userData == NULL) {
        return;
    }

    ExternalDevice *p = (ExternalDevice*)userData;
    p->HdcpLinkStatusListener(success);
}

void ExternalDevice::HdcpLinkStatusListener(bool success)
{
    // TODO:  send hotplug event only if HDCP is authenticated?
    if (mHotplugEventPending) {
        DTRACE("HDCP authentication status %d, sending hotplug event...", success);
        mHwc.hotplug(mType, mConnected);
        mHotplugEventPending = false;
    }

    if (!success) {
        ETRACE("HDCP is not authenticated");
    }
}

void ExternalDevice::onHotplug()
{
    bool ret;

    CTRACE();

    // abort mode settings if it is in the middle
    mAbortModeSettingCond.signal();

    // remember the current connection status before detection
    bool connected = mConnected;

    // detect display configs
    ret = detectDisplayConfigs();
    if (ret == false) {
        ETRACE("failed to detect display config");
        return;
    }

    ITRACE("hotpug event: %d", mConnected);

    if (connected == mConnected) {
        WTRACE("same connection status detected, hotplug event ignored");
        return;
    }

    if (mConnected == false) {
        mHotplugEventPending = false;
        mHdcpControl->stopHdcp();
        mHwc.hotplug(mType, mConnected);
    } else {
        DTRACE("start HDCP asynchronously...");
         // delay sending hotplug event till HDCP is authenticated.
        mHotplugEventPending = true;
        ret = mHdcpControl->startHdcpAsync(HdcpLinkStatusListener, this);
        if (ret == false) {
            ETRACE("failed to start HDCP");
            mHotplugEventPending = false;
            mHwc.hotplug(mType, mConnected);
        }
    }
}


} // namespace intel
} // namespace android
