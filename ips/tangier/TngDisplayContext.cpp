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
#include <Hwcomposer.h>
#include <DisplayPlane.h>
#include <IDisplayDevice.h>
#include <HwcLayerList.h>
#include <tangier/TngDisplayContext.h>


namespace android {
namespace intel {

TngDisplayContext::TngDisplayContext()
    : mIMGDisplayDevice(0),
      mInitialized(false),
      mCount(0)
{
    CTRACE();
}

TngDisplayContext::~TngDisplayContext()
{
    WARN_IF_NOT_DEINIT();
}

bool TngDisplayContext::initialize()
{
    CTRACE();

    // open frame buffer device
    hw_module_t const* module;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err) {
        ETRACE("failed to load gralloc module, error = %d", err);
        return false;
    }

    // init IMG display device
    mIMGDisplayDevice = (((IMG_gralloc_module_public_t *)module)->getDisplayDevice((IMG_gralloc_module_public_t *)module));
    if (!mIMGDisplayDevice) {
        ETRACE("failed to get display device");
        return false;
    }

    mCount = 0;
    mInitialized = true;
    return true;
}

bool TngDisplayContext::commitBegin(size_t numDisplays, hwc_display_contents_1_t **displays)
{
    RETURN_FALSE_IF_NOT_INIT();
    mCount = 0;
    return true;
}

bool TngDisplayContext::commitContents(hwc_display_contents_1_t *display, HwcLayerList *layerList)
{
    bool ret;

    RETURN_FALSE_IF_NOT_INIT();

    if (!display || !layerList) {
        ETRACE("invalid parameters");
        return false;
    }

    IMG_hwc_layer_t *imgLayerList = (IMG_hwc_layer_t*)mImgLayers;

    for (size_t i = 0; i < display->numHwLayers; i++) {
        if (mCount >= MAXIMUM_LAYER_NUMBER) {
            ETRACE("layer count exceeds the limit");
            return false;
        }

        // check layer parameters
        if (!display->hwLayers[i].handle) {
            continue;
        }

        DisplayPlane* plane = layerList->getPlane(i);
        if (!plane) {
            continue;
        }

        ret = plane->flip(NULL);
        if (ret == false) {
            VTRACE("failed to flip plane %d", i);
            continue;
        }

        IMG_hwc_layer_t *imgLayer = &imgLayerList[mCount++];
        // update IMG layer
        imgLayer->psLayer = &display->hwLayers[i];
        imgLayer->custom = (uint32_t)plane->getContext();
        struct intel_dc_plane_ctx *ctx =
            (struct intel_dc_plane_ctx *)imgLayer->custom;
        // update z order
        Hwcomposer& hwc = Hwcomposer::getInstance();
        DisplayPlaneManager *pm = hwc.getPlaneManager();
        void *config = pm->getZOrderConfig();
        if (config) {
            memcpy(&ctx->zorder, config, sizeof(ctx->zorder));
        } else {
            memset(&ctx->zorder, 0, sizeof(ctx->zorder));
        }

        VTRACE("count %d, handle %#x, trans %#x, blending %#x"
              " sourceCrop %f,%f - %fx%f, dst %d,%d - %dx%d, custom %#x",
              mCount,
              (uint32_t)imgLayer->psLayer->handle,
              imgLayer->psLayer->transform,
              imgLayer->psLayer->blending,
              imgLayer->psLayer->sourceCropf.left,
              imgLayer->psLayer->sourceCropf.top,
              imgLayer->psLayer->sourceCropf.right - imgLayer->psLayer->sourceCropf.left,
              imgLayer->psLayer->sourceCropf.bottom - imgLayer->psLayer->sourceCropf.top,
              imgLayer->psLayer->displayFrame.left,
              imgLayer->psLayer->displayFrame.top,
              imgLayer->psLayer->displayFrame.right - imgLayer->psLayer->displayFrame.left,
              imgLayer->psLayer->displayFrame.bottom - imgLayer->psLayer->displayFrame.top,
              imgLayer->custom);
    }

    layerList->postFlip();
    return true;
}

bool TngDisplayContext::commitEnd(size_t numDisplays, hwc_display_contents_1_t **displays)
{
    int releaseFenceFd = -1;

    VTRACE("count = %d", mCount);

    if (mIMGDisplayDevice && mCount) {
        int err = mIMGDisplayDevice->post(mIMGDisplayDevice,
                                          mImgLayers,
                                          mCount,
                                          &releaseFenceFd);
        if (err) {
            ETRACE("post failed, err = %d", err);
            return false;
        }
    }

    // close acquire fence
    for (size_t i = 0; i < numDisplays; i++) {
        // Wait and close HWC_OVERLAY typed layer's acquire fence
        hwc_display_contents_1_t* display = displays[i];
        if (!display) {
            continue;
        }

        for (size_t j = 0; j < display->numHwLayers-1; j++) {
            hwc_layer_1_t& layer = display->hwLayers[j];
            if (layer.compositionType == HWC_OVERLAY) {
                if (layer.acquireFenceFd != -1) {
                    // sync_wait(layer.acquireFenceFd, 16ms);
                    close(layer.acquireFenceFd);
                    layer.acquireFenceFd = -1;
                }
            }
        }

        // Wait and close framebuffer target layer's acquire fence
        hwc_layer_1_t& fbt = display->hwLayers[display->numHwLayers-1];
        if (fbt.acquireFenceFd != -1) {
            // sync_wait(fbt.acquireFencdFd, 16ms);
            close(fbt.acquireFenceFd);
            fbt.acquireFenceFd = -1;
        }

        // Wait and close outbuf's acquire fence
        if (display->outbufAcquireFenceFd != -1) {
            // sync_wait(display->outbufAcquireFenceFd, 16ms);
            close(display->outbufAcquireFenceFd);
            display->outbufAcquireFenceFd = -1;
        }
    }

    // update release fence and retire fence
    if (mCount > 0) {
        // For physical displays, dup the releaseFenceFd only for
        // HWC layers which successfully flipped to display planes
        IMG_hwc_layer_t *imgLayerList = (IMG_hwc_layer_t*)mImgLayers;

        for (size_t i = 0; i < mCount; i++) {
            IMG_hwc_layer_t *imgLayer = &imgLayerList[i];
            imgLayer->psLayer->releaseFenceFd =
                (releaseFenceFd != -1) ? dup(releaseFenceFd) : -1;
        }
    }

    for (size_t i = 0; i < numDisplays; i++) {
        if (!displays[i]) {
            continue;
        }

        // For virtual display, simply set releasefence to be -1
        if (i == IDisplayDevice::DEVICE_VIRTUAL) {
            for (size_t j = 0; j < displays[i]->numHwLayers; j++) {
                if (displays[i]->hwLayers[j].compositionType != HWC_FRAMEBUFFER)
                    displays[i]->hwLayers[j].releaseFenceFd = -1;
            }
        }

        // log for layer fence status
        for (size_t j = 0; j < displays[i]->numHwLayers; j++) {
            VTRACE("handle %#x, acquiredFD %d, releaseFD %d",
                 (uint32_t)displays[i]->hwLayers[j].handle,
                 displays[i]->hwLayers[j].acquireFenceFd,
                 displays[i]->hwLayers[j].releaseFenceFd);
        }

        // retireFence is used for SurfaceFlinger to do DispSync;
        // dup releaseFenceFd for physical displays and assign -1 for virtual
        // display; we don't distinguish between release and retire, and all
        // physical displays are using a single releaseFence; for virtual
        // display, we are using sync mode to do NV12 bliting, and composition
        // is always completed after commit.
        if (i < IDisplayDevice::DEVICE_VIRTUAL) {
            displays[i]->retireFenceFd =
                (releaseFenceFd != -1) ? dup(releaseFenceFd) : -1;
        } else {
            displays[i]->retireFenceFd = -1;
        }
    }

    // close original release fence fd
    if (releaseFenceFd != -1) {
        close(releaseFenceFd);
    }
    return true;
}

bool TngDisplayContext::compositionComplete()
{
    return true;
}

void TngDisplayContext::deinitialize()
{
    mIMGDisplayDevice = 0;

    mCount = 0;
    mInitialized = false;
}


} // namespace intel
} // namespace android
