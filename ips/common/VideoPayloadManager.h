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
#ifndef VIDEO_PAYLOAD_MANAGER_H
#define VIDEO_PAYLOAD_MANAGER_H

#include <IVideoPayloadManager.h>

namespace android {
namespace intel {

class BufferMapper;

class VideoPayloadManager : public IVideoPayloadManager {

public:
    VideoPayloadManager();
    virtual ~VideoPayloadManager();

    // IVideoPayloadManager
public:
    virtual bool getMetaData(BufferMapper *mapper, MetaData *metadata);
    virtual bool setRenderStatus(BufferMapper *mapper, bool renderStatus);

}; // class VideoPayloadManager

} // namespace intel
} // namespace android

#endif /* VIDEO_PAYLOAD_MANAGER_H */
