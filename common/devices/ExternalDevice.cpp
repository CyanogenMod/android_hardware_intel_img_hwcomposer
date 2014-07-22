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
      mHotplugControl(NULL),
      mHdcpControl(NULL),
      mHotplugObserver(),
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

    mHotplugControl = createHotplugControl();
    if (!mHotplugControl) {
        DEINIT_AND_RETURN_FALSE("failed to create hotplug control");
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
    mHotplugObserver = new HotplugEventObserver(*this, *mHotplugControl);
    if (!mHotplugObserver.get()) {
        DEINIT_AND_RETURN_FALSE("failed to create hotplug observer");
    }
    return true;
}

void ExternalDevice::deinitialize()
{
    // destroy hotplug event observer
    if (mHotplugObserver.get()) {
        mHotplugObserver->requestExit();
        mHotplugObserver = 0;
    }

    // destroy hotplug control
    if (mHotplugControl) {
        delete mHotplugControl;
        mHotplugControl = 0;
    }

    if (mHdcpControl) {
        mHdcpControl->stopHdcp();
        delete mHdcpControl;
        mHdcpControl = 0;
    }

    mHotplugEventPending = false;
    PhysicalDevice::deinitialize();
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
        ITRACE("HDCP authentication status %d, sending hotplug event...", success);
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

    // detect display configs
    ret = detectDisplayConfigs();
    if (ret == false) {
        DTRACE("failed to detect display config");
        return;
    }

    {   // lock scope
        Mutex::Autolock _l(mLock);
        // delete device layer list
        if (!mConnected && mLayerList){
            delete mLayerList;
            mLayerList = 0;
        }
    }

    ITRACE("hotpug event: %d", mConnected);

    if (mConnected == false) {
        mHotplugEventPending = false;
        mHdcpControl->stopHdcp();
        mHwc.hotplug(mType, mConnected);
    } else {
        ITRACE("start HDCP asynchronously...");
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
