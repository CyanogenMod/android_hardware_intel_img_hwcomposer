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
#include <hardware/hwcomposer.h>

#include <Log.h>
#include <BufferManager.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();

BufferManager::BufferManager()
    : mGrallocModule(0),
      mBufferPool(0),
      mInitialized(false)
{

}

BufferManager::~BufferManager()
{

}

bool BufferManager::initCheck() const
{
    return mInitialized;
}

bool BufferManager::initialize()
{
    log.v("BufferManager::initialize");

    // create buffer pool
    mBufferPool = new BufferCache(BUFFER_POOL_SIZE);
    if (!mBufferPool) {
        log.e("failed to create gralloc buffer cache\n");
        return false;
    }

    // init gralloc module
    hw_module_t const* module;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module)) {
        log.e("failed to get gralloc module\n");
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

void BufferManager::dump(Dump& d)
{

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

    log.v("BufferManager::map");

    //try to get mapper from pool
    mapper = mBufferPool->getMapper(buffer.getKey());
    if (!mapper) {
        log.v("BufferManager::map: new buffer, will add it");
        mapper = createBufferMapper(mGrallocModule, buffer);
        if (!mapper) {
            log.e("BufferManager::map: failed to allocate mapper");
            goto mapper_err;
        }
        // map buffer
        ret = mapper->map();
        if (!ret) {
            log.e("TngOverlayPlane::getGrallocMapper: failed to map");
            goto map_err;
        }

        // add mapper
        mBufferPool->addMapper(buffer.getKey(), mapper);
    }

    // increase mapper ref count
    mapper->incRef();
    return mapper;
map_err:
    delete mapper;
mapper_err:
    return 0;
}

void BufferManager::unmap(BufferMapper& mapper)
{
    BufferMapper* cachedMapper;
    int refCount;

    log.v("BufferManager::unmap");

    // try to get mapper from pool
    cachedMapper = mBufferPool->getMapper(mapper.getKey());
    if (!cachedMapper) {
        log.e("BufferManager::unmap: invalid buffer mapper");
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
