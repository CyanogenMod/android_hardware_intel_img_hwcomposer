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
#include <Hwcomposer.h>
#include <DisplayPlane.h>

namespace android {
namespace intel {

DisplayPlane::DisplayPlane(int index, int type, int disp)
    : mIndex(index),
      mType(type),
      mDevice(disp),
      mInitialized(false),
      mDataBuffers(),
      mTransform(PLANE_TRANSFORM_0)
{
    LOGV("DisplayPlane");

    mPosition.x = 0;
    mPosition.y = 0;
    mPosition.w = 0;
    mPosition.h = 0;

    mSrcCrop.x = 0;
    mSrcCrop.y = 0;
    mSrcCrop.w = 0;
    mSrcCrop.h = 0;
}

DisplayPlane::~DisplayPlane()
{
    LOGV("~DisplayPlane");
    deinitialize();
}

bool DisplayPlane::initialize(uint32_t bufferCount)
{
    LOGV("DisplayPlane::initialize");

    // create buffer cache
    mDataBuffers.setCapacity(bufferCount);

    mInitialized = true;
    return true;
}

void DisplayPlane::deinitialize()
{
    // invalid buffer cache
    invalidateBufferCache();

    mInitialized = false;
}

void DisplayPlane::setPosition(int x, int y, int w, int h)
{
    LOGV("DisplayPlane::setPosition: %d, %d - %dx%d", x, y, w, h);

    mPosition.x = x;
    mPosition.y = y;
    mPosition.w = w;
    mPosition.h = h;
}

void DisplayPlane::setSourceCrop(int x, int y, int w, int h)
{
    LOGV("setSourceCrop: %d, %d - %dx%d", x, y, w, h);

    mSrcCrop.x = x;
    mSrcCrop.y = y;
    mSrcCrop.w = w;
    mSrcCrop.h = h;
}

void DisplayPlane::setTransform(int trans)
{
    LOGV("DisplayPlane::setTransform: %d", trans);

    switch (trans) {
    case PLANE_TRANSFORM_90:
    case PLANE_TRANSFORM_180:
    case PLANE_TRANSFORM_270:
        mTransform = trans;
        break;
    default:
        mTransform = PLANE_TRANSFORM_0;
    }
}

bool DisplayPlane::setDataBuffer(uint32_t handle)
{
    DataBuffer *buffer;
    BufferMapper *mapper;
    ssize_t index;
    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();

    LOGV("DisplayPlane::setDataBuffer 0x%x", handle);

    if (!initCheck()) {
        LOGE("DisplayPlane::setDataBuffer: plane hasn't been initialized");
        return false;
    }

    if (!handle) {
        LOGE("DisplayPlane::setDataBuffer: invalid buffer handle");
        return false;
    }

    if (!bm) {
        LOGE("DisplayPlane::setDataBuffer: failed to get buffer manager");
        return false;
    }

    buffer = bm->get(handle);
    if (!buffer) {
        LOGE("DisplayPlane::setDataBuffer: failed to get buffer");
        return false;
    }

    // update buffer's source crop
    buffer->setCrop(mSrcCrop.x, mSrcCrop.y, mSrcCrop.w, mSrcCrop.h);

    // map buffer if it's not in cache
    index = mDataBuffers.indexOfKey(buffer->getKey());
    if (index < 0) {
        LOGV("DisplayPlane::setDataBuffer: unmapped buffer, mapping...");
        mapper = bm->map(*buffer);
        if (!mapper) {
            LOGE("DisplayPlane::setDataBuffer: failed to map buffer");
            goto mapper_err;
        }

        // add it to data buffers
        index = mDataBuffers.add(buffer->getKey(), mapper);
        if (index < 0) {
            LOGE("DisplayPlane::setDataBuffer: failed to add mapper");
            goto add_err;
        }
    } else {
        LOGV("DisplayPlane::setDataBuffer: got mapper in saved data buffers");
        mapper = mDataBuffers.valueAt(index);
    }

    // put buffer after getting mapper
    bm->put(*buffer);

    return setDataBuffer(*mapper);
add_err:
    bm->unmap(*mapper);
mapper_err:
    bm->put(*buffer);
    return false;
}

void DisplayPlane::invalidateBufferCache()
{
    BufferManager *bm = Hwcomposer::getInstance().getBufferManager();
    BufferMapper* mapper;

    LOGV("DisplayPlane::invalidateBufferCache");

    if (!initCheck()) {
        LOGE("DisplayPlane:invalidateBufferCache: plane hasn't been initialized");
        return;
    }

    if (!bm) {
        LOGE("DisplayPlane::invalidateBufferCache: failed to get buffer manager");
        return;
    }

    for (size_t i = 0; i < mDataBuffers.size(); i++) {
        mapper = mDataBuffers.valueAt(i);
        // unmap it
        if (mapper)
            bm->unmap(*mapper);
    }

    // clear recorded data buffers
    mDataBuffers.clear();
}

bool DisplayPlane::assignToDevice(int disp)
{
    LOGV("DisplayPlane::assignToDevice: disp = %d", disp);

    if (!initCheck()) {
        LOGE("DisplayPlane::assignToDevice: plane hasn't been initialized");
        return false;
    }

    mDevice = disp;
    return true;
}

} // namespace intel
} // namespace android
