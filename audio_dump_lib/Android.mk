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

# Common variables
##################

hal_dump_src_files := HalAudioDump.cpp

hal_dump_cflags := -DDEBUG -Wall -Werror

hal_dump_includes_common := \
    $(LOCAL_PATH)/include

hal_dump_includes_dir_host := \
    $(call include-path-for, libc-kernel)

hal_dump_includes_dir_target := \
    $(call include-path-for, bionic)

#######################################################################
# Build for libhalaudiodump for host and target

# Compile macro
define make_hal_dump_lib
$( \
    $(eval LOCAL_EXPORT_C_INCLUDE_DIRS := $(hal_dump_includes_common)) \
    $(eval LOCAL_C_INCLUDES := $(hal_dump_includes_common)) \
    $(eval LOCAL_C_INCLUDES += $(hal_dump_includes_dir_$(1))) \
    $(eval LOCAL_SRC_FILES := $(hal_dump_src_files)) \
    $(eval LOCAL_CFLAGS := $(hal_dump_cflags)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
)
endef

# Build for target
##################
include $(CLEAR_VARS)
LOCAL_MODULE := libhalaudiodump
LOCAL_MODULE_OWNER := intel
$(call make_hal_dump_lib,target)
LOCAL_STATIC_LIBRARIES := libaudio_comms_utilities
#include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)


# Build for host
################
include $(CLEAR_VARS)
LOCAL_MODULE := libhalaudiodump_host
LOCAL_MODULE_OWNER := intel
$(call make_hal_dump_lib,host)
LOCAL_STATIC_LIBRARIES := libaudio_comms_utilities_host
include $(BUILD_HOST_STATIC_LIBRARY)
