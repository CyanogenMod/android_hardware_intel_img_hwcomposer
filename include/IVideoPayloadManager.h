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
