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
#include <HwcTrace.h>
#include <Hwcomposer.h>
#include <BufferManager.h>
#include <tangier/TngGrallocBuffer.h>
#include <anniedale/AnnDisplayPlane.h>
#include <common/PixelFormat.h>

namespace android {
namespace intel {

#define RETURN_VOID_IF_NO_REAL_PLANE() \
         do { \
             if (!mRealPlane) { \
                 VTRACE("No real plane set"); \
                 return; \
             } \
         } while(0)

#define RETURN_FALSE_IF_NO_REAL_PLANE() \
         do { \
             if (!mRealPlane) { \
                 VTRACE("No real plane set"); \
                 return false; \
             } \
         } while(0)

AnnDisplayPlane::AnnDisplayPlane(int index, int type, int disp)
    : DisplayPlane(index, type, disp),
      mRealPlane(0)
{

}

AnnDisplayPlane::~AnnDisplayPlane()
{
    mRealPlane = NULL;
}

void AnnDisplayPlane::setPosition(int x, int y, int w, int h)
{
    RETURN_VOID_IF_NO_REAL_PLANE();

    mRealPlane->setPosition(x, y, w, h);
}

void AnnDisplayPlane::setSourceCrop(int x, int y, int w, int h)
{
    RETURN_VOID_IF_NO_REAL_PLANE();

    mRealPlane->setSourceCrop(x, y, w, h);
}

void AnnDisplayPlane::setTransform(int transform)
{
    RETURN_VOID_IF_NO_REAL_PLANE();

    mRealPlane->setTransform(transform);
}

bool AnnDisplayPlane::setDataBuffer(uint32_t handle)
{
    RETURN_FALSE_IF_NO_REAL_PLANE();

    return mRealPlane->setDataBuffer(handle);
}

void AnnDisplayPlane::invalidateBufferCache()
{
    RETURN_VOID_IF_NO_REAL_PLANE();

    mRealPlane->invalidateBufferCache();
}

bool AnnDisplayPlane::assignToDevice(int disp)
{
    RETURN_FALSE_IF_NO_REAL_PLANE();

    return mRealPlane->assignToDevice(disp);
}

bool AnnDisplayPlane::flip(void *ctx)
{
    RETURN_FALSE_IF_NO_REAL_PLANE();

    return mRealPlane->flip(ctx);
}

bool AnnDisplayPlane::reset()
{
    RETURN_FALSE_IF_NO_REAL_PLANE();

    return mRealPlane->reset();
}

bool AnnDisplayPlane::enable()
{
    return true;
}

bool AnnDisplayPlane::disable()
{
    RETURN_FALSE_IF_NO_REAL_PLANE();

    return mRealPlane->disable();
}

void* AnnDisplayPlane::getContext() const
{
    if (!mRealPlane) {
        WTRACE("No real plane");
        return NULL;
    }

    return mRealPlane->getContext();
}

bool AnnDisplayPlane::initialize(uint32_t bufferCount)
{
    return true;
}

void AnnDisplayPlane::deinitialize()
{

}

void AnnDisplayPlane::setZOrderConfig(ZOrderConfig& config,
                                           void *nativeConfig)
{

}

void AnnDisplayPlane::setRealPlane(DisplayPlane *plane)
{
    mRealPlane = plane;
}

DisplayPlane* AnnDisplayPlane::getRealPlane() const
{
    return mRealPlane;
}

bool AnnDisplayPlane::setDataBuffer(BufferMapper& mapper)
{
    return false;
}

} // namespace intel
} // namespace android
