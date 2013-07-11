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
#include <utils/Mutex.h>

namespace android {
namespace intel {

// Gralloc Buffer Manager
class BufferManager {
public:
    BufferManager();
    virtual ~BufferManager();

    bool initCheck() const;
    virtual bool initialize();
    virtual void deinitialize();

    // dump interface
    void dump(Dump& d);

    // lockDataBuffer and unlockDataBuffer must be used in serial
    // nested calling of them will cause a deadlock
    DataBuffer* lockDataBuffer(uint32_t handle);
    void unlockDataBuffer(DataBuffer *buffer);

    // get and put interfaces are deprecated
    // use lockDataBuffer and unlockDataBuffer instead
    DataBuffer* get(uint32_t handle);
    void put(DataBuffer *buffer);

    // map/unmap a data buffer into/from display memory
    BufferMapper* map(DataBuffer& buffer);
    void unmap(BufferMapper *mapper);

    // frame buffer management
    //return 0 if allocation fails
    virtual uint32_t allocFrameBuffer(int width, int height, int *stride);
    virtual void freeFrameBuffer(uint32_t kHandle);

    uint32_t allocGrallocBuffer(uint32_t width, uint32_t height, uint32_t format, uint32_t usage);
    void freeGrallocBuffer(uint32_t handle);
    virtual bool convertRGBToNV12(uint32_t rgbHandle, uint32_t yuvHandle,
                                  crop_t& srcCrop, uint32_t async) = 0;
protected:
    virtual DataBuffer* createDataBuffer(gralloc_module_t *module,
                                             uint32_t handle) = 0;
    virtual BufferMapper* createBufferMapper(gralloc_module_t *module,
                                                 DataBuffer& buffer) = 0;

    gralloc_module_t *mGrallocModule;
private:
    enum {
        // make the buffer pool large enough
        DEFAULT_BUFFER_POOL_SIZE = 128,
    };

    alloc_device_t *mAllocDev;
    KeyedVector<uint32_t, BufferMapper*> mFrameBuffers;
    BufferCache *mBufferPool;
    DataBuffer *mDataBuffer;
    Mutex mDataBufferLock;
    bool mInitialized;
};

} // namespace intel
} // namespace android

#endif /* BUFFERMANAGER_H_ */
