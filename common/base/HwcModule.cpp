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
#include <hardware/hardware.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <cutils/log.h>
#include <cutils/atomic.h>

#include <Hwcomposer.h>

#define GET_HWC_RETURN_ERROR_IF_NULL() \
    LOGV("Entering %s", __func__); \
    Hwcomposer *hwc = static_cast<Hwcomposer*>(dev); \
    if (!hwc) { \
        LOGE("%s: Invalid HWC device.", __func__); \
        return -EINVAL; \
    }

#define GET_HWC_RETURN_VOID_IF_NULL() \
    LOGV("Entering %s", __func__); \
    Hwcomposer *hwc = static_cast<Hwcomposer*>(dev); \
    if (!hwc) { \
        LOGE("%s: Invalid HWC device.", __func__); \
        return; \
    }


namespace android {
namespace intel {

static int hwc_prepare(struct hwc_composer_device_1 *dev,
                          size_t numDisplays,
                          hwc_display_contents_1_t** displays)
{
    GET_HWC_RETURN_ERROR_IF_NULL();
    if (!hwc->prepare(numDisplays, displays)) {
        LOGE("%s: failed to prepare", __func__);
        return -EINVAL;
    }
    return 0;
}

static int hwc_set(struct hwc_composer_device_1 *dev,
                     size_t numDisplays,
                     hwc_display_contents_1_t **displays)
{
    GET_HWC_RETURN_ERROR_IF_NULL();
    if (!hwc->commit(numDisplays, displays)) {
        LOGE("%s: failed to commit", __func__);
        return -EINVAL;
    }
    return 0;
}

static void hwc_dump(struct hwc_composer_device_1 *dev,
                       char *buff,
                       int buff_len)
{
    GET_HWC_RETURN_VOID_IF_NULL();
    hwc->dump(buff, buff_len, 0);
}

void hwc_registerProcs(struct hwc_composer_device_1 *dev,
                          hwc_procs_t const *procs)
{
    GET_HWC_RETURN_VOID_IF_NULL();
    hwc->registerProcs(procs);
}

static int hwc_device_close(struct hw_device_t *dev)
{
    LOGV("Entering %s", __func__);
    Hwcomposer::releaseInstance();
    return 0;
}

static int hwc_query(struct hwc_composer_device_1 *dev,
                       int what,
                       int* value)
{
    LOGV("hwc_query: what %d\n", what);
    return -EINVAL;
}

static int hwc_eventControl(struct hwc_composer_device_1 *dev,
                                int disp,
                                int event,
                                int enabled)
{
    bool ret;
    GET_HWC_RETURN_ERROR_IF_NULL();

    switch (event) {
    case HWC_EVENT_VSYNC:
        ret = hwc->vsyncControl(disp, enabled);
        if (ret == false) {
            LOGE("%s: failed to enable/disable vsync", __func__);
            return -EINVAL;
        }
        break;
    default:
        LOGW("%s: unsupported event %d", __func__, event);
        break;
    }

    return 0;
}

static int hwc_blank(hwc_composer_device_1_t *dev, int disp, int blank)
{
    GET_HWC_RETURN_ERROR_IF_NULL();
    bool ret = hwc->blank(disp, blank);
    if (ret == false) {
        LOGE("%s: failed to blank disp %d, blank %d", __func__, disp, blank);
        return -EINVAL;
    }

    return 0;
}

static int hwc_getDisplayConfigs(hwc_composer_device_1_t *dev,
                                     int disp,
                                     uint32_t *configs,
                                     size_t *numConfigs)
{
    GET_HWC_RETURN_ERROR_IF_NULL();
    bool ret = hwc->getDisplayConfigs(disp, configs, numConfigs);
    if (ret == false) {
        LOGE("%s: failed to get configs of disp %d", __func__, disp);
        return -EINVAL;
    }

    return 0;
}

static int hwc_getDisplayAttributes(hwc_composer_device_1_t *dev,
                                        int disp,
                                        uint32_t config,
                                        const uint32_t *attributes,
                                        int32_t *values)
{
    GET_HWC_RETURN_ERROR_IF_NULL();
    bool ret = hwc->getDisplayAttributes(disp, config, attributes, values);
    if (ret == false) {
        LOGE("%s: failed to get attributes of disp %d", __func__, disp);
        return -EINVAL;
    }

    return 0;
}

static int hwc_compositionComplete(hwc_composer_device_1_t *dev, int disp)
{
    GET_HWC_RETURN_ERROR_IF_NULL();
    bool ret = hwc->compositionComplete(disp);
    if (ret == false) {
        LOGE("%s: faild for disp %d", __func__, disp);
        return -EINVAL;
    }

    return 0;
}

//------------------------------------------------------------------------------

static int hwc_device_open(const struct hw_module_t* module,
                              const char* name,
                              struct hw_device_t** device)
{
    if (!name) {
        LOGE("%s: Invalid name.", __func__);
        return -EINVAL;
    }

    LOGD("%s: open device %s", __func__, name);

    if (strcmp(name, HWC_HARDWARE_COMPOSER) != 0) {
        LOGE("%s: try to open unknown HWComposer %s", __func__, name);
        return -EINVAL;
    }

    Hwcomposer& hwc = Hwcomposer::getInstance();
    /* initialize our state here */
    if (hwc.initialize() == false) {
        LOGE("%s: failed to intialize HWComposer", __func__);
        Hwcomposer::releaseInstance();
        return -EINVAL;
    }

    /* initialize the procs */
    hwc.hwc_composer_device_1_t::common.tag = HARDWARE_DEVICE_TAG;
    hwc.hwc_composer_device_1_t::common.version = HWC_DEVICE_API_VERSION_1_1;
    hwc.hwc_composer_device_1_t::common.module =
        const_cast<hw_module_t*>(module);
    hwc.hwc_composer_device_1_t::common.close = hwc_device_close;

    hwc.hwc_composer_device_1_t::prepare = hwc_prepare;
    hwc.hwc_composer_device_1_t::set = hwc_set;
    hwc.hwc_composer_device_1_t::dump = hwc_dump;
    hwc.hwc_composer_device_1_t::registerProcs = hwc_registerProcs;
    hwc.hwc_composer_device_1_t::query = hwc_query;

    hwc.hwc_composer_device_1_t::blank = hwc_blank;
    hwc.hwc_composer_device_1_t::eventControl = hwc_eventControl;
    hwc.hwc_composer_device_1_t::getDisplayConfigs = hwc_getDisplayConfigs;
    hwc.hwc_composer_device_1_t::getDisplayAttributes = hwc_getDisplayAttributes;

    // This is used to hack FBO switch flush issue in SurfaceFlinger.
    hwc.hwc_composer_device_1_t::reserved_proc[0] = (void*)hwc_compositionComplete;

    *device = &hwc.hwc_composer_device_1_t::common;

    return 0;
}

} // namespace intel
} // namespace android

static struct hw_module_methods_t hwc_module_methods = {
    open: android::intel::hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 1,
        id: HWC_HARDWARE_MODULE_ID,
        name: "Intel Hardware Composer",
        author: "Intel UMSE",
        methods: &hwc_module_methods,
    }
};
