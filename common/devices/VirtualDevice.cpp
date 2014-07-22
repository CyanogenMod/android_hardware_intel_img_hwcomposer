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
#include <DisplayPlaneManager.h>
#include <VirtualDevice.h>

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

namespace android {
namespace intel {

VirtualDevice::VirtualDevice(Hwcomposer& hwc, DisplayPlaneManager& dpm)
    : mInitialized(false),
      mHwc(hwc),
      mDisplayPlaneManager(dpm)
{
    CTRACE();
}

VirtualDevice::~VirtualDevice()
{
    CTRACE();
    deinitialize();
}

status_t VirtualDevice::start(sp<IFrameTypeChangeListener> typeChangeListener)
{
    CTRACE();
    return NO_ERROR;
}

status_t VirtualDevice::stop(bool isConnected)
{
    CTRACE();
    return NO_ERROR;
}

status_t VirtualDevice::notifyBufferReturned(int khandle)
{
    CTRACE();
    return NO_ERROR;
}

bool VirtualDevice::prePrepare(hwc_display_contents_1_t *display)
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool VirtualDevice::prepare(hwc_display_contents_1_t *display)
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool VirtualDevice::commit(hwc_display_contents_1_t *display, IDisplayContext *context)
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool VirtualDevice::vsyncControl(int enabled)
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool VirtualDevice::blank(int blank)
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool VirtualDevice::getDisplayConfigs(uint32_t *configs,
                                         size_t *numConfigs)
{
    RETURN_FALSE_IF_NOT_INIT();
    if (!configs || !numConfigs) {
        ETRACE("invalid parameters");
        return false;
    }

    *configs = 0;
    *numConfigs = 1;

    return true;
}

bool VirtualDevice::getDisplayAttributes(uint32_t configs,
                                            const uint32_t *attributes,
                                            int32_t *values)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (!attributes || !values) {
        ETRACE("invalid parameters");
        return false;
    }

    int i = 0;
    while (attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE) {
        switch (attributes[i]) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            values[i] = 1e9 / 60;
            break;
        case HWC_DISPLAY_WIDTH:
            values[i] = 1280;
            break;
        case HWC_DISPLAY_HEIGHT:
            values[i] = 720;
            break;
        case HWC_DISPLAY_DPI_X:
            values[i] = 0;
            break;
        case HWC_DISPLAY_DPI_Y:
            values[i] = 0;
            break;
        default:
            ETRACE("unknown attribute %d", attributes[i]);
            break;
        }
        i++;
    }

    return true;
}

bool VirtualDevice::compositionComplete()
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool VirtualDevice::initialize()
{
    // Add initialization codes here. If init fails, invoke DEINIT_AND_RETURN_FALSE();
    mInitialized = true;
    // Publish frame server service with service manager
    status_t ret = defaultServiceManager()->addService(String16("hwc.widi"), this);
    if (ret != NO_ERROR) {
        ETRACE("Could not register hwc.widi with service manager, error = %d", ret);
        mInitialized = false;
        return false;
    }
    ProcessState::self()->startThreadPool();
    return true;
}

bool VirtualDevice::isConnected() const
{
    return true;
}

const char* VirtualDevice::getName() const
{
    return "Virtual";
}

int VirtualDevice::getType() const
{
    return DEVICE_VIRTUAL;
}

void VirtualDevice::dump(Dump& d)
{
}

void VirtualDevice::deinitialize()
{
    mInitialized = false;
}

} // namespace intel
} // namespace android
