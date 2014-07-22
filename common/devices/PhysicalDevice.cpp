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
#include <common/utils/HwcTrace.h>
#include <common/base/Drm.h>
#include <Hwcomposer.h>
#include <PhysicalDevice.h>

namespace android {
namespace intel {

PhysicalDevice::PhysicalDevice(uint32_t type, Hwcomposer& hwc, DisplayPlaneManager& dpm)
    : mType(type),
      mHwc(hwc),
      mDisplayPlaneManager(dpm),
      mActiveDisplayConfig(-1),
      mBlankControl(NULL),
      mPrepareListener(NULL),
      mVsyncObserver(NULL),
      mLayerList(NULL),
      mConnected(false),
      mBlank(false),
      mDisplayState(DEVICE_DISPLAY_ON),
      mInitialized(false)
{
    CTRACE();

    switch (type) {
    case DEVICE_PRIMARY:
        mName = "Primary";
        break;
    case DEVICE_EXTERNAL:
        mName = "External";
        break;
    default:
        mName = "Unknown";
    }

    mDisplayConfigs.setCapacity(DEVICE_COUNT);
}

PhysicalDevice::~PhysicalDevice()
{
    WARN_IF_NOT_DEINIT();
}

void PhysicalDevice::onGeometryChanged(hwc_display_contents_1_t *list)
{
    if (!list) {
        ETRACE("list is NULL");
        return;
    }

    ATRACE("disp = %d, layer number = %d", mType, list->numHwLayers);

    // NOTE: should NOT be here
    if (mLayerList) {
        WTRACE("mLayerList exists");
        DEINIT_AND_DELETE_OBJ(mLayerList);
    }

    // create a new layer list
    mLayerList = new HwcLayerList(list, mType);
    if (!mLayerList) {
        WTRACE("failed to create layer list");
    }
}

bool PhysicalDevice::prePrepare(hwc_display_contents_1_t *display)
{
    RETURN_FALSE_IF_NOT_INIT();

    // for a null list, delete hwc list
    if (!mConnected || !display || mBlank) {
        if (mLayerList) {
            DEINIT_AND_DELETE_OBJ(mLayerList);
        }
        return true;
    }

    // check if geometry is changed, if changed delete list
    if ((display->flags & HWC_GEOMETRY_CHANGED) && mLayerList) {
        DEINIT_AND_DELETE_OBJ(mLayerList);
    }
    return true;
}

bool PhysicalDevice::prepare(hwc_display_contents_1_t *display)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (!mConnected || !display || mBlank)
        return true;

    // check if geometry is changed
    if (display->flags & HWC_GEOMETRY_CHANGED) {
        onGeometryChanged(display);
    }
    if (!mLayerList) {
        WTRACE("null HWC layer list");
        return true;
    }

    // update list with new list
    return mLayerList->update(display);
}


bool PhysicalDevice::commit(hwc_display_contents_1_t *display, IDisplayContext *context)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (!display || !context || !mLayerList || mBlank) {
        return true;
    }
    return context->commitContents(display, mLayerList);
}

bool PhysicalDevice::vsyncControl(bool enabled)
{
    RETURN_FALSE_IF_NOT_INIT();

    ATRACE("disp = %d, enabled = %d", mType, enabled);
    return mVsyncObserver->control(enabled);
}

bool PhysicalDevice::blank(bool blank)
{
    RETURN_FALSE_IF_NOT_INIT();

    if ((!mConnected) && (!blank))
    {
        int i = 0;
        while ((!mConnected ) && (i <= 1000))
        {
            usleep(1000);
            i ++ ;
        }
    }

    if (!mConnected)
        return false;

    mBlank = blank;
    bool ret = mBlankControl->blank(mType, blank);
    if (ret == false) {
        ETRACE("failed to blank device");
        return false;
    }

    return true;
}

bool PhysicalDevice::getDisplaySize(int *width, int *height)
{
    RETURN_FALSE_IF_NOT_INIT();
    Mutex::Autolock _l(mLock);
    if (!width || !height) {
        ETRACE("invalid parameters");
        return false;
    }

    *width = 0;
    *height = 0;
    drmModeModeInfo mode;
    Drm *drm = Hwcomposer::getInstance().getDrm();
    bool ret = drm->getModeInfo(mType, mode);
    if (!ret) {
        return false;
    }

    *width = mode.hdisplay;
    *height = mode.vdisplay;
    return true;
}

bool PhysicalDevice::getDisplayConfigs(uint32_t *configs,
                                         size_t *numConfigs)
{
    RETURN_FALSE_IF_NOT_INIT();

    Mutex::Autolock _l(mLock);

    if (!mConnected) {
        ITRACE("device is not connected");
        return false;
    }

    if (!configs || !numConfigs) {
        ETRACE("invalid parameters");
        return false;
    }

    *configs = 0;
    *numConfigs = mDisplayConfigs.size();

    return true;
}

bool PhysicalDevice::getDisplayAttributes(uint32_t /* configs */,
        const uint32_t *attributes,
        int32_t *values)
{
    RETURN_FALSE_IF_NOT_INIT();

    Mutex::Autolock _l(mLock);

    if (!mConnected) {
        ITRACE("device is not connected");
        return false;
    }

    if (!attributes || !values) {
        ETRACE("invalid parameters");
        return false;
    }

    DisplayConfig *config = mDisplayConfigs.itemAt(mActiveDisplayConfig);
    if  (!config) {
        WTRACE("failed to get display config");
        return false;
    }

    int i = 0;
    while (attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE) {
        switch (attributes[i]) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            if (config->getRefreshRate()) {
                values[i] = 1e9 / config->getRefreshRate();
            } else {
                ETRACE("refresh rate is 0!!!");
                values[i] = 0;
            }
            break;
        case HWC_DISPLAY_WIDTH:
            values[i] = config->getWidth();
            break;
        case HWC_DISPLAY_HEIGHT:
            values[i] = config->getHeight();
            break;
        case HWC_DISPLAY_DPI_X:
            values[i] = config->getDpiX() * 1000.0f;
            break;
        case HWC_DISPLAY_DPI_Y:
            values[i] = config->getDpiY() * 1000.0f;
            break;
        default:
            ETRACE("unknown attribute %d", attributes[i]);
            break;
        }
        i++;
    }

    return true;
}

bool PhysicalDevice::compositionComplete()
{
    CTRACE();
    // do nothing by default
    return true;
}

void PhysicalDevice::removeDisplayConfigs()
{
    for (size_t i = 0; i < mDisplayConfigs.size(); i++) {
        DisplayConfig *config = mDisplayConfigs.itemAt(i);
        delete config;
    }

    mDisplayConfigs.clear();
    mActiveDisplayConfig = -1;
}

bool PhysicalDevice::detectDisplayConfigs()
{
    Mutex::Autolock _l(mLock);

    Drm *drm = Hwcomposer::getInstance().getDrm();
    if (!drm->detect(mType)) {
        ETRACE("drm detection on device %d failed ", mType);
        return false;
    }
    return updateDisplayConfigs();
}

bool PhysicalDevice::updateDisplayConfigs()
{
    bool ret;
    Drm *drm = Hwcomposer::getInstance().getDrm();

    // reset display configs
    removeDisplayConfigs();

    // update device connection status
    mConnected = drm->isConnected(mType);
    if (!mConnected) {
        return true;
    }

    // reset the number of display configs
    mDisplayConfigs.setCapacity(1);

    drmModeModeInfo mode;
    ret = drm->getModeInfo(mType, mode);
    if (!ret) {
        ETRACE("failed to get mode info");
        mConnected = false;
        return false;
    }

    uint32_t mmWidth, mmHeight;
    ret = drm->getPhysicalSize(mType, mmWidth, mmHeight);
    if (!ret) {
        ETRACE("failed to get physical size");
        mConnected = false;
        return false;
    }

    float physWidthInch = (float)mmWidth * 0.039370f;
    float physHeightInch = (float)mmHeight * 0.039370f;

    // use current drm mode, likely it's preferred mode
    int dpiX = 0;
    int dpiY = 0;
    if (physWidthInch && physHeightInch) {
        dpiX = mode.hdisplay / physWidthInch;
        dpiY = mode.vdisplay / physHeightInch;
    } else {
        ETRACE("invalid physical size, EDID read error?");
        // don't bail out as it is not a fatal error
    }
    // use active fb dimension as config width/height
    DisplayConfig *config = new DisplayConfig(mode.vrefresh,
                                              mode.hdisplay,
                                              mode.vdisplay,
                                              dpiX, dpiY);
    // add it to the front of other configs
    mDisplayConfigs.push_front(config);

    // init the active display config
    mActiveDisplayConfig = 0;

    return true;
}

bool PhysicalDevice::initialize()
{
    CTRACE();

    if (mType != DEVICE_PRIMARY && mType != DEVICE_EXTERNAL) {
        ETRACE("invalid device type");
        return false;
    }

    // detect display configs
    bool ret = detectDisplayConfigs();
    if (ret == false) {
        DEINIT_AND_RETURN_FALSE("failed to detect display config");
    }

    // create blank control
    mBlankControl = createBlankControl();
    if (!mBlankControl) {
        DEINIT_AND_RETURN_FALSE("failed to create blank control");
    }

    // create hwc prepare listener
    mPrepareListener = createPrepareListener();
    if (!mPrepareListener) {
        DEINIT_AND_RETURN_FALSE("failed to create prepare listener");
    }

    // create vsync event observer
    mVsyncObserver = new VsyncEventObserver(*this);
    if (!mVsyncObserver || !mVsyncObserver->initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to create vsync observer");
    }

    mInitialized = true;
    return true;
}

void PhysicalDevice::deinitialize()
{
    if (mLayerList) {
        DEINIT_AND_DELETE_OBJ(mLayerList);
    }

    DEINIT_AND_DELETE_OBJ(mVsyncObserver);

    // destroy blank control
    if (mBlankControl) {
        delete mBlankControl;
        mBlankControl = 0;
    }

    if (mPrepareListener) {
        delete mPrepareListener;
        mPrepareListener = 0;
    }

    // remove configs
    removeDisplayConfigs();

    mInitialized = false;
}

bool PhysicalDevice::isConnected() const
{
    RETURN_FALSE_IF_NOT_INIT();

    return mConnected;
}

const char* PhysicalDevice::getName() const
{
    return mName;
}

int PhysicalDevice::getType() const
{
    return mType;
}

void PhysicalDevice::onVsync(int64_t timestamp)
{
    RETURN_VOID_IF_NOT_INIT();
    ATRACE("timestamp = %lld", timestamp);

    if (!mConnected)
        return;

    // notify hwc
    mHwc.vsync(mType, timestamp);
}

void PhysicalDevice::dump(Dump& d)
{
    d.append("-------------------------------------------------------------\n");
    d.append("Device Name: %s (%s)\n", mName,
            mConnected ? "connected" : "disconnected");
    d.append("Display configs (count = %d):\n", mDisplayConfigs.size());
    d.append(" CONFIG | VSYNC_PERIOD | WIDTH | HEIGHT | DPI_X | DPI_Y \n");
    d.append("--------+--------------+-------+--------+-------+-------\n");
    for (size_t i = 0; i < mDisplayConfigs.size(); i++) {
        DisplayConfig *config = mDisplayConfigs.itemAt(i);
        if (config) {
            d.append("%s %2d   |     %4d     | %5d |  %4d  |  %3d  |  %3d  \n",
                     (i == (size_t)mActiveDisplayConfig) ? "* " : "  ",
                     i,
                     config->getRefreshRate(),
                     config->getWidth(),
                     config->getHeight(),
                     config->getDpiX(),
                     config->getDpiY());
        }
    }
    // dump layer list
    if (mLayerList)
        mLayerList->dump(d);
}

} // namespace intel
} // namespace android
