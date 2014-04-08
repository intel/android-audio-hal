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

ifeq ($(BOARD_USES_ALSA_AUDIO),true)

LOCAL_PATH := $(call my-dir)

#######################################################################
# Common variables

audio_conversion_src_files :=  \
    AudioConversion.cpp \
    AudioConverter.cpp \
    AudioReformatter.cpp \
    AudioRemapper.cpp \
    AudioResampler.cpp \
    Resampler.cpp

audio_conversion_includes_dir := \
    libaudioresample

audio_conversion_includes_common := \
    $(LOCAL_PATH)/include \
    $(call include-path-for, frameworks-av) \
    $(call include-path-for, tinyalsa)

audio_conversion_includes_dir_host := \
    $(foreach inc, $(audio_conversion_includes_dir), $(HOST_OUT_HEADERS)/$(inc)) \
    $(call include-path-for, libc-kernel)

audio_conversion_includes_dir_target := \
    $(foreach inc, $(audio_conversion_includes_dir), $(TARGET_OUT_HEADERS)/$(inc)) \
    $(call include-path-for, bionic)

audio_conversion_static_lib += \
    libsamplespec_static \
    libaudio_comms_utilities

audio_conversion_static_lib_host += \
    $(foreach lib, $(audio_conversion_static_lib), $(lib)_host) \
    libaudioresample_static_host

audio_conversion_static_lib_target += \
    $(audio_conversion_static_lib)

audio_conversion_cflags := -Wall -Werror

#######################################################################
# Build for libaudioconversion with and without gcov for host and target

# Compile macro
define make_audio_conversion_lib
$( \
    $(eval LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include) \
    $(eval LOCAL_C_INCLUDES := $(audio_conversion_includes_common)) \
    $(eval LOCAL_C_INCLUDES += $(audio_conversion_includes_dir_$(1))) \
    $(eval LOCAL_SRC_FILES := $(audio_conversion_src_files)) \
    $(eval LOCAL_CFLAGS := $(audio_conversion_cflags)) \
    $(eval LOCAL_STATIC_LIBRARIES := $(audio_conversion_static_lib_$(1))) \
    $(eval LOCAL_MODULE_TAGS := optional) \
)
endef
define add_gcov
$( \
    $(eval LOCAL_CFLAGS += -O0 --coverage) \
    $(eval LOCAL_LDFLAGS += --coverage) \
)
endef

# Build for host test with gcov
ifeq ($(audiocomms_test_gcov_host),true)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudioconversion_static_gcov_host
$(call make_audio_conversion_lib,host)
$(call add_gcov)
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target test with gcov
ifeq ($(audiocomms_test_gcov_target),true)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudioconversion_static_gcov
$(call make_audio_conversion_lib,target)
$(call add_gcov)
include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)

endif

# Build for host test
ifeq ($(audiocomms_test_host),true)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudioconversion_static_host
$(call make_audio_conversion_lib,host)
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target (inconditionnal)
include $(CLEAR_VARS)
LOCAL_MODULE := libaudioconversion_static
$(call make_audio_conversion_lib,target)
include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)


endif #ifeq ($(BOARD_USES_ALSA_AUDIO),true)
