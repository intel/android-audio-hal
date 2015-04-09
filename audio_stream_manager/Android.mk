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

audio_stream_manager_src_files :=  \
    src/Stream.cpp \
    src/audio_hw.cpp \
    src/Device.cpp \
    src/StreamIn.cpp \
    src/StreamOut.cpp \
    src/Patch.cpp \
    src/Port.cpp \
    src/AudioParameterHandler.cpp

audio_stream_manager_includes_dir := \
    $(TARGET_OUT_HEADERS)/libaudioresample \
    $(TARGET_OUT_HEADERS)/hal_audio_dump \
    $(TARGET_OUT_HEADERS)/hw \
    $(TARGET_OUT_HEADERS)/parameter \
    $(call include-path-for, frameworks-av) \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils) \
    $(call include-path-for, audio-effects)

audio_stream_manager_includes_dir_host := \
    $(audio_stream_manager_includes_dir)

audio_stream_manager_includes_dir_target := \
    $(audio_stream_manager_includes_dir) \
    $(call include-path-for, bionic)

audio_stream_manager_static_lib += \
    libsamplespec_static \
    libaudioconversion_static \
    libstream_static \
    libaudioplatformstate \
    libaudioparameters \
    libparametermgr_static \
    libaudio_hal_utilities \
    libaudio_comms_utilities \
    libaudio_comms_convert \
    libhalaudiodump \
    liblpepreprocessinghelper \
    libaudiocomms_naive_tokenizer

audio_stream_manager_static_lib_host += \
    $(foreach lib, $(audio_stream_manager_static_lib), $(lib)_host)

audio_stream_manager_static_lib_target += \
    $(audio_stream_manager_static_lib) \
    libmedia_helper \
    audio.routemanager.includes

audio_stream_manager_shared_lib_target += \
    libtinyalsa \
    libcutils \
    libutils \
    libmedia \
    libhardware \
    libhardware_legacy \
    libparameter \
    libicuuc \
    libevent-listener \
    libaudioresample \
    libaudioutils \
    libproperty \
    libaudioroutemanager \

audio_stream_manager_include_dirs_from_static_libraries := \
    libevent-listener_static \
    libproperty_static

audio_stream_manager_include_dirs_from_static_libraries_target := \
    $(audio_stream_manager_include_dirs_from_static_libraries)

audio_stream_manager_whole_static_lib := \
    libaudiohw_intel

audio_stream_manager_include_dirs_from_static_libraries_host := \
    $(foreach lib, $(audio_stream_manager_include_dirs_from_static_libraries), $(lib)_host)

audio_stream_manager_cflags := -Wall -Werror -Wextra

#######################################################################
# Phony package definition

include $(CLEAR_VARS)
LOCAL_MODULE := audio_hal_configurable
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

LOCAL_REQUIRED_MODULES := \
    audio.primary.$(TARGET_BOARD_PLATFORM) \
    liblpepreprocessing \
    route_criteria.conf

include $(BUILD_PHONY_PACKAGE)

#######################################################################
# Build for target

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(audio_stream_manager_includes_dir_target)

LOCAL_SRC_FILES := $(audio_stream_manager_src_files)
LOCAL_CFLAGS := $(audio_stream_manager_cflags)

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

LOCAL_MODULE := audio.primary.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := \
    $(audio_stream_manager_static_lib_target)
LOCAL_WHOLE_STATIC_LIBRARIES := \
    $(audio_stream_manager_whole_static_lib)
LOCAL_SHARED_LIBRARIES := \
    $(audio_stream_manager_shared_lib_target)

include external/stlport/libstlport.mk

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_SHARED_LIBRARY)

#######################################################################
# Build for host

ifeq ($(audiocomms_test_host),true)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(audio_stream_manager_includes_dir_host)
LOCAL_STATIC_LIBRARIES := $(audio_stream_manager_static_lib_host)
# libraries included for their headers
LOCAL_STATIC_LIBRARIES += \
    $(audio_stream_manager_include_dirs_from_static_libraries_host)
LOCAL_WHOLE_STATIC_LIBRARIES := $(audio_stream_manager_whole_static_lib)
LOCAL_SRC_FILES := $(audio_stream_manager_src_files)
LOCAL_CFLAGS := $(audio_stream_manager_cflags)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libaudio_stream_manager_static_host
LOCAL_MODULE_OWNER := intel
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Component functional test
#######################################################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= test/FunctionalTest.cpp

LOCAL_C_INCLUDES := \
        $(TARGET_OUT_HEADERS)/hw \
        $(TARGET_OUT_HEADERS)/parameter \
        frameworks/av/include \
        system/media/audio_utils/include \
        system/media/audio_effects/include \

LOCAL_STATIC_LIBRARIES := \
        libaudioparameters \
        libaudio_comms_utilities \
        libaudio_comms_convert \
        libmedia_helper \

LOCAL_SHARED_LIBRARIES := \
        libhardware \
        liblog \

LOCAL_MODULE := audio-hal-functional_test
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -Wall -Werror -Wextra -O0 -ggdb

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_NATIVE_TEST)


include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
