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

#include <PlatfExternalDevice.h>
#include <common/VsyncControl.h>
#include <common/BlankControl.h>
#include <common/HotplugControl.h>
#include <HwcUtils.h>

namespace android {
namespace intel {

PlatfExternalDevice::PlatfExternalDevice(Hwcomposer& hwc,
                                       DisplayPlaneManager& dpm)
    : ExternalDevice(hwc, dpm)
{
    LOGV("Entering %s", __func__);
}

PlatfExternalDevice::~PlatfExternalDevice()
{
    LOGV("Entering %s", __func__);
}

bool PlatfExternalDevice::commit(hwc_display_contents_1_t *display,
                             void *contexts,
                             int& count)
{
    bool ret;

    LOGV("Entering %s", __func__);
    INIT_CHECK();

    if (!display || !contexts) {
        LOGE("%s: invalid parameters", __func__);
        return false;
    }

    Mutex::Autolock _l(mLock);

    IMG_hwc_layer_t *imgLayerList = (IMG_hwc_layer_t*)contexts;

    for (size_t i = 0; i < display->numHwLayers; i++) {
        // check layer parameters
        if (!display->hwLayers[i].handle)
            continue;

        DisplayPlane* plane = mLayerList->getPlane(i);
        if (!plane)
            continue;

        ret = plane->flip();
        if (ret == false) {
            LOGW("%s: failed to flip plane %d", __func__, i);
            continue;
        }

        IMG_hwc_layer_t *imgLayer = &imgLayerList[count++];
        // update IMG layer
        imgLayer->handle = display->hwLayers[i].handle;
        imgLayer->transform = display->hwLayers[i].transform;
        imgLayer->blending = display->hwLayers[i].blending;
        imgLayer->sourceCrop = display->hwLayers[i].sourceCrop;
        imgLayer->displayFrame = display->hwLayers[i].displayFrame;
        imgLayer->custom = (uint32_t)plane->getContext();

        LOGV("%s: type %d, count %d, handle 0x%x, trans 0x%x, blending 0x%x"
              " sourceCrop %d,%d - %dx%d, dst %d,%d - %dx%d, custom 0x%x",
              __func__,
              mType,
              count,
              imgLayer->handle,
              imgLayer->transform,
              imgLayer->blending,
              imgLayer->sourceCrop.left,
              imgLayer->sourceCrop.top,
              imgLayer->sourceCrop.right - imgLayer->sourceCrop.left,
              imgLayer->sourceCrop.bottom - imgLayer->sourceCrop.top,
              imgLayer->displayFrame.left,
              imgLayer->displayFrame.top,
              imgLayer->displayFrame.right - imgLayer->displayFrame.left,
              imgLayer->displayFrame.bottom - imgLayer->displayFrame.top,
              imgLayer->custom);
    }
    return true;
}

IVsyncControl* PlatfExternalDevice::createVsyncControl()
{
    return new VsyncControl();
}

IBlankControl* PlatfExternalDevice::createBlankControl()
{
    return new BlankControl();
}

IHotplugControl* PlatfExternalDevice::createHotplugControl()
{
    return new HotplugControl();
}

} // namespace intel
} // namespace android


