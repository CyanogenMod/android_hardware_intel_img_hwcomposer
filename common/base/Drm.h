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
#ifndef __DRM_H__
#define __DRM_H__

#include <utils/Mutex.h>

// TODO: psb_drm.h is IP specific defintion
#include <psb_drm.h>

extern "C" {
#include "xf86drm.h"
#include "xf86drmMode.h"
}

namespace android {
namespace intel {

class Drm {
public:
    Drm();
    virtual ~Drm();
public:
    bool initialize();
    void deinitialize();
    bool detect(int device);
    bool setDrmMode(int device, drmModeModeInfo& value);
    bool setRefreshRate(int device, int hz);
    bool writeReadIoctl(unsigned long cmd, void *data,
                      unsigned long size);
    bool writeIoctl(unsigned long cmd, void *data,
                      unsigned long size);

    bool isConnected(int device);
    bool setDpmsMode(int device, int mode);
    int getDrmFd() const;
    bool getModeInfo(int device, drmModeModeInfo& mode);
    bool getPhysicalSize(int device, uint32_t& width, uint32_t& height);

private:
    bool initDrmMode(int index);
    bool setDrmMode(int index, drmModeModeInfoPtr mode);
    void resetOutput(int index);

    // map device type to output index, return -1 if not mapped
    inline int getOutputIndex(int device);

private:
    // DRM object index
    enum {
        OUTPUT_PRIMARY = 0,
        OUTPUT_EXTERNAL,
        OUTPUT_MAX,
    };

    struct DrmOutput {
        drmModeConnectorPtr connector;
        drmModeEncoderPtr encoder;
        drmModeCrtcPtr crtc;
        drmModeModeInfo mode;
        uint32_t fbHandle;
        uint32_t fbId;
        int connected;
    } mOutputs[OUTPUT_MAX];

    int mDrmFd;
    Mutex mLock;
    bool mInitialized;
};

} // namespace intel
} // namespace android



#endif /* __DRM_H__ */
