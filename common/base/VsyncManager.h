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
#ifndef VSYNC_MANAGER_H
#define VSYNC_MANAGER_H

#include <IDisplayDevice.h>
#include <utils/threads.h>

namespace android {
namespace intel {


class VsyncManager {
public:
    VsyncManager(Vector<IDisplayDevice*>& devices);
    virtual ~VsyncManager();

public:
    bool initialize();
    void deinitialize();
    bool handleVsyncControl(int disp, bool enabled);
    void resetVsyncSource();
    int getVsyncSource();

private:
    inline bool enableVsync();
    inline bool disableVsync();

private:
    Vector<IDisplayDevice*>& mDevices;
    bool mInitialized;
    bool mEnableDynamicVsync;
    bool mEnabled;
    int  mVsyncSource;
    Mutex mLock;
};

} // namespace intel
} // namespace android



#endif /* VSYNC_MANAGER_H */
