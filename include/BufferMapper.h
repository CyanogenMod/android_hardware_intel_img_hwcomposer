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
#ifndef BUFFERMAPPER_H__
#define BUFFERMAPPER_H__

#include <DataBuffer.h>

namespace android {
namespace intel {

class BufferMapper : public DataBuffer {
public:
    BufferMapper(DataBuffer& buffer)
        : DataBuffer(buffer),
          mRefCount(0)
    {
    }
    virtual ~BufferMapper() {}
public:
    int incRef()
    {
        mRefCount++;
        return mRefCount;
    }
    int decRef()
    {
        mRefCount--;
        return mRefCount;
    }

    // map the given buffer into both DC & CPU MMU
    virtual bool map() = 0;
    // unmap the give buffer from both DC & CPU MMU
    virtual bool unmap() = 0;

    // return gtt page offset
    virtual uint32_t getGttOffsetInPage(int subIndex) const = 0;
    virtual void* getCpuAddress(int subIndex) const = 0;
    virtual uint32_t getSize(int subIndex) const = 0;
    virtual uint32_t getKHandle(int subIndex) const = 0;

private:
    int mRefCount;
};

} // namespace intel
} // namespace android

#endif /* BUFFERMAPPER_H__ */
