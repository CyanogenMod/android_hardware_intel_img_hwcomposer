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
#ifndef TNG_PLANE_MANAGER_H
#define TNG_PLANE_MANAGER_H

#include <DisplayPlaneManager.h>
#include <linux/psb_drm.h>

namespace android {
namespace intel {

class TngPlaneManager : public DisplayPlaneManager {
public:
    TngPlaneManager();
    virtual ~TngPlaneManager();

public:
    virtual bool initialize();
    virtual void deinitialize();
    virtual bool isValidZOrder(int dsp, ZOrderConfig& config);
    virtual bool assignPlanes(int dsp, ZOrderConfig& config);
    // TODO: remove this API
    virtual void* getZOrderConfig() const;

protected:
    DisplayPlane* allocPlane(int index, int type);
    DisplayPlane* getPlaneHelper(int dsp, int type);

private:
    struct intel_dc_plane_zorder mZorder;
};

} // namespace intel
} // namespace android


#endif /* TNG_PLANE_MANAGER_H */
