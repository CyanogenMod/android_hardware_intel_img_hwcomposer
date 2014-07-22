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
#ifndef DISPLAYDEVICE_H_
#define DISPLAYDEVICE_H_

#include <DisplayPlane.h>
#include <IVsyncControl.h>
#include <IBlankControl.h>
#include <VsyncEventObserver.h>
#include <HotplugEventObserver.h>
#include <HwcLayerList.h>

namespace android {
namespace intel {

// display config
class DisplayConfig {
public:
    DisplayConfig(int rr, int w, int h, int dpix, int dpiy)
        : mRefreshRate(rr),
          mWidth(w),
          mHeight(h),
          mDpiX(dpix),
          mDpiY(dpiy)
    {}
public:
    int getRefreshRate() const { return mRefreshRate; }
    int getWidth() const { return mWidth; }
    int getHeight() const { return mHeight; }
    int getDpiX() const { return mDpiX; }
    int getDpiY() const { return mDpiY; }
private:
    int mRefreshRate;
    int mWidth;
    int mHeight;
    int mDpiX;
    int mDpiY;
};

class Hwcomposer;

// generic display device
class DisplayDevice {
public:
    // display device type
    enum {
        DEVICE_PRIMARY = HWC_DISPLAY_PRIMARY,
        DEVICE_EXTERNAL = HWC_DISPLAY_EXTERNAL,
        DEVICE_VIRTUAL = HWC_NUM_DISPLAY_TYPES,
        DEVICE_COUNT,
    };
    enum {
        DEVICE_DISCONNECTED = 0,
        DEVICE_CONNECTED,
    };
public:
    DisplayDevice(uint32_t type, Hwcomposer& hwc, DisplayPlaneManager& dpm);
    virtual ~DisplayDevice();
public:
    virtual void prePrepare(hwc_display_contents_1_t *display);
    virtual bool prepare(hwc_display_contents_1_t *display);
    virtual bool commit(hwc_display_contents_1_t *display,
                          void* context,
                          int& count) = 0;

    virtual bool vsyncControl(int enabled);
    virtual bool blank(int blank);
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
    virtual bool isConnected() const;
    virtual const char* getName() const;
    virtual int getType() const;

    //events
    virtual void onHotplug();
    virtual void onVsync(int64_t timestamp);

    virtual void dump(Dump& d);
protected:
    virtual void deinitialize();
protected:
    void onGeometryChanged(hwc_display_contents_1_t *list);
    bool updateDisplayConfigs(struct Output *output);
protected:
    virtual IVsyncControl* createVsyncControl() = 0;
    virtual IBlankControl* createBlankControl() = 0;
    virtual IHotplugControl* createHotplugControl() = 0;
protected:
    uint32_t mType;
    const char *mName;

    Hwcomposer& mHwc;
    DisplayPlaneManager& mDisplayPlaneManager;

    // display configs
    Vector<DisplayConfig*> mDisplayConfigs;
    int mActiveDisplayConfig;

    // vsync control
    IVsyncControl *mVsyncControl;
    // blank control
    IBlankControl *mBlankControl;
    // hotplug control
    IHotplugControl *mHotplugControl;

    // hotplug event observer
    sp<HotplugEventObserver> mHotplugObserver;
    // vsync event observer
    sp<VsyncEventObserver> mVsyncObserver;

    // layer list
    HwcLayerList *mLayerList;
    DisplayPlane *mPrimaryPlane;
    bool mConnection;

    // lock
    Mutex mLock;

    bool mInitialized;
};

}
}

#endif /* DISPLAYDEVICE_H_ */
