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
#ifndef IPOWER_MANAGER_H
#define IPOWER_MANAGER_H

namespace android {
namespace intel {

class IPowerManager {
public:
    virtual ~IPowerManager() {}

public:
    virtual bool initialize() = 0;
    virtual void deinitialize() = 0;
    virtual bool isSupported() = 0;
    virtual bool isEnabled() = 0;
    virtual void enableIdleControl() = 0;
    virtual void disableIdleControl() = 0;
    virtual void enterIdleState() = 0;
    virtual void exitIdleState() = 0;
    virtual void setIdleReady() = 0;
    virtual bool getIdleReady() = 0;
    virtual void resetIdleControl() = 0;

}; //class IPowerManager

} // intel
} // android

#endif
