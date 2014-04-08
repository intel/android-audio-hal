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

audio_stream_manager_src_files :=  \
    AudioStream.cpp \
    AudioIntelHAL.cpp \
    AudioStreamInImpl.cpp \
    AudioStreamOutImpl.cpp \
    AudioParameterHandler.cpp \
    AudioPlatformState.cpp \
    VolumeKeys.cpp

audio_stream_manager_includes_dir := \
    $(TARGET_OUT_HEADERS)/libaudioresample \
    $(TARGET_OUT_HEADERS)/mamgr-interface \
    $(TARGET_OUT_HEADERS)/mamgr-core \
    $(TARGET_OUT_HEADERS)/hal_audio_dump \
    $(TARGET_OUT_HEADERS)/hw \
    $(TARGET_OUT_HEADERS)/parameter \
    $(call include-path-for, frameworks-av) \
    $(call include-path-for, tinyalsa) \
    $(call include-path-for, audio-utils) \
    $(call include-path-for, audio-effects)

audio_stream_manager_includes_dir_host := \
    $(audio_stream_manager_includes_dir)

audio_stream_manager_includes_dir_target := \
    $(audio_stream_manager_includes_dir) \
    $(call include-path-for, bionic)

audio_stream_manager_static_lib += \
    libsamplespec_static \
    libaudioconversion_static \
    libstream_static \
    libparametermgr_static \
    libaudio_comms_utilities \
    libaudio_comms_convert \
    libhalaudiodump \
    liblpepreprocessinghelper \
    libaudiocomms_naive_tokenizer

audio_stream_manager_static_lib_host += \
    $(foreach lib, $(audio_stream_manager_static_lib), $(lib)_host)

audio_stream_manager_static_lib_target += \
    $(audio_stream_manager_static_lib) \
    libmedia_helper

audio_stream_manager_shared_lib_target += \
    libtinyalsa \
    libcutils \
    libutils \
    libmedia \
    libhardware \
    libhardware_legacy \
    libparameter \
    libicuuc \
    libevent-listener \
    libaudioresample \
    libaudioutils \
    libproperty \
    libinterface-provider-lib

audio_stream_manager_include_dirs_from_static_libraries := \
    libevent-listener_static \
    libinterface-provider-lib_static \
    libproperty_static

audio_stream_manager_include_dirs_from_static_libraries_target := \
    $(audio_stream_manager_include_dirs_from_static_libraries)

audio_stream_manager_whole_static_lib := \
    libaudiohw_legacy

audio_stream_manager_include_dirs_from_static_libraries_host := \
    $(foreach lib, $(audio_stream_manager_include_dirs_from_static_libraries), $(lib)_host)

audio_stream_manager_cflags := -Wall -Werror -Wno-unused-parameter

#######################################################################
# Phony package definition

include $(CLEAR_VARS)
LOCAL_MODULE := audio_hal_configurable
LOCAL_MODULE_TAGS := optional

LOCAL_REQUIRED_MODULES := \
    audio.primary.$(TARGET_DEVICE) \
    audio_policy.$(TARGET_DEVICE) \
    audio.routemanager \
    liblpepreprocessing \
    route_criteria.conf

include $(BUILD_PHONY_PACKAGE)

#######################################################################
# Build for target audio.primary

include $(CLEAR_VARS)
LOCAL_MODULE := route_criteria.conf
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

#######################################################################
# Build for target audio.primary

include $(CLEAR_VARS)

LOCAL_IMPORT_C_INCLUDE_DIRS_FROM_SHARED_LIBRARIES := audio.routemanager
LOCAL_C_INCLUDES := \
    $(audio_stream_manager_includes_dir_target)

LOCAL_SRC_FILES := $(audio_stream_manager_src_files)
LOCAL_CFLAGS := $(audio_stream_manager_cflags)

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

LOCAL_MODULE := audio.primary.$(TARGET_DEVICE)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := \
    $(audio_stream_manager_static_lib_target)
LOCAL_WHOLE_STATIC_LIBRARIES := \
    $(audio_stream_manager_whole_static_lib)
LOCAL_SHARED_LIBRARIES := \
    $(audio_stream_manager_shared_lib_target)

# gcov build
ifeq ($($(LOCAL_MODULE).gcov),true)
  LOCAL_CFLAGS += -O0 --coverage -include GcovFlushWithProp.h
  LOCAL_LDFLAGS += -fprofile-arcs --coverage
  LOCAL_STATIC_LIBRARIES += gcov_flush_with_prop
endif

include external/stlport/libstlport.mk

include $(BUILD_SHARED_LIBRARY)

#######################################################################
# Build for test with and without gcov for host and target

# Compile macro
define make_audio_stream_manager_test_lib
$( \
    $(eval LOCAL_C_INCLUDES := $(audio_stream_manager_includes_dir_$(1))) \
    $(eval LOCAL_STATIC_LIBRARIES := $(audio_stream_manager_static_lib_$(1))) \
    $(eval LOCAL_WHOLE_STATIC_LIBRARIES := $(audio_stream_manager_whole_static_lib)) \
    $(eval LOCAL_SRC_FILES := $(audio_stream_manager_src_files)) \
    $(eval LOCAL_CFLAGS := $(audio_stream_manager_cflags)) \
    $(eval LOCAL_IMPORT_C_INCLUDE_DIRS_FROM_STATIC_LIBRARIES := $(audio_stream_manager_include_dirs_from_static_libraries_$(1)))
    $(eval LOCAL_MODULE_TAGS := optional) \
)
endef

define add_gcov
$( \
    $(eval LOCAL_CFLAGS += -O0 -fprofile-arcs -ftest-coverage) \
    $(eval LOCAL_LDFLAGS += -lgcov) \
)
endef

# Build for host test with gcov
ifeq ($(audiocomms_test_gcov_host),true)

include $(CLEAR_VARS)
$(call make_audio_stream_manager_test_lib,host)
$(call add_gcov)
LOCAL_MODULE := libaudio_stream_manager_static_gcov_host
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target test with gcov
ifeq ($(audiocomms_test_gcov_target),true)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudio_stream_manager_static_gcov
$(call make_audio_stream_manager_test_lib,target)
$(call add_gcov)
include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)

endif

# Build for host test
ifeq ($(audiocomms_test_host),true)

include $(CLEAR_VARS)
$(call make_audio_stream_manager_test_lib,host)
LOCAL_MODULE := libaudio_stream_manager_static_host
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target test
ifeq ($(audiocomms_test_target),true)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudio_stream_manager_static
$(call make_audio_stream_manager_test_lib,target)
include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)

endif


endif #ifeq ($(BOARD_USES_AUDIO_HAL_XML),true)
