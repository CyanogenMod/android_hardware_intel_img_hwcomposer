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

#include <HwcTrace.h>
#include <BufferMapper.h>
#include <common/GrallocSubBuffer.h>
#include <common/VideoPayloadManager.h>
#include <common/VideoPayloadBuffer.h>

namespace android {
namespace intel {

VideoPayloadManager::VideoPayloadManager()
    : IVideoPayloadManager()
{
}

VideoPayloadManager::~VideoPayloadManager()
{
}

bool VideoPayloadManager::getMetaData(BufferMapper *mapper, MetaData *metadata)
{
    if (!mapper || !metadata) {
        ETRACE("Null input params");
        return false;
    }

    VideoPayloadBuffer *p = (VideoPayloadBuffer*) mapper->getCpuAddress(SUB_BUFFER1);
    if (!p) {
        ETRACE("Got null payload from display buffer");
        return false;
    }

    metadata->format = p->format;
    metadata->transform = p->metadata_transform;
    metadata->timestamp = p->timestamp;

    if (p->metadata_transform == 0) {
        metadata->width = p->width;
        metadata->height = p->height;
        metadata->lumaStride = p->luma_stride;
        metadata->chromaUStride = p->chroma_u_stride;
        metadata->chromaVStride = p->chroma_v_stride;
        metadata->kHandle = p->khandle;
        metadata->crop_width = p->crop_width;
        metadata->crop_height = p->crop_height;
    } else {
        metadata->width = p->rotated_width;
        metadata->height = p->rotated_height;
        metadata->lumaStride = p->rotate_luma_stride;
        metadata->chromaUStride = p->rotate_chroma_u_stride;
        metadata->chromaVStride = p->rotate_chroma_v_stride;
        metadata->kHandle = p->rotated_buffer_handle;
        metadata->crop_width = p->crop_height;
        metadata->crop_height = p->crop_width;
    }

    return true;
}

bool VideoPayloadManager::setRenderStatus(BufferMapper *mapper, bool renderStatus)
{
    if (!mapper) {
        ETRACE("Null mapper param");
        return false;
    }

    VideoPayloadBuffer* p = (VideoPayloadBuffer*) mapper->getCpuAddress(SUB_BUFFER1);
    if (!p) {
        ETRACE("Got null payload from display buffer");
        return false;
    }

    p->renderStatus = renderStatus ? 1 : 0;
    return true;
}

} // namespace intel
} // namespace android
