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
#include <common/utils/HwcTrace.h>
#include <IDisplayDevice.h>
#include <common/base/Drm.h>
#include <DrmConfig.h>


namespace android {
namespace intel {

const char* DrmConfig::getDrmPath()
{
    return "/dev/card0";
}

uint32_t DrmConfig::getDrmConnector(int device)
{
    if (device == IDisplayDevice::DEVICE_PRIMARY)
        return DRM_MODE_CONNECTOR_MIPI;
    else if (device == IDisplayDevice::DEVICE_EXTERNAL)
        return DRM_MODE_CONNECTOR_DVID;
    return DRM_MODE_CONNECTOR_Unknown;
}

uint32_t DrmConfig::getDrmEncoder(int device)
{
    if (device == IDisplayDevice::DEVICE_PRIMARY)
        return DRM_MODE_ENCODER_MIPI;
    else if (device == IDisplayDevice::DEVICE_EXTERNAL)
        return DRM_MODE_ENCODER_TMDS;
    return DRM_MODE_ENCODER_NONE;
}

uint32_t DrmConfig::getFrameBufferFormat()
{
    return HAL_PIXEL_FORMAT_RGBX_8888;
}

uint32_t DrmConfig::getFrameBufferDepth()
{
    return 24;
}

uint32_t DrmConfig::getFrameBufferBpp()
{
    return 32;
}

const char* DrmConfig::getUeventEnvelope()
{
    return "change@/devices/pci0000:00/0000:00:02.0/drm/card0";
}

const char* DrmConfig::getHotplugString()
{
    return "HOTPLUG=1";
}

const char* DrmConfig::getRepeatedFrameString()
{
    return "REPEATED_FRAME";
}

} // namespace intel
} // namespace android
