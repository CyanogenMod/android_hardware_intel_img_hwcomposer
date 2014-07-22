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
#ifndef GRALLOC_BUFFER_MAPPER_BASE_H
#define GRALLOC_BUFFER_MAPPER_BASE_H

#include <BufferMapper.h>
#include <ips/common/GrallocSubBuffer.h>
#include <ips/common/GrallocBufferBase.h>

namespace android {
namespace intel {

class GrallocBufferMapperBase : public BufferMapper {
public:
    GrallocBufferMapperBase(DataBuffer& buffer);
    virtual ~GrallocBufferMapperBase();
public:
    virtual bool map() = 0;
    virtual bool unmap() = 0;

    uint32_t getGttOffsetInPage(int subIndex) const;
    void* getCpuAddress(int subIndex) const;
    uint32_t getSize(int subIndex) const;
    virtual uint32_t getKHandle(int subIndex);
    virtual uint32_t getFbHandle(int subIndex) = 0;
    virtual void putFbHandle() = 0;

protected:
    // mapped info
    uint32_t mGttOffsetInPage[SUB_BUFFER_MAX];
    void* mCpuAddress[SUB_BUFFER_MAX];
    uint32_t mSize[SUB_BUFFER_MAX];
    uint32_t mKHandle[SUB_BUFFER_MAX];
};

} // namespace intel
} // namespace android

#endif /* TNG_GRALLOC_BUFFER_MAPPER_H */
