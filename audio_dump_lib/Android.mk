#
#
# Copyright (C) Intel 2013-2016
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

#######################################################################
# Common variables

component_src_files := HalAudioDump.cpp

component_cflags := -Wall -Werror -Wno-unused-parameter

component_export_includes_dir := $(LOCAL_PATH)/include

component_includes_dir_host := \
    $(component_export_includes_dir) \
    $(call include-path-for, libc-kernel)

component_includes_dir_target := \
    $(component_export_includes_dir) \
    $(call include-path-for, bionic)

component_static_lib := libaudio_comms_utilities

component_static_lib_host := \
    $(foreach lib, $(component_static_lib), $(lib)_host)

#######################################################################
# Component Target Build

include $(CLEAR_VARS)

LOCAL_MODULE := libhalaudiodump
LOCAL_MODULE_OWNER := intel

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_includes_dir)

LOCAL_C_INCLUDES := $(component_includes_dir_target)

LOCAL_SRC_FILES := $(component_src_files)
LOCAL_CFLAGS := $(component_cflags)
LOCAL_MODULE_TAGS := optional

LOCAL_STATIC_LIBRARIES := $(component_static_lib)
include $(BUILD_STATIC_LIBRARY)

#######################################################################
# Component Host Build
ifeq (0,1)
include $(CLEAR_VARS)

LOCAL_MODULE := libhalaudiodump_host
LOCAL_MODULE_OWNER := intel

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_includes_dir)

LOCAL_C_INCLUDES := $(component_includes_dir_host)

LOCAL_SRC_FILES := $(component_src_files)
LOCAL_CFLAGS := $(component_cflags) -O0 -ggdb
LOCAL_MODULE_TAGS := optional
LOCAL_STRIP_MODULE := false

LOCAL_STATIC_LIBRARIES := $(component_static_lib_host)

include $(BUILD_HOST_STATIC_LIBRARY)
endif
