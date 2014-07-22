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
#ifndef BUFFERCACHE_H_
#define BUFFERCACHE_H_

#include <utils/KeyedVector.h>
#include <BufferMapper.h>

namespace android {
namespace intel {

// Generic buffer cache
class BufferCache {
public:
    BufferCache(int size);
    virtual ~BufferCache();
    // add a new mapper into buffer cache
    virtual bool addMapper(uint64_t handle, BufferMapper* mapper);
    //remove mapper
    virtual void removeMapper(BufferMapper* mapper);
    // get a buffer mapper
    virtual BufferMapper* getMapper(uint64_t handle);
    // get cache size
    virtual size_t getCacheSize() const;
    // get mapper with an index
    virtual BufferMapper* getMapper(size_t index);
private:
    KeyedVector<uint64_t, BufferMapper*> mBufferPool;
};

}
}


#endif /* BUFFERCACHE_H_ */
