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
#include <common/PowerManager.h>

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

