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
#include <HwcTrace.h>
#include <Hwcomposer.h>
#include <DisplayPlane.h>
#include <GraphicBuffer.h>

namespace android {
namespace intel {

DisplayPlane::DisplayPlane(int index, int type, int disp)
    : mIndex(index),
      mType(type),
      mDevice(disp),
      mInitialized(false),
      mDataBuffers(),
      mRingBuffers(),
      mDataBuffersCap(0),
      mIsProtectedBuffer(false),
      mTransform(PLANE_TRANSFORM_0),
      mCurrentDataBuffer(0),
      mUpdateMasks(0)
{
    CTRACE();
    memset(&mPosition, 0, sizeof(mPosition));
    memset(&mSrcCrop, 0, sizeof(mSrcCrop));
}

DisplayPlane::~DisplayPlane()
{
    WARN_IF_NOT_DEINIT();
}

bool DisplayPlane::initialize(uint32_t bufferCount)
{
    CTRACE();

    if (bufferCount < MIN_DATA_BUFFER_COUNT) {
        WTRACE("buffer count %d is too small", bufferCount);
        bufferCount = MIN_DATA_BUFFER_COUNT;
    }

    // create buffer cache, adding few extra slots as buffer rendering is async
    // buffer could still be queued in the display pipeline such that they
    // can't be unmapped
    // TODO: determine the optimal number of extra slots, 1 is minimum
    // otherwise buffer cache will be unnecessarily invalidated
    mDataBuffersCap = bufferCount + 1;
    mDataBuffers.setCapacity(mDataBuffersCap);
    mRingBuffers.setCapacity(MIN_DATA_BUFFER_COUNT);
    mInitialized = true;
    return true;
}

void DisplayPlane::deinitialize()
{
    if (mDataBuffers.size() || mRingBuffers.size()) {
        // invalidateBufferCache will assert if object is not initialized
        // so invoking it only there is buffer to invalidate.
        invalidateBufferCache();
    }
    mCurrentDataBuffer = 0;
    mDataBuffersCap = 0;
    mInitialized = false;
}

void DisplayPlane::checkPosition(int& x, int& y, int& w, int& h)
{
    Drm *drm = Hwcomposer::getInstance().getDrm();
    drmModeModeInfo modeInfo;
    if (!drm->getModeInfo(mDevice, modeInfo)) {
        ETRACE("failed to get mode info");
        return;
    }
    drmModeModeInfoPtr mode = &modeInfo;

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if ((x + w) > mode->hdisplay)
        w = mode->hdisplay - x;
    if ((y + h) > mode->vdisplay)
        h = mode->vdisplay - y;
}

void DisplayPlane::setPosition(int x, int y, int w, int h)
{
    ATRACE("Position = %d, %d - %dx%d", x, y, w, h);

    // if position is unchanged, skip it
    if (mPosition.x == x && mPosition.y == y &&
        mPosition.w == w && mPosition.h == h) {
        mUpdateMasks &= ~PLANE_POSITION_CHANGED;
        return;
    }

    mPosition.x = x;
    mPosition.y = y;
    mPosition.w = w;
    mPosition.h = h;

    mUpdateMasks |= PLANE_POSITION_CHANGED;
}

void DisplayPlane::setSourceCrop(int x, int y, int w, int h)
{
    ATRACE("Source crop = %d, %d - %dx%d", x, y, w, h);

    // if source crop is unchanged, skip it
    if (mSrcCrop.x == x && mSrcCrop.y == y &&
        mSrcCrop.w == w && mSrcCrop.h == h) {
        mUpdateMasks &= ~PLANE_SOURCE_CROP_CHANGED;
        return;
    }

    mSrcCrop.x = x;
    mSrcCrop.y = y;
    mSrcCrop.w = w;
    mSrcCrop.h = h;

    mUpdateMasks |= PLANE_SOURCE_CROP_CHANGED;
}

void DisplayPlane::setTransform(int trans)
{
    ATRACE("transform = %d", trans);

    if (mTransform == trans) {
        mUpdateMasks &= ~PLANE_TRANSFORM_CHANGED;
        return;
    }

    switch (trans) {
    case PLANE_TRANSFORM_90:
    case PLANE_TRANSFORM_180:
    case PLANE_TRANSFORM_270:
        mTransform = trans;
        break;
    default:
        mTransform = PLANE_TRANSFORM_0;
        break;
    }

    mUpdateMasks |= PLANE_TRANSFORM_CHANGED;
}

bool DisplayPlane::setDataBuffer(uint32_t handle)
{
    DataBuffer *buffer;
    BufferMapper *mapper;
    ssize_t index;
    bool ret;
    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();

    RETURN_FALSE_IF_NOT_INIT();
    ATRACE("handle = %#x", handle);

    if (!handle) {
        ETRACE("invalid buffer handle");
        return false;
    }

    // do not need to update the buffer handle
    if (mCurrentDataBuffer != handle)
        mUpdateMasks |= PLANE_BUFFER_CHANGED;
    else
        mUpdateMasks &= ~PLANE_BUFFER_CHANGED;

    // if no update then do Not need set data buffer
    if (!mUpdateMasks)
        return true;

    buffer = bm->get(handle);
    if (!buffer) {
        ETRACE("failed to get buffer");
        return false;
    }

    // update buffer's source crop
    buffer->setCrop(mSrcCrop.x, mSrcCrop.y, mSrcCrop.w, mSrcCrop.h);

    mIsProtectedBuffer = GraphicBuffer::isProtectedBuffer((GraphicBuffer*)buffer);
    // map buffer if it's not in cache
    index = mDataBuffers.indexOfKey(buffer->getKey());
    if (index < 0) {
        VTRACE("unmapped buffer, mapping...");
        mapper = bm->map(*buffer);
        if (!mapper) {
            ETRACE("failed to map buffer, invalidate buffer cache and map again!");
            invalidateBufferCache();
            mapper = bm->map(*buffer);
            if (!mapper) {
                ETRACE("failed to map buffer");
                goto mapper_err;
            }
        }

        if ((int)mDataBuffers.size() == mDataBuffersCap) {
            DTRACE("rebuilding data buffer cache...");
            // remove all stale buffers from the cache and
            // add the most recent used buffers to the cache
            invalidateDataBuffers();
            for (size_t i = 0; i < mRingBuffers.size(); i++) {
                BufferMapper *temp = mRingBuffers.itemAt(i);
                if (mDataBuffers.indexOfKey(temp->getKey()) < 0) {
                    temp->incRef();
                    mDataBuffers.add(temp->getKey(), temp);
                } else {
                    WTRACE("duplicate buffer found in the ring buffers");
                }
            }
        }

        // add it to data buffers
        index = mDataBuffers.add(buffer->getKey(), mapper);
        if (index < 0) {
            ETRACE("failed to add mapper");
            goto add_err;
        }
    } else {
        VTRACE("got mapper in saved data buffers");
        mapper = mDataBuffers.valueAt(index);
    }

    // unmap and remove the oldest buffer
    if (mRingBuffers.size() == MIN_DATA_BUFFER_COUNT) {
        BufferMapper *temp = mRingBuffers.itemAt(0);
        bm->unmap(*temp);
        mRingBuffers.removeAt(0);
    }

    // cache the latest buffer
    mapper->incRef();
    mRingBuffers.push(mapper);

    // put buffer after getting mapper
    bm->put(*buffer);

    ret = setDataBuffer(*mapper);
    if (ret)
        mCurrentDataBuffer = handle;
    return ret;
add_err:
    bm->unmap(*mapper);
mapper_err:
    bm->put(*buffer);
    return false;
}

void DisplayPlane::invalidateBufferCache()
{
    RETURN_VOID_IF_NOT_INIT();

    invalidateDataBuffers();

    // clean up ring buffers
    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();
    for (size_t i = 0; i < mRingBuffers.size(); i++) {
       BufferMapper *mapper = mRingBuffers.itemAt(i);
        if (mapper)
            bm->unmap(*mapper);
    }
    mRingBuffers.clear();
}

void DisplayPlane::invalidateDataBuffers()
{
    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();
    BufferMapper* mapper;

    for (size_t i = 0; i < mDataBuffers.size(); i++) {
        mapper = mDataBuffers.valueAt(i);
        // unmap it
        if (mapper)
            bm->unmap(*mapper);
    }

    // clear recorded data buffers
    mDataBuffers.clear();

    // reset current buffer
    mCurrentDataBuffer = 0;
}

bool DisplayPlane::assignToDevice(int disp)
{
    RETURN_FALSE_IF_NOT_INIT();
    ATRACE("disp = %d", disp);

    mDevice = disp;
    return true;
}

bool DisplayPlane::flip(void *ctx)
{
    RETURN_FALSE_IF_NOT_INIT();

    // don't flip if no updates
    if (!mUpdateMasks)
        return false;
    else
        return true;
}

} // namespace intel
} // namespace android
