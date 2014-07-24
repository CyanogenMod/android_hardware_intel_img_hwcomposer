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
#include <Hwcomposer.h>
#include <DisplayPlaneManager.h>
#include <platforms/merrifield/PlatfVirtualDevice.h>
#include <ips/common/VideoPayloadManager.h>

namespace android {
namespace intel {

PlatfVirtualDevice::PlatfVirtualDevice(Hwcomposer& hwc,
                                       DisplayPlaneManager& dpm)
    : VirtualDevice(hwc, dpm)
{
    CTRACE();
}

PlatfVirtualDevice::~PlatfVirtualDevice()
{
    CTRACE();
}

IVideoPayloadManager* PlatfVirtualDevice::createVideoPayloadManager()
{
    return new VideoPayloadManager();
}


} // namespace intel
} // namespace android


