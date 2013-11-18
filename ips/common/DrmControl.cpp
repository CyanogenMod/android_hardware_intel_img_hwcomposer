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
#include <linux/psb_drm.h>
#include <Hwcomposer.h>
#include <common/DrmControl.h>

namespace android {
namespace intel {

DrmControl::DrmControl()
    : mVideoExtCommand(0)
{
}

DrmControl::~DrmControl()
{
}

int DrmControl::getVideoExtCommand()
{
    if (mVideoExtCommand) {
        return mVideoExtCommand;
    }

    int fd = Hwcomposer::getInstance().getDrm()->getDrmFd();

    union drm_psb_extension_arg video_getparam_arg;
    strncpy(video_getparam_arg.extension,
            "lnc_video_getparam", sizeof(video_getparam_arg.extension));
    int ret = drmCommandWriteRead(fd, DRM_PSB_EXTENSION,
            &video_getparam_arg, sizeof(video_getparam_arg));
    if (ret != 0) {
        VTRACE("failed to get video extension command");
        return 0;
    }

    mVideoExtCommand = video_getparam_arg.rep.driver_ioctl_offset;

    return mVideoExtCommand;
}

} // namespace intel
} // namespace android
