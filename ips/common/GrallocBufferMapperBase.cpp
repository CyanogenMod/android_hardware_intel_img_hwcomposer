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

#include <Drm.h>
#include <Hwcomposer.h>
#include <common/GrallocBufferMapperBase.h>

namespace android {
namespace intel {

GrallocBufferMapperBase::GrallocBufferMapperBase(DataBuffer& buffer)
    : BufferMapper(buffer)
{
    LOGV("GrallocBufferMapperBase::GrallocBufferMapperBase");

    for (int i = 0; i < SUB_BUFFER_MAX; i++) {
        mGttOffsetInPage[i] = 0;
        mCpuAddress[i] = 0;
        mSize[i] = 0;
    }
}

GrallocBufferMapperBase::~GrallocBufferMapperBase()
{
    LOGV("GrallocBufferMapperBase::~GrallocBufferMapperBase");
}

uint32_t GrallocBufferMapperBase::getGttOffsetInPage(int subIndex) const
{
    if (subIndex >= 0 && subIndex < SUB_BUFFER_MAX)
        return mGttOffsetInPage[subIndex];
    return 0;
}

void* GrallocBufferMapperBase::getCpuAddress(int subIndex) const
{
    if (subIndex >=0 && subIndex < SUB_BUFFER_MAX)
        return mCpuAddress[subIndex];
    return 0;
}

uint32_t GrallocBufferMapperBase::getSize(int subIndex) const
{
    if (subIndex >= 0 && subIndex < SUB_BUFFER_MAX)
        return mSize[subIndex];
    return 0;
}

} // namespace intel
} // namespace android
