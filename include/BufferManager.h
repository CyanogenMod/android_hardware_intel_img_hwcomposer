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
#ifndef BUFFERMANAGER_H_
#define BUFFERMANAGER_H_

#include <Dump.h>
#include <DataBuffer.h>
#include <BufferMapper.h>
#include <BufferCache.h>

namespace android {
namespace intel {

// Gralloc Buffer Manager
class BufferManager {
enum {
    BUFFER_POOL_SIZE = 20,
};
public:
    BufferManager();
    virtual ~BufferManager();

    bool initCheck() const;
    bool initialize();

    // dump interface
    void dump(Dump& d);

    DataBuffer* get(uint32_t handle);
    void put(DataBuffer& buffer);

    // map/unmap a data buffer into/from display memory
    BufferMapper* map(DataBuffer& buffer);
    void unmap(BufferMapper& mapper);

protected:
    virtual DataBuffer* createDataBuffer(gralloc_module_t *module,
                                             uint32_t handle) = 0;
    virtual BufferMapper* createBufferMapper(gralloc_module_t *module,
                                                 DataBuffer& buffer) = 0;
private:
    gralloc_module_t *mGrallocModule;
    BufferCache *mBufferPool;
    bool mInitialized;
};

} // namespace intel
} // namespace android

#endif /* BUFFERMANAGER_H_ */
