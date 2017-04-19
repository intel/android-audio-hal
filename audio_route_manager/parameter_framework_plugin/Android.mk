# INTEL CONFIDENTIAL
#
# Copyright (c) 2013-2016 Intel Corporation All Rights Reserved.
#
# The source code contained or described herein and all documents related to
# the source code ("Material") are owned by Intel Corporation or its suppliers
# or licensors.
#
# Title to the Material remains with Intel Corporation or its suppliers and
# licensors. The Material contains trade secrets and proprietary and
# confidential information of Intel or its suppliers and licensors. The
# Material is protected by worldwide copyright and trade secret laws and treaty
# provisions. No part of the Material may be used, copied, reproduced,
# modified, published, uploaded, posted, transmitted, distributed, or disclosed
# in any way without Intel's prior express written permission.
#
# No license under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or delivery
# of the Materials, either expressly, by implication, inducement, estoppel or
# otherwise. Any license under such intellectual property rights must be
# express and approved by Intel in writing.
#

LOCAL_PATH := $(call my-dir)
include $(OPTIONAL_QUALITY_ENV_SETUP)

# Component build
#######################################################################
# Common variables

component_src_files :=  \
    RouteSubsystemBuilder.cpp \
    RouteSubsystem.cpp \
    AudioPort.cpp \
    AudioRoute.cpp \
    AudioStreamRoute.cpp \
    Criterion.cpp

component_common_includes_dir := \
    external/tinyalsa/include \

component_static_lib := \
    libaudio_comms_utilities \
    libaudio_comms_convert \
    libsamplespec_static \
    libpfw_utility \

component_static_lib_target := \
    $(component_static_lib) \
    $(component_static_lib_common)

component_static_lib_host := \
    $(foreach lib, $(component_static_lib), $(lib)_host) \
    $(component_static_lib_common)

component_shared_lib:= \
    libparameter \
    libaudioroutemanager \
    libasound

component_shared_lib_common := \
    liblog \

component_shared_lib_target := \
    $(component_shared_lib_common) \
    $(component_shared_lib)

component_shared_lib_host := \
    $(foreach lib, $(component_shared_lib), $(lib)_host) \
    $(component_shared_lib_common)

component_cflags := -Wall -Werror -Wextra

#######################################################################
# Host Component Build
ifeq (0,1)
include $(CLEAR_VARS)

LOCAL_MODULE := libroute-subsystem_host
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES := $(component_common_includes_dir)

LOCAL_SRC_FILES := $(component_src_files)
LOCAL_STATIC_LIBRARIES := $(component_static_lib_host)
LOCAL_SHARED_LIBRARIES := $(component_shared_lib_host)

LOCAL_CFLAGS := $(component_cflags)

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_HOST_SHARED_LIBRARY)
endif
#######################################################################
# Target Component Build

include $(CLEAR_VARS)

LOCAL_MODULE := libroute-subsystem
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES :=  $(component_common_includes_dir)

LOCAL_SRC_FILES := $(component_src_files)
LOCAL_STATIC_LIBRARIES := $(component_static_lib_target)
LOCAL_SHARED_LIBRARIES := $(component_shared_lib_target)

LOCAL_CFLAGS := $(component_cflags)

include $(BUILD_SHARED_LIBRARY)
