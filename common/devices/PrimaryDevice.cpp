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
#include <DrmConfig.h>
#include <PrimaryDevice.h>

namespace android {
namespace intel {

PrimaryDevice::PrimaryDevice(Hwcomposer& hwc, DisplayPlaneManager& dpm)
    : PhysicalDevice(DEVICE_PRIMARY, hwc, dpm)
{
    CTRACE();
}

PrimaryDevice::~PrimaryDevice()
{
    CTRACE();
}

bool PrimaryDevice::initialize()
{
    if (!PhysicalDevice::initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to initialize physical device");
    }

    UeventObserver *observer = Hwcomposer::getInstance().getUeventObserver();
    if (observer) {
        observer->registerListener(
            DrmConfig::getRepeatedFrameString(),
            repeatedFrameEventListener,
            this);
    } else {
        ETRACE("Uevent observer is NULL");
    }

    return true;
}

void PrimaryDevice::deinitialize()
{
    PhysicalDevice::deinitialize();
}


void PrimaryDevice::repeatedFrameEventListener(void *data)
{
    PrimaryDevice *pThis = (PrimaryDevice*)data;
    if (pThis) {
        pThis->repeatedFrameListener();
    }
}

void PrimaryDevice::repeatedFrameListener()
{
    Hwcomposer::getInstance().getDisplayAnalyzer()->postIdleEntryEvent();
    Hwcomposer::getInstance().invalidate();
}

bool PrimaryDevice::blank(bool blank)
{
    if (!mConnected)
        return true;

    // control of repeated frames
    IPowerManager *pm = Hwcomposer::getInstance().getPowerManager();
    if (!blank) {
        pm->enableIdleControl();
    } else {
        pm->disableIdleControl();
    }

    return PhysicalDevice::blank(blank);
}

} // namespace intel
} // namespace android
