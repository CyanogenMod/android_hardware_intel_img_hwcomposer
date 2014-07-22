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
#include <PlatfBufferManager.h>
#include <tangier/TngGrallocBuffer.h>
#include <tangier/TngGrallocBufferMapper.h>

namespace android {
namespace intel {

PlatfBufferManager::PlatfBufferManager()
    : BufferManager()
{

}

PlatfBufferManager::~PlatfBufferManager()
{

}

bool PlatfBufferManager::initialize()
{
    return BufferManager::initialize();
}

void PlatfBufferManager::deinitialize()
{
    BufferManager::deinitialize();
}

DataBuffer* PlatfBufferManager::createDataBuffer(gralloc_module_t *module,
                                                 uint32_t handle)
{
    return new TngGrallocBuffer(handle);
}

BufferMapper* PlatfBufferManager::createBufferMapper(gralloc_module_t *module,
                                                        DataBuffer& buffer)
{
    if (!module)
        return 0;

    return new TngGrallocBufferMapper(*(IMG_gralloc_module_public_t*)module,
                                        buffer);
}

} // namespace intel
} // namespace android
