#
#
# Copyright (C) Intel 2014-2016
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

#######################################################################
# Common variables

component_export_include_dir := \
    $(LOCAL_PATH)/include \

component_src_files :=  \
    src/KeyValuePairs.cpp \
    src/Parameters.cpp \

component_static_lib := \
    libaudio_hal_utilities \
    libaudio_comms_utilities \
    libaudio_comms_convert \

component_static_lib_host := \
    $(foreach lib, $(component_static_lib), $(lib)_host) \

component_cflags := -Wall -Werror -Wno-unused-parameter

#######################################################################
# Target Component Build

include $(CLEAR_VARS)

LOCAL_STATIC_LIBRARIES := $(component_static_lib)

LOCAL_SRC_FILES := $(component_src_files)

LOCAL_C_INCLUDES := $(component_export_include_dir)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_include_dir)
LOCAL_CFLAGS := $(component_cflags)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libaudioparameters
LOCAL_MODULE_OWNER := intel

include $(BUILD_STATIC_LIBRARY)

#######################################################################
# Host Component Build
ifeq (0,1)
include $(CLEAR_VARS)

LOCAL_STATIC_LIBRARIES := $(component_static_lib_host)

LOCAL_SRC_FILES := $(component_src_files)

LOCAL_C_INCLUDES := $(component_export_include_dir)
LOCAL_CFLAGS := $(component_cflags) -O0 -ggdb
LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_include_dir)

LOCAL_STRIP_MODULE := false
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libaudioparameters_host
LOCAL_MODULE_OWNER := intel

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_HOST_STATIC_LIBRARY)
endif
# Functional test
#######################################################################
ifeq (0,1)
include $(CLEAR_VARS)

LOCAL_SRC_FILES += test/KeyValuePairsTest.cpp \

LOCAL_C_INCLUDES := \

LOCAL_STATIC_LIBRARIES += \
    libaudioparameters_host \
    libaudio_comms_utilities_host \
    libaudio_comms_convert_host \

LOCAL_CFLAGS := -Wall -Werror -Wextra

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := key_value_pairs_test
LOCAL_MODULE_OWNER := intel
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_NATIVE_TEST)
endif

include $(OPTIONAL_QUALITY_RUN_TEST)

#######################################################################

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
