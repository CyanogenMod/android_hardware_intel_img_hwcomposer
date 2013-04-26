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
#include <Drm.h>
#include <Hwcomposer.h>
#include <PhysicalDevice.h>

namespace android {
namespace intel {

PhysicalDevice::PhysicalDevice(uint32_t type, Hwcomposer& hwc, DisplayPlaneManager& dpm)
    : mType(type),
      mHwc(hwc),
      mDisplayPlaneManager(dpm),
      mActiveDisplayConfig(-1),
      mVsyncControl(0),
      mBlankControl(0),
      mPrepareListener(0),
      mVsyncObserver(0),
      mLayerList(0),
      mPrimaryPlane(0),
      mConnection(DEVICE_DISCONNECTED),
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
    CTRACE();
    deinitialize();
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
        delete mLayerList;
    }

    // create a new layer list
    mLayerList = new HwcLayerList(list,
                                  mDisplayPlaneManager,
                                  mPrimaryPlane,
                                  mType);
    if (!mLayerList) {
        WTRACE("failed to create layer list");
    } else if (mType == IDisplayDevice::DEVICE_PRIMARY) {
#if 0  // display driver does not support run-time power management yet
        Hwcomposer& hwc = Hwcomposer::getInstance();
        if (hwc.getDisplayAnalyzer()->checkVideoExtendedMode()) {
            bool hasVisibleLayer = mLayerList->hasVisibleLayer();
            Drm *drm = hwc.getDrm();
            if (hasVisibleLayer == true && mDisplayState == DEVICE_DISPLAY_OFF) {
                ITRACE("turn on device %d as there is visible layer", mType);
                if (drm->setDpmsMode(mType, DEVICE_DISPLAY_ON) == true) {
                    mDisplayState = DEVICE_DISPLAY_ON;
                }
            }
            if (hasVisibleLayer == false && mDisplayState == DEVICE_DISPLAY_ON) {
                ITRACE("turn off device %d as there is no visible layer", mType);
                if (drm->setDpmsMode(mType, DEVICE_DISPLAY_OFF) == true) {
                    mDisplayState = DEVICE_DISPLAY_OFF;
                }
            }
        }
#endif
    }
}

bool PhysicalDevice::prePrepare(hwc_display_contents_1_t *display)
{
    RETURN_FALSE_IF_NOT_INIT();

    Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return false;

    // for a null list, delete hwc list
    if (!display) {
        delete mLayerList;
        mLayerList = 0;
        return true;
    }

    // check if geometry is changed, if changed delete list
    if ((display->flags & HWC_GEOMETRY_CHANGED) && mLayerList) {
        delete mLayerList;
        mLayerList = 0;
    }
    return true;
}

bool PhysicalDevice::prepare(hwc_display_contents_1_t *display)
{
    RETURN_FALSE_IF_NOT_INIT();
    Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return true;

    if (!display)
        return true;

    // check if geometry is changed
    if (display->flags & HWC_GEOMETRY_CHANGED) {
        onGeometryChanged(display);
        if (mLayerList && mLayerList->hasProtectedLayer()) {
            mPrepareListener->onProtectedLayerStart(mType);
        }
    }
    if (!mLayerList) {
        ETRACE("null HWC layer list");
        return false;
    }

    // update list with new list
    return mLayerList->update(display);
}


bool PhysicalDevice::commit(hwc_display_contents_1_t *display, IDisplayContext *context)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (!display || !context) {
        return false;
    }
    return context->commitContents(display, mLayerList);
}

bool PhysicalDevice::vsyncControl(int enabled)
{
    RETURN_FALSE_IF_NOT_INIT();

    //Mutex::Autolock _l(mLock);

    ATRACE("disp = %d, enabled = %d", mType, enabled);

    bool ret = mVsyncControl->control(mType, enabled);
    if (ret == false) {
        ETRACE("failed set vsync");
        return false;
    }

    mVsyncObserver->control(enabled);
    return true;
}

bool PhysicalDevice::blank(int blank)
{
    RETURN_FALSE_IF_NOT_INIT();

    //Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return false;

    bool ret = mBlankControl->blank(mType, blank);
    if (ret == false) {
        ETRACE("failed to blank device");
        return false;
    }

    return true;
}

bool PhysicalDevice::getDisplayConfigs(uint32_t *configs,
                                         size_t *numConfigs)
{
    RETURN_FALSE_IF_NOT_INIT();

    //Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return false;

    if (!configs || !numConfigs) {
        ETRACE("invalid parameters");
        return false;
    }

    *configs = 0;
    *numConfigs = mDisplayConfigs.size();

    return true;
}

bool PhysicalDevice::getDisplayAttributes(uint32_t configs,
                                            const uint32_t *attributes,
                                            int32_t *values)
{
    RETURN_FALSE_IF_NOT_INIT();

    //Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return false;

    if (!attributes || !values) {
        ETRACE("invalid parameters");
        return false;
    }

    DisplayConfig *config = mDisplayConfigs.itemAt(mActiveDisplayConfig);
    if  (!config) {
        ETRACE("failed to get display config");
        return false;
    }

    int i = 0;
    while (attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE) {
        switch (attributes[i]) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            values[i] = 1e9 / config->getRefreshRate();
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

bool PhysicalDevice::updateDisplayConfigs(struct Output *output)
{
    drmModeConnectorPtr drmConnector;
    drmModeCrtcPtr drmCrtc;
    drmModeModeInfoPtr drmMode;
    drmModeModeInfoPtr drmPreferredMode;
    drmModeFBPtr drmFb;
    int drmModeCount;
    float physWidthInch;
    float physHeightInch;
    int dpiX, dpiY;

    CTRACE();

    Mutex::Autolock _l(mLock);

    drmConnector = output->connector;
    if (!drmConnector) {
        ETRACE("output has no connector");
        return false;
    }

    physWidthInch = (float)drmConnector->mmWidth * 0.039370f;
    physHeightInch = (float)drmConnector->mmHeight * 0.039370f;

    drmModeCount = drmConnector->count_modes;
    drmPreferredMode = 0;

    // reset display configs
    removeDisplayConfigs();

    // reset the number of display configs
    mDisplayConfigs.setCapacity(drmModeCount + 1);

    VTRACE("mode count %d", drmModeCount);

    // find preferred mode of this display device
    for (int i = 0; i < drmModeCount; i++) {
        drmMode = &drmConnector->modes[i];
        dpiX = drmMode->hdisplay / physWidthInch;
        dpiY = drmMode->vdisplay / physHeightInch;

        if ((drmMode->type & DRM_MODE_TYPE_PREFERRED)) {
            drmPreferredMode = drmMode;
            continue;
        }

        VTRACE("adding new config %dx%d@%d",
              drmMode->hdisplay,
              drmMode->vdisplay,
              drmMode->vrefresh);

        // if not preferred mode add it to display configs
        DisplayConfig *config = new DisplayConfig(drmMode->vrefresh,
                                                  drmMode->hdisplay,
                                                  drmMode->vdisplay,
                                                  dpiX, dpiY);
        mDisplayConfigs.add(config);
    }

    // update device connection status
    mConnection = (output->connected) ? DEVICE_CONNECTED : DEVICE_DISCONNECTED;

    // if device is connected, continue checking current mode
    if (mConnection != DEVICE_CONNECTED)
        goto use_preferred_mode;
    else if (mConnection == DEVICE_CONNECTED) {
        // use active fb and mode
        drmCrtc = output->crtc;
        drmFb = output->fb;
        if (!drmCrtc || !drmFb) {
            ETRACE("impossible");
            goto use_preferred_mode;
        }

        drmMode = &drmCrtc->mode;
        if (!drmCrtc->mode_valid)
            goto use_preferred_mode;

        VTRACE("using current mode %dx%d@%d",
              drmMode->hdisplay,
              drmMode->vdisplay,
              drmMode->vrefresh);

        // use current drm mode, likely it's preferred mode
        dpiX = drmMode->hdisplay / physWidthInch;
        dpiY = drmMode->vdisplay / physHeightInch;
        // use active fb dimension as config width/height
        DisplayConfig *config = new DisplayConfig(drmMode->vrefresh,
                                                  //drmFb->width,
                                                  //drmFb->height,
                                                  drmMode->hdisplay,
                                                  drmMode->vdisplay,
                                                  dpiX, dpiY);
        // add it to the front of other configs
        mDisplayConfigs.push_front(config);
    }

    // init the active display config
    mActiveDisplayConfig = 0;

    return true;
use_preferred_mode:
    if (drmPreferredMode) {
        dpiX = drmPreferredMode->hdisplay / physWidthInch;
        dpiY = drmPreferredMode->vdisplay / physHeightInch;
        DisplayConfig *config = new DisplayConfig(drmPreferredMode->vrefresh,
                                                  drmPreferredMode->hdisplay,
                                                  drmPreferredMode->vdisplay,
                                                  dpiX, dpiY);
        if (!config) {
            ETRACE("failed to allocate display config");
            return false;
        }

        VTRACE("using preferred mode %dx%d@%d",
              drmPreferredMode->hdisplay,
              drmPreferredMode->vdisplay,
              drmPreferredMode->vrefresh);

        // add it to the front of other configs
        mDisplayConfigs.push_front(config);
    }

    // init the active display config
    mActiveDisplayConfig = 0;
    return true;
}

bool PhysicalDevice::detectDisplayConfigs()
{
    struct Output *output;
    bool ret;
    Drm *drm = Hwcomposer::getInstance().getDrm();

    CTRACE();

    if (!drm) {
        ETRACE("failed to get drm");
        return false;
    }

    // detect
    ret = drm->detect();
    if (ret == false) {
        ETRACE("drm detection failed");
        return false;
    }

    // get output
    output = drm->getOutput(mType);
    if (!output) {
        ETRACE("failed to get output");
        return false;
    }

    // update display configs
    return updateDisplayConfigs(output);
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

    // get primary plane of this device
    mPrimaryPlane = mDisplayPlaneManager.getPrimaryPlane(mType);
    if (!mPrimaryPlane) {
        DEINIT_AND_RETURN_FALSE("failed to get primary plane");
    }

    // create vsync control
    mVsyncControl = createVsyncControl();
    if (!mVsyncControl) {
        DEINIT_AND_RETURN_FALSE("failed to create vsync control");
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
    mVsyncObserver = new VsyncEventObserver(*this, *mVsyncControl);
    if (!mVsyncObserver.get()) {
        DEINIT_AND_RETURN_FALSE("failed to create vsync observer");
    }

    mInitialized = true;
    return true;
}

void PhysicalDevice::deinitialize()
{
    // destroy vsync event observer
    if (mVsyncObserver.get()) {
        mVsyncObserver->requestExit();
        mVsyncObserver = 0;
    }

    // destroy blank control
    if (mBlankControl) {
        delete mBlankControl;
        mBlankControl = 0;
    }

    // destroy vsync control
    if (mVsyncControl) {
        delete mVsyncControl;
        mVsyncControl = 0;
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

    return (mConnection == DEVICE_CONNECTED) ? true : false;
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

    //Mutex::Autolock _l(mLock);

    if (mConnection != DEVICE_CONNECTED)
        return;

    // notify hwc
    mHwc.vsync(mType, timestamp);
}

void PhysicalDevice::dump(Dump& d)
{
    d.append("-------------------------------------------------------------\n");
    d.append("Device Name: %s (%s)\n", mName,
            mConnection ? "connected" : "disconnected");
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
