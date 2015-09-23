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
    src/AudioUtils.cpp \
    src/SampleSpec.cpp

component_export_include_dir := \
    $(LOCAL_PATH)/include \

component_common_includes_dir := \
    $(component_export_include_dir) \
    $(call include-path-for, frameworks-av) \
    external/tinyalsa/include

component_includes_dir_host := \
    $(call include-path-for, libc-kernel)

component_includes_dir_target := \
    $(call include-path-for, bionic)

component_static_lib += \
    libaudio_comms_convert \
    libaudio_comms_utilities \

component_static_lib_host += \
    $(foreach lib, $(component_static_lib), $(lib)_host)

component_cflags := -Wall -Werror -Wextra -Wno-unused-parameter

#######################################################################
# Host Component Build

include $(CLEAR_VARS)

LOCAL_MODULE := libsamplespec_static_host
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

LOCAL_STRIP_MODULE := false

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_include_dir)
LOCAL_C_INCLUDES := \
    $(component_common_includes_dir) \
    $(component_includes_dir_host) \

LOCAL_SRC_FILES := $(component_src_files)
LOCAL_STATIC_LIBRARIES := $(component_static_lib_host)
LOCAL_CFLAGS := $(component_cflags) -O0 -ggdb

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_HOST_STATIC_LIBRARY)

#######################################################################
# Target Component Build

include $(CLEAR_VARS)

LOCAL_MODULE := libsamplespec_static
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_include_dir)
LOCAL_C_INCLUDES :=  \
    $(component_common_includes_dir) \
    $(component_includes_dir_target)

LOCAL_SRC_FILES := $(component_src_files)
LOCAL_STATIC_LIBRARIES := $(component_static_lib)
LOCAL_CFLAGS := $(component_cflags)

include $(BUILD_STATIC_LIBRARY)


#######################################################################
# Component Functional Test Common variables

component_functional_test_src_files += \
    test/SampleSpecTest.cpp \
    test/AudioUtilsTest.cpp

component_functional_test_export_c_includes += \
    mock

# Android gtest and gmock
component_functional_test_static_lib := \
    libgmock \
    unistd_mock \
    libsamplespec_static

# Compile macro
component_functional_test_c_includes_target += \
    $(foreach inc, $(component_functional_test_export_c_includes), $(TARGET_OUT_HEADERS)/$(inc)) \
    external/tinyalsa/include \

component_functional_test_c_includes_host += \
    $(foreach inc, $(component_functional_test_export_c_includes), $(HOST_OUT_HEADERS)/$(inc)) \
    external/tinyalsa/include \
    bionic/libc/kernel/common \
    external/gtest/include

component_functional_test_static_lib_host += \
    $(foreach lib, $(component_functional_test_static_lib), $(lib)_host) \
    libtinyalsa \
    liblog \
    libgtest_host \
    libgtest_main_host \

component_functional_test_static_lib_target += \
    $(component_functional_test_static_lib) \

component_functional_test_shared_lib_target += \
    libcutils


#######################################################################
# Component Functional Test Target Build

include $(CLEAR_VARS)
LOCAL_MODULE := samplespec_functional_test
LOCAL_MODULE_OWNER := intel

LOCAL_SRC_FILES := $(component_functional_test_src_files)
LOCAL_C_INCLUDES := $(component_functional_test_c_includes_target)
LOCAL_CFLAGS := $(component_functional_test_defines)
LOCAL_STATIC_LIBRARIES := $(component_functional_test_static_lib_target)
LOCAL_SHARED_LIBRARIES := $(component_functional_test_shared_lib_target)
LOCAL_MODULE_TAGS := optional

# GMock and GTest requires C++ Technical Report 1 (TR1) tuple library, which is not available
# on target (stlport). GTest provides its own implementation of TR1 (and substiture to standard
# implementation). This trick does not work well with latest compiler. Flags must be forced
# by each client of GMock and / or tuple.
LOCAL_CFLAGS += \
    -DGTEST_HAS_TR1_TUPLE=1 \
    -DGTEST_USE_OWN_TR1_TUPLE=1 \

include $(BUILD_NATIVE_TEST)

#######################################################################
# Component Functional Test Host Build

include $(CLEAR_VARS)
LOCAL_MODULE := samplespec_functional_test_host
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

LOCAL_LDFLAGS := -pthread
LOCAL_SRC_FILES := $(component_functional_test_src_files)
LOCAL_C_INCLUDES := $(component_functional_test_c_includes_host)
LOCAL_CFLAGS := $(component_functional_test_defines)
LOCAL_STATIC_LIBRARIES := $(component_functional_test_static_lib_host)
LOCAL_SHARED_LIBRARIES := $(component_functional_test_shared_lib_host)

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
# Cannot use $(BUILD_HOST_NATIVE_TEST) because of compilation flag
# misalignment against gtest mk files
include $(BUILD_HOST_EXECUTABLE)


include $(OPTIONAL_QUALITY_RUN_TEST)

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
