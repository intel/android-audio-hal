#
# Copyright (c) 2013-2017 Intel Corporation
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

component_src_files :=  \
    RouteSubsystemBuilder.cpp \
    RouteSubsystem.cpp \
    AudioPort.cpp \
    AudioRoute.cpp \
    AudioStreamRoute.cpp \
    Criterion.cpp

component_common_includes_dir := \
    external/tinyalsa/include

component_static_lib := \
    libaudio_comms_utilities \
    libaudio_comms_convert \
    libsamplespec_static \
    libpfw_utility

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
    liblog

component_shared_lib_target := \
    $(component_shared_lib_common) \
    $(component_shared_lib)

component_shared_lib_host := \
    $(foreach lib, $(component_shared_lib), $(lib)_host) \
    $(component_shared_lib_common)

component_cflags := -Wall -Werror -Wextra

#######################################################################
# Host Component Build
ifeq (ENABLE_HOST_VERSION,1)
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
