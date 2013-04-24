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
#include <Drm.h>
#include <Hwcomposer.h>
#include <tangier/TngGrallocBufferMapper.h>

namespace android {
namespace intel {

TngGrallocBufferMapper::TngGrallocBufferMapper(IMG_gralloc_module_public_t& module,
                                                    DataBuffer& buffer)
    : GrallocBufferMapperBase(buffer),
      mIMGGrallocModule(module),
      mBufferObject(0)
{
    CTRACE();
}

TngGrallocBufferMapper::~TngGrallocBufferMapper()
{
    CTRACE();
}

bool TngGrallocBufferMapper::gttMap(void *vaddr,
                                      uint32_t size,
                                      uint32_t gttAlign,
                                      int *offset)
{
    struct psb_gtt_mapping_arg arg;
    Drm *drm = Hwcomposer::getInstance().getDrm();
    bool ret;

    ATRACE("vaddr = %p, size = %d", vaddr, size);

    if (!vaddr || !size || !offset || !drm) {
        VTRACE("invalid parameters");
        return false;
    }

    arg.type = PSB_GTT_MAP_TYPE_VIRTUAL;
    arg.page_align = gttAlign;
    arg.vaddr = (uint32_t)vaddr;
    arg.size = size;

    ret = drm->writeReadIoctl(DRM_PSB_GTT_MAP, &arg, sizeof(arg));
    if (ret == false) {
        ETRACE("gtt mapping failed");
        return false;
    }

    VTRACE("offset = %#x", arg.offset_pages);
    *offset =  arg.offset_pages;
    return true;
}

bool TngGrallocBufferMapper::gttUnmap(void *vaddr)
{
    struct psb_gtt_mapping_arg arg;
    Drm *drm = Hwcomposer::getInstance().getDrm();
    bool ret;

    ATRACE("vaddr = %p", vaddr);

    if(!vaddr || !drm) {
        ETRACE("invalid parameter");
        return false;
    }

    arg.type = PSB_GTT_MAP_TYPE_VIRTUAL;
    arg.vaddr = (uint32_t)vaddr;

    ret = drm->writeIoctl(DRM_PSB_GTT_UNMAP, &arg, sizeof(arg));
    if(ret == false) {
        ETRACE("gtt unmapping failed");
        return false;
    }

    return true;
}

bool TngGrallocBufferMapper::map()
{
    void *vaddr[SUB_BUFFER_MAX];
    uint32_t size[SUB_BUFFER_MAX];
    int gttOffsetInPage = 0;
    bool ret;
    int err;
    int i;

    CTRACE();
    // get virtual address
    err = mIMGGrallocModule.getCpuAddress(&mIMGGrallocModule,
                                          (buffer_handle_t)getHandle(),
                                          vaddr,
                                          size);
    if (err) {
        ETRACE("failed to map. err = %d", err);
        goto map_err;
    }

    for (i = 0; i < SUB_BUFFER_MAX; i++) {
        // skip gtt mapping for empty sub buffers
        if (!vaddr[i] || !size[i])
            continue;

        // map to gtt
        ret = gttMap(vaddr[i], size[i], 0, &gttOffsetInPage);
        if (!ret) {
            VTRACE("failed to map %d into gtt", i);
            goto gtt_err;
        }

        mCpuAddress[i] = vaddr[i];
        mSize[i] = size[i];
        mGttOffsetInPage[i] = gttOffsetInPage;
    }

    return true;
gtt_err:
    for (i = 0; i < SUB_BUFFER_MAX; i++) {
        if (mCpuAddress[i])
            gttUnmap(mCpuAddress[i]);
    }
map_err:
    err = mIMGGrallocModule.putCpuAddress(&mIMGGrallocModule,
                                    (buffer_handle_t)getHandle());
    return false;
}

bool TngGrallocBufferMapper::unmap()
{
    int i;
    int err;

    CTRACE();

    for (i = 0; i < SUB_BUFFER_MAX; i++) {
        if (mCpuAddress[i])
            gttUnmap(mCpuAddress[i]);

        mGttOffsetInPage[i] = 0;
        mCpuAddress[i] = 0;
        mSize[i] = 0;
    }

    err = mIMGGrallocModule.putCpuAddress(&mIMGGrallocModule,
                                    (buffer_handle_t)getHandle());
    if (err) {
        ETRACE("failed to unmap. err = %d", err);
    }
    return err;
}

} // namespace intel
} // namespace android
