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
#include <hal_public.h>
#include <common/utils/HwcTrace.h>
#include <ips/common/PowerManager.h>
#include <tangier/TngDisplayContext.h>
#include <tangier/TngPlaneManager.h>
#include <platforms/merrifield/PlatfBufferManager.h>
#include <IDisplayDevice.h>
#include <platforms/merrifield/PlatfPrimaryDevice.h>
#include <platforms/merrifield/PlatfExternalDevice.h>
#include <platforms/merrifield/PlatfVirtualDevice.h>
#include <platforms/merrifield/PlatfHwcomposer.h>



namespace android {
namespace intel {

PlatfHwcomposer::PlatfHwcomposer()
    : Hwcomposer()
{
    CTRACE();
}

PlatfHwcomposer::~PlatfHwcomposer()
{
    CTRACE();
}

DisplayPlaneManager* PlatfHwcomposer::createDisplayPlaneManager()
{
    CTRACE();
    return (new TngPlaneManager());
}

BufferManager* PlatfHwcomposer::createBufferManager()
{
    CTRACE();
    return (new PlatfBufferManager());
}

IDisplayDevice* PlatfHwcomposer::createDisplayDevice(int disp,
                                                     DisplayPlaneManager& dpm)
{
    CTRACE();

    switch (disp) {
        case IDisplayDevice::DEVICE_PRIMARY:
            return new PlatfPrimaryDevice(*this, dpm);
        case IDisplayDevice::DEVICE_EXTERNAL:
            return new PlatfExternalDevice(*this, dpm);
        case IDisplayDevice::DEVICE_VIRTUAL:
            return new PlatfVirtualDevice(*this, dpm);
        default:
            ETRACE("invalid display device %d", disp);
            return NULL;
    }
}

IDisplayContext* PlatfHwcomposer::createDisplayContext()
{
    CTRACE();
    return new TngDisplayContext();
}

IPowerManager* PlatfHwcomposer::createPowerManager()
{
    return new PowerManager();
}

Hwcomposer* Hwcomposer::createHwcomposer()
{
    CTRACE();
    return new PlatfHwcomposer();
}

} //namespace intel
} //namespace android
