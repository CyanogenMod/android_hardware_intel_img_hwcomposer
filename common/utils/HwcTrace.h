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
#ifndef HWC_TRACE_H
#define HWC_TRACE_H

#define LOG_TAG "hwcomposer"
//#define LOG_NDEBUG 0
#include <cutils/log.h>


#ifdef _cplusplus
extern "C" {
#endif

// Helper to automatically preappend classname::functionname to the log message
#define VTRACE(fmt,...)     ALOGV("%s: "fmt, __func__, ##__VA_ARGS__)
#define DTRACE(fmt,...)     ALOGD("%s: "fmt, __func__, ##__VA_ARGS__)
#define ITRACE(fmt,...)     ALOGI("%s: "fmt, __func__, ##__VA_ARGS__)
#define WTRACE(fmt,...)     ALOGW("%s: "fmt, __func__, ##__VA_ARGS__)
#define ETRACE(fmt,...)     ALOGE("%s: "fmt, __func__, ##__VA_ARGS__)


// Function call tracing
#if 0
#define CTRACE()            ALOGV("Calling %s", __func__)
#define XTRACE()            ALOGV("Leaving %s", __func__)
#else
#define CTRACE()            ((void)0)
#define XTRACE()            ((void)0)
#endif


// Arguments tracing
#if 0
#define ATRACE(fmt,...)     ALOGV("%s(args): "fmt, __func__, ##__VA_ARGS__);
#else
#define ATRACE(fmt,...)     ((void)0)
#endif



// Helper to abort the execution if object is not initialized.
// This should never happen if the rules below are followed during design:
// 1) Create an object.
// 2) Initialize the object immediately.
// 3) If failed, delete the object.
// These helpers should be disabled and stripped out of release build

#define RETURN_X_IF_NOT_INIT(X) \
do { \
    CTRACE(); \
    if (false == mInitialized) { \
        LOG_ALWAYS_FATAL("%s: Object is not initialized! Line = %d", __func__, __LINE__); \
        return X; \
    } \
} while (0)

#if 1
#define RETURN_FALSE_IF_NOT_INIT()      RETURN_X_IF_NOT_INIT(false)
#define RETURN_VOID_IF_NOT_INIT()       RETURN_X_IF_NOT_INIT()
#define RETURN_NULL_IF_NOT_INIT()       RETURN_X_IF_NOT_INIT(0)
#else
#define RETURN_FALSE_IF_NOT_INIT()      ((void)0)
#define RETURN_VOID_IF_NOT_INIT()       ((void)0)
#define RETURN_NULL_IF_NOT_INIT()       ((void)0)
#endif


// Helper to log error message, call de-initializer and return false.
#define DEINIT_AND_RETURN_FALSE(...) \
do { \
    ETRACE(__VA_ARGS__); \
    deinitialize(); \
    return false; \
} while (0)


#define DEINIT_AND_DELETE_OBJ(X) \
    if (X) {\
        X->deinitialize();\
        delete X; \
        X = NULL; \
    }


#define WARN_IF_NOT_DEINIT() \
    CTRACE(); \
    if (mInitialized) {\
        LOG_ALWAYS_FATAL("%s: Object is not deinitialized! Line = %d", __func__, __LINE__); \
    }


// _cplusplus
#ifdef _cplusplus
}
#endif


#endif /* HWC_TRACE_H */
