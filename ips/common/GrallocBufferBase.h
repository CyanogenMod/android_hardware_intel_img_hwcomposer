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
#ifndef GRALLOC_BUFFER_BASE_H
#define GRALLOC_BUFFER_BASE_H

#include <GraphicBuffer.h>
#include <hal_public.h>
// FIXME: remove it, why define NV12_VED based on OMX's value?
#include <OMX_IVCommon.h>

namespace android {
namespace intel {

class GrallocBufferBase : public GraphicBuffer {
public:
    GrallocBufferBase(uint32_t handle);
    virtual ~GrallocBufferBase() {}
    virtual void resetBuffer(uint32_t handle);

protected:
    // helper function to be invoked by the derived class
    void initStride();

private:
    void initBuffer(uint32_t handle);
};

} // namespace intel
} // namespace android

#endif /* GRALLOC_BUFFER_BASE_H */
