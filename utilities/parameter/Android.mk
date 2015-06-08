#
#
# Copyright (C) Intel 2014-2015
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)
include $(OPTIONAL_QUALITY_ENV_SETUP)

# Target build
#######################################################################

include $(CLEAR_VARS)

LOCAL_STATIC_LIBRARIES := \
    libaudio_hal_utilities \
    libaudio_comms_utilities \
    libaudio_comms_convert \

LOCAL_SRC_FILES := \
    src/KeyValuePairs.cpp \
    src/Parameters.cpp \

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wall -Werror -Wextra

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libaudioparameters
LOCAL_MODULE_OWNER := intel

#include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)

# Host build
#######################################################################

include $(CLEAR_VARS)

LOCAL_STATIC_LIBRARIES := \
    libaudio_comms_utilities_host \
    libaudio_comms_convert_host \

LOCAL_SRC_FILES := src/KeyValuePairs.cpp \

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wall -Werror -Wextra
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libkeyvaluepairs_host
LOCAL_MODULE_OWNER := intel
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)

# Functional test
#######################################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES += test/KeyValuePairsTest.cpp \

LOCAL_C_INCLUDES := \

LOCAL_STATIC_LIBRARIES += \
    libkeyvaluepairs_host \
    libaudio_comms_utilities_host \
    libaudio_comms_convert_host \

LOCAL_CFLAGS := -Wall -Werror -Wextra

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := key_value_pairs_test
LOCAL_MODULE_OWNER := intel
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_NATIVE_TEST)


include $(OPTIONAL_QUALITY_RUN_TEST)

#######################################################################

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
