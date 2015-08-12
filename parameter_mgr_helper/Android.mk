#
#
# Copyright (C) Intel 2013-2015
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
    $(LOCAL_PATH)/includes \

component_src_files := \
    ParameterMgrHelper.cpp \
    Criterion.cpp \
    CriterionType.cpp

component_common_includes_dir := \
    $(component_export_include_dir) \

component_includes_dir := \
    parameter \

component_includes_dir_host := \
    $(foreach inc, $(component_includes_dir), $(HOST_OUT_HEADERS)/$(inc)) \
    $(component_common_includes_dir)

component_includes_dir_target := \
    $(foreach inc, $(component_includes_dir), $(TARGET_OUT_HEADERS)/$(inc)) \
    $(call include-path-for, bionic) \
    $(component_common_includes_dir)

component_static_lib := \
    libaudio_comms_utilities \
    libaudio_comms_convert

component_static_lib_host := \
    $(foreach lib, $(component_static_lib), $(lib)_host)

component_cflags := -Wall -Werror -Wextra

#######################################################################
# Component Host Build

include $(CLEAR_VARS)

LOCAL_MODULE := libparametermgr_static_host
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_STRIP_MODULE := false

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_include_dir)
LOCAL_C_INCLUDES := $(component_includes_dir_host)
LOCAL_SRC_FILES := $(component_src_files)
LOCAL_STATIC_LIBRARIES := $(component_static_lib_host)
LOCAL_CFLAGS := $(component_cflags) -O0 -ggdb

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_HOST_STATIC_LIBRARY)

#######################################################################
# Component Target Build

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_include_dir)
LOCAL_C_INCLUDES := $(component_includes_dir_target)
LOCAL_SRC_FILES := $(component_src_files)
LOCAL_STATIC_LIBRARIES := $(component_static_lib)
LOCAL_CFLAGS := $(component_cflags)

LOCAL_MODULE := libparametermgr_static
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

include external/stlport/libstlport.mk

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_STATIC_LIBRARY)


include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
