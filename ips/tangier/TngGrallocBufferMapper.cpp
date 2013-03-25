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
#include <cutils/log.h>

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
    LOGV("TngGrallocBufferMapper::TngGrallocBufferMapper");
}

TngGrallocBufferMapper::~TngGrallocBufferMapper()
{
    LOGV("TngGrallocBufferMapper::~TngGrallocBufferMapper");
}

bool TngGrallocBufferMapper::gttMap(void *vaddr,
                                      uint32_t size,
                                      uint32_t gttAlign,
                                      int *offset)
{
    struct psb_gtt_mapping_arg arg;
    Drm *drm = Hwcomposer::getInstance().getDrm();
    bool ret;

    LOGV("TngGrallocBufferMapper::gttMap: virt 0x%x, size %d\n", vaddr, size);

    if (!vaddr || !size || !offset || !drm) {
        LOGV("TngGrallocBufferMapper::gttMap: invalid parameters.");
        return false;
    }

    arg.type = PSB_GTT_MAP_TYPE_VIRTUAL;
    arg.page_align = gttAlign;
    arg.vaddr = (uint32_t)vaddr;
    arg.size = size;

    ret = drm->writeReadIoctl(DRM_PSB_GTT_MAP, &arg, sizeof(arg));
    if (ret == false) {
        LOGE("TngGrallocBufferMapper::gttMap: gtt mapping failed");
        return false;
    }

    LOGV("TngGrallocBufferMapper::gttMap: offset %d", arg.offset_pages);
    *offset =  arg.offset_pages;
    return true;
}

bool TngGrallocBufferMapper::gttUnmap(void *vaddr)
{
    struct psb_gtt_mapping_arg arg;
    Drm *drm = Hwcomposer::getInstance().getDrm();
    bool ret;

    LOGV("TngGrallocBufferMapper::gttUnmap: virt 0x%x", vaddr);

    if(!vaddr || !drm) {
        LOGE("TngGrallocBufferMapper::gttUnmap: invalid parameter");
        return false;
    }

    arg.type = PSB_GTT_MAP_TYPE_VIRTUAL;
    arg.vaddr = (uint32_t)vaddr;

    ret = drm->writeIoctl(DRM_PSB_GTT_UNMAP, &arg, sizeof(arg));
    if(ret == false) {
        LOGE("TngGrallocBufferMapper::gttUnmap: gtt unmapping failed");
        return false;
    }

    return true;
}

bool TngGrallocBufferMapper::map()
{
    void *vaddr = 0;
    uint32_t size = 0;
    int gttOffsetInPage = 0;
    bool ret;
    int err;
    int i;

    LOGV("TngGrallocBufferMapper::map");

    // get virtual address
    for (i = 0; i < SUB_BUFFER_MAX; i++) {
        err = mIMGGrallocModule.getCpuAddress(&mIMGGrallocModule,
                                              getKey(),
                                              i,
                                              &vaddr,
                                              &size);
        if (err) {
            LOGE("TngGrallocBufferMapper::map: failed to map. err = %d",
                  err);
            goto map_err;
        }

        // skip gtt mapping for empty sub buffers
        if (!vaddr || !size)
            continue;

        // map to gtt
        ret = gttMap(vaddr, size, 0, &gttOffsetInPage);
        if (!ret) {
            LOGV("TngGrallocBufferMapper::map: failed to map %d into gtt", i);
            goto gtt_err;
        }

        mCpuAddress[i] = vaddr;
        mSize[i] = size;
        mGttOffsetInPage[i] = gttOffsetInPage;
    }

    return true;
gtt_err:
    for (i = 0; i < SUB_BUFFER_MAX; i++) {
        if (mCpuAddress[i])
            gttUnmap(mCpuAddress[i]);
    }
map_err:
    mIMGGrallocModule.putCpuAddress(&mIMGGrallocModule,
                                    getKey());
    return false;
}

bool TngGrallocBufferMapper::unmap()
{
    int i;

    LOGV("TngGrallocBufferMapper::unmap");

    for (i = 0; i < SUB_BUFFER_MAX; i++) {
        if (mCpuAddress[i])
            gttUnmap(mCpuAddress[i]);

        mGttOffsetInPage[i] = 0;
        mCpuAddress[i] = 0;
        mSize[i] = 0;
    }

    mIMGGrallocModule.putCpuAddress(&mIMGGrallocModule,
                                    getKey());
    return true;
}

} // namespace intel
} // namespace android
