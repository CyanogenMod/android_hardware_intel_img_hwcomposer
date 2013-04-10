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
#include <tangier/TngDisplayContext.h>
#include <HwcUtils.h>
#include <DisplayPlane.h>
#include <IDisplayDevice.h>
#include <HwcLayerList.h>

namespace android {
namespace intel {

TngDisplayContext::TngDisplayContext()
    : mFBDev(NULL),
      mInitialized(false),
      mCount(0)
{
    LOGV("Entering %s", __func__);
}

TngDisplayContext::~TngDisplayContext()
{
    LOGV("Entering %s", __func__);
    deinitialize();
}

bool TngDisplayContext::initialize()
{
    LOGV("Entering %s", __func__);

    // open frame buffer device
    hw_module_t const* module;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err) {
        LOGE("%s: failed to load gralloc module, %d", __func__, err);
        return false;
    }

    // open frame buffer device
    err = framebuffer_open(module, (framebuffer_device_t**)&mFBDev);
    if (err) {
        LOGE("%s: failed to open frame buffer device, %d", __func__, err);
        return false;
    }

    mFBDev->bBypassPost = 1;
    mCount = 0;
    mInitialized = true;
    return true;
}

bool TngDisplayContext::commitBegin()
{
    INIT_CHECK();
    mCount = 0;
    return true;
}

bool TngDisplayContext::commitContents(hwc_display_contents_1_t *display, HwcLayerList *layerList)
{
    bool ret;

    LOGV("Entering %s", __func__);
    INIT_CHECK();

    if (!display || !layerList) {
        LOGE("%s: invalid parameters", __func__);
        return false;
    }

    IMG_hwc_layer_t *imgLayerList = (IMG_hwc_layer_t*)mImgLayers;

    for (size_t i = 0; i < display->numHwLayers; i++) {
        if (mCount >= MAXIMUM_LAYER_NUMBER) {
            LOGE("%s: layer count exceeds the limit.", __func__);
            return false;
        }

        // check layer parameters
        if (!display->hwLayers[i].handle)
            continue;

        DisplayPlane* plane = layerList->getPlane(i);
        if (!plane)
            continue;

        ret = plane->flip();
        if (ret == false) {
            LOGW("%s: failed to flip plane %d", __func__, i);
            continue;
        }

        IMG_hwc_layer_t *imgLayer = &imgLayerList[mCount++];
        // update IMG layer
        imgLayer->handle = display->hwLayers[i].handle;
        imgLayer->transform = display->hwLayers[i].transform;
        imgLayer->blending = display->hwLayers[i].blending;
        imgLayer->sourceCrop = display->hwLayers[i].sourceCrop;
        imgLayer->displayFrame = display->hwLayers[i].displayFrame;
        imgLayer->custom = (uint32_t)plane->getContext();

        LOGV("%s: count %d, handle 0x%x, trans 0x%x, blending 0x%x"
              " sourceCrop %d,%d - %dx%d, dst %d,%d - %dx%d, custom 0x%x",
              __func__,
              mCount,
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

bool TngDisplayContext::commitEnd()
{
    LOGV("Entering %s: count = %d", __func__, mCount);

    // nothing need to be submitted
    if (!mCount)
        return true;

    if (mFBDev) {
        int err = mFBDev->Post2(&mFBDev->base, mImgLayers, mCount);
        if (err) {
            LOGE("%s: Post2 failed, err = %d", __func__, err);
            return false;
        }
    }

    return true;
}

bool TngDisplayContext::compositionComplete()
{
    if (mFBDev) {
        mFBDev->base.compositionComplete(&mFBDev->base);
    }
    return true;
}

void TngDisplayContext::deinitialize()
{
    if (mFBDev) {
        framebuffer_close((framebuffer_device_t*)mFBDev);
        mFBDev = NULL;
    }
    mCount = 0;
    mInitialized = false;
}


} // namespace intel
} // namespace android
