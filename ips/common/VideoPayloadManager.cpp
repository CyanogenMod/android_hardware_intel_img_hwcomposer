/*
// Copyright (c) 2014 Intel Corporation 
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <common/utils/HwcTrace.h>
#include <BufferMapper.h>
#include <ips/common/GrallocSubBuffer.h>
#include <ips/common/VideoPayloadManager.h>
#include <ips/common/VideoPayloadBuffer.h>

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
    metadata->scaling_khandle = p->scaling_khandle;
    metadata->scaling_width = p->scaling_width;
    metadata->scaling_height = p->scaling_height;

    metadata->scaling_luma_stride = p->scaling_luma_stride;
    metadata->scaling_chroma_u_stride = p->scaling_chroma_u_stride;
    metadata->scaling_chroma_v_stride = p->scaling_chroma_v_stride;

    if (p->metadata_transform == 0) {
        metadata->width = p->width;
        metadata->height = p->height;
        metadata->lumaStride = p->luma_stride;
        metadata->chromaUStride = p->chroma_u_stride;
        metadata->chromaVStride = p->chroma_v_stride;
        metadata->kHandle = p->khandle;
    } else {
        metadata->width = p->rotated_width;
        metadata->height = p->rotated_height;
        metadata->lumaStride = p->rotate_luma_stride;
        metadata->chromaUStride = p->rotate_chroma_u_stride;
        metadata->chromaVStride = p->rotate_chroma_v_stride;
        metadata->kHandle = p->rotated_buffer_handle;
    }
    if (metadata->scaling_khandle)
        VTRACE("Scaled video buffer, %dx%d",
                metadata->scaling_width, metadata->scaling_height);
    if (metadata->transform != 0)
        VTRACE("Rotated video buffer, %dx%d", metadata->width, metadata->height);

    if ((p->metadata_transform == 0) || (p->metadata_transform == HAL_TRANSFORM_ROT_180)) {
        metadata->crop_width = p->crop_width;
        metadata->crop_height = p->crop_height;
    } else {
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
