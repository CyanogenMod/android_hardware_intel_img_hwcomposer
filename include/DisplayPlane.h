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
#ifndef DISPLAYPLANE_H_
#define DISPLAYPLANE_H_

#include <utils/KeyedVector.h>
#include <BufferMapper.h>

namespace android {
namespace intel {

typedef struct {
    // align with android, using 'int' here
    int x;
    int y;
    int w;
    int h;
} PlanePosition;

enum {
    // support up to 4 overlays
    MAX_OVERLAY_COUNT = 4,
    MAX_SPRITE_COUNT = 4,
};

class ZOrderConfig;

class DisplayPlane {
public:
    // transform
    enum {
        PLANE_TRANSFORM_0 = 0,
        PLANE_TRANSFORM_90 = HWC_TRANSFORM_ROT_90,
        PLANE_TRANSFORM_180 = HWC_TRANSFORM_ROT_180,
        PLANE_TRANSFORM_270 = HWC_TRANSFORM_ROT_270,
    };

    // blending
    enum {
        PLANE_BLENDING_NONE = HWC_BLENDING_NONE,
        PLANE_BLENDING_PREMULT = HWC_BLENDING_PREMULT,
    };

    // plane type
    enum {
        PLANE_SPRITE = 0,
        PLANE_OVERLAY,
        PLANE_PRIMARY,
        PLANE_MAX,
    };

    enum {
        // align with android's back buffer count
        // each plane will cache the latest MIN_DATA_BUFFER_COUNT buffers
        // in case that these buffers are still in-using by display device
        // other buffers will be released on cache invalidation
        MIN_DATA_BUFFER_COUNT = 3,
    };

protected:
    enum {
        PLANE_POSITION_CHANGED    = 0x00000001UL,
        PLANE_BUFFER_CHANGED      = 0x00000002UL,
        PLANE_SOURCE_CROP_CHANGED = 0x00000004UL,
        PLANE_TRANSFORM_CHANGED   = 0x00000008UL,
    };
public:
    DisplayPlane(int index, int type, int disp);
    virtual ~DisplayPlane();
public:
    virtual int getIndex() const { return mIndex; }
    virtual int getType() const { return mType; }
    virtual bool initCheck() const { return mInitialized; }

    // data destination
    virtual void setPosition(int x, int y, int w, int h);
    virtual void setSourceCrop(int x, int y, int w, int h);
    virtual void setTransform(int transform);
    virtual void setPlaneAlpha(uint8_t alpha);

    // data source
    virtual bool setDataBuffer(uint32_t handle);

    virtual void invalidateBufferCache();

    // display device
    virtual bool assignToDevice(int disp);

    // hardware operations
    virtual bool flip(void *ctx);
    virtual void postFlip();

    virtual bool reset();
    virtual bool enable() = 0;
    virtual bool disable() = 0;

    // set z order config
    virtual void setZOrderConfig(ZOrderConfig& config,
                                     void *nativeConfig) = 0;

    virtual void setZOrder(int zorder);
    virtual int getZOrder() const;

    virtual void* getContext() const = 0;

    virtual bool initialize(uint32_t bufferCount);
    virtual void deinitialize();

protected:
    virtual void checkPosition(int& x, int& y, int& w, int& h);
    virtual bool setDataBuffer(BufferMapper& mapper) = 0;
private:
    inline BufferMapper* mapBuffer(DataBuffer *buffer);

    inline bool isActiveBuffer(BufferMapper *mapper);
    void updateActiveBuffers(BufferMapper *mapper);
    void invalidateActiveBuffers();
protected:
    int mIndex;
    int mType;
    int mZOrder;
    int mDevice;
    bool mInitialized;

    // cached data buffers
    KeyedVector<uint64_t, BufferMapper*> mDataBuffers;
    // holding the most recent buffers
    Vector<BufferMapper*> mActiveBuffers;
    int mCacheCapacity;

    PlanePosition mPosition;
    crop_t mSrcCrop;
    bool mIsProtectedBuffer;
    int mTransform;
    uint8_t mPlaneAlpha;
    uint32_t mCurrentDataBuffer;
    uint32_t mUpdateMasks;
};

} // namespace intel
} // namespace android

#endif /* DISPLAYPLANE_H_ */
