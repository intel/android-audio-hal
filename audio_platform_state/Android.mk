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

# Target build
#######################################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libaudioplatformstate
LOCAL_MODULE_OWNER := intel
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    external/tinyalsa/include \
    frameworks/av/include/media \
    $(TARGET_OUT_HEADERS)/hw \
    $(TARGET_OUT_HEADERS)/parameter \

LOCAL_SRC_FILES := \
    src/Parameter.cpp \
    src/CriterionParameter.cpp \
    src/AudioPlatformState.cpp \
    src/VolumeKeys.cpp \

LOCAL_STATIC_LIBRARIES := \
    libsamplespec_static \
    libstream_static \
    libparametermgr_static \
    libaudio_comms_utilities \
    libaudio_comms_convert \
    liblpepreprocessinghelper \
    libaudiocomms_naive_tokenizer \
    libproperty_static \
    audio.routemanager.includes \
    libmedia_helper \
    libaudioparameters \
    libevent-listener_static \
    libparameter_includes

LOCAL_CFLAGS := -Wall -Werror -Wextra -Wno-unused-parameter

ifneq ($(strip $(PFW_CONFIGURATION_FOLDER)),)
LOCAL_CFLAGS += -DPFW_CONF_FILE_PATH=\"$(PFW_CONFIGURATION_FOLDER)\"
endif

LOCAL_MODULE_TAGS := optional
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

#include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)

# Build for configuration file
#######################################################################

include $(CLEAR_VARS)
LOCAL_MODULE := route_criteria.conf
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_SRC_FILES := config/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

#######################################################################

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
