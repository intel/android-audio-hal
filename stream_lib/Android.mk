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

#######################################################################
# Common variables

stream_lib_src_files :=  \
    IoStream.cpp \
    TinyAlsaAudioDevice.cpp \
    StreamLib.cpp \
    TinyAlsaIoStream.cpp

stream_lib_includes_common := \
    $(LOCAL_PATH)/include

stream_lib_includes_dir := \
    $(TARGET_OUT_HEADERS)/hw \
    $(TARGET_OUT_HEADERS)/parameter \
    $(call include-path-for, frameworks-av) \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils)

stream_lib_includes_dir_host := \
    $(stream_lib_includes_dir)

stream_lib_includes_dir_target := \
    $(stream_lib_includes_dir) \
    $(call include-path-for, bionic)

stream_lib_header_copy_folder_unit_test := \
    stream_lib_unit_test

stream_lib_static_lib += \
    libsamplespec_static \
    libaudio_comms_utilities \
    audio.routemanager.includes

stream_lib_static_lib_host += \
    $(foreach lib, $(stream_lib_static_lib), $(lib)_host)

stream_lib_static_lib_target += \
    $(stream_lib_static_lib) \
    libmedia_helper

stream_lib_include_dirs_from_static_libraries := \
    libproperty_static

stream_lib_include_dirs_from_static_libraries_target := \
    $(stream_lib_include_dirs_from_static_libraries)

stream_lib_include_dirs_from_static_libraries_host := \
    $(foreach lib, $(stream_lib_include_dirs_from_static_libraries), $(lib)_host)

stream_lib_cflags := -Wall -Werror -Wno-unused-parameter


#######################################################################
# Build for test with and without gcov for host and target

# Compile macro
define make_stream_lib_test_lib
$( \
    $(eval LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include) \
    $(eval LOCAL_C_INCLUDES := $(stream_lib_includes_common)) \
    $(eval LOCAL_C_INCLUDES += $(stream_lib_includes_dir_$(1))) \
    $(eval LOCAL_STATIC_LIBRARIES := $(stream_lib_static_lib_$(1)) \
        $(stream_lib_include_dirs_from_static_libraries_$(1)) \
        audio.routemanager.includes )
    $(eval LOCAL_SRC_FILES := $(stream_lib_src_files)) \
    $(eval LOCAL_CFLAGS := $(stream_lib_cflags)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
)
endef

define add_gcov
$( \
    $(eval LOCAL_CFLAGS += -O0 -fprofile-arcs -ftest-coverage) \
    $(eval LOCAL_LDFLAGS += -lgcov) \
)
endef

# Build for host test with gcov
ifeq ($(audiocomms_test_gcov_host),true)

include $(CLEAR_VARS)
$(call make_stream_lib_test_lib,host)
$(call add_gcov)
LOCAL_MODULE := libstream_static_gcov_host
LOCAL_MODULE_OWNER := intel
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target test with gcov
ifeq ($(audiocomms_test_gcov_target),true)

include $(CLEAR_VARS)
LOCAL_MODULE := libstream_static_gcov
LOCAL_MODULE_OWNER := intel
$(call make_stream_lib_test_lib,target)
$(call add_gcov)
include $(BUILD_STATIC_LIBRARY)

endif

# Build for host test
ifeq ($(audiocomms_test_host),true)

include $(CLEAR_VARS)
$(call make_stream_lib_test_lib,host)
LOCAL_MODULE := libstream_static_host
LOCAL_MODULE_OWNER := intel
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target (Inconditionnal)

include $(CLEAR_VARS)
LOCAL_MODULE := libstream_static
LOCAL_MODULE_OWNER := intel
$(call make_stream_lib_test_lib,target)
include $(BUILD_STATIC_LIBRARY)
