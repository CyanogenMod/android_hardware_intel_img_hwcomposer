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
#ifndef IDISPLAY_DEVICE_H
#define IDISPLAY_DEVICE_H

#include <Dump.h>
#include <IDisplayContext.h>
#include <DisplayPlane.h>

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


//  display device interface
class IDisplayDevice {
public:
    // display device type
    enum {
        DEVICE_PRIMARY = HWC_DISPLAY_PRIMARY,
        DEVICE_EXTERNAL = HWC_DISPLAY_EXTERNAL,
#ifdef INTEL_WIDI_MERRIFIELD
        DEVICE_VIRTUAL = HWC_NUM_DISPLAY_TYPES,
#endif
        DEVICE_COUNT,
    };
    enum {
        DEVICE_DISCONNECTED = 0,
        DEVICE_CONNECTED,
    };
    enum {
        DEVICE_DISPLAY_OFF = 0,
        DEVICE_DISPLAY_ON,
    };
public:
    IDisplayDevice() {}
    virtual ~IDisplayDevice() {}
public:
    virtual bool prePrepare(hwc_display_contents_1_t *display) = 0;
    virtual bool prepare(hwc_display_contents_1_t *display) = 0;
    virtual bool commit(hwc_display_contents_1_t *display,
                          IDisplayContext *context) = 0;

    virtual bool vsyncControl(bool enabled) = 0;
    virtual bool blank(bool blank) = 0;
    virtual bool getDisplaySize(int *width, int *height) = 0;
    virtual bool getDisplayConfigs(uint32_t *configs,
                                       size_t *numConfigs) = 0;
    virtual bool getDisplayAttributes(uint32_t config,
                                          const uint32_t *attributes,
                                          int32_t *values) = 0;
    virtual bool compositionComplete() = 0;

    virtual bool initialize() = 0;
    virtual void deinitialize() = 0;
    virtual bool isConnected() const = 0;
    virtual const char* getName() const = 0;
    virtual int getType() const = 0;
    virtual void dump(Dump& d) = 0;
};

}
}

#endif /* IDISPLAY_DEVICE_H */
