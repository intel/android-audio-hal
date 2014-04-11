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
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)


LOCAL_SRC_FILES := \
    RouteSubsystemBuilder.cpp \
    RouteSubsystem.cpp \
    AudioPort.cpp \
    AudioRoute.cpp \
    AudioStreamRoute.cpp \
    Criterion.cpp

LOCAL_CFLAGS += \
    -Wall \
    -Werror \
    -Wextra \

LOCAL_C_INCLUDES += external/tinyalsa/include \

LOCAL_SHARED_LIBRARIES := \
    libparameter \
    libinterface-provider-lib \
    libproperty \
    liblog \

LOCAL_STATIC_LIBRARIES := \
    libparameter_includes \
    libxmlserializer_includes \
    libaudio_comms_utilities \
    audio.routemanager.includes \
    libsamplespec_static \
    libpfw_utility \

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/parameter-framework-plugins/Route
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libroute-subsystem
LOCAL_MODULE_OWNER := intel

include external/stlport/libstlport.mk
include $(BUILD_SHARED_LIBRARY)

