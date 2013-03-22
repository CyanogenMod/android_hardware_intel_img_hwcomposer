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
#include <Log.h>
#include <Hwcomposer.h>
#include <DisplayPlane.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();

DisplayPlane::DisplayPlane(int index, int type, int disp)
    : mIndex(index),
      mType(type),
      mDevice(disp),
      mInitialized(false),
      mGrallocBufferCache(0),
      mTransform(PLANE_TRANSFORM_0)
{
    log.v("DisplayPlane");

    mPosition.x = 0;
    mPosition.y = 0;
    mPosition.w = 0;
    mPosition.h = 0;

    mSrcCrop.x = 0;
    mSrcCrop.y = 0;
    mSrcCrop.w = 0;
    mSrcCrop.h = 0;

    // initialize
    initialize();
}

DisplayPlane::~DisplayPlane()
{
    log.v("~DisplayPlane");
}

bool DisplayPlane::initialize()
{
    log.v("DisplayPlane::initialize");

    // create buffer cache
    mGrallocBufferCache = new BufferCache(5);
    if (!mGrallocBufferCache) {
        LOGE("failed to create gralloc buffer cache\n");
        goto cache_err;
    }

    mInitialized = true;
    return true;
cache_err:
    mInitialized = false;
    return false;
}

void DisplayPlane::setPosition(int x, int y, int w, int h)
{
    log.v("DisplayPlane::setPosition: %d, %d - %dx%d", x, y, w, h);

    if (!initCheck()) {
        log.e("DisplayPlane::setPosition: plane hasn't been initialized");
        return;
    }

    mPosition.x = x;
    mPosition.y = y;
    mPosition.w = w;
    mPosition.h = h;
}

void DisplayPlane::setSourceCrop(int x, int y, int w, int h)
{
    log.v("setSourceCrop: %d, %d - %dx%d", x, y, w, h);

    if (!initCheck()) {
        log.e("DisplayPlane::setSourceCrop: plane hasn't been initialized");
        return;
    }

    mSrcCrop.x = x;
    mSrcCrop.y = y;
    mSrcCrop.w = w;
    mSrcCrop.h = h;
}

void DisplayPlane::setTransform(int trans)
{
    log.v("DisplayPlane::setTransform: %d", trans);

    if (!initCheck()) {
        log.e("DisplayPlane::setTransform: plane hasn't been initialized");
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
    }
}

bool DisplayPlane::setDataBuffer(uint32_t handle)
{
    DataBuffer *buffer;
    BufferMapper *mapper;
    BufferManager& bm(Hwcomposer::getInstance().getBufferManager());
    bool ret;

    log.v("DisplayPlane::setDataBuffer 0x%x", handle);

    if (!initCheck()) {
        log.e("DisplayPlane::setDataBuffer: plane hasn't been initialized");
        return false;
    }

    if (!handle) {
        log.e("DisplayPlane::setDataBuffer: invalid buffer handle");
        return false;
    }

    buffer = bm.get(handle);
    if (!buffer) {
        log.e("DisplayPlane::setDataBuffer: failed to get buffer");
        return false;
    }

    // update buffer's source crop
    buffer->setCrop(mSrcCrop.x, mSrcCrop.y, mSrcCrop.w, mSrcCrop.h);

    // map buffer if it's not in cache
    mapper = mGrallocBufferCache->getMapper(buffer->getKey());
    if (!mapper) {
        log.v("DisplayPlane::setDataBuffer: new buffer, will add it");
        mapper = bm.map(*buffer);
        if (!mapper) {
            log.e("DisplayPlane::setDataBuffer: failed to map buffer");
            goto mapper_err;
        }

        // add mapper
        mGrallocBufferCache->addMapper(buffer->getKey(), mapper);
    }

    // put buffer after getting mapper
    bm.put(*buffer);

    return setDataBuffer(*mapper);
mapper_err:
    bm.put(*buffer);
    return false;
}

void DisplayPlane::invalidateBufferCache()
{
    BufferManager& bm(Hwcomposer::getInstance().getBufferManager());
    BufferMapper* mapper;

    log.v("DisplayPlane::invalidateBufferCache");

    if (!initCheck()) {
        log.e("DisplayPlane:invalidateBufferCache: plane hasn't been initialized");
        return;
    }

    for (size_t i = 0; i < mGrallocBufferCache->getCacheSize(); i++) {
        mapper = mGrallocBufferCache->getMapper(i);
        if (mapper) {
            // unmap it
            bm.unmap(*mapper);
            // remove it from buffer cache
            mGrallocBufferCache->removeMapper(mapper);
        }
    }
}

bool DisplayPlane::assignToDevice(int disp)
{
    log.v("DisplayPlane::assignToDevice: disp = %d", disp);

    if (!initCheck()) {
        log.e("DisplayPlane::assignToDevice: plane hasn't been initialized");
        return false;
    }

    mDevice = disp;
    return true;
}

} // namespace intel
} // namespace android
