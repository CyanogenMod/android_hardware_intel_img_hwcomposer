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
#include <cutils/atomic.h>

#include <Hwcomposer.h>
#include <Dump.h>

namespace android {
namespace intel {

Hwcomposer* Hwcomposer::sInstance(0);

Hwcomposer::Hwcomposer()
    : mProcs(0),
      mDrm(0),
      mPlaneManager(0),
      mBufferManager(0),
      mInitialized(false)
{
    LOGD("Hwcomposer");

    mDisplayDevices.setCapacity(DisplayDevice::DEVICE_COUNT);
}

Hwcomposer::~Hwcomposer()
{
    LOGD("~Hwcomposer");
    deinitialize();
}

bool Hwcomposer::initCheck() const
{
    return mInitialized;
}

bool Hwcomposer::prepare(size_t numDisplays,
                          hwc_display_contents_1_t** displays)
{
    bool ret = true;

    //Mutex::Autolock _l(mLock);

    LOGV("prepare display count %d\n", numDisplays);

    if (!initCheck())
        return false;

    if (!numDisplays || !displays) {
        LOGE("prepare: invalid parameters");
        return false;
    }

    // disable reclaimed planes
    if (mPlaneManager)
        mPlaneManager->disableReclaimedPlanes();

    // reclaim all allocated planes if possible
    for (size_t i = 0; i < numDisplays; i++) {
        DisplayDevice *device = mDisplayDevices.itemAt(i);
        if (!device) {
            LOGV("prepare: device %d doesn't exist", i);
            continue;
        }

        if (!device->isConnected()) {
            LOGV("prepare: device %d is disconnected", i);
            continue;
        }

        device->prePrepare(displays[i]);
    }

    for (size_t i = 0; i < numDisplays; i++) {
        DisplayDevice *device = mDisplayDevices.itemAt(i);
        if (!device) {
            LOGV("prepare: device %d doesn't exist", i);
            continue;
        }

        if (!device->isConnected()) {
            LOGV("prepare: device %d is disconnected", i);
            continue;
        }

        ret = device->prepare(displays[i]);
        if (ret == false) {
            LOGE("prepare: failed to do prepare for device %d", i);
            continue;
        }
    }

    return ret;
}

bool Hwcomposer::commit(size_t numDisplays,
                         hwc_display_contents_1_t **displays)
{
    bool ret = true;

    LOGV("commit display count %d\n", numDisplays);

    //Mutex::Autolock _l(mLock);

    if (!initCheck())
        return false;

    if (!numDisplays || !displays) {
        LOGE("commit: invalid parameters");
        return false;
    }

    void *hwContexts = getContexts();
    int count = 0;
    if (!hwContexts) {
        LOGE("Hwcomposer::commit: invalid hwContexts");
        return false;
    }

    for (size_t i = 0; i < numDisplays; i++) {
        DisplayDevice *device = mDisplayDevices.itemAt(i);
        if (!device) {
            LOGV("commit: device %d doesn't exist", i);
            continue;
        }

        if (!device->isConnected()) {
            LOGV("commit: device %d is disconnected", i);
            continue;
        }

        ret = device->commit(displays[i], hwContexts, count);
        if (ret == false) {
            LOGE("commit: failed to do commit for device %d", i);
            continue;
        }
    }

    // commit hwContexts to hardware
    ret = commitContexts(hwContexts, count);
    if (ret == false) {
        LOGE("Hwcomposer::commit: failed to commit hwContexts");
        return false;
    }

    return ret;
}

bool Hwcomposer::vsyncControl(int disp, int enabled)
{
    LOGV("vsyncControl: disp %d, enabled %d", disp, enabled);

    if (!initCheck())
        return false;

    if (disp < 0 || disp >= DisplayDevice::DEVICE_COUNT) {
        LOGE("vsyncControl: invalid disp %d", disp);
        return false;
    }

    DisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        LOGE("vsyncControl: no device found");
        return false;
    }

    return device->vsyncControl(enabled);
}

bool Hwcomposer::blank(int disp, int blank)
{
    LOGV("blank: disp %d, blank %d", disp, blank);

    if (!initCheck())
        return false;

    if (disp < 0 || disp >= DisplayDevice::DEVICE_COUNT) {
        LOGE("blank: invalid disp %d", disp);
        return false;
    }

    DisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        LOGE("blank: no device found");
        return false;
    }

    return device->blank(blank);
}

bool Hwcomposer::getDisplayConfigs(int disp,
                                      uint32_t *configs,
                                      size_t *numConfigs)
{
    LOGV("getDisplayConfig");

    if (!initCheck())
        return false;

    if (disp < 0 || disp >= DisplayDevice::DEVICE_COUNT) {
        LOGE("getDisplayConfigs: invalid disp %d", disp);
        return false;
    }

    DisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        LOGE("getDisplayConfigs: no device %d found", disp);
        return false;
    }

    return device->getDisplayConfigs(configs, numConfigs);
}

bool Hwcomposer::getDisplayAttributes(int disp,
                                         uint32_t config,
                                         const uint32_t *attributes,
                                         int32_t *values)
{
    LOGV("getDisplayAttributes");

    if (!initCheck())
        return false;

    if (disp < 0 || disp >= DisplayDevice::DEVICE_COUNT) {
        LOGE("getDisplayAttributes: invalid disp %d", disp);
        return false;
    }

    DisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        LOGE("getDisplayAttributes: no device found");
        return false;
    }

    return device->getDisplayAttributes(config, attributes, values);
}

bool Hwcomposer::compositionComplete(int disp)
{
    LOGV("compositionComplete");

    if (!initCheck())
        return false;

    if (disp < 0 || disp >= DisplayDevice::DEVICE_COUNT) {
        LOGE("compositionComplete: invalid disp %d", disp);
        return false;
    }

    DisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        LOGE("compositionComplete: no device found");
        return false;
    }

    return device->compositionComplete();
}

void Hwcomposer::vsync(int disp, int64_t timestamp)
{
    //Mutex::Autolock _l(mLock);

    if (!initCheck())
        return;

    if (mProcs && mProcs->vsync) {
        LOGV("report vsync disp %d timestamp %llu", disp, timestamp);
        mProcs->vsync(const_cast<hwc_procs_t*>(mProcs), disp, timestamp);
    }
}

void Hwcomposer::hotplug(int disp, int connected)
{
    //Mutex::Autolock _l(mLock);

    if (!initCheck())
        return;

    if (mProcs && mProcs->hotplug) {
        LOGV("report hotplug disp %d connected %d", disp, connected);
        mProcs->hotplug(const_cast<hwc_procs_t*>(mProcs), disp, connected);
    }
}

bool Hwcomposer::release()
{
    LOGD("release");

    if (!initCheck())
        return false;

    return true;
}

bool Hwcomposer::dump(char *buff, int buff_len, int *cur_len)
{
    LOGD("dump");

    if (!initCheck())
        return false;

    Dump d(buff, buff_len);

    // dump composer status
    d.append("Intel Hardware Composer state:\n");
    // dump device status
    for (size_t i= 0; i < mDisplayDevices.size(); i++) {
        DisplayDevice *device = mDisplayDevices.itemAt(i);
        if (device)
            device->dump(d);
    }

    // dump plane manager status
    if (mPlaneManager)
        mPlaneManager->dump(d);

    return true;
}

void Hwcomposer::registerProcs(hwc_procs_t const *procs)
{
    LOGD("registerProcs");

    if (!procs)
        LOGW("registerProcs: procs is NULL");

    mProcs = procs;
}

bool Hwcomposer::initialize()
{
    LOGD("initialize");

    // create drm
    mDrm = new Drm();
    if (!mDrm) {
        LOGE("%s: failed to create DRM", __func__);
        return false;
    }

    // create display plane manager
    mPlaneManager = createDisplayPlaneManager();
    if (!mPlaneManager || !mPlaneManager->initialize()) {
        LOGE("initialize: failed to create display plane manager");
        goto init_err;
    }

    // create buffer manager
    mBufferManager = createBufferManager();
    if (!mBufferManager || !mBufferManager->initialize()) {
        LOGE("initialize: failed to create buffer manager");
        goto init_err;
    }

    // create display device
    for (int i = 0; i < DisplayDevice::DEVICE_COUNT; i++) {
        DisplayDevice *device = createDisplayDevice(i, *mPlaneManager);
        if (!device || !device->initialize()) {
            LOGE("initialize: failed to create device %d", i);
            continue;
        }
        // add this device
        mDisplayDevices.insertAt(device, i, 1);
    }

    mInitialized = true;
    return true;
init_err:
    deinitialize();
    return false;
}

void Hwcomposer::deinitialize()
{
    // destroy display devices
    for (size_t i = 0; i < mDisplayDevices.size(); i++) {
        DisplayDevice *device = mDisplayDevices.itemAt(i);
        if (device)
            delete device;
    }
    mDisplayDevices.clear();

    // destroy buffer manager
    if (mBufferManager) {
        delete mBufferManager;
        mBufferManager = 0;
    }

    // destroy display plane manager
    if (mPlaneManager) {
        delete mPlaneManager;
        mPlaneManager = 0;
    }

    // destroy drm
    if (mDrm) {
        delete mDrm;
        mDrm = 0;
    }

    mInitialized = false;
}

Drm* Hwcomposer::getDrm()
{
    return mDrm;
}

DisplayPlaneManager* Hwcomposer::getPlaneManager()
{
    return mPlaneManager;
}

BufferManager* Hwcomposer::getBufferManager()
{
    return mBufferManager;
}

} // namespace intel
} // namespace android
