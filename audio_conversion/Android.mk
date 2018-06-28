#
#
# Copyright (C) Intel 2013-2018
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

component_export_include_dir := \
    $(LOCAL_PATH)/include

component_src_files :=  \
    src/AudioConversion.cpp \
    src/AudioConverter.cpp \
    src/AudioReformatter.cpp \
    src/AudioRemapper.cpp \
    src/AudioResampler.cpp

component_includes_common := \
    $(component_export_include_dir) \
    $(call include-path-for, frameworks-av) \
    $(call include-path-for, audio-utils) \
    external/tinyalsa/include

component_includes_dir_host := \
    $(call include-path-for, libc-kernel)

component_includes_dir_target := \
    $(call include-path-for, bionic)

component_static_lib := \
    libsamplespec_static \
    libaudio_comms_utilities

component_static_lib_host += \
    $(foreach lib, $(component_static_lib), $(lib)_host)


ifeq ($(USE_ALSA_LIB), 1)
component_dynamic_lib += libasound
endif

component_cflags := $(HAL_COMMON_CFLAGS)

#######################################################################
# Component Host Build

ifeq (ENABLE_HOST_VERSION,1)
include $(CLEAR_VARS)

LOCAL_MODULE := libaudioconversion_static_host
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_STRIP_MODULE := false

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_include_dir)
LOCAL_C_INCLUDES := \
    $(component_includes_common) \
    $(component_includes_dir_host)

LOCAL_SRC_FILES := $(component_src_files)
LOCAL_CFLAGS := $(component_cflags) -O0 -ggdb
LOCAL_STATIC_LIBRARIES := $(component_static_lib_host)

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_HOST_STATIC_LIBRARY)
endif

#######################################################################
# Component Target Build

include $(CLEAR_VARS)

LOCAL_MODULE := libaudioconversion_static
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := intel

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_include_dir)
LOCAL_C_INCLUDES := \
    $(component_includes_common) \
    $(component_includes_dir_target)

LOCAL_SRC_FILES := $(component_src_files)
LOCAL_CFLAGS := $(component_cflags)
LOCAL_STATIC_LIBRARIES := $(component_static_lib)
LOCAL_SHARED_LIBRARIES := $(component_dynamic_lib)
LOCAL_HEADER_LIBRARIES += libaudioclient_headers libhardware_headers libutils_headers

LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)


#######################################################################
# Component Functional Test Common variables

component_fcttest_src_files := \
    test/AudioConversionTest.cpp

component_fcttest_c_includes := \
    external/tinyalsa/include \
    frameworks/av/include/media

# Other Lib
component_fcttest_static_lib := \
    libsamplespec_static \
    libaudio_comms_utilities \
    libaudioconversion_static

# Compile macro
component_fcttest_c_includes_target := \
    $(component_fcttest_c_includes) \
    $(foreach inc, $(component_fcttest_export_c_includes), $(TARGET_OUT_HEADERS)/$(inc))

component_fcttest_c_includes_host := \
    $(component_fcttest_c_includes) \
    $(foreach inc, $(component_fcttest_export_c_includes), $(HOST_OUT_HEADERS)/$(inc)) \
    bionic/libc/kernel/common \
    external/gtest/include

component_fcttest_static_lib_host := \
    $(foreach lib, $(component_fcttest_static_lib), $(lib)_host) \
    libgtest_host \
    libgtest_main_host \
    libaudioutils \
    libspeexresampler \
    liblog

component_fcttest_static_lib_target := \
    $(component_fcttest_static_lib) \
    liblog \
    libutils


component_fcttest_shared_lib_target := \
    libcutils \
    libaudioutils

ifeq ($(USE_ALSA_LIB), 1)
component_fcttest_shared_lib_target += libasound
endif


#######################################################################
# Component Functional Test Host Build

ifeq (ENABLE_HOST_VERSION,1)
include $(CLEAR_VARS)

LOCAL_MODULE := audio_conversion_fcttest_host
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(component_fcttest_src_files)
LOCAL_C_INCLUDES := $(component_fcttest_c_includes_host)
LOCAL_CFLAGS := $(component_fcttest_defines)
LOCAL_STATIC_LIBRARIES := $(component_fcttest_static_lib_host)
LOCAL_LDFLAGS := $(component_fcttest_static_ldflags_host)

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
# Cannot use $(BUILD_HOST_NATIVE_TEST) because of compilation flag
# misalignment against gtest mk files

include $(BUILD_HOST_EXECUTABLE)
endif

#######################################################################
# Component Functional Test Target Build
include $(CLEAR_VARS)

LOCAL_MODULE := audio_conversion_functional_test
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(component_fcttest_src_files)
LOCAL_C_INCLUDES := $(component_fcttest_c_includes_target)
LOCAL_CFLAGS := $(component_fcttest_defines)
LOCAL_STATIC_LIBRARIES := $(component_fcttest_static_lib_target)
LOCAL_SHARED_LIBRARIES := $(component_fcttest_shared_lib_target)
LOCAL_LDFLAGS := $(component_fcttest_static_ldflags_target)

# GMock and GTest requires C++ Technical Report 1 (TR1) tuple library, which is not available
# on target (stlport). GTest provides its own implementation of TR1 (and substiture to standard
# implementation). This trick does not work well with latest compiler. Flags must be forced
# by each client of GMock and / or tuple.
LOCAL_CFLAGS += \
    -DGTEST_HAS_TR1_TUPLE=1 \
    -DGTEST_USE_OWN_TR1_TUPLE=1

include $(BUILD_NATIVE_TEST)

include $(OPTIONAL_QUALITY_RUN_TEST)

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
