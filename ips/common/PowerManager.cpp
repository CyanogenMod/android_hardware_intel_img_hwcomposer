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
#include <Hwcomposer.h>
#include <ips/common/PowerManager.h>

namespace android {
namespace intel {

PowerManager::PowerManager()
    : mSupported(false),
      mEnabled(false),
      mIdle(false),
      mIdleReady(false)
{
}

PowerManager::~PowerManager()
{
}

bool PowerManager::initialize()
{
    uint32_t videoMode = 0;
    Drm* drm = Hwcomposer::getInstance().getDrm();
    drm->readIoctl(DRM_PSB_PANEL_QUERY, &videoMode, sizeof(uint32_t));
    if (videoMode == 1) {
        ITRACE("Video mode panel, idle control is supported");
        mSupported = true;
    }
    if (!drm->isConnected(IDisplayDevice::DEVICE_EXTERNAL)) {
        enableIdleControl();
    } else {
        disableIdleControl();
    }
    mIdle = false;
    return true;
}

void PowerManager::deinitialize()
{
    disableIdleControl();
}

bool PowerManager::isSupported()
{
    return mSupported;
}

bool PowerManager::isEnabled()
{
    return mEnabled;
}

void PowerManager::enableIdleControl()
{
    if (!mSupported) {
        return;
    }

    if (mEnabled) {
        WTRACE("idle control has been enabled");
        return;
    }

    DTRACE("enable repeated frame interrupt");
    struct drm_psb_idle_ctrl ctrl = {IDLE_CTRL_ENABLE, IDLE_THRESHOLD};
    Hwcomposer::getInstance().getDrm()->writeIoctl(
        DRM_PSB_IDLE_CTRL, &ctrl, sizeof(struct drm_psb_idle_ctrl));

    mEnabled = true;
    mIdleReady = false;
}

void PowerManager::disableIdleControl()
{
    if (!mEnabled) {
        return;
    }

    DTRACE("disable repeated frame interrupt");
    Drm *drm = Hwcomposer::getInstance().getDrm();
    struct drm_psb_idle_ctrl ctrl = {IDLE_CTRL_DISABLE, 0};
    Hwcomposer::getInstance().getDrm()->writeIoctl(
        DRM_PSB_IDLE_CTRL, &ctrl, sizeof(struct drm_psb_idle_ctrl));

    resetIdleControl();
}

void PowerManager::enterIdleState()
{
    if (!mEnabled || mIdle) {
        WTRACE("Invalid state: enabled %d, idle %d", mEnabled, mIdle);
        return;
    }

    DTRACE("entering s0i1 mode");
    Drm *drm = Hwcomposer::getInstance().getDrm();
    struct drm_psb_idle_ctrl ctrl = {IDLE_CTRL_ENTER, 0};
    Hwcomposer::getInstance().getDrm()->writeIoctl(
        DRM_PSB_IDLE_CTRL, &ctrl, sizeof(struct drm_psb_idle_ctrl));

    mIdle = true;
}

void PowerManager::exitIdleState()
{
    if (!mIdle) {
        WTRACE("invalid idle state");
        return;
    }
    DTRACE("exiting s0i1 mode");
    struct drm_psb_idle_ctrl ctrl = {IDLE_CTRL_EXIT, 0};
    Hwcomposer::getInstance().getDrm()->writeIoctl(
        DRM_PSB_IDLE_CTRL, &ctrl, sizeof(struct drm_psb_idle_ctrl));

    mIdle = false;
    mIdleReady = false;
}

void PowerManager::setIdleReady()
{
    mIdleReady = true;
}

bool PowerManager::getIdleReady()
{
    return mIdleReady;
}

void PowerManager::resetIdleControl()
{
    mEnabled = false;
    mIdle = false;
    mIdleReady = false;
}

}
}

