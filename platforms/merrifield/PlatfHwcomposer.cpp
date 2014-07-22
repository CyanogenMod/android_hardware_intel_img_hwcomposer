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

#include <tangier/TngDisplayContext.h>
#include <PlatfDisplayPlaneManager.h>
#include <PlatfBufferManager.h>
#include <IDisplayDevice.h>
#include <PlatfPrimaryDevice.h>
#include <PlatfExternalDevice.h>
#include <PlatfVirtualDevice.h>
#include <PlatfHwcomposer.h>



namespace android {
namespace intel {

PlatfHwcomposer::PlatfHwcomposer()
    : Hwcomposer()
{
    LOGV("Entering %s", __func__);
}

PlatfHwcomposer::~PlatfHwcomposer()
{
    LOGV("Entering %s", __func__);
}

DisplayPlaneManager* PlatfHwcomposer::createDisplayPlaneManager()
{
    LOGV("Entering %s", __func__);
    return (new PlatfDisplayPlaneManager());
}

BufferManager* PlatfHwcomposer::createBufferManager()
{
    LOGV("Entering %s", __func__);
    return (new PlatfBufferManager());
}

IDisplayDevice* PlatfHwcomposer::createDisplayDevice(int disp,
                                                     DisplayPlaneManager& dpm)
{
    LOGV("Entering %s", __func__);

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

IDisplayContext* PlatfHwcomposer::createDisplayContext()
{
    LOGV("Entering %s", __func__);
    return new TngDisplayContext();
}

Hwcomposer* Hwcomposer::createHwcomposer()
{
    LOGV("Entering %s", __func__);
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
