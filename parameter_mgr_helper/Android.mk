#
#
# Copyright (C) Intel 2013-2017
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
    $(LOCAL_PATH)/includes

component_src_files := \
    ParameterMgrHelper.cpp \
    Criterion.cpp \
    CriterionType.cpp \
    CriterionParameter.cpp \
    Parameter.cpp

component_common_includes_dir := \
    $(component_export_include_dir)

component_includes_dir := \
    parameter

component_includes_dir_host := \
    $(foreach inc, $(component_includes_dir), $(HOST_OUT_HEADERS)/$(inc)) \
    $(component_common_includes_dir)

component_includes_dir_target := \
    $(foreach inc, $(component_includes_dir), $(TARGET_OUT_HEADERS)/$(inc)) \
    $(call include-path-for, bionic) \
    $(component_common_includes_dir)

component_static_lib := \
    libaudio_comms_utilities \
    libaudio_comms_convert \
    libpfw_utility

ifeq ($(HAVE_BOOST), 1)
component_static_lib += libboost
endif


component_static_lib_host := \
    $(foreach lib, $(component_static_lib), $(lib)_host)

component_shared_lib := \
    libparameter

component_shared_lib_host := \
    $(foreach lib, $(component_shared_lib), $(lib)_host)

component_cflags := $(HAL_COMMON_CFLAGS)

#######################################################################
# Component Host Build
ifeq (ENABLE_HOST_VERSION,1)
include $(CLEAR_VARS)

LOCAL_MODULE := libparametermgr_static_host
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_STRIP_MODULE := false

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_include_dir)
LOCAL_C_INCLUDES := $(component_includes_dir_host)
LOCAL_SRC_FILES := $(component_src_files)
LOCAL_STATIC_LIBRARIES := $(component_static_lib_host)
LOCAL_SHARED_LIBRARIES := $(component_shared_lib_host)
LOCAL_CFLAGS := $(component_cflags) -O0 -ggdb

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_HOST_STATIC_LIBRARY)
endif
#######################################################################
# Component Target Build

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_include_dir)
LOCAL_C_INCLUDES := $(component_includes_dir_target)
LOCAL_SRC_FILES := $(component_src_files)
LOCAL_STATIC_LIBRARIES := $(component_static_lib)
LOCAL_SHARED_LIBRARIES := $(component_shared_lib)
LOCAL_CFLAGS := $(component_cflags)

LOCAL_MODULE := libparametermgr_static
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_STATIC_LIBRARY)


include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
