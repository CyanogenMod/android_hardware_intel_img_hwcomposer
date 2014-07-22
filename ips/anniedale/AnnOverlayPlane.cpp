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

#include <math.h>
#include <HwcTrace.h>
#include <Drm.h>
#include <Hwcomposer.h>
#include <PhysicalDevice.h>
#include <common/TTMBufferMapper.h>
#include <common/GrallocSubBuffer.h>
#include <DisplayQuery.h>
#include <anniedale/AnnOverlayPlane.h>

// FIXME: remove it
#include <OMX_IVCommon.h>

namespace android {
namespace intel {

AnnOverlayPlane::AnnOverlayPlane(int index, int disp)
    : DisplayPlane(index, PLANE_OVERLAY, disp),
      mCurrent(0),
      mWsbm(0),
      mPipeConfig(0),
      mDisablePending(false),
      mDisablePendingDevice(0),
      mDisablePendingCount(0),
      mRotationConfig(0),
      mZOrderConfig(0)
{
    CTRACE();
    for (int i = 0; i < OVERLAY_BACK_BUFFER_COUNT; i++) {
        mBackBuffer[i] = 0;
    }
}

AnnOverlayPlane::~AnnOverlayPlane()
{
    CTRACE();
}

bool AnnOverlayPlane::initialize(uint32_t bufferCount)
{
    Drm *drm = Hwcomposer::getInstance().getDrm();
    CTRACE();

    // NOTE: use overlay's data buffer count for the overlay plane
    if (bufferCount < OVERLAY_DATA_BUFFER_COUNT) {
        ITRACE("override overlay buffer count from %d to %d",
             bufferCount, OVERLAY_DATA_BUFFER_COUNT);
        bufferCount = OVERLAY_DATA_BUFFER_COUNT;
    }
    if (!DisplayPlane::initialize(bufferCount)) {
        DEINIT_AND_RETURN_FALSE("failed to initialize display plane");
    }

    // init wsbm
    mWsbm = new Wsbm(drm->getDrmFd());
    if (!mWsbm || !mWsbm->initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to create wsbm");
    }

    // create overlay back buffer
    for (int i = 0; i < OVERLAY_BACK_BUFFER_COUNT; i++) {
        mBackBuffer[i] = createBackBuffer();
        if (!mBackBuffer[i]) {
            DEINIT_AND_RETURN_FALSE("failed to create overlay back buffer");
        }
        // reset back buffer
        resetBackBuffer(i);
    }

    // disable overlay when created
    flush(PLANE_DISABLE);

    // overlay by default is in "disabled" status
    mDisablePending = false;
    mDisablePendingDevice = 0;
    mDisablePendingCount = 0;
    return true;
}

void AnnOverlayPlane::deinitialize()
{

    // delete back buffer
    for (int i = 0; i < OVERLAY_BACK_BUFFER_COUNT; i++) {
        if (mBackBuffer[i]) {
            deleteBackBuffer(i);
        }
    }
    DEINIT_AND_DELETE_OBJ(mWsbm);

    DisplayPlane::deinitialize();
}

bool AnnOverlayPlane::setDataBuffer(uint32_t handle)
{
    if (mIndex == 1 && mTransform != 0)
        return false;

    if (mDisablePending) {
        if (isFlushed() || mDisablePendingCount >= OVERLAY_DISABLING_COUNT_MAX) {
            mDisablePending = false;
            mDisablePendingDevice = 0;
            mDisablePendingCount = 0;
            enable();
        } else {
            mDisablePendingCount++;
            WTRACE("overlay %d disabling on device %d is still pending, count: %d",
                mIndex, mDisablePendingDevice, mDisablePendingCount);
            return false;
        }
    }

    return DisplayPlane::setDataBuffer(handle);
}

void AnnOverlayPlane::invalidateBufferCache()
{
    // clear plane buffer cache
    DisplayPlane::invalidateBufferCache();
}

bool AnnOverlayPlane::assignToDevice(int disp)
{
    uint32_t pipeConfig = 0;

    RETURN_FALSE_IF_NOT_INIT();
    VTRACE("overlay %d assigned to disp %d", mIndex, disp);

    switch (disp) {
    case IDisplayDevice::DEVICE_EXTERNAL:
        pipeConfig = (0x2 << 6);
        break;
    case IDisplayDevice::DEVICE_PRIMARY:
    default:
        pipeConfig = 0;
        break;
    }

    // if pipe switching happened, then disable overlay first
    if (mPipeConfig != pipeConfig) {
        DTRACE("overlay %d switched from %d to %d", mIndex, mDevice, disp);
        disable();
    }

    mPipeConfig = pipeConfig;
    mDevice = disp;

    if (!mDisablePending) {
        enable();
    }

    return true;
}

void AnnOverlayPlane::setZOrderConfig(ZOrderConfig& zorderConfig,
                                            void *nativeConfig)
{
    int slot = (int)nativeConfig;

    CTRACE();

    switch (slot) {
    case 0:
        mZOrderConfig = 0;
        break;
    case 1:
        mZOrderConfig = (1 << 8);
        break;
    case 2:
        mZOrderConfig = (2 << 8);
        break;
    case 3:
        mZOrderConfig = (3 << 8);
        break;
    default:
        ETRACE("Invalid overlay plane zorder %d", slot);
        return;
    }
}

bool AnnOverlayPlane::reset()
{
    RETURN_FALSE_IF_NOT_INIT();

    if (mDisablePending) {
        if (isFlushed() || mDisablePendingCount >= OVERLAY_DISABLING_COUNT_MAX) {
            mDisablePending = false;
            mDisablePendingDevice = 0;
            mDisablePendingCount = 0;
        } else {
            mDisablePendingCount++;
            WTRACE("overlay %d disabling is still pending on device %d, count %d",
                 mIndex, mDisablePendingDevice, mDisablePendingCount);
            return false;
        }
    }

    DisplayPlane::reset();

    // reset back buffers
    for (int i = 0; i < OVERLAY_BACK_BUFFER_COUNT; i++) {
        resetBackBuffer(i);
    }

    return true;
}

bool AnnOverlayPlane::enable()
{
    RETURN_FALSE_IF_NOT_INIT();
    OverlayBackBufferBlk *backBuffer = mBackBuffer[mCurrent]->buf;
    if (!backBuffer)
        return false;

    if (mContext.ctx.ov_ctx.ovadd & (0x1 << 15))
        return true;

    mContext.ctx.ov_ctx.ovadd |= (0x1 << 15);

    // flush
    flush(PLANE_ENABLE);
    return true;
}

bool AnnOverlayPlane::disable()
{
    RETURN_FALSE_IF_NOT_INIT();

    for (int i = 0; i < OVERLAY_BACK_BUFFER_COUNT; i++) {
        OverlayBackBufferBlk *backBuffer = mBackBuffer[i]->buf;
        if (!backBuffer)
            continue;

        if (!(mContext.ctx.ov_ctx.ovadd & (0x1 << 15)))
            continue;

        mContext.ctx.ov_ctx.ovadd &= ~(0x1 << 15);

        mContext.ctx.ov_ctx.ovadd &= ~(0x300);

        mContext.ctx.ov_ctx.ovadd |= mPipeConfig;
    }

    if (mDisablePending) {
        WTRACE("overlay %d disabling is still pending on device %d, skip disabling",
             mIndex, mDisablePendingDevice);
        return true;
    }

    // flush
    flush(PLANE_DISABLE);

    // "disable" is asynchronous and needs at least one vsync to complete
    mDisablePending = true;
    mDisablePendingDevice = mDevice;
    mDisablePendingCount = 0;
    return true;
}

OverlayBackBuffer* AnnOverlayPlane::createBackBuffer()
{
    CTRACE();

    // create back buffer
    OverlayBackBuffer *backBuffer = (OverlayBackBuffer *)malloc(sizeof(OverlayBackBuffer));
    if (!backBuffer) {
        ETRACE("failed to allocate back buffer");
        return 0;
    }


    int size = sizeof(OverlayBackBufferBlk);
    int alignment = 64 * 1024;
    void *wsbmBufferObject = 0;
    bool ret = mWsbm->allocateTTMBuffer(size, alignment, &wsbmBufferObject);
    if (ret == false) {
        ETRACE("failed to allocate TTM buffer");
        return 0;
    }

    void *virtAddr = mWsbm->getCPUAddress(wsbmBufferObject);
    uint32_t gttOffsetInPage = mWsbm->getGttOffset(wsbmBufferObject);

    backBuffer->buf = (OverlayBackBufferBlk *)virtAddr;
    backBuffer->gttOffsetInPage = gttOffsetInPage;
    backBuffer->bufObject = (uint32_t)wsbmBufferObject;

    VTRACE("cpu %p, gtt %d", virtAddr, gttOffsetInPage);

    return backBuffer;
}

void AnnOverlayPlane::deleteBackBuffer(int buf)
{
    if (!mBackBuffer[buf])
        return;

    void *wsbmBufferObject = (void *)mBackBuffer[buf]->bufObject;
    bool ret = mWsbm->destroyTTMBuffer(wsbmBufferObject);
    if (ret == false) {
        WTRACE("failed to destroy TTM buffer");
    }
    // free back buffer
    free(mBackBuffer[buf]);
    mBackBuffer[buf] = 0;
}

void AnnOverlayPlane::resetBackBuffer(int buf)
{
    CTRACE();

    if (!mBackBuffer[buf] || !mBackBuffer[buf]->buf)
        return;

    OverlayBackBufferBlk *backBuffer = mBackBuffer[buf]->buf;

    memset(backBuffer, 0, sizeof(OverlayBackBufferBlk));

    // reset overlay
    backBuffer->OCLRC0 = (OVERLAY_INIT_CONTRAST << 18) |
                         (OVERLAY_INIT_BRIGHTNESS & 0xff);
    backBuffer->OCLRC1 = OVERLAY_INIT_SATURATION;
    backBuffer->DCLRKV = OVERLAY_INIT_COLORKEY;
    backBuffer->DCLRKM = OVERLAY_INIT_COLORKEYMASK;
    backBuffer->OCONFIG = 0;
    backBuffer->OCONFIG |= (0x1 << 27);
    // use 3 line buffers
    backBuffer->OCONFIG |= 0x1;
    backBuffer->SCHRKEN &= ~(0x7 << 24);
    backBuffer->SCHRKEN |= 0xff;
}

void AnnOverlayPlane::checkPosition(int& x, int& y, int& w, int& h)
{
    Drm *drm = Hwcomposer::getInstance().getDrm();
    drmModeModeInfo modeInfo;
    if (!drm->getModeInfo(mDevice, modeInfo)) {
        ETRACE("failed to get mode info");
        return;
    }
    drmModeModeInfoPtr mode = &modeInfo;

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if ((x + w) > mode->hdisplay)
        w = mode->hdisplay - x;
    if ((y + h) > mode->vdisplay)
        h = mode->vdisplay - y;
}

bool AnnOverlayPlane::bufferOffsetSetup(BufferMapper& mapper)
{
    CTRACE();

    OverlayBackBufferBlk *backBuffer = mBackBuffer[mCurrent]->buf;
    if (!backBuffer) {
        ETRACE("invalid back buffer");
        return false;
    }

    uint32_t format = mapper.getFormat();
    uint32_t gttOffsetInBytes = (mapper.getGttOffsetInPage(0) << 12);
    uint32_t yStride = mapper.getStride().yuv.yStride;
    uint32_t uvStride = mapper.getStride().yuv.uvStride;
    uint32_t w = mapper.getWidth();
    uint32_t h = mapper.getHeight();
    uint32_t srcX= mapper.getCrop().x;
    uint32_t srcY= mapper.getCrop().y;
    uint32_t ySurface, uSurface, vSurface;
    uint32_t yTileOffsetX, yTileOffsetY;
    uint32_t uTileOffsetX, uTileOffsetY;
    uint32_t vTileOffsetX, vTileOffsetY;

    // clear original format setting
    backBuffer->OCMD &= ~(0xf << 10);
    backBuffer->OCMD &= ~OVERLAY_MEMORY_LAYOUT_TILED;

    // Y/U/V plane must be 4k bytes aligned.
    ySurface = gttOffsetInBytes;
    if (mIsProtectedBuffer) {
        // temporary workaround until vsync event logic is corrected.
        // it seems that overlay buffer update and renderring can be overlapped,
        // as such encryption bit may be cleared during HW rendering
        ySurface |= 0x01;
    }

    switch(format) {
    case HAL_PIXEL_FORMAT_YV12:    // YV12
        vSurface = ySurface + yStride * h;
        uSurface = vSurface + uvStride * (h / 2);
        yTileOffsetX = srcX;
        yTileOffsetY = srcY;
        uTileOffsetX = srcX / 2;
        uTileOffsetY = srcY / 2;
        vTileOffsetX = uTileOffsetX;
        vTileOffsetY = uTileOffsetY;
        backBuffer->OCMD |= OVERLAY_FORMAT_PLANAR_YUV420;
        break;
    case HAL_PIXEL_FORMAT_I420:    // I420
        uSurface = ySurface + yStride * h;
        vSurface = uSurface + uvStride * (h / 2);
        yTileOffsetX = srcX;
        yTileOffsetY = srcY;
        uTileOffsetX = srcX / 2;
        uTileOffsetY = srcY / 2;
        vTileOffsetX = uTileOffsetX;
        vTileOffsetY = uTileOffsetY;
        backBuffer->OCMD |= OVERLAY_FORMAT_PLANAR_YUV420;
        break;
    case HAL_PIXEL_FORMAT_NV12:    // NV12
        uSurface = ySurface + yStride * align_to(h, 32);
        vSurface = ySurface + yStride * align_to(h, 32);
        yTileOffsetX = srcX;
        yTileOffsetY = srcY;
        uTileOffsetX = srcX / 2;
        uTileOffsetY = srcY / 2;
        vTileOffsetX = uTileOffsetX;
        vTileOffsetY = uTileOffsetY;
        backBuffer->OCMD |= OVERLAY_FORMAT_PLANAR_NV12_2;
        break;
    // NOTE: this is the decoded video format, align the height to 32B
    //as it's defined by video driver
    case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar:    // NV12
        uSurface = ySurface + yStride * align_to(h, 32);
        vSurface = ySurface + yStride * align_to(h, 32);
        yTileOffsetX = srcX;
        yTileOffsetY = srcY;
        uTileOffsetX = srcX / 2;
        uTileOffsetY = srcY / 2;
        vTileOffsetX = uTileOffsetX;
        vTileOffsetY = uTileOffsetY;
        backBuffer->OCMD |= OVERLAY_FORMAT_PLANAR_NV12_2;
        break;
    case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar_Tiled:  //NV12_tiled
        uSurface = ySurface + yStride * align_to(h, 32);
        vSurface = ySurface + yStride * align_to(h, 32);
        yTileOffsetX = srcX;
        yTileOffsetY = srcY;
        uTileOffsetX = srcX / 2;
        uTileOffsetY = srcY / 2;
        vTileOffsetX = uTileOffsetX;
        vTileOffsetY = uTileOffsetY;
        backBuffer->OCMD |= OVERLAY_FORMAT_PLANAR_NV12_2;
        backBuffer->OCMD |= OVERLAY_MEMORY_LAYOUT_TILED;
        break;
    case HAL_PIXEL_FORMAT_YUY2:    // YUY2
        uSurface = ySurface;
        vSurface = ySurface;
        yTileOffsetX = srcX;
        yTileOffsetY = srcY;
        uTileOffsetX = yTileOffsetX;
        uTileOffsetY = yTileOffsetY;
        vTileOffsetX = yTileOffsetX;
        vTileOffsetY = yTileOffsetY;
        backBuffer->OCMD |= OVERLAY_FORMAT_PACKED_YUV422;
        backBuffer->OCMD |= OVERLAY_PACKED_ORDER_YUY2;
        break;
    case HAL_PIXEL_FORMAT_UYVY:    // UYVY
        uSurface = ySurface;
        vSurface = ySurface;
        yTileOffsetX = srcX;
        yTileOffsetY = srcY;
        uTileOffsetX = yTileOffsetX;
        uTileOffsetY = yTileOffsetY;
        vTileOffsetX = yTileOffsetX;
        vTileOffsetY = yTileOffsetY;
        backBuffer->OCMD |= OVERLAY_FORMAT_PACKED_YUV422;
        backBuffer->OCMD |= OVERLAY_PACKED_ORDER_UYVY;
        break;
    default:
        ETRACE("unsupported format %d", format);
        return false;
    }

    backBuffer->OSTART_0Y = ySurface;
    backBuffer->OSTART_0U = uSurface;
    backBuffer->OSTART_0V = vSurface;
    backBuffer->OBUF_0Y = srcY * yStride + srcX;
    backBuffer->OBUF_0V = (srcY / 2) * uvStride + srcX;
    backBuffer->OBUF_0U = (srcY / 2) * uvStride + srcX;
    backBuffer->OTILEOFF_0Y = yTileOffsetY << 16 | yTileOffsetX;
    backBuffer->OTILEOFF_0U = uTileOffsetY << 16 | uTileOffsetX;
    backBuffer->OTILEOFF_0V = vTileOffsetY << 16 | vTileOffsetX;

    VTRACE("done. offset (%d, %d, %d)",
          backBuffer->OBUF_0Y,
          backBuffer->OBUF_0U,
          backBuffer->OBUF_0V);

    return true;
}

uint32_t AnnOverlayPlane::calculateSWidthSW(uint32_t offset, uint32_t width)
{
    ATRACE("offset = %d, width = %d", offset, width);

    uint32_t swidth = ((offset + width + 0x3F) >> 6) - (offset >> 6);

    swidth <<= 1;
    swidth -= 1;

    return swidth;
}

bool AnnOverlayPlane::coordinateSetup(BufferMapper& mapper)
{
    CTRACE();

    OverlayBackBufferBlk *backBuffer = mBackBuffer[mCurrent]->buf;
    if (!backBuffer) {
        ETRACE("invalid back buffer");
        return false;
    }

    uint32_t swidthy = 0;
    uint32_t swidthuv = 0;
    uint32_t format = mapper.getFormat();
    uint32_t width = mapper.getCrop().w;
    uint32_t height = mapper.getCrop().h;
    uint32_t yStride = mapper.getStride().yuv.yStride;
    uint32_t uvStride = mapper.getStride().yuv.uvStride;
    uint32_t offsety = backBuffer->OBUF_0Y;
    uint32_t offsetu = backBuffer->OBUF_0U;

    switch (format) {
    case HAL_PIXEL_FORMAT_YV12:              // YV12
    case HAL_PIXEL_FORMAT_I420:              // I420
    case HAL_PIXEL_FORMAT_NV12:              // NV12
    case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar:          // NV12
    case OMX_INTEL_COLOR_FormatYUV420PackedSemiPlanar_Tiled:    // NV12_tiled
        break;
    case HAL_PIXEL_FORMAT_YUY2:              // YUY2
    case HAL_PIXEL_FORMAT_UYVY:              // UYVY
        width <<= 1;
        break;
    default:
        ETRACE("unsupported format %d", format);
        return false;
    }

    if (width <= 0 || height <= 0) {
        ETRACE("invalid src dim");
        return false;
    }

    if (yStride <=0 && uvStride <= 0) {
        ETRACE("invalid source stride");
        return false;
    }

    backBuffer->SWIDTH = width | ((width / 2) << 16);
    swidthy = calculateSWidthSW(offsety, width);
    swidthuv = calculateSWidthSW(offsetu, width / 2);
    backBuffer->SWIDTHSW = (swidthy << 2) | (swidthuv << 18);
    backBuffer->SHEIGHT = height | ((height / 2) << 16);
    backBuffer->OSTRIDE = (yStride & (~0x3f)) | ((uvStride & (~0x3f)) << 16);

    XTRACE();

    return true;
}

bool AnnOverlayPlane::setCoeffRegs(double *coeff, int mantSize,
                                  coeffPtr pCoeff, int pos)
{
    int maxVal, icoeff, res;
    int sign;
    double c;

    sign = 0;
    maxVal = 1 << mantSize;
    c = *coeff;
    if (c < 0.0) {
        sign = 1;
        c = -c;
    }

    res = 12 - mantSize;
    if ((icoeff = (int)(c * 4 * maxVal + 0.5)) < maxVal) {
        pCoeff[pos].exponent = 3;
        pCoeff[pos].mantissa = icoeff << res;
        *coeff = (double)icoeff / (double)(4 * maxVal);
    } else if ((icoeff = (int)(c * 2 * maxVal + 0.5)) < maxVal) {
        pCoeff[pos].exponent = 2;
        pCoeff[pos].mantissa = icoeff << res;
        *coeff = (double)icoeff / (double)(2 * maxVal);
    } else if ((icoeff = (int)(c * maxVal + 0.5)) < maxVal) {
        pCoeff[pos].exponent = 1;
        pCoeff[pos].mantissa = icoeff << res;
        *coeff = (double)icoeff / (double)(maxVal);
    } else if ((icoeff = (int)(c * maxVal * 0.5 + 0.5)) < maxVal) {
        pCoeff[pos].exponent = 0;
        pCoeff[pos].mantissa = icoeff << res;
        *coeff = (double)icoeff / (double)(maxVal / 2);
    } else {
        // Coeff out of range
        return false;
    }

    pCoeff[pos].sign = sign;
    if (sign)
        *coeff = -(*coeff);
    return true;
}

void AnnOverlayPlane::updateCoeff(int taps, double fCutoff,
                                 bool isHoriz, bool isY,
                                 coeffPtr pCoeff)
{
    int i, j, j1, num, pos, mantSize;
    double pi = 3.1415926535, val, sinc, window, sum;
    double rawCoeff[MAX_TAPS * 32], coeffs[N_PHASES][MAX_TAPS];
    double diff;
    int tapAdjust[MAX_TAPS], tap2Fix;
    bool isVertAndUV;

    if (isHoriz)
        mantSize = 7;
    else
        mantSize = 6;

    isVertAndUV = !isHoriz && !isY;
    num = taps * 16;
    for (i = 0; i < num  * 2; i++) {
        val = (1.0 / fCutoff) * taps * pi * (i - num) / (2 * num);
        if (val == 0.0)
            sinc = 1.0;
        else
            sinc = sin(val) / val;

        // Hamming window
        window = (0.54 - 0.46 * cos(2 * i * pi / (2 * num - 1)));
        rawCoeff[i] = sinc * window;
    }

    for (i = 0; i < N_PHASES; i++) {
        // Normalise the coefficients
        sum = 0.0;
        for (j = 0; j < taps; j++) {
            pos = i + j * 32;
            sum += rawCoeff[pos];
        }
        for (j = 0; j < taps; j++) {
            pos = i + j * 32;
            coeffs[i][j] = rawCoeff[pos] / sum;
        }

        // Set the register values
        for (j = 0; j < taps; j++) {
            pos = j + i * taps;
            if ((j == (taps - 1) / 2) && !isVertAndUV)
                setCoeffRegs(&coeffs[i][j], mantSize + 2, pCoeff, pos);
            else
                setCoeffRegs(&coeffs[i][j], mantSize, pCoeff, pos);
        }

        tapAdjust[0] = (taps - 1) / 2;
        for (j = 1, j1 = 1; j <= tapAdjust[0]; j++, j1++) {
            tapAdjust[j1] = tapAdjust[0] - j;
            tapAdjust[++j1] = tapAdjust[0] + j;
        }

        // Adjust the coefficients
        sum = 0.0;
        for (j = 0; j < taps; j++)
            sum += coeffs[i][j];
        if (sum != 1.0) {
            for (j1 = 0; j1 < taps; j1++) {
                tap2Fix = tapAdjust[j1];
                diff = 1.0 - sum;
                coeffs[i][tap2Fix] += diff;
                pos = tap2Fix + i * taps;
                if ((tap2Fix == (taps - 1) / 2) && !isVertAndUV)
                    setCoeffRegs(&coeffs[i][tap2Fix], mantSize + 2, pCoeff, pos);
                else
                    setCoeffRegs(&coeffs[i][tap2Fix], mantSize, pCoeff, pos);

                sum = 0.0;
                for (j = 0; j < taps; j++)
                    sum += coeffs[i][j];
                if (sum == 1.0)
                    break;
            }
        }
    }
}

bool AnnOverlayPlane::scalingSetup(BufferMapper& mapper)
{
    int xscaleInt, xscaleFract, yscaleInt, yscaleFract;
    int xscaleIntUV, xscaleFractUV;
    int yscaleIntUV, yscaleFractUV;
    // UV is half the size of Y -- YUV420
    int uvratio = 2;
    uint32_t newval;
    coeffRec xcoeffY[N_HORIZ_Y_TAPS * N_PHASES];
    coeffRec xcoeffUV[N_HORIZ_UV_TAPS * N_PHASES];
    coeffRec ycoeffY[N_VERT_Y_TAPS * N_PHASES];
    coeffRec ycoeffUV[N_VERT_UV_TAPS * N_PHASES];
    int i, j, pos;
    bool scaleChanged = false;
    int x, y, w, h;

    OverlayBackBufferBlk *backBuffer = mBackBuffer[mCurrent]->buf;
    if (!backBuffer) {
        ETRACE("invalid back buffer");
        return false;
    }

    x = mPosition.x;
    y = mPosition.y;
    w = mPosition.w;
    h = mPosition.h;

    // check position
    checkPosition(x, y, w, h);
    VTRACE("final position (%d, %d, %d, %d)", x, y, w, h);

    if ((w <= 0) || (h <= 0)) {
         ETRACE("invalid dst width/height");
         return false;
    }

    // setup dst position
    backBuffer->DWINPOS = (y << 16) | x;
    backBuffer->DWINSZ = (h << 16) | w;

    uint32_t srcWidth = mapper.getCrop().w;
    uint32_t srcHeight = mapper.getCrop().h;
    uint32_t dstWidth = w;
    uint32_t dstHeight = h;

    VTRACE("src (%dx%d), dst (%dx%d), transform %d",
          srcWidth, srcHeight,
          dstWidth, dstHeight,
          mTransform);

    // switch destination width/height for scale factor calculation
    // for 90/270 transformation
    if ((mTransform == DisplayPlane::PLANE_TRANSFORM_90) ||
        (mTransform == DisplayPlane::PLANE_TRANSFORM_270)) {
        uint32_t tmp = srcHeight;
        srcHeight = srcWidth;
        srcWidth = tmp;
    }

     // Y down-scale factor as a multiple of 4096
    if (srcWidth == dstWidth && srcHeight == dstHeight) {
        xscaleFract = (1 << 12);
        yscaleFract = (1 << 12);
    } else {
        xscaleFract = ((srcWidth - 1) << 12) / dstWidth;
        yscaleFract = ((srcHeight - 1) << 12) / dstHeight;
    }

    // Calculate the UV scaling factor
    xscaleFractUV = xscaleFract / uvratio;
    yscaleFractUV = yscaleFract / uvratio;


    // To keep the relative Y and UV ratios exact, round the Y scales
    // to a multiple of the Y/UV ratio.
    xscaleFract = xscaleFractUV * uvratio;
    yscaleFract = yscaleFractUV * uvratio;

    // Integer (un-multiplied) values
    xscaleInt = xscaleFract >> 12;
    yscaleInt = yscaleFract >> 12;

    xscaleIntUV = xscaleFractUV >> 12;
    yscaleIntUV = yscaleFractUV >> 12;

    // Check scaling ratio
    if (xscaleInt > INTEL_OVERLAY_MAX_SCALING_RATIO) {
        ETRACE("xscaleInt > %d", INTEL_OVERLAY_MAX_SCALING_RATIO);
        return false;
    }

    // shouldn't get here
    if (xscaleIntUV > INTEL_OVERLAY_MAX_SCALING_RATIO) {
        ETRACE("xscaleIntUV > %d", INTEL_OVERLAY_MAX_SCALING_RATIO);
        return false;
    }

    newval = (xscaleInt << 15) |
    ((xscaleFract & 0xFFF) << 3) | ((yscaleFract & 0xFFF) << 20);
    if (newval != backBuffer->YRGBSCALE) {
        scaleChanged = true;
        backBuffer->YRGBSCALE = newval;
    }

    newval = (xscaleIntUV << 15) | ((xscaleFractUV & 0xFFF) << 3) |
    ((yscaleFractUV & 0xFFF) << 20);
    if (newval != backBuffer->UVSCALE) {
        scaleChanged = true;
        backBuffer->UVSCALE = newval;
    }

    newval = yscaleInt << 16 | yscaleIntUV;
    if (newval != backBuffer->UVSCALEV) {
        scaleChanged = true;
        backBuffer->UVSCALEV = newval;
    }

    // Recalculate coefficients if the scaling changed
    // Only Horizontal coefficients so far.
    if (scaleChanged) {
        double fHCutoffY;
        double fHCutoffUV;
        double fVCutoffY;
        double fVCutoffUV;

        fHCutoffY = xscaleFract / 4096.0;
        fHCutoffUV = xscaleFractUV / 4096.0;
        fVCutoffY = yscaleFract / 4096.0;
        fVCutoffUV = yscaleFractUV / 4096.0;

        // Limit to between 1.0 and 3.0
        if (fHCutoffY < MIN_CUTOFF_FREQ)
            fHCutoffY = MIN_CUTOFF_FREQ;
        if (fHCutoffY > MAX_CUTOFF_FREQ)
            fHCutoffY = MAX_CUTOFF_FREQ;
        if (fHCutoffUV < MIN_CUTOFF_FREQ)
            fHCutoffUV = MIN_CUTOFF_FREQ;
        if (fHCutoffUV > MAX_CUTOFF_FREQ)
            fHCutoffUV = MAX_CUTOFF_FREQ;

        if (fVCutoffY < MIN_CUTOFF_FREQ)
            fVCutoffY = MIN_CUTOFF_FREQ;
        if (fVCutoffY > MAX_CUTOFF_FREQ)
            fVCutoffY = MAX_CUTOFF_FREQ;
        if (fVCutoffUV < MIN_CUTOFF_FREQ)
            fVCutoffUV = MIN_CUTOFF_FREQ;
        if (fVCutoffUV > MAX_CUTOFF_FREQ)
            fVCutoffUV = MAX_CUTOFF_FREQ;

        updateCoeff(N_HORIZ_Y_TAPS, fHCutoffY, true, true, xcoeffY);
        updateCoeff(N_HORIZ_UV_TAPS, fHCutoffUV, true, false, xcoeffUV);
        updateCoeff(N_VERT_Y_TAPS, fVCutoffY, false, true, ycoeffY);
        updateCoeff(N_VERT_UV_TAPS, fVCutoffUV, false, false, ycoeffUV);

        for (i = 0; i < N_PHASES; i++) {
            for (j = 0; j < N_HORIZ_Y_TAPS; j++) {
                pos = i * N_HORIZ_Y_TAPS + j;
                backBuffer->Y_HCOEFS[pos] =
                        (xcoeffY[pos].sign << 15 |
                         xcoeffY[pos].exponent << 12 |
                         xcoeffY[pos].mantissa);
            }
        }
        for (i = 0; i < N_PHASES; i++) {
            for (j = 0; j < N_HORIZ_UV_TAPS; j++) {
                pos = i * N_HORIZ_UV_TAPS + j;
                backBuffer->UV_HCOEFS[pos] =
                         (xcoeffUV[pos].sign << 15 |
                          xcoeffUV[pos].exponent << 12 |
                          xcoeffUV[pos].mantissa);
            }
        }

        for (i = 0; i < N_PHASES; i++) {
            for (j = 0; j < N_VERT_Y_TAPS; j++) {
                pos = i * N_VERT_Y_TAPS + j;
                backBuffer->Y_VCOEFS[pos] =
                        (ycoeffY[pos].sign << 15 |
                         ycoeffY[pos].exponent << 12 |
                         ycoeffY[pos].mantissa);
            }
        }
        for (i = 0; i < N_PHASES; i++) {
            for (j = 0; j < N_VERT_UV_TAPS; j++) {
                pos = i * N_VERT_UV_TAPS + j;
                backBuffer->UV_VCOEFS[pos] =
                         (ycoeffUV[pos].sign << 15 |
                          ycoeffUV[pos].exponent << 12 |
                          ycoeffUV[pos].mantissa);
            }
        }
    }

    XTRACE();
    return true;
}

bool AnnOverlayPlane::setDataBuffer(BufferMapper& grallocMapper)
{
    BufferMapper *mapper;
    bool ret;

    RETURN_FALSE_IF_NOT_INIT();

    // get gralloc mapper
    mapper = &grallocMapper;

    OverlayBackBufferBlk *backBuffer = mBackBuffer[mCurrent]->buf;
    if (!backBuffer) {
        ETRACE("invalid back buffer");
        return false;
    }

    ret = bufferOffsetSetup(*mapper);
    if (ret == false) {
        ETRACE("failed to set up buffer offsets");
        return false;
    }

    ret = coordinateSetup(*mapper);
    if (ret == false) {
        ETRACE("failed to set up overlay coordinates");
        return false;
    }

    ret = scalingSetup(*mapper);
    if (ret == false) {
        ETRACE("failed to set up scaling parameters");
        return false;
    }

    return true;
}

void AnnOverlayPlane::setTransform(int transform)
{
    RETURN_VOID_IF_NOT_INIT();

    DisplayPlane::setTransform(transform);

    // setup transform config
    switch (mTransform) {
    case PLANE_TRANSFORM_90:
        mRotationConfig = (0x1 << 10);
        break;
    case PLANE_TRANSFORM_180:
        mRotationConfig = (0x2 << 10);
        break;
    case PLANE_TRANSFORM_270:
        mRotationConfig = (0x3 << 10);
        break;
    case PLANE_TRANSFORM_0:
    default:
        mRotationConfig = 0;
        break;
    }
}

bool AnnOverlayPlane::flip(void *ctx)
{
    uint32_t ovadd = 0;

    RETURN_FALSE_IF_NOT_INIT();

    if (!DisplayPlane::flip(ctx))
        return false;

    // update back buffer address
    ovadd = (mBackBuffer[mCurrent]->gttOffsetInPage << 12);

    // enable rotation mode and setup rotation config
    if (mIndex == 0 && mTransform) {
        ovadd |= (1 << 12);
        ovadd |= mRotationConfig;
    }

    // setup z-order config
    ovadd |= mZOrderConfig;

    // load coefficients
    ovadd |= 0x1;

    // enable overlay
    ovadd |= (1 << 15);

    mContext.type = DC_OVERLAY_PLANE;
    mContext.ctx.ov_ctx.ovadd = ovadd;
    mContext.ctx.ov_ctx.index = mIndex;
    mContext.ctx.ov_ctx.pipe = mPipeConfig;

    // move to next back buffer
    mCurrent = (mCurrent + 1) % OVERLAY_BACK_BUFFER_COUNT;

    VTRACE("ovadd = %#x, index = %d, device = %d",
          mContext.ctx.ov_ctx.ovadd,
          mIndex,
          mDevice);

    return true;
}

void* AnnOverlayPlane::getContext() const
{
    CTRACE();
    return (void *)&mContext;
}

bool AnnOverlayPlane::flush(uint32_t flags)
{
    RETURN_FALSE_IF_NOT_INIT();
    ATRACE("flags = %#x, type = %d, index = %d", flags, mType, mIndex);

    if (!(flags & PLANE_ENABLE) && !(flags & PLANE_DISABLE))
        return false;

    struct drm_psb_register_rw_arg arg;
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));

    if (flags & PLANE_DISABLE)
        arg.plane_disable_mask = 1;
    else if (flags & PLANE_ENABLE)
        arg.plane_enable_mask = 1;

    arg.plane.type = DC_OVERLAY_PLANE;
    arg.plane.index = mIndex;
    arg.plane.ctx = mContext.ctx.ov_ctx.ovadd;

    // issue ioctl
    Drm *drm = Hwcomposer::getInstance().getDrm();
    bool ret = drm->writeReadIoctl(DRM_PSB_REGISTER_RW, &arg, sizeof(arg));
    if (ret == false) {
        WTRACE("overlay update failed with error code %d", ret);
        return false;
    }

    return true;
}

bool AnnOverlayPlane::isFlushed()
{
    RETURN_FALSE_IF_NOT_INIT();

    struct drm_psb_register_rw_arg arg;
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));

    arg.overlay_read_mask = OVSTATUS_REGRBIT_OVR_UPDT;
    arg.plane.type = DC_OVERLAY_PLANE;
    arg.plane.index = mIndex;
    // pass the pipe index to check its enabled status
    // now we can pass the device id directly since
    // their values are just equal
    arg.plane.ctx = mDisablePendingDevice;

    Drm *drm = Hwcomposer::getInstance().getDrm();
    bool ret = drm->writeReadIoctl(DRM_PSB_REGISTER_RW, &arg, sizeof(arg));
    if (ret == false) {
        WTRACE("overlay read failed with error code %d", ret);
        return false;
    }

    DTRACE("overlay %d flush status %s on device %d, current device %d",
        mIndex, arg.plane.ctx ? "DONE" : "PENDING", mDisablePendingDevice, mDevice);
    return arg.plane.ctx == 1;
}

} // namespace intel
} // namespace android
