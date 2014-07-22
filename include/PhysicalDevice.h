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
#ifndef PHYSICAL_DEVICE_H
#define PHYSICAL_DEVICE_H

#include <DisplayPlane.h>
#include <IVsyncControl.h>
#include <IBlankControl.h>
#include <IPrepareListener.h>
#include <VsyncEventObserver.h>
#include <HwcLayerList.h>
#include <Drm.h>
#include <IDisplayDevice.h>

namespace android {
namespace intel {

class Hwcomposer;

// Base class for primary and external devices
class PhysicalDevice : public IDisplayDevice {
public:
    PhysicalDevice(uint32_t type, Hwcomposer& hwc, DisplayPlaneManager& dpm);
    virtual ~PhysicalDevice();
public:
    virtual bool prePrepare(hwc_display_contents_1_t *display);
    virtual bool prepare(hwc_display_contents_1_t *display);
    virtual bool commit(hwc_display_contents_1_t *display, IDisplayContext *context);

    virtual bool vsyncControl(bool enabled);
    virtual bool blank(bool blank);
    virtual bool getDisplaySize(int *width, int *height);
    virtual bool getDisplayConfigs(uint32_t *configs,
                                       size_t *numConfigs);
    virtual bool getDisplayAttributes(uint32_t config,
                                          const uint32_t *attributes,
                                          int32_t *values);
    virtual bool compositionComplete();

    // display config operations
    virtual void removeDisplayConfigs();
    virtual bool detectDisplayConfigs();

    // device related operations
    virtual bool initCheck() const { return mInitialized; }
    virtual bool initialize();
    virtual void deinitialize();
    virtual bool isConnected() const;
    virtual const char* getName() const;
    virtual int getType() const;

    //events
    virtual void onVsync(int64_t timestamp);

    virtual void dump(Dump& d);

protected:
    void onGeometryChanged(hwc_display_contents_1_t *list);
    bool updateDisplayConfigs();
    virtual IVsyncControl* createVsyncControl() = 0;
    virtual IBlankControl* createBlankControl() = 0;
    virtual IPrepareListener* createPrepareListener() = 0;
    friend class VsyncEventObserver;

protected:
    uint32_t mType;
    const char *mName;

    Hwcomposer& mHwc;
    DisplayPlaneManager& mDisplayPlaneManager;

    // display configs
    Vector<DisplayConfig*> mDisplayConfigs;
    int mActiveDisplayConfig;


    IBlankControl *mBlankControl;
    IPrepareListener *mPrepareListener;
    VsyncEventObserver *mVsyncObserver;

    // layer list
    HwcLayerList *mLayerList;
    bool mConnected;
    bool mBlank;

    // lock
    Mutex mLock;

    // DPMS on (1) or off (0)
    int mDisplayState;
    bool mInitialized;
};

}
}

#endif /* PHYSICAL_DEVICE_H */
