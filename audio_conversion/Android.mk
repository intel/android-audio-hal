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
audio_conversion_src_files :=  \
    src/AudioConversion.cpp \
    src/AudioConverter.cpp \
    src/AudioReformatter.cpp \
    src/AudioRemapper.cpp \
    src/AudioResampler.cpp \

audio_conversion_includes_common := \
    $(LOCAL_PATH)/include \
    $(call include-path-for, frameworks-av) \
    $(call include-path-for, audio-utils) \
    external/tinyalsa/include

audio_conversion_includes_dir_host := \
    $(call include-path-for, libc-kernel)

audio_conversion_includes_dir_target := \
    $(call include-path-for, bionic)

audio_conversion_static_lib += \
    libsamplespec_static \
    libaudio_comms_utilities

audio_conversion_static_lib_host += \
    $(foreach lib, $(audio_conversion_static_lib), $(lib)_host)

audio_conversion_static_lib_target += \
    $(audio_conversion_static_lib)

audio_conversion_cflags := -Wall -Werror -Wextra

# Compile macro
define make_audio_conversion_lib
$( \
    $(eval LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include) \
    $(eval LOCAL_C_INCLUDES := $(audio_conversion_includes_common)) \
    $(eval LOCAL_C_INCLUDES += $(audio_conversion_includes_dir_$(1))) \
    $(eval LOCAL_SRC_FILES := $(audio_conversion_src_files)) \
    $(eval LOCAL_CFLAGS := $(audio_conversion_cflags)) \
    $(eval LOCAL_STATIC_LIBRARIES := $(audio_conversion_static_lib_$(1))) \
    $(eval LOCAL_MODULE_TAGS := optional) \
)
endef


# Build for host test
include $(CLEAR_VARS)
LOCAL_MODULE := libaudioconversion_static_host
LOCAL_MODULE_OWNER := intel
$(call make_audio_conversion_lib,host)
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)


# Build for target
include $(CLEAR_VARS)
LOCAL_MODULE := libaudioconversion_static
LOCAL_MODULE_OWNER := intel
$(call make_audio_conversion_lib,target)
#include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)


# Component functional test
#######################################################################
audio_conversion_fcttest_src_files += \
    test/AudioConversionTest.cpp

audio_conversion_fcttest_c_includes += \
    external/tinyalsa/include \
    frameworks/av/include/media

# Other Lib
audio_conversion_fcttest_static_lib += \
    libsamplespec_static \
    libaudio_comms_utilities

# Compile macro
audio_conversion_fcttest_c_includes_target += \
    $(foreach inc, $(audio_conversion_fcttest_export_c_includes), $(TARGET_OUT_HEADERS)/$(inc))

audio_conversion_fcttest_c_includes_host += \
    $(foreach inc, $(audio_conversion_fcttest_export_c_includes), $(HOST_OUT_HEADERS)/$(inc)) \
    bionic/libc/kernel/common \
    external/gtest/include

audio_conversion_fcttest_static_lib_host += \
    $(foreach lib, $(audio_conversion_fcttest_static_lib), $(lib)_host) \
    libgtest_host \
    libgtest_main_host \
    libaudioutils \
    libspeexresampler \
    liblog

audio_conversion_fcttest_static_lib_target += \
    $(audio_conversion_fcttest_static_lib)

audio_conversion_fcttest_shared_lib_target += \
    libstlport libcutils \
    libaudioutils

# $(1): "_target" or "_host"
define make_audio_conversion_functional_test
$( \
    $(eval LOCAL_SRC_FILES += $(audio_conversion_fcttest_src_files)) \
    $(eval LOCAL_C_INCLUDES += $(audio_conversion_fcttest_c_includes)) \
    $(eval LOCAL_C_INCLUDES += $(audio_conversion_fcttest_c_includes$(1))) \
    $(eval LOCAL_CFLAGS += $(audio_conversion_fcttest_defines)) \
    $(eval LOCAL_STATIC_LIBRARIES += $(audio_conversion_fcttest_static_lib$(1))) \
    $(eval LOCAL_SHARED_LIBRARIES += $(audio_conversion_fcttest_shared_lib$(1))) \
    $(eval LOCAL_LDFLAGS += $(audio_conversion_fcttest_static_ldflags$(1))) \
)
endef


# Functional test target
include $(CLEAR_VARS)
LOCAL_MODULE := audio_conversion_functional_test
LOCAL_MODULE_OWNER := intel
LOCAL_STATIC_LIBRARIES += libaudioconversion_static
$(call make_audio_conversion_functional_test,_target)
LOCAL_MODULE_TAGS := optional

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
LOCAL_MODULE := audio_conversion_fcttest_host
LOCAL_MODULE_OWNER := intel
LOCAL_STATIC_LIBRARIES += libaudioconversion_static_host
$(call make_audio_conversion_functional_test,_host)
LOCAL_MODULE_TAGS := optional
LOCAL_LDFLAGS += -pthread
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
# Cannot use $(BUILD_HOST_NATIVE_TEST) because of compilation flag
# misalignment against gtest mk files
include $(BUILD_HOST_EXECUTABLE)

include $(OPTIONAL_QUALITY_RUN_TEST)

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
