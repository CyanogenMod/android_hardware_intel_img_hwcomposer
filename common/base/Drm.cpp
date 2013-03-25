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
#include <fcntl.h>
#include <errno.h>

#include <Drm.h>
#include <Hwcomposer.h>
#include <cutils/log.h>

namespace android {
namespace intel {

Drm::Drm()
{
    const char *path = Hwcomposer::getDrmPath();
    int fd = open(path, O_RDWR, 0);
    if (fd < 0) {
        LOGE("Drm(): drmOpen failed. %s", strerror(errno));
    }

    mDrmFd = fd;
    memset(&mOutputs, 0, sizeof(mOutputs));

    LOGD("Drm(): successfully. mDrmFd %d", fd);
}

bool Drm::detect()
{
    Mutex::Autolock _l(mLock);

    if (mDrmFd < 0) {
        LOGE("detect(): invalid Fd");
        return false;
    }

    // try to get drm resources
    drmModeResPtr resources = drmModeGetResources(mDrmFd);
    if (!resources) {
        LOGE("detect(): fail to get drm resources. %s\n", strerror(errno));
        return false;
    }

    drmModeConnectorPtr connector = NULL;
    drmModeEncoderPtr encoder = NULL;
    drmModeCrtcPtr crtc = NULL;
    drmModeFBPtr fbInfo = NULL;
    struct Output *output = NULL;

    const uint32_t primaryConnector =
        Hwcomposer::getDrmConnector(OUTPUT_PRIMARY);
    const uint32_t externalConnector =
        Hwcomposer::getDrmConnector(OUTPUT_EXTERNAL);

    for (int i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(mDrmFd, resources->connectors[i]);
        if (!connector) {
            LOGE("detect(): fail to get drm connector\n");
            continue;
        }

        int outputIndex = -1;
        if (connector->connector_type == primaryConnector) {
            LOGV("detect(): got primary connector\n");
            outputIndex = OUTPUT_PRIMARY;
        } else if (connector->connector_type == externalConnector) {
            LOGV("detect(): got external connector\n");
            outputIndex = OUTPUT_EXTERNAL;
        }

        if (outputIndex < 0)
            continue;

        // update output, free the old objects first
        output = &mOutputs[outputIndex];

        output->connected =  0;

        if (output->connector) {
            drmModeFreeConnector(output->connector);
            output->connector = 0;
        }
        if (output->encoder) {
            drmModeFreeEncoder(output->encoder);
            output->encoder = 0;
        }

        if (output->crtc) {
            drmModeFreeCrtc(output->crtc);
            output->crtc = 0;
        }

        if (output->fb) {
            drmModeFreeFB(output->fb);
            output->fb = 0;
        }

        output->connector = connector;

        // get current encoder
        encoder = drmModeGetEncoder(mDrmFd, connector->encoder_id);
        if (!encoder) {
            LOGE("detect(): fail to get drm encoder\n");
            continue;
        }

        output->encoder = encoder;

        // get crtc
        crtc = drmModeGetCrtc(mDrmFd, encoder->crtc_id);
        if (!crtc) {
            LOGE("detect(): fail to get drm crtc\n");
            continue;
        }

        output->crtc = crtc;

        // get fb info
        fbInfo = drmModeGetFB(mDrmFd, crtc->buffer_id);
        if (!fbInfo) {
            LOGE("detect(): fail to get fb info\n");
            continue;
        }

        output->fb = fbInfo;

        output->connected = (output->connector &&
                             output->connector->connection == DRM_MODE_CONNECTED &&
                             output->encoder &&
                             output->crtc &&
                             output->fb) ? 1 : 0;
    }

    drmModeFreeResources(resources);

    return true;
}

bool Drm::writeReadIoctl(unsigned long cmd, void *data,
                           unsigned long size)
{
    int err;

    Mutex::Autolock _l(mLock);

    if (mDrmFd <= 0) {
        LOGE("Drm is not initialized");
        return false;
    }

    if (!data || !size) {
        LOGE("Invalid parameters");
        return false;
    }

    err = drmCommandWriteRead(mDrmFd, cmd, data, size);
    if (err) {
        LOGE("Drm::Failed call 0x%lx ioct with failure %d", cmd, err);
        return false;
    }

    return true;
}

bool Drm::writeIoctl(unsigned long cmd, void *data,
                       unsigned long size)
{
    int err;

    Mutex::Autolock _l(mLock);

    if (mDrmFd <= 0) {
        LOGE("Drm is not initialized");
        return false;
    }

    if (!data || !size) {
        LOGE("Invalid parameters");
        return false;
    }

    err = drmCommandWrite(mDrmFd, cmd, data, size);
    if (err) {
        LOGE("Drm::Failed call 0x%lx ioct with failure %d", cmd, err);
        return false;
    }

    return true;
}

int Drm::getDrmFd() const
{
    return mDrmFd;
}

struct Output* Drm::getOutput(int output)
{
    Mutex::Autolock _l(mLock);

    if (output < 0 || output >= OUTPUT_MAX) {
        LOGE("getOutput(): invalid output %d", output);
        return 0;
    }

    return &mOutputs[output];
}

bool Drm::outputConnected(int output)
{
    Mutex::Autolock _l(mLock);

    if (output < 0 || output >= OUTPUT_MAX) {
        LOGE("outputConnected(): invalid output %d", output);
        return false;
    }

    return mOutputs[output].connected ? true : false;
}

} // namespace intel
} // namespace android

