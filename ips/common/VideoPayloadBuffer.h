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
#ifndef VIDEO_PAYLOAD_BUFFER_H
#define VIDEO_PAYLOAD_BUFFER_H

#include <utils/Timers.h>
namespace android {
namespace intel {

struct VideoPayloadBuffer {
    // transform made by clients (clients to hwc)
    int client_transform;
    int metadata_transform;
    int rotated_width;
    int rotated_height;
    int surface_protected;
    int force_output_method;
    uint32_t rotated_buffer_handle;
    uint32_t renderStatus;
    unsigned int used_by_widi;
    int bob_deinterlace;
    int tiling;
    uint32_t width;
    uint32_t height;
    uint32_t luma_stride;
    uint32_t chroma_u_stride;
    uint32_t chroma_v_stride;
    uint32_t format;
    uint32_t khandle;
    int64_t  timestamp;

    uint32_t rotate_luma_stride;
    uint32_t rotate_chroma_u_stride;
    uint32_t rotate_chroma_v_stride;

    nsecs_t hwc_timestamp;
    uint32_t layer_transform;

    void *native_window;
};


// force output method values
enum {
    FORCE_OUTPUT_INVALID = 0,
    FORCE_OUTPUT_GPU,
    FORCE_OUTPUT_OVERLAY,
};


} // namespace intel
} // namespace android


#endif // VIDEO_PAYLOAD_BUFFER_H


