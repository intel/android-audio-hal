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

# Component build
#######################################################################
# Common variables

component_src_files :=  \
    AudioStreamRoute.cpp \
    AudioRouteManager.cpp \
    AudioRouteManagerObserver.cpp \
    StreamRouteConfig.cpp \
    AudioCapabilities.cpp \
    Serializer.cpp

component_export_includes := \
    $(LOCAL_PATH)/includes \
    $(LOCAL_PATH)/interface

component_includes_common := \
    $(component_export_includes) \
    $(call include-path-for, frameworks-av) \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils) \
    external/libxml2/include \
    external/icu/icu4c/source/common

component_includes_dir := \
    hw \
    parameter

component_includes_dir_host := \
    $(foreach inc, $(component_includes_dir), $(HOST_OUT_HEADERS)/$(inc)) \
    $(component_includes_common)

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
    libaudiocomms_naive_tokenizer

component_static_lib_host := \
    $(foreach lib, $(component_static_lib), $(lib)_host) \
    libtinyalsa \
    libcutils \
    libutils \
    libaudioutils \
    libxml2

component_static_lib_target := \
    $(component_static_lib) \
    libxml2

component_shared_lib_common := \
    libtypeconverter \
    libparameter \
    liblog

ifeq ($(USE_ALSA_LIB), 1)
component_shared_lib_common += libasound
endif


component_shared_lib_target := \
    $(component_shared_lib_common) \
    libtinyalsa \
    libcutils \
    libutils \
    libaudioutils \
    libicuuc

component_shared_lib_host := \
    libicuuc-host \
    libparameter_host \
    liblog

component_cflags := $(HAL_COMMON_CFLAGS)

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

LOCAL_REQUIRED_MODULES := route_manager_configuration.xml

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_SHARED_LIBRARY)

#######################################################################
# Component Host Build
ifeq (ENABLE_HOST_VERSION,1)
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
ifeq (ENABLE_HOST_VERSION,1)
include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_includes_common)
LOCAL_MODULE := audio.routemanager.includes_host
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_HOST_STATIC_LIBRARY)
endif
#######################################################################
# Build for configuration file

include $(CLEAR_VARS)
LOCAL_MODULE := route_manager_configuration.xml
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_SRC_FILES := config/$(LOCAL_MODULE)
LOCAL_ADDITIONAL_DEPENDENCIES := \
    audio_criteria.xml \
    audio_criterion_types.xml \
    audio_rogue_parameters.xml
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := audio_criteria.xml
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_SRC_FILES := config/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := audio_criterion_types.xml
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_SRC_FILES := config/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := audio_rogue_parameters.xml
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_SRC_FILES := config/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

#######################################################################
# Tools for audio pfw settings generation

include $(CLEAR_VARS)
LOCAL_MODULE := buildCriterionTypes.py
LOCAL_MODULE_OWNER := intel
LOCAL_SRC_FILES := tools/$(LOCAL_MODULE)
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_IS_HOST_MODULE := true
include $(BUILD_PREBUILT)

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
