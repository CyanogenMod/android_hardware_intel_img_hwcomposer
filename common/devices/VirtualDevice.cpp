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
#include <cutils/log.h>

#include <Drm.h>
#include <Hwcomposer.h>
#include <DisplayPlaneManager.h>
#include <VirtualDevice.h>
#include <HwcUtils.h>


namespace android {
namespace intel {

VirtualDevice::VirtualDevice(Hwcomposer& hwc, DisplayPlaneManager& dpm)
    : mInitialized(false),
      mHwc(hwc),
      mDisplayPlaneManager(dpm)
{
    LOGV("Entering %s", __func__);
}

VirtualDevice::~VirtualDevice()
{
    LOGV("Entering %s", __func__);
    deinitialize();
}

bool VirtualDevice::prePrepare(hwc_display_contents_1_t *display)
{
    LOGV("Entering %s", __func__);
    INIT_CHECK();
    return true;
}

bool VirtualDevice::prepare(hwc_display_contents_1_t *display)
{
    LOGV("Entering %s", __func__);
    INIT_CHECK();
    return true;
}

bool VirtualDevice::commit(hwc_display_contents_1_t *display,
                      void* context,
                      int& count)
{
    LOGV("Entering %s", __func__);
    INIT_CHECK();
    return true;
}

bool VirtualDevice::vsyncControl(int enabled)
{
    LOGV("Entering %s", __func__);
    INIT_CHECK();
    return true;
}

bool VirtualDevice::blank(int blank)
{
    LOGV("Entering %s", __func__);
    INIT_CHECK();
    return true;
}

bool VirtualDevice::getDisplayConfigs(uint32_t *configs,
                                         size_t *numConfigs)
{
    LOGV("Entering %s", __func__);
    INIT_CHECK();
    if (!configs || !numConfigs) {
        LOGE("%s: invalid parameters", __func__);
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
    LOGV("Entering %s", __func__);
    INIT_CHECK();

    if (!attributes || !values) {
        LOGE("%s: invalid parameters", __func__);
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
            LOGE("%s: unknown attribute %d", __func__, attributes[i]);
            break;
        }
        i++;
    }

    return true;
}

bool VirtualDevice::compositionComplete()
{
    LOGV("Entering %s", __func__);
    INIT_CHECK();
    return true;
}

bool VirtualDevice::initialize()
{
    // Add initialization codes here. If init fails, invoke DEINIT_AND_RETURN_FALSE();
    mInitialized = true;

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
