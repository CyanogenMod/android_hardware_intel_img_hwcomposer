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
#ifndef HWCOMPOSER_H
#define HWCOMPOSER_H

#include <EGL/egl.h>
#include <hardware/hwcomposer.h>
#include <utils/Vector.h>

#include <IDisplayDevice.h>
#include <BufferManager.h>
#include <IDisplayContext.h>
#include <Drm.h>
#include <DisplayPlaneManager.h>
#include <DisplayAnalyzer.h>
#include <VsyncManager.h>
#include <MultiDisplayObserver.h>
#include <UeventObserver.h>
#include <IPowerManager.h>

namespace android {
namespace intel {

class Hwcomposer : public hwc_composer_device_1_t {
public:
    virtual ~Hwcomposer();
public:
    // callbacks implementation
    virtual bool prepare(size_t numDisplays,
                           hwc_display_contents_1_t** displays);
    virtual bool commit(size_t numDisplays,
                           hwc_display_contents_1_t** displays);
    virtual bool vsyncControl(int disp, int enabled);
    virtual bool release();
    virtual bool dump(char *buff, int buff_len, int *cur_len);
    virtual void registerProcs(hwc_procs_t const *procs);

    virtual bool blank(int disp, int blank);
    virtual bool getDisplayConfigs(int disp,
                                       uint32_t *configs,
                                       size_t *numConfigs);
    virtual bool getDisplayAttributes(int disp,
                                          uint32_t config,
                                          const uint32_t *attributes,
                                          int32_t *values);
    virtual bool compositionComplete(int disp);

    // callbacks
    virtual void vsync(int disp, int64_t timestamp);
    virtual void hotplug(int disp, bool connected);
    virtual void invalidate();

    virtual bool initCheck() const;
    virtual bool initialize();
    virtual void deinitialize();

public:
    Drm* getDrm();
    DisplayPlaneManager* getPlaneManager();
    BufferManager* getBufferManager();
    IDisplayContext* getDisplayContext();
    DisplayAnalyzer* getDisplayAnalyzer();
    VsyncManager* getVsyncManager();
    MultiDisplayObserver* getMultiDisplayObserver();
    IDisplayDevice* getDisplayDevice(int disp);
    UeventObserver* getUeventObserver();
    IPowerManager* getPowerManager();

protected:
    Hwcomposer();

public:
    static Hwcomposer& getInstance() {
        Hwcomposer *instance = sInstance;
        if (instance == 0) {
            instance = createHwcomposer();
            sInstance = instance;
        }
        return *sInstance;
    }
    static void releaseInstance() {
        delete sInstance;
        sInstance = NULL;
    }
    // Need to be implemented
    static Hwcomposer* createHwcomposer();
protected:
    virtual DisplayPlaneManager* createDisplayPlaneManager() = 0;
    virtual BufferManager* createBufferManager() = 0;
    virtual IDisplayDevice* createDisplayDevice(int disp,
                                                 DisplayPlaneManager& dpm) = 0;
    virtual IDisplayContext* createDisplayContext() = 0;
    virtual IPowerManager* createPowerManager() = 0;

protected:
    hwc_procs_t const *mProcs;
    Drm *mDrm;
    DisplayPlaneManager *mPlaneManager;
    BufferManager *mBufferManager;
    DisplayAnalyzer *mDisplayAnalyzer;
    Vector<IDisplayDevice*> mDisplayDevices;
    IDisplayContext *mDisplayContext;
    VsyncManager *mVsyncManager;
    MultiDisplayObserver *mMultiDisplayObserver;
    UeventObserver *mUeventObserver;
    IPowerManager *mPowerManager;
    bool mInitialized;
private:
    static Hwcomposer *sInstance;
};

} // namespace intel
}

#endif /*HW_COMPOSER_H*/
