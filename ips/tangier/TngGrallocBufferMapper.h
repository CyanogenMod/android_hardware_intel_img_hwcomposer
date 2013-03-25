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
#ifndef TNG_GRALLOC_BUFFER_MAPPER_H
#define TNG_GRALLOC_BUFFER_MAPPER_H

#include <BufferMapper.h>
#include <hal_public.h>
#include <common/GrallocBufferMapperBase.h>
#include <tangier/TngGrallocBuffer.h>

namespace android {
namespace intel {

class TngGrallocBufferMapper : public GrallocBufferMapperBase {
public:
    TngGrallocBufferMapper(IMG_gralloc_module_public_t& module,
                               DataBuffer& buffer);
    ~TngGrallocBufferMapper();
public:
    bool map();
    bool unmap();
private:
    bool gttMap(void *vaddr, uint32_t size, uint32_t gttAlign, int *offset);
    bool gttUnmap(void *vaddr);
private:
    IMG_gralloc_module_public_t& mIMGGrallocModule;
    void* mBufferObject;
};

} // namespace intel
} // namespace android

#endif /* TNG_GRALLOC_BUFFER_MAPPER_H */
