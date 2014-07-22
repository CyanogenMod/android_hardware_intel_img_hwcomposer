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
#include <GraphicBuffer.h>

namespace android {
namespace intel {

GraphicBuffer::GraphicBuffer(uint32_t handle)
    : DataBuffer(handle)
{
    initBuffer(handle);
}

void GraphicBuffer::resetBuffer(uint32_t handle)
{
    DataBuffer::resetBuffer(handle);
    initBuffer(handle);
}

bool GraphicBuffer::isProtectedUsage(uint32_t usage)
{
    if (usage == USAGE_INVALID) {
        return false;
    }

    return (usage & GRALLOC_USAGE_PROTECTED) != 0;
}

bool GraphicBuffer::isProtectedBuffer(GraphicBuffer *buffer)
{
    if (buffer == NULL) {
        return false;
    }

    return isProtectedUsage(buffer->mUsage);
}

bool GraphicBuffer::isCompressionUsage(uint32_t usage)
{
    if (usage == USAGE_INVALID) {
        return false;
    }

    return (usage & GRALLOC_USAGE_COMPRESSION) != 0;
}

bool GraphicBuffer::isCompressionBuffer(GraphicBuffer *buffer)
{
    if (buffer == NULL) {
        return false;
    }

    return isCompressionUsage(buffer->mUsage);
}

void GraphicBuffer::initBuffer(uint32_t handle)
{
    mUsage = USAGE_INVALID;
    mBpp = 0;
}

}
}
