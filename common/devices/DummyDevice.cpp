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
 *    Lei Zhang <lei.zhang@intel.com>
 *
 */
#include <common/utils/HwcTrace.h>
#include <Hwcomposer.h>
#include <DisplayQuery.h>
#include <common/observers/SoftVsyncObserver.h>
#include <DummyDevice.h>

namespace android {
namespace intel {
DummyDevice::DummyDevice(Hwcomposer& hwc)
    : mInitialized(false),
      mConnected(false),
      mBlank(false),
      mHwc(hwc),
      mVsyncObserver(NULL),
      mName("Dummy")
{
    CTRACE();
}

DummyDevice::~DummyDevice()
{
    WARN_IF_NOT_DEINIT();
}

bool DummyDevice::prePrepare(hwc_display_contents_1_t *display)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (!display) {
        return true;
    }

    // nothing need to do for dummy display
    return true;
}

bool DummyDevice::prepare(hwc_display_contents_1_t *display)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (!display) {
        return true;
    }

    // skip all layers composition on dummy display
    if (display->flags & HWC_GEOMETRY_CHANGED) {
        for (size_t i=0; i < display->numHwLayers-1; i++) {
            hwc_layer_1 * player = &display->hwLayers[i];
            player->compositionType = HWC_OVERLAY;
            player->flags &= ~HWC_SKIP_LAYER;
        }
    }

    return true;
}

bool DummyDevice::commit(hwc_display_contents_1_t *display, IDisplayContext *context)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (!display || !context)
        return true;

    // nothing need to do for dummy display
    return true;
}

bool DummyDevice::vsyncControl(bool enabled)
{
    RETURN_FALSE_IF_NOT_INIT();
    return mVsyncObserver->control(enabled);
}

bool DummyDevice::blank(bool blank)
{
    RETURN_FALSE_IF_NOT_INIT();

    mBlank = blank;

    return true;
}

bool DummyDevice::getDisplaySize(int *width, int *height)
{
    RETURN_FALSE_IF_NOT_INIT();
    if (!width || !height) {
        ETRACE("invalid parameters");
        return false;
    }

    // TODO: make this platform specifc
    *width = 1280;//720;
    *height = 720;//1280;
    return true;
}

bool DummyDevice::getDisplayConfigs(uint32_t *configs,
                                         size_t *numConfigs)
{
    RETURN_FALSE_IF_NOT_INIT();
    if (!configs || !numConfigs) {
        ETRACE("invalid parameters");
        return false;
    }

    if (!mConnected) {
        ITRACE("dummy device is not connected");
        return false;
    }

    *configs = 0;
    *numConfigs = 1;

    return true;
}

bool DummyDevice::getDisplayAttributes(uint32_t configs,
                                            const uint32_t *attributes,
                                            int32_t *values)
{
    RETURN_FALSE_IF_NOT_INIT();

    if ((configs > 0) || !attributes || !values) {
        ETRACE("invalid parameters");
        return false;
    }

    if (!mConnected) {
        ITRACE("dummy device is not connected");
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

bool DummyDevice::compositionComplete()
{
    RETURN_FALSE_IF_NOT_INIT();
    return true;
}

bool DummyDevice::initialize()
{
    mInitialized = true;

    mVsyncObserver = new SoftVsyncObserver(*this);
    if (!mVsyncObserver || !mVsyncObserver->initialize()) {
        DEINIT_AND_RETURN_FALSE("Failed to create Soft Vsync Observer");
        mInitialized = false;
    }

    return mInitialized;
}

bool DummyDevice::isConnected() const
{
    return mConnected;
}

const char* DummyDevice::getName() const
{
    return "Dummy";
}

int DummyDevice::getType() const
{
    return DEVICE_DUMMY;
}

void DummyDevice::onVsync(int64_t timestamp)
{
    mHwc.vsync(DEVICE_DUMMY, timestamp);
}

void DummyDevice::dump(Dump& d)
{
    d.append("-------------------------------------------------------------\n");
    d.append("Device Name: %s (%s)\n", mName,
            mConnected ? "connected" : "disconnected");
}

void DummyDevice::deinitialize()
{
    DEINIT_AND_DELETE_OBJ(mVsyncObserver);
    mInitialized = false;
}

} // namespace intel
} // namespace android
