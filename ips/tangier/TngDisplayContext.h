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
#ifndef TNG_DISPLAY_CONTEXT_H
#define TNG_DISPLAY_CONTEXT_H

#include <IDisplayContext.h>
#include <hal_public.h>

namespace android {
namespace intel {

class TngDisplayContext : public IDisplayContext {
public:
    TngDisplayContext();
    virtual ~TngDisplayContext();
public:
    bool initialize();
    void deinitialize();
    bool commitBegin(size_t numDisplays, hwc_display_contents_1_t **displays);
    bool commitContents(hwc_display_contents_1_t *display, HwcLayerList* layerList);
    bool commitEnd(size_t numDisplays, hwc_display_contents_1_t **displays);
    bool compositionComplete();

private:
    enum {
        MAXIMUM_LAYER_NUMBER = 20,
    };
    IMG_display_device_public_t *mIMGDisplayDevice;
    IMG_hwc_layer_t mImgLayers[MAXIMUM_LAYER_NUMBER];
    bool mInitialized;
    size_t mCount;
};

} // namespace intel
} // namespace android

#endif /* TNG_DISPLAY_CONTEXT_H */
