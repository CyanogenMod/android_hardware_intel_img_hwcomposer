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
#include <common/OverlayHardware.h>
#include <common/VideoPayloadBuffer.h>

namespace android {
namespace intel {

typedef struct {
    OverlayBackBufferBlk *buf;
    uint32_t gttOffsetInPage;
    uint32_t bufObject;
} OverlayBackBuffer;

class AnnOverlayPlane : public DisplayPlane {
public:
    AnnOverlayPlane(int index, int disp);
    virtual ~AnnOverlayPlane();

    virtual bool setDataBuffer(uint32_t handle);

    virtual void invalidateBufferCache();

    virtual void setTransform(int transform);
    virtual bool assignToDevice(int disp);

    virtual void setZOrderConfig(ZOrderConfig& config, void *nativeConfig);

    // plane operations
    virtual bool flip(void *ctx) ;
    virtual bool reset();
    virtual bool enable();
    virtual bool disable();

    virtual void* getContext() const;
    virtual bool initialize(uint32_t bufferCount);
    virtual void deinitialize();

protected:
    // generic overlay register flush
    virtual bool flush(uint32_t flags);
    virtual bool isFlushed();
    virtual bool setDataBuffer(BufferMapper& mapper);
    virtual bool bufferOffsetSetup(BufferMapper& mapper);
    virtual uint32_t calculateSWidthSW(uint32_t offset, uint32_t width);
    virtual bool coordinateSetup(BufferMapper& mapper);
    virtual bool setCoeffRegs(double *coeff, int mantSize,
                                 coeffPtr pCoeff, int pos);
    virtual void updateCoeff(int taps, double fCutoff,
                                bool isHoriz, bool isY,
                                coeffPtr pCoeff);
    virtual bool scalingSetup(BufferMapper& mapper);
    virtual void checkPosition(int& x, int& y, int& w, int& h);

protected:
    // back buffer operations
    virtual OverlayBackBuffer* createBackBuffer();
    virtual void deleteBackBuffer(int buf);
    virtual void resetBackBuffer(int buf);
protected:
    // flush flags
    enum {
        PLANE_ENABLE     = 0x00000001UL,
        PLANE_DISABLE    = 0x00000002UL,
        UPDATE_COEF      = 0x00000004UL,
    };

    enum {
        OVERLAY_BACK_BUFFER_COUNT = 3,
        OVERLAY_DATA_BUFFER_COUNT = 30,
    };

    // overlay back buffer
    OverlayBackBuffer *mBackBuffer[OVERLAY_BACK_BUFFER_COUNT];
    int mCurrent;

    // wsbm
    Wsbm *mWsbm;
    // pipe config
    uint32_t mPipeConfig;

    // variables for asynchronous overlay disabling
    enum {
        // maximum wait count before aborting overlay disabling
        OVERLAY_DISABLING_COUNT_MAX = 60,
    };
    bool mDisablePending;
    bool mDisablePendingDevice;
    int mDisablePendingCount;

    // rotation config
    uint32_t mRotationConfig;
    // z order config
    uint32_t mZOrderConfig;
    // hardware context
    struct intel_dc_plane_ctx mContext;
};

} // namespace intel
} // namespace android

#endif /* ANN_OVERLAY_PLANE_H */

