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
#include <cutils/log.h>

#include <hardware/hwcomposer.h>

#include <BufferManager.h>

namespace android {
namespace intel {

BufferManager::BufferManager()
    : mGrallocModule(0),
      mBufferPool(0),
      mInitialized(false)
{

}

BufferManager::~BufferManager()
{
    LOGV("~BufferManager");
    deinitialize();
}

bool BufferManager::initCheck() const
{
    return mInitialized;
}

bool BufferManager::initialize()
{
    LOGV("BufferManager::initialize");

    // create buffer pool
    mBufferPool = new BufferCache(defaultBufferPoolSize);
    if (!mBufferPool) {
        LOGE("failed to create gralloc buffer cache\n");
        return false;
    }

    // init gralloc module
    hw_module_t const* module;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module)) {
        LOGE("failed to get gralloc module\n");
        goto gralloc_err;
    }

    mGrallocModule = (gralloc_module_t*)module;
    mInitialized = true;
    return true;
gralloc_err:
    delete mBufferPool;
    mInitialized = false;
    return false;
}

void BufferManager::deinitialize()
{
    mInitialized = false;

    if (!mBufferPool)
        return;

    // unmap & delete all cached buffer mappers
    for (size_t i = 0; i < mBufferPool->getCacheSize(); i++) {
        BufferMapper *mapper = mBufferPool->getMapper(i);
        if (mapper) {
            mapper->unmap();
            // remove mapper from buffer pool
            mBufferPool->removeMapper(mapper);
            // delete this mapper
            delete mapper;
        }
    }

    // delete buffer pool
    delete mBufferPool;
    mBufferPool = 0;
}

void BufferManager::dump(Dump& d)
{
    //TODO: implement it later
    return;
}

DataBuffer* BufferManager::get(uint32_t handle)
{
    return createDataBuffer(mGrallocModule, handle);
}

void BufferManager::put(DataBuffer& buffer)
{
    delete &buffer;
}

BufferMapper* BufferManager::map(DataBuffer& buffer)
{
    bool ret;
    BufferMapper* mapper;

    LOGV("BufferManager::map");

    //try to get mapper from pool
    mapper = mBufferPool->getMapper(buffer.getKey());
    if (!mapper) {
        LOGV("BufferManager::map: new buffer, will add it");
        mapper = createBufferMapper(mGrallocModule, buffer);
        if (!mapper) {
            LOGE("BufferManager::map: failed to allocate mapper");
            goto mapper_err;
        }
        // map buffer
        ret = mapper->map();
        if (!ret) {
            LOGE("BufferManager::map: failed to map");
            goto map_err;
        }

        // add mapper
        ret = mBufferPool->addMapper(buffer.getKey(), mapper);
        if (!ret) {
            LOGE("BufferManager::map: failed to add mapper");
            goto add_err;
        }
    }

    // increase mapper ref count
    mapper->incRef();
    return mapper;
add_err:
    mapper->unmap();
map_err:
    delete mapper;
mapper_err:
    return 0;
}

void BufferManager::unmap(BufferMapper& mapper)
{
    BufferMapper* cachedMapper;
    int refCount;

    LOGV("BufferManager::unmap");

    // try to get mapper from pool
    cachedMapper = mBufferPool->getMapper(mapper.getKey());
    if (!cachedMapper) {
        LOGE("BufferManager::unmap: invalid buffer mapper");
        return;
    }

    // decrease mapper ref count
    refCount = cachedMapper->decRef();

    // unmap & remove this mapper from buffer when refCount = 0
    if (!refCount) {
        // unmap it
        cachedMapper->unmap();
        // remove mapper from buffer pool
        mBufferPool->removeMapper(cachedMapper);
        // delete this mapper
        delete cachedMapper;
    }
}

} // namespace intel
} // namespace android
