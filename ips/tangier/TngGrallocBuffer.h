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
#ifndef TNG_GRALLOC_BUFFER_H
#define TNG_GRALLOC_BUFFER_H

#include <DataBuffer.h>
#include <hal_public.h>
#include <OMX_IVCommon.h>
#include <penwell/PnwGrallocSubBuffer.h>

namespace android {
namespace intel {

struct IMGGrallocBuffer{
    native_handle_t base;
    int syncFD;
    int fd[SUB_BUFFER_MAX];
    unsigned long long ui64Stamp;
    int usage;
    int width;
    int height;
    int format;
    int bpp;
}__attribute__((aligned(sizeof(int)),packed));


class TngGrallocBuffer : public DataBuffer {
public:
    TngGrallocBuffer(uint32_t handle);
    ~TngGrallocBuffer();
    // gralloc buffer operation
    uint32_t getUsage() const { return mUsage; };
private:
    uint32_t mUsage;
};

} // namespace intel
} // namespace android


#endif /* TNG_GRALLOC_BUFFER_H */
