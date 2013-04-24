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
#include <BufferCache.h>

namespace android {
namespace intel {

BufferCache::BufferCache(int size)
{
    mBufferPool.setCapacity(size);
    mBufferPool.clear();
}

BufferCache::~BufferCache()
{
    mBufferPool.clear();
}

bool BufferCache::addMapper(uint64_t handle, BufferMapper* mapper)
{
    ssize_t index = mBufferPool.indexOfKey(handle);
    if (index >= 0) {
        ETRACE("buffer %#llx exists", handle);
        return false;
    }

    // add mapper
    index = mBufferPool.add(handle, mapper);
    if (index < 0) {
        ETRACE("failed to add mapper. err = %ld", index);
        return false;
    }

    return true;
}

bool BufferCache::removeMapper(BufferMapper* mapper)
{
    ssize_t index;

    if (!mapper)
        return false;

    index = mBufferPool.removeItem(mapper->getKey());
    if (index < 0) {
        WTRACE("failed to remove mapper. err = %ld", index);
        return false;
    }

    return true;
}

BufferMapper* BufferCache::getMapper(uint64_t handle)
{
    ssize_t index = mBufferPool.indexOfKey(handle);
    if (index < 0)
        return 0;
    return mBufferPool.valueAt(index);
}

size_t BufferCache::getCacheSize() const
{
    return mBufferPool.size();
}

BufferMapper* BufferCache::getMapper(size_t index)
{
    if (index >= mBufferPool.size())
        return 0;
    BufferMapper* mapper = mBufferPool.valueAt(index);
    return mapper;
}

} // namespace intel
} // namespace android
