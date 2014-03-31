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
#ifndef ANN_DISPLAY_PLANE_H
#define ANN_DISPLAY_PLANE_H

#include <utils/KeyedVector.h>
#include <hal_public.h>
#include <Hwcomposer.h>
#include <BufferCache.h>
#include <DisplayPlane.h>

namespace android {
namespace intel {

class AnnDisplayPlane : public DisplayPlane {
public:
    AnnDisplayPlane(int index, int type, int disp);
    virtual ~AnnDisplayPlane();
public:
    // data destination
    void setPosition(int x, int y, int w, int h);
    void setSourceCrop(int x, int y, int w, int h);
    void setTransform(int transform);
    void setPlaneAlpha(uint8_t alpha, uint32_t blending);

    // data source
    bool setDataBuffer(uint32_t handle);

    void invalidateBufferCache();

    // display device
    bool assignToDevice(int disp);

    // hardware operations
    bool flip(void *ctx);

    bool reset();
    bool enable();
    bool disable();
    bool isDisabled();

    void* getContext() const;

    bool initialize(uint32_t bufferCount);
    void deinitialize();
    void setZOrderConfig(ZOrderConfig& config, void *nativeConfig);

    void setRealPlane(DisplayPlane *plane);
    DisplayPlane* getRealPlane() const;
protected:
    bool setDataBuffer(BufferMapper& mapper);
private:
    DisplayPlane *mRealPlane;
};

} // namespace intel
} // namespace android

#endif /* ANN_DISPLAY_PLANE_H */
