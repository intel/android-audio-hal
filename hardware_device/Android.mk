#
#
# Copyright (C) Intel 2014-2018
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


include $(CLEAR_VARS)

# Target build
#######################################################################
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := \
    src/StreamWrapper.cpp
LOCAL_CFLAGS := $(HAL_COMMON_CFLAGS)
LOCAL_STATIC_LIBRARIES := libaudio_comms_utilities

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libaudiohw_intel
LOCAL_PROPRIETARY_MODULE := true
LOCAL_HEADER_LIBRARIES += libutils_headers libaudio_system_headers libhardware_headers
LOCAL_MODULE_OWNER := intel

include $(BUILD_STATIC_LIBRARY)


# Host build
#######################################################################
ifeq (ENABLE_HOST_VERSION,1)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := \
    src/StreamWrapper.cpp
LOCAL_CFLAGS := -Wall -Werror -Wextra -O0 -ggdb
LOCAL_STATIC_LIBRARIES := libaudio_comms_utilities_host

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libaudiohw_intel_host
LOCAL_MODULE_OWNER := intel
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)


# Host tests
#######################################################################
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/test \
    external/gtest/include

LOCAL_SRC_FILES := \
    test/DeviceWrapperTest.cpp \
    test/StreamWrapperTest.cpp

LOCAL_STATIC_LIBRARIES := libaudio_comms_utilities_host

LOCAL_STATIC_LIBRARIES += \
    libaudiohw_intel_host \
    libgtest_host \
    libgtest_main_host

LOCAL_CFLAGS := -ggdb -O0
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := audiohw_intel_test
LOCAL_MODULE_OWNER := intel
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
# Cannot use $(BUILD_HOST_NATIVE_TEST) because of compilation flag
# misalignment against gtest mk files
include $(BUILD_HOST_EXECUTABLE)
endif

include $(OPTIONAL_QUALITY_RUN_TEST)

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
