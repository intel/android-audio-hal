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
include $(OPTIONAL_QUALITY_ENV_SETUP)

# Component build
#######################################################################
# Common variables

component_src_files :=  \
    RoutingElement.cpp \
    AudioPort.cpp \
    AudioPortGroup.cpp \
    AudioRoute.cpp \
    AudioStreamRoute.cpp \
    AudioRouteManager.cpp \
    AudioRouteManagerObserver.cpp \
    RouteManagerInstance.cpp \

component_export_includes := \
    $(LOCAL_PATH)/includes \
    $(LOCAL_PATH)/interface

component_includes_common := \
    $(component_export_includes) \
    $(call include-path-for, frameworks-av) \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils)

component_includes_dir := \
    hw \
    parameter \

component_includes_dir_host := \
    $(foreach inc, $(component_includes_dir), $(HOST_OUT_HEADERS)/$(inc)) \
    $(component_includes_common) \

component_includes_dir_target := \
    $(foreach inc, $(component_includes_dir), $(TARGET_OUT_HEADERS)/$(inc)) \
    $(component_includes_common) \
    $(call include-path-for, bionic) \
    $(TOP_DIR)frameworks/av/services/audiopolicy/common/include

component_static_lib := \
    libstream_static \
    libsamplespec_static \
    libaudioconversion_static \
    libaudio_hal_utilities \
    libaudioplatformstate \
    libparametermgr_static \
    libaudioparameters \
    libaudio_comms_utilities \
    libaudio_comms_convert \
    libproperty \
    liblpepreprocessinghelper \
    libevent-listener_static \
    libaudiocomms_naive_tokenizer \

component_static_lib_host := \
    $(foreach lib, $(component_static_lib), $(lib)_host) \
    libtinyalsa \
    libcutils \
    libutils \
    libaudioutils \

component_static_lib_target := \
    $(component_static_lib) \

component_shared_lib_common := \
    libparameter \
    liblog \
    libasound

component_shared_lib_target := \
    $(component_shared_lib_common) \
    libtinyalsa \
    libcutils \
    libutils \
    libaudioutils \

component_shared_lib_host := \
    libicuuc-host \
    libparameter_host \
    liblog

component_cflags := -Wall -Werror -Wextra

#######################################################################
# Component Target Build

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_includes)

LOCAL_C_INCLUDES := $(component_includes_dir_target)

LOCAL_SRC_FILES := $(component_src_files)
LOCAL_CFLAGS := $(component_cflags)

ifneq ($(strip $(PFW_CONFIGURATION_FOLDER)),)
LOCAL_CFLAGS += -DPFW_CONF_FILE_PATH=\"$(PFW_CONFIGURATION_FOLDER)\"
endif

LOCAL_MODULE := libaudioroutemanager
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := $(component_static_lib_target)

LOCAL_SHARED_LIBRARIES := $(component_shared_lib_target)

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_SHARED_LIBRARY)

#######################################################################
# Component Host Build
ifeq (0,1)
include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_includes)

LOCAL_C_INCLUDES := $(component_includes_dir_host)

LOCAL_STATIC_LIBRARIES := $(component_static_lib_host)
LOCAL_SHARED_LIBRARIES := $(component_shared_lib_host)
LOCAL_SRC_FILES := $(component_src_files) test/HdmiAudioStreamRoute.cpp
LOCAL_CFLAGS := \
    $(component_cflags) -O0 -ggdb \
    -DPFW_CONF_FILE_PATH=\"$(HOST_OUT)\"'"/etc/parameter-framework/"'

LOCAL_MODULE := libaudioroutemanager_host
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_STRIP_MODULE := false

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_HOST_SHARED_LIBRARY)
endif
#######################################################################
# Build for target to export headers

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_includes_common)
LOCAL_MODULE := audio.routemanager.includes
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

#######################################################################
# Build for host to export headers
ifeq (0,1)
include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_includes_common)
LOCAL_MODULE := audio.routemanager.includes_host
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_HOST_STATIC_LIBRARY)
endif
#######################################################################

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
