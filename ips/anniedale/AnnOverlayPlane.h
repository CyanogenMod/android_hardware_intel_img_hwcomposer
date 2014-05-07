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
#ifndef ANN_OVERLAY_PLANE_H
#define ANN_OVERLAY_PLANE_H

#include <utils/KeyedVector.h>
#include <hal_public.h>
#include <DisplayPlane.h>
#include <BufferMapper.h>
#include <common/Wsbm.h>
#include <common/OverlayPlaneBase.h>
#include <common/RotationBufferProvider.h>

namespace android {
namespace intel {

class AnnOverlayPlane : public OverlayPlaneBase {
public:
    AnnOverlayPlane(int index, int disp);
    virtual ~AnnOverlayPlane();

    virtual void setTransform(int transform);
    virtual void setZOrderConfig(ZOrderConfig& config, void *nativeConfig);

    // plane operations
    virtual bool flip(void *ctx);
    virtual bool reset();
    virtual bool enable();
    virtual bool disable();
    virtual void postFlip();
    virtual void* getContext() const;
    virtual bool initialize(uint32_t bufferCount);
    virtual void deinitialize();
    virtual bool rotatedBufferReady(BufferMapper& mapper, BufferMapper* &rotatedMapper);
    virtual bool useOverlayRotation(BufferMapper& mapper);

protected:
    virtual bool setDataBuffer(BufferMapper& mapper);
    virtual bool flush(uint32_t flags);
    virtual bool bufferOffsetSetup(BufferMapper& mapper);
    virtual bool scalingSetup(BufferMapper& mapper);

    virtual void resetBackBuffer(int buf);

    RotationBufferProvider *mRotationBufProvider;

    // rotation config
    uint32_t mRotationConfig;
    // z order config
    uint32_t mZOrderConfig;
    bool mUseOverlayRotation;
    // hardware context
    struct intel_dc_plane_ctx mContext;
};

} // namespace intel
} // namespace android

#endif /* ANN_OVERLAY_PLANE_H */

