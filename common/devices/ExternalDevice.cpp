/*
// Copyright (c) 2014 Intel Corporation 
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <common/utils/HwcTrace.h>
#include <common/base/Drm.h>
#include <DrmConfig.h>
#include <Hwcomposer.h>
#include <ExternalDevice.h>
#include <cutils/properties.h>

#define HDMI_WIDTH_PROPERTY     "persist.hdmi.width"
#define HDMI_HEIGHT_PROPERTY    "persist.hdmi.height"
#define HDMI_WIDTH_DEFAULT      1920
#define HDMI_HEIGHT_DEFAULT     1080


namespace android {
namespace intel {

ExternalDevice::ExternalDevice(Hwcomposer& hwc, DisplayPlaneManager& dpm)
    : PhysicalDevice(DEVICE_EXTERNAL, hwc, dpm),
      mHdcpControl(NULL),
      mAbortModeSettingCond(),
      mPendingDrmMode(),
      mHotplugEventPending(false),
      mExpectedRefreshRate(0),
      mDefaultWidth(HDMI_WIDTH_DEFAULT),
      mDefaultHeight(HDMI_HEIGHT_DEFAULT)
{
    CTRACE();
}

ExternalDevice::~ExternalDevice()
{
    CTRACE();
}

bool ExternalDevice::initialize()
{
    if (!PhysicalDevice::initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to initialize physical device");
    }

    mHdcpControl = createHdcpControl();
    if (!mHdcpControl) {
        DEINIT_AND_RETURN_FALSE("failed to create HDCP control");
    }

    mHotplugEventPending = false;
    if (mConnected) {
        mHdcpControl->startHdcpAsync(HdcpLinkStatusListener, this);
    }

#ifdef INTEL_SUPPORT_HDMI_PRIMARY
    if (mConnected) {
        drmModeModeInfo mode;
        Hwcomposer::getInstance().getDrm()->getModeInfo(mType, mode);
        mDefaultWidth = mode.hdisplay;
        mDefaultHeight = mode.vdisplay;
        char prop[PROPERTY_VALUE_MAX];
        sprintf(prop, "%d", mDefaultWidth);
        if (property_set(HDMI_WIDTH_PROPERTY, prop) != 0) {
            ETRACE("failed to set property %s", HDMI_WIDTH_PROPERTY);
        }
        sprintf(prop, "%d", mDefaultHeight);
        if (property_set(HDMI_HEIGHT_PROPERTY, prop) != 0) {
            ETRACE("failed to set property %s", HDMI_HEIGHT_PROPERTY);
        }
    } else {
        mDefaultWidth = property_get_int32(HDMI_WIDTH_PROPERTY, HDMI_WIDTH_DEFAULT);
        mDefaultHeight = property_get_int32(HDMI_HEIGHT_PROPERTY, HDMI_HEIGHT_DEFAULT);
    }
#endif

    UeventObserver *observer = Hwcomposer::getInstance().getUeventObserver();
    if (observer) {
        observer->registerListener(
            DrmConfig::getHotplugString(),
            hotplugEventListener,
            this);
    } else {
        ETRACE("Uevent observer is NULL");
    }
    return true;
}

void ExternalDevice::deinitialize()
{
    // abort mode settings if it is in the middle
    mAbortModeSettingCond.signal();
    if (mThread.get()) {
        mThread->join();
        mThread = NULL;
    }

    if (mHdcpControl) {
        mHdcpControl->stopHdcp();
        delete mHdcpControl;
        mHdcpControl = 0;
    }

    mHotplugEventPending = false;
    PhysicalDevice::deinitialize();
}

bool ExternalDevice::setDrmMode(drmModeModeInfo& value)
{
    if (!mConnected) {
        WTRACE("external device is not connected");
        return false;
    }

    if (mThread.get()) {
        mThread->join();
        mThread = NULL;
    }

    Drm *drm = Hwcomposer::getInstance().getDrm();
    drmModeModeInfo mode;
    drm->getModeInfo(mType, mode);
    if (drm->isSameDrmMode(&value, &mode))
        return true;

    // any issue here by faking connection status?
    mConnected = false;
    mPendingDrmMode = value;

    // setting mode in a working thread
    mThread = new ModeSettingThread(this);
    if (!mThread.get()) {
        ETRACE("failed to create mode settings thread");
        return false;
    }

    mThread->run("ModeSettingsThread", PRIORITY_URGENT_DISPLAY);
    return true;
}

bool ExternalDevice::threadLoop()
{
    // one-time execution
    setDrmMode();
    return false;
}

void ExternalDevice::setDrmMode()
{
    ITRACE("start mode setting...");

    Drm *drm = Hwcomposer::getInstance().getDrm();

    mConnected = false;
    mHwc.hotplug(mType, false);

    {
        Mutex::Autolock lock(mLock);
        // TODO: make timeout value flexible, or wait until surface flinger
        // acknowledges hot unplug event.
        status_t err = mAbortModeSettingCond.waitRelative(mLock, milliseconds(20));
        if (err != -ETIMEDOUT) {
            ITRACE("Mode settings is interrupted");
            mHwc.hotplug(mType, true);
            return;
        }
    }

    // TODO: potential threading issue with onHotplug callback
    mHdcpControl->stopHdcp();
    if (!drm->setDrmMode(mType, mPendingDrmMode)) {
        ETRACE("failed to set Drm mode");
        mHwc.hotplug(mType, true);
        return;
    }

    if (!PhysicalDevice::updateDisplayConfigs()) {
        ETRACE("failed to update display configs");
        mHwc.hotplug(mType, true);
        return;
    }
    mConnected = true;
    mHotplugEventPending = true;
    // delay sending hotplug event until HDCP is authenticated
    if (mHdcpControl->startHdcpAsync(HdcpLinkStatusListener, this) == false) {
        ETRACE("startHdcpAsync() failed; HDCP is not enabled");
        mHotplugEventPending = false;
        mHwc.hotplug(mType, true);
    }
    mExpectedRefreshRate = 0;
}


void ExternalDevice::HdcpLinkStatusListener(bool success, void *userData)
{
    if (userData == NULL) {
        return;
    }

    ExternalDevice *p = (ExternalDevice*)userData;
    p->HdcpLinkStatusListener(success);
}

void ExternalDevice::HdcpLinkStatusListener(bool success)
{
    if (mHotplugEventPending) {
        DTRACE("HDCP authentication status %d, sending hotplug event...", success);
        mHwc.hotplug(mType, mConnected);
        mHotplugEventPending = false;
    }
}

void ExternalDevice::hotplugEventListener(void *data)
{
    ExternalDevice *pThis = (ExternalDevice*)data;
    if (pThis) {
        pThis->hotplugListener();
    }
}

void ExternalDevice::hotplugListener()
{
    bool ret;

    CTRACE();

    // abort mode settings if it is in the middle
    mAbortModeSettingCond.signal();

    // remember the current connection status before detection
    bool connected = mConnected;

    // detect display configs
    ret = detectDisplayConfigs();
    if (ret == false) {
        ETRACE("failed to detect display config");
        return;
    }

    ITRACE("hotpug event: %d", mConnected);

    if (connected == mConnected) {
        WTRACE("same connection status detected, hotplug event ignored");
        return;
    }

    if (mConnected == false) {
        mHotplugEventPending = false;
        mHdcpControl->stopHdcp();
        mHwc.hotplug(mType, mConnected);
    } else {
#ifdef INTEL_SUPPORT_HDMI_PRIMARY
        drmModeModeInfo mode;
        Hwcomposer::getInstance().getDrm()->getModeInfo(mType, mode);
        if (mode.hdisplay != mDefaultWidth ||
            mode.vdisplay != mDefaultHeight) {
            WTRACE("default width %d, height %d, new width %d, height %d",
                mDefaultWidth, mDefaultHeight, mode.hdisplay, mode.vdisplay);
            WTRACE("rebooting device...");
            system("reboot");
        }
#endif
        DTRACE("start HDCP asynchronously...");
         // delay sending hotplug event till HDCP is authenticated.
        mHotplugEventPending = true;
        ret = mHdcpControl->startHdcpAsync(HdcpLinkStatusListener, this);
        if (ret == false) {
            ETRACE("failed to start HDCP");
            mHotplugEventPending = false;
            mHwc.hotplug(mType, mConnected);
        }
    }
}

void ExternalDevice::setRefreshRate(int hz)
{
    RETURN_VOID_IF_NOT_INIT();

    ITRACE("setting refresh rate to %d", hz);

    if (mBlank) {
        WTRACE("external device is blank");
        return;
    }

    Drm *drm = Hwcomposer::getInstance().getDrm();
    drmModeModeInfo mode;
    if (!drm->getModeInfo(IDisplayDevice::DEVICE_EXTERNAL, mode))
        return;

    if (hz == 0 && (mode.type & DRM_MODE_TYPE_PREFERRED))
        return;

    if (hz == (int)mode.vrefresh)
        return;

    if (mExpectedRefreshRate != 0 &&
            mExpectedRefreshRate == hz && mHotplugEventPending) {
        ITRACE("Ignore a new refresh setting event because there is a same event is handling");
        return;
    }
    mExpectedRefreshRate = hz;

    ITRACE("changing refresh rate from %d to %d", mode.vrefresh, hz);

    mHdcpControl->stopHdcp();

    drm->setRefreshRate(IDisplayDevice::DEVICE_EXTERNAL, hz);

    mHotplugEventPending = false;
    mHdcpControl->startHdcpAsync(HdcpLinkStatusListener, this);
}


bool ExternalDevice::getDisplaySize(int *width, int *height)
{
#ifndef INTEL_SUPPORT_HDMI_PRIMARY
    return PhysicalDevice::getDisplaySize(width, height);
#else
    if (mConnected)
        return PhysicalDevice::getDisplaySize(width, height);

    if (!width || !height)
        return false;

    *width = mDefaultWidth;
    *height = mDefaultHeight;
    return true;
#endif
}

bool ExternalDevice::getDisplayConfigs(uint32_t *configs, size_t *numConfigs)
{
#ifndef INTEL_SUPPORT_HDMI_PRIMARY
    return PhysicalDevice::getDisplayConfigs(configs, numConfigs);
#else
    if (mConnected)
        return PhysicalDevice::getDisplayConfigs(configs, numConfigs);

    if (!configs || !numConfigs)
        return false;

    *configs = 0;
    *numConfigs = 1;
    return true;
#endif
}

bool ExternalDevice::getDisplayAttributes(uint32_t config,
                                      const uint32_t *attributes,
                                      int32_t *values)
{
#ifndef INTEL_SUPPORT_HDMI_PRIMARY
    return PhysicalDevice::getDisplayAttributes(config, attributes, values);
#else
    if (mConnected)
        return PhysicalDevice::getDisplayAttributes(config, attributes, values);
    if (!attributes || !values)
        return false;
    int i = 0;
    while (attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE) {
        switch (attributes[i]) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            values[i] = 1e9 / 60;
            break;
        case HWC_DISPLAY_WIDTH:
            values[i] = mDefaultWidth;
            break;
        case HWC_DISPLAY_HEIGHT:
            values[i] = mDefaultHeight;
            break;
        case HWC_DISPLAY_DPI_X:
            values[i] = 1;
            break;
        case HWC_DISPLAY_DPI_Y:
            values[i] = 1;
            break;
        default:
            ETRACE("unknown attribute %d", attributes[i]);
            break;
        }
        i++;
    }
    return true;
#endif
}


} // namespace intel
} // namespace android
