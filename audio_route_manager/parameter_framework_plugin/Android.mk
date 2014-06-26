# INTEL CONFIDENTIAL
#
# Copyright (c) 2013-2014 Intel Corporation All Rights Reserved.
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

ifeq ($(BOARD_USES_AUDIO_HAL_XML),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

common_includes := \

common_cflags := \
    -Wall \
    -Werror \
    -Wextra \
    -Wno-unused-parameter

common_static_libraries := \
    libaudio_comms_utilities

common_local_import_c_include_dirs_from_static_libraries := \
    libsamplespec_static \
    libpfw_utility \
    libparameter_includes \
    libxmlserializer

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/parameter-framework-plugins/Route

LOCAL_SRC_FILES := \
    RouteSubsystem.cpp \
    RouteSubsystemBuilder.cpp \
    AudioPort.cpp \
    AudioRoute.cpp \
    AudioStreamRoute.cpp \
    Criterion.cpp

LOCAL_MODULE := libroute-subsystem

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += $(common_cflags)

LOCAL_C_INCLUDES += \
    $(common_includes) \
    $(call include-path-for, stlport) \
    $(call include-path-for, tinyalsa) \
    $(call include-path-for, bionic)

LOCAL_IMPORT_C_INCLUDE_DIRS_FROM_SHARED_LIBRARIES := audio.routemanager
LOCAL_IMPORT_C_INCLUDE_DIRS_FROM_STATIC_LIBRARIES := \
    $(common_local_import_c_include_dirs_from_static_libraries)

LOCAL_STATIC_LIBRARIES := \
    $(common_static_libraries)

LOCAL_SHARED_LIBRARIES := libstlport libparameter libinterface-provider-lib libproperty liblog

include $(BUILD_SHARED_LIBRARY)

endif #ifeq ($(BOARD_USES_AUDIO_HAL_XML),true)
