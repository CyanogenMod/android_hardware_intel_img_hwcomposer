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
