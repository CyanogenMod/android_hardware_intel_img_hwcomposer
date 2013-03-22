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

#include <Log.h>
#include <Drm.h>
#include <Hwcomposer.h>
#include <penwell/PnwVsyncControl.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();

PnwVsyncControl::PnwVsyncControl()
    : VsyncControl()
{

}

bool PnwVsyncControl::control(int disp, int enabled)
{
    Drm& drm(Hwcomposer::getInstance().getDrm());
    struct drm_psb_vsync_set_arg arg;
    bool ret;

    log.v("PnwVsyncControl::control: disp %d, enabled %d", disp, enabled);

    memset(&arg, 0, sizeof(struct drm_psb_vsync_set_arg));

    // pipe equals to disp
    arg.vsync.pipe = disp;

    if (enabled)
        arg.vsync_operation_mask = VSYNC_ENABLE;
    else
        arg.vsync_operation_mask = VSYNC_DISABLE;

    return drm.writeReadIoctl(DRM_PSB_VSYNC_SET, &arg, sizeof(arg));
}

bool PnwVsyncControl::wait(int disp, int64_t& timestamp)
{
    Drm& drm(Hwcomposer::getInstance().getDrm());
    struct drm_psb_vsync_set_arg arg;
    bool ret;

    log.v("PnwVsyncControl::wait: disp %d", disp);

    memset(&arg, 0, sizeof(struct drm_psb_vsync_set_arg));

    arg.vsync_operation_mask = VSYNC_WAIT;

    // pipe equals to disp
    arg.vsync.pipe = disp;

    ret = drm.writeReadIoctl(DRM_PSB_VSYNC_SET, &arg, sizeof(arg));
    timestamp = (int64_t)arg.vsync.timestamp;
    return ret;
}

} // namespace intel
} // namespace android
