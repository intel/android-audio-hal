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

effect_pre_proc_src_files :=  \
    src/LpeNs.cpp \
    src/LpeAgc.cpp \
    src/LpeAec.cpp \
    src/LpeBmf.cpp \
    src/LpeWnr.cpp \
    src/AudioEffect.cpp \
    src/AudioEffectSession.cpp \
    src/LpeEffectLibrary.cpp \
    src/LpePreProcessing.cpp

effect_pre_proc_includes_dir := \

effect_pre_proc_includes_common := \
    $(LOCAL_PATH)/include \
    $(call include-path-for, audio-effects)

effect_pre_proc_includes_dir_host := \
    $(foreach inc, $(effect_pre_proc_includes_dir), $(HOST_OUT_HEADERS)/$(inc)) \
    $(call include-path-for, libc-kernel)

effect_pre_proc_includes_dir_target := \
    $(foreach inc, $(effect_pre_proc_includes_dir), $(TARGET_OUT_HEADERS)/$(inc))

effect_pre_proc_static_lib += \
    libaudio_comms_utilities \
    libaudio_comms_convert \

effect_pre_proc_static_lib_host += \
    $(foreach lib, $(effect_pre_proc_static_lib), $(lib)_host) \

effect_pre_proc_static_lib_target += \
    $(effect_pre_proc_static_lib) \
    libmedia_helper \

effect_pre_proc_shared_lib_target += \
    libutils  \
    libcutils \
    libmedia

effect_pre_proc_cflags := -Wall -Werror -Wno-unused-parameter

#######################################################################
# Build for target

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(effect_pre_proc_includes_dir_target) \
    $(effect_pre_proc_includes_common)

LOCAL_SRC_FILES := $(effect_pre_proc_src_files)
LOCAL_CFLAGS := $(effect_pre_proc_cflags)

LOCAL_MODULE := liblpepreprocessing
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := \
    $(effect_pre_proc_static_lib_target)
LOCAL_SHARED_LIBRARIES := \
    $(effect_pre_proc_shared_lib_target)
LOCAL_MODULE_RELATIVE_PATH := soundfx

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_SHARED_LIBRARY)

# Helper Effect Lib
#######################################################################
# Build for target

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_CFLAGS := $(effect_pre_proc_cflags)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := src/EffectHelper.cpp
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := liblpepreprocessinghelper
LOCAL_MODULE_OWNER := intel

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_STATIC_LIBRARY)

#######################################################################
# Build for host
ifeq (0,1)
include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := $(effect_pre_proc_cflags)
LOCAL_SRC_FILES := src/EffectHelper.cpp
LOCAL_MODULE_TAGS := tests

LOCAL_MODULE := liblpepreprocessinghelper_host
LOCAL_MODULE_OWNER := intel
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)
endif

# Component functional test
#######################################################################

audio_effects_functional_test_static_lib += \
    libaudio_comms_utilities \

audio_effects_functional_test_src_files := \
    test/AudioEffectsFcct.cpp

audio_effects_functional_test_c_includes := \
    $(call include-path-for, audio-effects)

audio_effects_functional_test_static_lib_target := \
    $(audio_effects_functional_test_static_lib)

audio_effects_functional_test_shared_lib_target := \
    libcutils \
    libbinder \
    libmedia \
    libutils

audio_effects_functional_test_defines += -Wall -Werror -ggdb -O0

###############################
# Functional test target
ifeq (0,1)
include $(CLEAR_VARS)

LOCAL_MODULE := audio_effects_functional_test
LOCAL_MODULE_OWNER := intel
LOCAL_SRC_FILES := $(audio_effects_functional_test_src_files)
LOCAL_C_INCLUDES := $(audio_effects_functional_test_c_includes)
LOCAL_CFLAGS := $(audio_effects_functional_test_defines)
LOCAL_STATIC_LIBRARIES := $(audio_effects_functional_test_static_lib_target)
LOCAL_SHARED_LIBRARIES := $(audio_effects_functional_test_shared_lib_target)
LOCAL_MODULE_TAGS := optional
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_NATIVE_TEST)
endif

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
