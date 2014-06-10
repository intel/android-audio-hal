#
# INTEL CONFIDENTIAL
# Copyright (c) 2013-2014 Intel
# Corporation All Rights Reserved.
#
# The source code contained or described herein and all documents related to
# the source code ("Material") are owned by Intel Corporation or its suppliers
# or licensors. Title to the Material remains with Intel Corporation or its
# suppliers and licensors. The Material contains trade secrets and proprietary
# and confidential information of Intel or its suppliers and licensors. The
# Material is protected by worldwide copyright and trade secret laws and
# treaty provisions. No part of the Material may be used, copied, reproduced,
# modified, published, uploaded, posted, transmitted, distributed, or
# disclosed in any way without Intel's prior express written permission.
#
# No license under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or delivery
# of the Materials, either expressly, by implication, inducement, estoppel or
# otherwise. Any license under such intellectual property rights must be
# express and approved by Intel in writing.
#

ifeq ($(BOARD_USES_AUDIO_HAL_XML),true)

LOCAL_PATH := $(call my-dir)

#######################################################################
# Common variables

audio_platform_state_src_files :=  \
    AudioPlatformState.cpp \
    VolumeKeys.cpp

audio_platform_state_includes_dir := \
    $(TARGET_OUT_HEADERS)/hw \
    $(TARGET_OUT_HEADERS)/parameter \
    $(call include-path-for, tinyalsa) \
    $(call include-path-for, audio-utils) \
    $(call include-path-for, audio-effects)

audio_platform_state_includes_dir_target := \
    $(audio_platform_state_includes_dir) \
    $(call include-path-for, stlport) \
    $(call include-path-for, bionic)

audio_platform_state_static_lib += \
    libsamplespec_static \
    libstream_static \
    libparametermgr_static \
    libaudio_comms_utilities \
    libaudio_comms_convert \
    liblpepreprocessinghelper \
    libaudiocomms_naive_tokenizer \
    libinterface-provider-lib_static \
    libproperty_static \
    audio.routemanager.includes \
    libmedia_helper

audio_platform_state_cflags := -Wall -Werror -Wextra -Wno-unused-parameter

#######################################################################
# Build for target lib with coverage

include $(CLEAR_VARS)
LOCAL_MODULE := libaudioplatformstate_static_gcov
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
LOCAL_C_INCLUDES := $(audio_platform_state_includes_dir_target)
LOCAL_SRC_FILES := $(audio_platform_state_src_files)
LOCAL_STATIC_LIBRARIES := $(audio_platform_state_static_lib)
LOCAL_CFLAGS := $(audio_platform_state_cflags)
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -O0 --coverage
LOCAL_LDFLAGS += --coverage
include $(BUILD_STATIC_LIBRARY)

#######################################################################
# Build for target lib

include $(CLEAR_VARS)
LOCAL_MODULE := libaudioplatformstate_static
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
LOCAL_C_INCLUDES := $(audio_platform_state_includes_dir_target)
LOCAL_SRC_FILES := $(audio_platform_state_src_files)
LOCAL_STATIC_LIBRARIES := $(audio_platform_state_static_lib)
LOCAL_CFLAGS := $(audio_platform_state_cflags)
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)


#######################################################################
# Build for target configuration file

include $(CLEAR_VARS)
LOCAL_MODULE := route_criteria.conf
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)


endif #ifeq ($(BOARD_USES_AUDIO_HAL_XML),true)
