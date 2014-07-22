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
#include <hal_public.h>

#include <PlatfHwcomposer.h>
#include <PlatfDisplayPlaneManager.h>
#include <PlatfBufferManager.h>
#include <IDisplayDevice.h>
#include <PlatfPrimaryDevice.h>
#include <PlatfExternalDevice.h>
#include <PlatfVirtualDevice.h>


namespace android {
namespace intel {

PlatfHwcomposer::PlatfHwcomposer()
    : Hwcomposer(),
      mFBDev(0)
{

}

PlatfHwcomposer::~PlatfHwcomposer()
{

}

bool PlatfHwcomposer::initialize()
{
    int err;

    LOGV("PlatfHwcomposer::initialize");

    // open frame buffer device
    hw_module_t const* module;
    err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err) {
        LOGE("PlatfHwcomposer::initialize: failed to load gralloc module, %d",
              err);
        return false;
    }

    // open frame buffer device
    err = framebuffer_open(module, (framebuffer_device_t**)&mFBDev);
    if (err) {
        LOGE("PlatfHwcomposer::initialize: failed to open frame buffer device, %d",
              err);
        return false;
    }

    mFBDev->bBypassPost = 1;

    if (!Hwcomposer::initialize()) {
        LOGE("PlatfHwcomposer::initialize: failed to call initialize");
        // close frame buffer device
        framebuffer_close((framebuffer_device_t*)mFBDev);
        return false;
    }

    return true;
}

bool PlatfHwcomposer::compositionComplete(int disp)
{
    // complete fb device
    if (mFBDev) {
        mFBDev->base.compositionComplete(&mFBDev->base);
    }

    return Hwcomposer::compositionComplete(disp);
}

void* PlatfHwcomposer::getContexts()
{
    LOGV("PlatfHwcomposer::getContexts");
    return (void *)mImgLayers;
}

bool PlatfHwcomposer::commitContexts(void *contexts, int count)
{
    LOGV("PlatfHwcomposer::commitContexts: contexts = 0x%x, count = %d",
          contexts, count);

    // nothing need to be submitted
    if (!count)
        return true;

    if (!contexts) {
        LOGE("PlatfHwcomposer::commitContexts: invalid parameters");
        return false;
    }

    if (mFBDev) {
        int err = mFBDev->Post2(&mFBDev->base, mImgLayers, count);
        if (err) {
            LOGE("PlatfHwcomposer::commitContexts: Post2 failed err = %d", err);
            return false;
        }
    }

    return true;
}

// implement createDisplayPlaneManager()
DisplayPlaneManager* PlatfHwcomposer::createDisplayPlaneManager()
{
    LOGV("PlatfHwcomposer::createDisplayPlaneManager");
    return (new PlatfDisplayPlaneManager());
}

BufferManager* PlatfHwcomposer::createBufferManager()
{
    LOGV("PlatfHwcomposer::createBufferManager");
    return (new PlatfBufferManager());
}

IDisplayDevice* PlatfHwcomposer::createDisplayDevice(int disp,
                                                     DisplayPlaneManager& dpm)
{
    LOGV("PlatfHwcomposer::createDisplayDevice");

    switch (disp) {
        case IDisplayDevice::DEVICE_PRIMARY:
            return new PlatfPrimaryDevice(*this, dpm);
        case IDisplayDevice::DEVICE_EXTERNAL:
            return new PlatfExternalDevice(*this, dpm);
        case IDisplayDevice::DEVICE_VIRTUAL:
            return new PlatfVirtualDevice(*this, dpm);
        default:
            LOGE("%s: Invalid display device %d", __func__, disp);
            return NULL;
    }
}

Hwcomposer* Hwcomposer::createHwcomposer()
{
    return new PlatfHwcomposer();
}

const char* Hwcomposer::getDrmPath()
{
    return "/dev/card0";
}

uint32_t Hwcomposer::getDrmConnector(int32_t output)
{
    if (output == Drm::OUTPUT_PRIMARY)
        return DRM_MODE_CONNECTOR_MIPI;
    else if (output == Drm::OUTPUT_EXTERNAL)
        return DRM_MODE_CONNECTOR_DVID;

    return DRM_MODE_CONNECTOR_Unknown;
}

} //namespace intel
} //namespace android
