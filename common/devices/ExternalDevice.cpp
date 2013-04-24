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
    : PhysicalDevice(DEVICE_EXTERNAL, hwc, dpm)
{
    CTRACE();
}

ExternalDevice::~ExternalDevice()
{
    CTRACE();
    deinitialize();
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

    PhysicalDevice::deinitialize();
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

    // TODO: HDCP authentication

    {   // lock scope
        Mutex::Autolock _l(mLock);
        // delete device layer list
        if (!mConnection && mLayerList){
            delete mLayerList;
            mLayerList = 0;
        }
    }

    // notify hwcomposer
    mHwc.hotplug(mType, mConnection);
}


} // namespace intel
} // namespace android
