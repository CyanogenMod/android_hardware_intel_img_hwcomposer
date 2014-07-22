/*
 * Copyright © 2013 Intel Corporation
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
 *    Robert Crabtree <robert.crabtree@intel.com>
 *
 */
#ifndef IVIDEO_PAYLOAD_MANAGER_H
#define IVIDEO_PAYLOAD_MANAGER_H

#include <hardware/hwcomposer.h>

namespace android {
namespace intel {

class BufferMapper;

class IVideoPayloadManager {
public:
    IVideoPayloadManager() {}
    virtual ~IVideoPayloadManager() {}

public:
    struct MetaData {
        uint32_t kHandle;
        uint32_t transform;
        uint32_t width;
        uint32_t height;
        uint32_t format;
        uint16_t lumaStride;
        uint16_t chromaUStride;
        uint16_t chromaVStride;
        int64_t  timestamp;
        uint32_t crop_width;
        uint32_t crop_height;
        // Downscaling
        uint32_t scaling_khandle;
        uint32_t scaling_width;
        uint32_t scaling_height;
        uint32_t scaling_luma_stride;
        uint32_t scaling_chroma_u_stride;
        uint32_t scaling_chroma_v_stride;
    };

public:
    virtual bool getMetaData(BufferMapper *mapper, MetaData *metadata) = 0;
    virtual bool setRenderStatus(BufferMapper *mapper, bool renderStatus) = 0;
};

} // namespace intel
} // namespace android

#endif /* IVIDEO_PAYLOAD_MANAGER_H */
