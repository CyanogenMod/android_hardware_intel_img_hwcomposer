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
#ifndef HWC_UTILS_H
#define HWC_UTILS_H


namespace android {
namespace intel {

#if 1 //LOG_NDEBUG
#define INIT_CHECK() \
    if (false == mInitialized) { \
        LOGE("Object is not initialized!  File: %s, line %d", __func__, __LINE__); \
        __builtin_trap(); \
        return false; \
    }
#else
#define INIT_CHECK()   // INIT_CHECK will be stripped out of release build
#endif

#define DEINIT_AND_RETURN_FALSE(...) \
    LOGE(__VA_ARGS__); \
    deinitialize(); \
    return false;



} // namespace intel
} // namespace android


#endif /* HWC_UTILS_H */
