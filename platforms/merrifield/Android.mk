# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libdrm \
                          libwsbm libutils libhardware
LOCAL_SRC_FILES := \
    ../../common/base/Drm.cpp \
    ../../common/base/HwcLayerList.cpp \
    ../../common/base/Hwcomposer.cpp \
    ../../common/base/HwcModule.cpp \
    ../../common/buffers/BufferCache.cpp \
    ../../common/buffers/BufferManager.cpp \
    ../../common/devices/PhysicalDevice.cpp \
    ../../common/devices/PrimaryDevice.cpp \
    ../../common/devices/ExternalDevice.cpp \
    ../../common/devices/VirtualDevice.cpp \
    ../../common/observers/HotplugEventObserver.cpp \
    ../../common/observers/VsyncEventObserver.cpp \
    ../../common/planes/DisplayPlane.cpp \
    ../../common/planes/DisplayPlaneManager.cpp \
    ../../common/utils/Dump.cpp


LOCAL_SRC_FILES += \
    ../../ips/common/BlankControl.cpp \
    ../../ips/common/HotplugControl.cpp \
    ../../ips/common/VsyncControl.cpp \
    ../../ips/common/OverlayPlaneBase.cpp \
    ../../ips/common/SpritePlaneBase.cpp \
    ../../ips/common/PixelFormat.cpp \
    ../../ips/common/PlaneCapabilities.cpp \
    ../../ips/common/GrallocBufferBase.cpp \
    ../../ips/common/GrallocBufferMapperBase.cpp \
    ../../ips/common/TTMBufferMapper.cpp \
    ../../ips/common/DrmConfig.cpp \
    ../../ips/common/Wsbm.cpp \
    ../../ips/common/WsbmWrapper.c

LOCAL_SRC_FILES += \
    ../../ips/tangier/TngGrallocBuffer.cpp \
    ../../ips/tangier/TngGrallocBufferMapper.cpp \
    ../../ips/tangier/TngOverlayPlane.cpp \
    ../../ips/tangier/TngPrimaryPlane.cpp \
    ../../ips/tangier/TngSpritePlane.cpp \
    ../../ips/tangier/TngDisplayContext.cpp


LOCAL_SRC_FILES += \
    PlatfBufferManager.cpp \
    PlatfPrimaryDevice.cpp \
    PlatfExternalDevice.cpp \
    PlatfVirtualDevice.cpp \
    PlatfDisplayPlaneManager.cpp \
    PlatfHwcomposer.cpp


LOCAL_C_INCLUDES := $(addprefix $(LOCAL_PATH)/../../../, $(SGX_INCLUDES)) \
    frameworks/native/include/media/openmax \
    frameworks/native/opengl/include \
    hardware/libhardware_legacy/include/hardware_legacy \
    $(KERNEL_SRC_DIR)/drivers/staging/mrfl/drv \
    $(KERNEL_SRC_DIR)/drivers/staging/mrfl/interface \
    vendor/intel/hardware/PRIVATE/rgx/rogue/android/graphicshal \
    vendor/intel/hardware/PRIVATE/rgx/rogue/include/ \
    $(TARGET_OUT_HEADERS)/drm \
    $(TARGET_OUT_HEADERS)/libdrm \
    $(TARGET_OUT_HEADERS)/libdrm/shared-core \
    $(TARGET_OUT_HEADERS)/libwsbm/wsbm \
    $(TARGET_OUT_HEADERS)/libttm

LOCAL_C_INCLUDES += $(LOCAL_PATH) \
    $(LOCAL_PATH)/../../include \
    $(LOCAL_PATH)/../../common/base \
    $(LOCAL_PATH)/../../common/buffers \
    $(LOCAL_PATH)/../../common/devices \
    $(LOCAL_PATH)/../../common/observers \
    $(LOCAL_PATH)/../../common/planes \
    $(LOCAL_PATH)/../../common/utils \
    $(LOCAL_PATH)/../../ips/ \
    $(LOCAL_PATH)/


LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := hwcomposer.$(TARGET_DEVICE)
LOCAL_CFLAGS:= -DLINUX

#$(error local path is: $(LOCAL_C_INCLUDES))
include $(BUILD_SHARED_LIBRARY)

