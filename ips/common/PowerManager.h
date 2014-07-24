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

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <IPowerManager.h>

namespace android {
namespace intel {
class PowerManager : public IPowerManager
{
public:
    PowerManager();
    virtual ~PowerManager();

public:
    virtual bool initialize();
    virtual void deinitialize();
    virtual bool isSupported();
    virtual bool isEnabled();
    virtual void enableIdleControl();
    virtual void disableIdleControl();
    virtual void enterIdleState();
    virtual void exitIdleState();
    virtual void setIdleReady();
    virtual bool getIdleReady();
    virtual void resetIdleControl();

private:
    // TODO: use property instead
    enum {
        IDLE_THRESHOLD = 60
    };
    bool mSupported;
    bool mEnabled;
    bool mIdle;
    bool mIdleReady;
};

}
}


#endif // POWER_MANAGER_H
