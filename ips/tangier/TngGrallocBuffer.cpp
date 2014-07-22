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
#include <tangier/TngGrallocBuffer.h>

namespace android {
namespace intel {

TngGrallocBuffer::TngGrallocBuffer(uint32_t handle)
    :GrallocBufferBase(handle)
{
    initBuffer(handle);
}

TngGrallocBuffer::~TngGrallocBuffer()
{
}

void TngGrallocBuffer::resetBuffer(uint32_t handle)
{
    GrallocBufferBase::resetBuffer(handle);
    initBuffer(handle);
}

void TngGrallocBuffer::initBuffer(uint32_t handle)
{
    TngIMGGrallocBuffer *grallocHandle = (TngIMGGrallocBuffer *)handle;

    CTRACE();

    if (!grallocHandle) {
        ETRACE("gralloc handle is null");
        return;
    }

    mFormat = grallocHandle->iFormat;
    mWidth = grallocHandle->iWidth;
    mHeight = grallocHandle->iHeight;
    mUsage = grallocHandle->usage;
    mKey = grallocHandle->ui64Stamp;
    mBpp = grallocHandle->uiBpp;

    // stride can only be initialized after format is set
    initStride();
}


}
}
