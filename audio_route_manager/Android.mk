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

# Component build
#######################################################################
# Common variables

audio_route_manager_src_files :=  \
    RoutingElement.cpp \
    AudioPort.cpp \
    AudioPortGroup.cpp \
    AudioRoute.cpp \
    AudioStreamRoute.cpp \
    AudioRouteManager.cpp \
    AudioRouteManagerObserver.cpp \
    RouteManagerInstance.cpp \

audio_route_manager_includes_common := \
    $(LOCAL_PATH)/includes \
    $(LOCAL_PATH)/interface

audio_route_manager_includes_dir := \
    $(TARGET_OUT_HEADERS)/libaudioresample \
    $(TARGET_OUT_HEADERS)/hw \
    $(TARGET_OUT_HEADERS)/parameter \
    $(call include-path-for, frameworks-av) \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils)

audio_route_manager_includes_dir_host := \
    $(audio_route_manager_includes_dir)

audio_route_manager_includes_dir_target := \
    $(audio_route_manager_includes_dir) \
    $(call include-path-for, bionic)

audio_route_manager_header_copy_folder_unit_test := \
    audio_route_manager_unit_test

audio_route_manager_static_lib := \
    libstream_static \
    libsamplespec_static \
    libparametermgr_static \
    libaudio_hal_utilities \
    libaudio_comms_utilities \
    liblpepreprocessinghelper \
    libevent-listener_static \

audio_route_manager_static_lib_host := \
    $(foreach lib, $(audio_route_manager_static_lib), $(lib)_host) \
    libproperty_includes_host \

audio_route_manager_static_lib_target := \
    $(audio_route_manager_static_lib) \
    libmedia_helper \
    libproperty_static

audio_route_manager_shared_lib_target := \
    libtinyalsa \
    libcutils \
    libutils \
    libparameter \
    libhardware_legacy \
    libicuuc \
    libevent-listener \
    libaudioutils \
    libproperty

audio_route_manager_cflags := -Wall -Werror

ifneq ($(strip $(PFW_CONFIGURATION_FOLDER)),)
audio_route_manager_cflags += -DPFW_CONF_FILE_PATH=\"$(PFW_CONFIGURATION_FOLDER)\"
endif

#######################################################################
# Build for target

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(audio_route_manager_includes_common)

LOCAL_C_INCLUDES := \
    $(audio_route_manager_includes_common) \
    $(audio_route_manager_includes_dir_target)

LOCAL_SRC_FILES := $(audio_route_manager_src_files)
LOCAL_CFLAGS := $(audio_route_manager_cflags)

LOCAL_MODULE := libaudioroutemanager
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := \
    $(audio_route_manager_static_lib_target)

LOCAL_SHARED_LIBRARIES := \
    $(audio_route_manager_shared_lib_target)

#include external/stlport/libstlport.mk

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_SHARED_LIBRARY)

#######################################################################
# Build for host

ifeq ($(audiocomms_test_gcov_host),true)

include $(CLEAR_VARS)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include)
LOCAL_C_INCLUDES := $(audio_route_manager_includes_common)
LOCAL_C_INCLUDES += $(audio_route_manager_includes_dir_host)
LOCAL_STATIC_LIBRARIES := $(audio_route_manager_static_lib_host)
# libraries included for their headers
LOCAL_STATIC_LIBRARIES += \
    $(audio_route_manager_include_dirs_from_static_libraries_host)
LOCAL_SRC_FILES := $(audio_route_manager_src_files)
LOCAL_CFLAGS := $(audio_route_manager_cflags)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libaudio_route_manager_static_gcov_host
LOCAL_MODULE_OWNER := intel

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)

endif


#######################################################################
# Build for target to export headers

include $(CLEAR_VARS)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(audio_route_manager_includes_common)
LOCAL_MODULE := audio.routemanager.includes
LOCAL_MODULE_OWNER := intel
include $(BUILD_STATIC_LIBRARY)


include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
