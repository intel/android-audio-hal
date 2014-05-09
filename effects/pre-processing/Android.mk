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

LOCAL_PATH := $(call my-dir)

#######################################################################
# Common variables

effect_pre_proc_src_files :=  \
    LpeNs.cpp \
    LpeAgc.cpp \
    LpeAec.cpp \
    LpeBmf.cpp \
    AudioEffectStub.cpp \
    AudioEffectSessionStub.cpp \
    LpeEffectLibrary.cpp \
    LpePreProcessingStub.cpp

effect_pre_proc_includes_dir := \

effect_pre_proc_includes_common := \
    $(LOCAL_PATH)/include \
    $(call include-path-for, audio-effects)

effect_pre_proc_includes_dir_host := \
    $(foreach inc, $(effect_pre_proc_includes_dir), $(HOST_OUT_HEADERS)/$(inc)) \
    $(call include-path-for, libc-kernel)

effect_pre_proc_includes_dir_target := \
    $(foreach inc, $(effect_pre_proc_includes_dir), $(TARGET_OUT_HEADERS)/$(inc))

effect_pre_proc_static_lib += \
    libaudio_comms_utilities

effect_pre_proc_static_lib_host += \
    $(foreach lib, $(effect_pre_proc_static_lib), $(lib)_host) \
    libaudioresample_static_host

effect_pre_proc_static_lib_target += \
    $(effect_pre_proc_static_lib)

effect_pre_proc_shared_lib_target += \
    libutils  \
    liblog \
    libcutils \

effect_pre_proc_cflags := -Wall -Werror -Wno-unused-parameter

#######################################################################
# Build for target

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(effect_pre_proc_includes_dir_target) \
    $(effect_pre_proc_includes_common)

LOCAL_SRC_FILES := $(effect_pre_proc_src_files)
LOCAL_CFLAGS := $(effect_pre_proc_cflags)

LOCAL_MODULE := liblpepreprocessing
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := \
    $(effect_pre_proc_static_lib_target)
LOCAL_SHARED_LIBRARIES := \
    $(effect_pre_proc_shared_lib_target)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/soundfx

# gcov build
ifeq ($($(LOCAL_MODULE).gcov),true)
  LOCAL_CFLAGS += -O0 --coverage -include GcovFlushWithProp.h
  LOCAL_LDFLAGS += -fprofile-arcs --coverage
  LOCAL_STATIC_LIBRARIES += gcov_flush_with_prop
endif

include external/stlport/libstlport.mk

include $(BUILD_SHARED_LIBRARY)

#######################################################################
###########################
# helper static lib target

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_CFLAGS := $(effect_pre_proc_cflags)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := EffectHelper.cpp
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := liblpepreprocessinghelper

include external/stlport/libstlport.mk

include $(BUILD_STATIC_LIBRARY)

#########################
# helper static lib host

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := $(effect_pre_proc_cflags)
LOCAL_SRC_FILES := EffectHelper.cpp
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := liblpepreprocessinghelper_host

include $(BUILD_HOST_STATIC_LIBRARY)

#######################################################################
# Build for liblpepreprocessing with and without gcov for host and target

# Compile macro
define make_effect_pre_proc_lib
$( \
    $(eval LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include) \
    $(eval LOCAL_C_INCLUDES := $(effect_pre_proc_includes_common)) \
    $(eval LOCAL_C_INCLUDES += $(effect_pre_proc_includes_dir_$(1))) \
    $(eval LOCAL_SRC_FILES := $(effect_pre_proc_src_files)) \
    $(eval LOCAL_CFLAGS := $(effect_pre_proc_cflags)) \
    $(eval LOCAL_STATIC_LIBRARIES := $(effect_pre_proc_static_lib_$(1))) \
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
LOCAL_MODULE := liblpepreprocessing_static_gcov_host
$(call make_effect_pre_proc_lib,host)
$(call add_gcov)
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target test with gcov
ifeq ($(audiocomms_test_gcov_target),true)

include $(CLEAR_VARS)
LOCAL_MODULE := liblpepreprocessing_static_gcov
$(call make_effect_pre_proc_lib,target)
$(call add_gcov)
include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)

endif

# Build for host test
ifeq ($(audiocomms_test_host),true)

include $(CLEAR_VARS)
LOCAL_MODULE := liblpepreprocessing_static_host
$(call make_effect_pre_proc_lib,host)
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target (inconditionnal)
include $(CLEAR_VARS)
LOCAL_MODULE := liblpepreprocessing_static
$(call make_effect_pre_proc_lib,target)
include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)
