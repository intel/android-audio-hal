#
# Copyright (c) 2013-2018 Intel Corporation
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

# Component build
#######################################################################
# Common variables

component_export_includes_dir := $(LOCAL_PATH)/include

component_src_files := TypeConverter.cpp

component_includes_common := \
    $(LOCAL_PATH)/include/typeconverter \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils) \
    $(TOP_DIR)frameworks/av/services/audiopolicy/common/include

component_includes_dir_host := \
    $(foreach inc, $(component_includes_dir), $(HOST_OUT_HEADERS)/$(inc)) \
    $(component_includes_common)

component_includes_dir_target := \
    $(foreach inc, $(component_includes_dir), $(TARGET_OUT_HEADERS)/$(inc)) \
    $(component_includes_common) \
    $(call include-path-for, bionic)

component_static_lib := \
    libaudio_comms_utilities \
    libaudio_comms_convert

component_static_lib_target := \
    $(component_static_lib) \
    $(component_static_lib_common)

component_static_lib_host := \
    $(foreach lib, $(component_static_lib), $(lib)_host)

component_static_lib_target := \
    $(component_static_lib) \
    libmedia_helper

component_shared_lib_target := \
    $(component_shared_lib_common) \
    libtinyalsa

component_cflags := \
    $(HAL_COMMON_CFLAGS) \
    -frtti -fexceptions

#######################################################################
# Component Target Build

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_includes_dir)
LOCAL_C_INCLUDES := $(component_includes_dir_target)
LOCAL_SRC_FILES := $(component_src_files)
LOCAL_CFLAGS := $(component_cflags)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE := libtypeconverter
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_HEADER_LIBRARIES += libaudio_system_headers
LOCAL_STATIC_LIBRARIES := $(component_static_lib_target)
LOCAL_SHARED_LIBRARIES := $(component_shared_lib_target)

include $(BUILD_SHARED_LIBRARY)

#######################################################################
# Component Host Build
ifeq (0,1)
include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_includes_dir)
LOCAL_C_INCLUDES := $(component_includes_dir_host)
LOCAL_STATIC_LIBRARIES := $(component_static_lib_host)
LOCAL_SHARED_LIBRARIES := $(component_shared_lib_host)
LOCAL_SRC_FILES := $(component_src_files)
LOCAL_CFLAGS := $(component_cflags) -O0 -ggdb
LOCAL_MODULE := libtypeconverter_host
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_STRIP_MODULE := false

include $(BUILD_HOST_STATIC_LIBRARY)
endif
