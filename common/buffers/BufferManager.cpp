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
#include <hardware/hwcomposer.h>
#include <BufferManager.h>
#include <DrmConfig.h>

namespace android {
namespace intel {

BufferManager::BufferManager()
    : mGrallocModule(NULL),
      mAllocDev(NULL),
      mFrameBuffers(),
      mBufferPool(NULL),
      mDataBuffer(NULL),
      mDataBufferLock(),
      mInitialized(false)
{
    CTRACE();
}

BufferManager::~BufferManager()
{
    WARN_IF_NOT_DEINIT();
}

bool BufferManager::initCheck() const
{
    return mInitialized;
}

bool BufferManager::initialize()
{
    CTRACE();

    // create buffer pool
    mBufferPool = new BufferCache(DEFAULT_BUFFER_POOL_SIZE);
    if (!mBufferPool) {
        ETRACE("failed to create gralloc buffer cache");
        return false;
    }

    // init gralloc module
    hw_module_t const* module;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module)) {
        DEINIT_AND_RETURN_FALSE("failed to get gralloc module");
    }
    mGrallocModule = (gralloc_module_t*)module;

    gralloc_open(module, &mAllocDev);
    if (!mAllocDev) {
        DEINIT_AND_RETURN_FALSE("failed to open alloc device");
    }

    // create a dummy data buffer
    mDataBuffer = createDataBuffer(mGrallocModule, 0);
    if (!mDataBuffer) {
        DEINIT_AND_RETURN_FALSE("failed to create data buffer");
    }

    mInitialized = true;
    return true;
}

void BufferManager::deinitialize()
{
    mInitialized = false;

    if (mBufferPool) {
        // unmap & delete all cached buffer mappers
        for (size_t i = 0; i < mBufferPool->getCacheSize(); i++) {
            BufferMapper *mapper = mBufferPool->getMapper(i);
            mapper->unmap();
            delete mapper;
        }

        delete mBufferPool;
        mBufferPool = NULL;
    }

    for (size_t j = 0; j < mFrameBuffers.size(); j++) {
        BufferMapper *mapper = mFrameBuffers.valueAt(j);
        mapper->unmap();
        delete mapper;
    }
    mFrameBuffers.clear();

    if (mAllocDev) {
        gralloc_close(mAllocDev);
        mAllocDev = NULL;
    }

    if (mDataBuffer) {
        delete mDataBuffer;
        mDataBuffer = NULL;
    }
}

void BufferManager::dump(Dump& d)
{
    d.append("Buffer Manager status: pool size %d\n", mBufferPool->getCacheSize());
    d.append("-------------------------------------------------------------\n");
    for (size_t i = 0; i < mBufferPool->getCacheSize(); i++) {
        BufferMapper *mapper = mBufferPool->getMapper(i);
        d.append("Buffer %d: handle %#x, (%dx%d), format %d, refCount %d\n",
                 i,
                 mapper->getHandle(),
                 mapper->getWidth(),
                 mapper->getHeight(),
                 mapper->getFormat(),
                 mapper->getRef());
    }
    return;
}

DataBuffer* BufferManager::lockDataBuffer(uint32_t handle)
{
    mDataBufferLock.lock();
    mDataBuffer->resetBuffer(handle);
    return mDataBuffer;
}

void BufferManager::unlockDataBuffer(DataBuffer *buffer)
{
    mDataBufferLock.unlock();
}

DataBuffer* BufferManager::get(uint32_t handle)
{
    return createDataBuffer(mGrallocModule, handle);
}

void BufferManager::put(DataBuffer *buffer)
{
    delete buffer;
}

BufferMapper* BufferManager::map(DataBuffer& buffer)
{
    bool ret;
    BufferMapper* mapper;

    CTRACE();

    //try to get mapper from pool
    mapper = mBufferPool->getMapper(buffer.getKey());
    if (mapper) {
        // increase mapper ref count
        mapper->incRef();
        return mapper;
    }

    // create a new buffer mapper and add it to pool
    do {
        VTRACE("new buffer, will add it");
        mapper = createBufferMapper(mGrallocModule, buffer);
        if (!mapper) {
            ETRACE("failed to allocate mapper");
            break;
        }
        ret = mapper->map();
        if (!ret) {
            ETRACE("failed to map");
            break;
        }
        ret = mBufferPool->addMapper(buffer.getKey(), mapper);
        if (!ret) {
            ETRACE("failed to add mapper");
            break;
        }
        // increase mapper ref count
        mapper->incRef();
        return mapper;
    } while (0);

    // error handling
    if (mapper) {
        mapper->unmap();
        delete mapper;
    }
    return NULL;
}

void BufferManager::unmap(BufferMapper *mapper)
{
    if (!mapper) {
        ETRACE("invalid mapper");
        return;
    }

    // unmap & remove this mapper from buffer when refCount = 0
    int refCount = mapper->decRef();
    if (refCount < 0) {
        ETRACE("invalid ref count");
    } else if (!refCount) {
        // remove mapper from buffer pool
        mBufferPool->removeMapper(mapper);
        mapper->unmap();
        delete mapper;
    }
}

uint32_t BufferManager::allocFrameBuffer(int width, int height, int *stride)
{
    RETURN_NULL_IF_NOT_INIT();

    if (!width || !height || !stride) {
        ETRACE("invalid input parameter");
        return 0;
    }

    ITRACE("size of frame buffer to create: %dx%d", width, height);
    uint32_t handle = 0;
    status_t err  = mAllocDev->alloc(
            mAllocDev,
            width,
            height,
            DrmConfig::getFrameBufferFormat(),
            0, // GRALLOC_USAGE_HW_FB
            (buffer_handle_t *)&handle,
            stride);

    if (err != 0) {
        ETRACE("failed to allocate frame buffer, error = %d", err);
        return 0;
    }

    DataBuffer *buffer = NULL;
    BufferMapper *mapper = NULL;

    do {
        buffer = lockDataBuffer(handle);
        if (!buffer) {
            ETRACE("failed to get data buffer, handle = %#x", handle);
            break;
        }

        mapper = createBufferMapper(mGrallocModule, *buffer);
        if (!mapper) {
            ETRACE("failed to create buffer mapper");
            break;
        }

        if (!mapper->map()) {
            ETRACE("failed to map buffer");
            break;
        }

        uint32_t kHandle = mapper->getKHandle(0);
        if (!kHandle) {
            ETRACE("invalid kernel handle");
            break;
        }

        mFrameBuffers.add(kHandle, mapper);
        unlockDataBuffer(buffer);
        return kHandle;
    } while (0);

    // error handling, release all allocated resources
    if (buffer) {
        unlockDataBuffer(buffer);
    }
    if (mapper) {
        mapper->unmap();
        delete mapper;
    }
    mAllocDev->free(mAllocDev, (buffer_handle_t)handle);
    return 0;
}

void BufferManager::freeFrameBuffer(uint32_t kHandle)
{
    RETURN_VOID_IF_NOT_INIT();
    ssize_t index = mFrameBuffers.indexOfKey(kHandle);
    if (index < 0) {
        ETRACE("invalid kernel handle");
        return;
    }

    BufferMapper *mapper = mFrameBuffers.valueAt(index);
    uint32_t handle = mapper->getHandle();
    mapper->unmap();
    delete mapper;
    mFrameBuffers.removeItem(kHandle);
    mAllocDev->free(mAllocDev, (buffer_handle_t)handle);
}

} // namespace intel
} // namespace android
