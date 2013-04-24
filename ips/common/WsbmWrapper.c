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

#include <wsbm_pool.h>
#include <wsbm_driver.h>
#include <wsbm_manager.h>
#include <wsbm_util.h>
#include <linux/psb_drm.h>
#include <xf86drm.h>
#include <HwcTrace.h>

struct _WsbmBufferPool * mainPool = NULL;

struct PsbWsbmValidateNode
{
    struct  _ValidateNode base;
    struct psb_validate_arg arg;
};

static inline uint32_t align_to(uint32_t arg, uint32_t align)
{
    return ((arg + (align - 1)) & (~(align - 1)));
}

static struct _ValidateNode * pvrAlloc(struct _WsbmVNodeFuncs * func,
                                       int typeId)
{
    CTRACE();
    if(typeId == 0) {
        struct PsbWsbmValidateNode * vNode = malloc(sizeof(*vNode));
        if(!vNode) {
            ETRACE("failed to allocate memory");
            return NULL;
        }

        vNode->base.func = func;
        vNode->base.type_id = 0;
        return &vNode->base;
    } else {
        struct _ValidateNode * node = malloc(sizeof(*node));
        if(!node) {
            ETRACE("failed to allocate node");
            return NULL;
        }

        node->func = func;
        node->type_id = 1;
        return node;
    }
}

static void pvrFree(struct _ValidateNode * node)
{
    CTRACE();
    if(node->type_id == 0) {
        free(containerOf(node, struct PsbWsbmValidateNode, base));
    } else {
        free(node);
    }
}

static void pvrClear(struct _ValidateNode * node)
{
    CTRACE();
    if(node->type_id == 0) {
        struct PsbWsbmValidateNode * vNode =
            containerOf(node, struct PsbWsbmValidateNode, base);
        memset(&vNode->arg.d.req, 0, sizeof(vNode->arg.d.req));
    }
}

static struct _WsbmVNodeFuncs vNodeFuncs = {
    .alloc  = pvrAlloc,
    .free   = pvrFree,
    .clear  = pvrClear,
};

int psbWsbmInitialize(int drmFD)
{
    union drm_psb_extension_arg arg;
    const char drmExt[] = "psb_ttm_placement_alphadrop";
    int ret = 0;

    CTRACE();

    if (drmFD <= 0) {
        ETRACE("invalid drm fd %d", drmFD);
        return drmFD;
    }

    /*init wsbm*/
    ret = wsbmInit(wsbmNullThreadFuncs(), &vNodeFuncs);
    if (ret) {
        ETRACE("failed to initialize Wsbm, error code %d", ret);
        return ret;
    }

    VTRACE("DRM_PSB_EXTENSION %d", DRM_PSB_EXTENSION);

    /*get devOffset via drm IOCTL*/
    strncpy(arg.extension, drmExt, sizeof(drmExt));

    ret = drmCommandWriteRead(drmFD, 6/*DRM_PSB_EXTENSION*/, &arg, sizeof(arg));
    if(ret || !arg.rep.exists) {
        ETRACE("failed to get device offset, error code %d", ret);
        goto out;
    }

    VTRACE("ioctl offset %#x", arg.rep.driver_ioctl_offset);

    mainPool = wsbmTTMPoolInit(drmFD, arg.rep.driver_ioctl_offset);
    if(!mainPool) {
        ETRACE("failed to initialize TTM Pool");
        ret = -EINVAL;
        goto out;
    }

    VTRACE("Wsbm initialization succeeded. mainPool %p", mainPool);

    return 0;

out:
    wsbmTakedown();
    return ret;
}

void psbWsbmTakedown()
{
    CTRACE();

    wsbmPoolTakeDown(mainPool);
    wsbmTakedown();
}

int psbWsbmAllocateTTMBuffer(uint32_t size, uint32_t align, void ** buf)
{
    struct _WsbmBufferObject * wsbmBuf = NULL;
    int ret = 0;
    int offset = 0;

    ATRACE("size %d", align_to(size, 4096));

    if(!buf) {
        ETRACE("invalid parameter");
        return -EINVAL;
    }

    VTRACE("mainPool %p", mainPool);

    ret = wsbmGenBuffers(mainPool, 1, &wsbmBuf, align,
                        (WSBM_PL_FLAG_VRAM | WSBM_PL_FLAG_TT |
                         WSBM_PL_FLAG_SHARED | WSBM_PL_FLAG_NO_EVICT));
    if(ret) {
        ETRACE("wsbmGenBuffers failed with error code %d", ret);
        return ret;
    }

    ret = wsbmBOData(wsbmBuf, align_to(size, 4096), NULL, NULL, 0);
    if(ret) {
        ETRACE("wsbmBOData failed with error code %d", ret);
        /*FIXME: should I unreference this buffer here?*/
        return ret;
    }

    wsbmBOReference(wsbmBuf);

    *buf = wsbmBuf;

    VTRACE("ttm buffer allocated. %p", *buf);
    return 0;
}

int psbWsbmWrapTTMBuffer(uint32_t handle, void **buf)
{
    int ret = 0;
    struct _WsbmBufferObject *wsbmBuf;

    if (!buf) {
        ETRACE("invalid parameter");
        return -EINVAL;
    }

    ret = wsbmGenBuffers(mainPool, 1, &wsbmBuf, 0,
                        (WSBM_PL_FLAG_VRAM | WSBM_PL_FLAG_TT |
                        /*WSBM_PL_FLAG_NO_EVICT |*/ WSBM_PL_FLAG_SHARED));

    if (ret) {
        ETRACE("wsbmGenBuffers failed with error code %d", ret);
        return ret;
    }

    ret = wsbmBOSetReferenced(wsbmBuf, handle);
    if (ret) {
        ETRACE("wsbmBOSetReferenced failed with error code %d", ret);
        return ret;
    }

    *buf = (void *)wsbmBuf;

    VTRACE("wrap buffer %p for handle %#x", wsbmBuf, handle);
    return 0;
}

int psbWsbmUnReference(void *buf)
{
    struct _WsbmBufferObject *wsbmBuf;

    if (!buf) {
        ETRACE("invalid parameter");
        return -EINVAL;
    }

    wsbmBuf = (struct _WsbmBufferObject *)buf;

    wsbmBOUnreference(&wsbmBuf);

    return 0;
}

int psbWsbmDestroyTTMBuffer(void * buf)
{
    CTRACE();

    if(!buf) {
        ETRACE("invalid ttm buffer");
        return -EINVAL;
    }

    /*FIXME: should I unmap this buffer object first?*/
    wsbmBOUnmap((struct _WsbmBufferObject *)buf);

    wsbmBOUnreference((struct _WsbmBufferObject **)&buf);

    XTRACE();

    return 0;
}

void * psbWsbmGetCPUAddress(void * buf)
{
    if(!buf) {
        ETRACE("invalid ttm buffer");
        return NULL;
    }

    VTRACE("buffer object %p", buf);

    void * address = wsbmBOMap((struct _WsbmBufferObject *)buf,
                                WSBM_ACCESS_READ | WSBM_ACCESS_WRITE);
    if(!address) {
        ETRACE("failed to map buffer object");
        return NULL;
    }

    VTRACE("mapped successfully. %p, size %ld",
        address, wsbmBOSize((struct _WsbmBufferObject *)buf));

    return address;
}

uint32_t psbWsbmGetGttOffset(void * buf)
{
    if(!buf) {
        ETRACE("invalid ttm buffer");
        return 0;
    }

    VTRACE("buffer object %p", buf);

    uint32_t offset =
        wsbmBOOffsetHint((struct _WsbmBufferObject *)buf) & 0x0fffffff;

    VTRACE("offset %#x", offset >> 12);

    return offset >> 12;
}

uint32_t psbWsbmGetKBufHandle(void *buf)
{
    if (!buf) {
        ETRACE("invalid ttm buffer");
        return 0;
    }

    return (wsbmKBufHandle(wsbmKBuf((struct _WsbmBufferObject *)buf)));
}

uint32_t psbWsbmWaitIdle(void *buf)
{
    if (!buf) {
        ETRACE("invalid ttm buffer");
        return -EINVAL;
    }

    wsbmBOWaitIdle(buf, 0);
    return 0;
}
