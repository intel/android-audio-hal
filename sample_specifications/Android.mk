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
sample_specifications_src_files :=  \
    src/AudioUtils.cpp \
    src/SampleSpec.cpp

sample_specifications_common_includes_dir := \
    $(LOCAL_PATH)/include \
    $(call include-path-for, frameworks-av) \
    external/tinyalsa/include

sample_specifications_includes_dir_host := \
    $(foreach inc, $(sample_specifications_includes_dir), $(HOST_OUT_HEADERS)/$(inc)) \
    $(call include-path-for, libc-kernel)

sample_specifications_includes_dir_target := \
    $(foreach inc, $(sample_specifications_includes_dir), $(TARGET_OUT_HEADERS)/$(inc)) \
    $(call include-path-for, bionic)

sample_specifications_static_lib += \
    libaudio_comms_convert \
    libaudio_comms_utilities

sample_specifications_static_lib_host += \
    $(foreach lib, $(sample_specifications_static_lib), $(lib)_host)

sample_specifications_static_lib_target += \
    $(sample_specifications_static_lib)

sample_specifications_cflags := -Wall -Werror -Wextra -Wno-unused-parameter

# Compile macro
# $(1): "_target" or "_host"
define make_sample_specifications_lib
$( \
    $(eval LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include) \
    $(eval LOCAL_C_INCLUDES := $(sample_specifications_common_includes_dir)) \
    $(eval LOCAL_C_INCLUDES += $(sample_specifications_includes_dir_$(1))) \
    $(eval LOCAL_SRC_FILES := $(sample_specifications_src_files)) \
    $(eval LOCAL_STATIC_LIBRARIES := $(sample_specifications_static_lib_$(1))) \
    $(eval LOCAL_CFLAGS := $(sample_specifications_cflags)) \
)
endef

# Build for host test
include $(CLEAR_VARS)
LOCAL_MODULE := libsamplespec_static_host
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
$(call make_sample_specifications_lib,host)
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)

# Build for target
include $(CLEAR_VARS)
LOCAL_MODULE := libsamplespec_static
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
$(call make_sample_specifications_lib,target)
#include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)


# Component functional test
#######################################################################
sample_specifications_functional_test_src_files += \
    test/SampleSpecTest.cpp \
    test/AudioUtilsTest.cpp

sample_specifications_functional_test_c_includes += \
    external/tinyalsa/include

# Mock
sample_specifications_functional_test_static_lib += \
    unistd_mock

sample_specifications_functional_test_export_c_includes += \
    mock

# Android gtest and gmock
sample_specifications_functional_test_static_lib += \
    libgmock

# Compile macro
sample_specifications_functional_test_c_includes_target += \
    $(foreach inc, $(sample_specifications_functional_test_export_c_includes), $(TARGET_OUT_HEADERS)/$(inc)) \

sample_specifications_functional_test_c_includes_host += \
    $(foreach inc, $(sample_specifications_functional_test_export_c_includes), $(HOST_OUT_HEADERS)/$(inc)) \
    bionic/libc/kernel/common \
    external/gtest/include

sample_specifications_functional_test_static_lib_host += \
    $(foreach lib, $(sample_specifications_functional_test_static_lib), $(lib)_host) \
    liblog \
    libgtest_host \
    libgtest_main_host

sample_specifications_functional_test_static_lib_target += \
    $(sample_specifications_functional_test_static_lib)

sample_specifications_functional_test_shared_lib_target += \
    libstlport libcutils

# $(1): "_target" or "_host"
define make_samplespec_functional_test
$( \
    $(eval LOCAL_SRC_FILES += $(sample_specifications_functional_test_src_files)) \
    $(eval LOCAL_C_INCLUDES += $(sample_specifications_functional_test_c_includes)) \
    $(eval LOCAL_C_INCLUDES += $(sample_specifications_functional_test_c_includes$(1))) \
    $(eval LOCAL_CFLAGS += $(sample_specifications_functional_test_defines)) \
    $(eval LOCAL_STATIC_LIBRARIES += $(sample_specifications_functional_test_static_lib$(1))) \
    $(eval LOCAL_SHARED_LIBRARIES += $(sample_specifications_functional_test_shared_lib$(1))) \
    $(eval LOCAL_LDFLAGS += $(sample_specifications_functional_test_static_ldflags$(1))) \
    $(eval LOCAL_MODULE_TAGS := optional) \
)
endef


# Functional test target
include $(CLEAR_VARS)
LOCAL_MODULE := samplespec_functional_test
LOCAL_MODULE_OWNER := intel
LOCAL_STATIC_LIBRARIES += libsamplespec_static
$(call make_samplespec_functional_test,_target)

# GMock and GTest requires C++ Technical Report 1 (TR1) tuple library, which is not available
# on target (stlport). GTest provides its own implementation of TR1 (and substiture to standard
# implementation). This trick does not work well with latest compiler. Flags must be forced
# by each client of GMock and / or tuple.
LOCAL_CFLAGS += \
    -DGTEST_HAS_TR1_TUPLE=1 \
    -DGTEST_USE_OWN_TR1_TUPLE=1 \

include $(BUILD_NATIVE_TEST)


# Functional test host
include $(CLEAR_VARS)
LOCAL_MODULE := samplespec_functional_test_host
LOCAL_MODULE_OWNER := intel
LOCAL_STATIC_LIBRARIES += libsamplespec_static_host
$(call make_samplespec_functional_test,_host)
LOCAL_LDFLAGS += -pthread
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
# Cannot use $(BUILD_HOST_NATIVE_TEST) because of compilation flag
# misalignment against gtest mk files
include $(BUILD_HOST_EXECUTABLE)

include $(OPTIONAL_QUALITY_RUN_TEST)

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
