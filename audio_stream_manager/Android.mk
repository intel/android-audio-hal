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

component_src_files :=  \
    src/Stream.cpp \
    src/audio_hw.cpp \
    src/Device.cpp \
    src/StreamIn.cpp \
    src/StreamOut.cpp \
    src/CompressedStreamOut.cpp \
    src/Patch.cpp \
    src/Port.cpp \
    src/AudioParameterHandler.cpp

component_includes_dir := \
    $(TARGET_OUT_HEADERS)/hal_audio_dump \
    $(TARGET_OUT_HEADERS)/hw \
    $(TARGET_OUT_HEADERS)/parameter \
    $(call include-path-for, frameworks-av) \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils) \
    $(call include-path-for, audio-effects) \
    external/tinycompress/include \

component_includes_dir_host := \
    $(component_includes_dir) \
    bionic/libc/kernel/uapi/ \

component_includes_dir_target := \
    $(component_includes_dir) \
    $(call include-path-for, bionic)

component_static_lib += \
    libsamplespec_static \
    libaudioconversion_static \
    libstream_static \
    libparametermgr_static \
    libaudioparameters \
    libaudio_hal_utilities \
    libproperty \
    libaudio_comms_utilities \
    libaudio_comms_convert \
    libhalaudiodump \
    liblpepreprocessinghelper \

component_static_lib_host += \
    $(foreach lib, $(component_static_lib), $(lib)_host) \
    libcutils \
    libutils \
    libaudioutils \
    libspeexresampler \
    libtinyalsa \
    libtinycompress \

component_static_lib_target += \
    $(component_static_lib) \
    libmedia_helper \

component_shared_lib_common := \
    libparameter \
    libaudioroutemanager \

component_shared_lib_target := \
    $(component_shared_lib_common) \
    libtinyalsa \
    libtinycompress \
    libcutils \
    libutils \
    libmedia \
    libhardware \
    libaudioutils \
    libicuuc \

component_shared_lib_host := \
    $(foreach lib, $(component_shared_lib_common), $(lib)_host) \
    libicuuc-host \
    liblog \


component_whole_static_lib := \
    libaudiohw_intel

component_cflags := -Wall -Werror -Wextra

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
# Component Target Build

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(component_includes_dir_target)

LOCAL_SRC_FILES := $(component_src_files)
LOCAL_CFLAGS := $(component_cflags)

LOCAL_MODULE := audio.primary.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := $(component_static_lib_target)
LOCAL_WHOLE_STATIC_LIBRARIES := $(component_whole_static_lib)
LOCAL_SHARED_LIBRARIES := $(component_shared_lib_target)

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_SHARED_LIBRARY)

#######################################################################
# Component Host Build

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/src
LOCAL_C_INCLUDES := $(component_includes_dir_host)
LOCAL_STATIC_LIBRARIES := \
    $(component_static_lib_host) \
    $(component_whole_static_lib)_host
LOCAL_SHARED_LIBRARIES := $(component_shared_lib_host)
LOCAL_SRC_FILES := $(component_src_files)
LOCAL_CFLAGS := $(component_cflags)
LOCAL_LDFLAGS += -pthread -lrt

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := audio.primary_host
LOCAL_MODULE_OWNER := intel

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_HOST_STATIC_LIBRARY)


# Target Component functional test
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

# Component functional test for HOST
#######################################################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= test/FunctionalTestHost.cpp

LOCAL_C_INCLUDES := \
    test \
    external/gtest/include \
    $(component_includes_dir_host) \

LOCAL_STATIC_LIBRARIES := \
    audio.primary_host \
    $(component_static_lib_host) \
    $(component_whole_static_lib)_host \
    libgtest_host \
    libgtest_main_host \

LOCAL_SHARED_LIBRARIES := \
    $(component_shared_lib_host)

LOCAL_LDFLAGS += -lpthread -lrt
LOCAL_MODULE := audio-hal-functional_test_host
LOCAL_REQUIRED_MODULES := host_test_app_pfw_files
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_STRIP_MODULE := false

LOCAL_CFLAGS := -Wall -Werror -Wextra -O0 -ggdb

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

# Cannot use $(BUILD_HOST_NATIVE_TEST) because of compilation flag
# misalignment against gtest mk files
include $(BUILD_HOST_EXECUTABLE)

#######################################################################
# Build for configuration file

include $(CLEAR_VARS)
LOCAL_MODULE := route_criteria.conf
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := test/config/$(LOCAL_MODULE)
LOCAL_MODULE_PATH := $(HOST_OUT)/etc
LOCAL_IS_HOST_MODULE := true
include $(BUILD_PREBUILT)

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)

#######################################################################
# Recursive call sub-folder Android.mk
#######################################################################
include $(call all-makefiles-under,$(LOCAL_PATH))
