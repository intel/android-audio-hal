#
#
# Copyright (C) Intel 2015-2016
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

PFW_CORE := vendor/intel/audio/parameter-framework
BUILD_PFW_SETTINGS := $(PFW_CORE)/support/android/build_pfw_settings.mk
PFW_DEFAULT_SCHEMAS_DIR := $(PFW_CORE)/Schemas
PFW_SCHEMAS_DIR := $(PFW_DEFAULT_SCHEMAS_DIR)

#######################################################################
# Phony package definition

include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE := host_test_app_pfw_files
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional

LOCAL_REQUIRED_MODULES := \
    RouteParameterFrameworkTestHost.xml \
    AudioParameterFrameworkTestHost.xml

include $(BUILD_PHONY_PACKAGE)

##################Route Structures################

######### Route Subsystem  #######################

include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE := RouteSubsystemTestHost.xml
LOCAL_MODULE_STEM := RouteSubsystem.xml
LOCAL_REQUIRED_MODULES := \
        RouteSubsystem-RoutesTypesTestHost.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := Structure/Route/$(LOCAL_MODULE_STEM)
LOCAL_MODULE_PATH := $(HOST_OUT)/etc
LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Structure/Route
include $(BUILD_PREBUILT)

######### Route Common Types Component Set #########

include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE := RouteSubsystem-RoutesTypesTestHost.xml
LOCAL_MODULE_STEM := RouteSubsystem-RoutesTypes.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(HOST_OUT)/etc
LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Structure/Route
LOCAL_SRC_FILES := Structure/Route/$(LOCAL_MODULE_STEM)
include $(BUILD_PREBUILT)

####################################################
include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE := RouteClassTestHost.xml
LOCAL_MODULE_STEM := RouteClass.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(HOST_OUT)/etc
LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Structure/Route
LOCAL_SRC_FILES := Structure/Route/$(LOCAL_MODULE_STEM)
include $(BUILD_PREBUILT)

##################################################
#Generate routing domains file

include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE := RouteConfigurableDomainsTestHost.xml
LOCAL_MODULE_STEM := RouteConfigurableDomains.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(HOST_OUT)/etc
LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Settings/Route

include $(CLEAR_PFW_VARS)
PFW_TOPLEVEL_FILE := $(LOCAL_PATH)/RouteParameterFramework.xml
PFW_CRITERIA_FILE := $(LOCAL_PATH)/RouteCriteria.txt
PFW_EDD_FILES := $(LOCAL_PATH)/Settings/Route/routes.pfw

PFW_COPYBACK := $(LOCAL_PATH)/Settings/Route/$(LOCAL_MODULE)
include $(BUILD_PFW_SETTINGS)

##################################################

include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE := AudioClassTestHost.xml
LOCAL_MODULE_STEM := AudioClass.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := Structure/Audio/$(LOCAL_MODULE_STEM)
LOCAL_MODULE_PATH := $(HOST_OUT)/etc
LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Structure/Audio
include $(BUILD_PREBUILT)

##################################################

include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE := AudioSubsystemTestHost.xml
LOCAL_MODULE_STEM := AudioSubsystem.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := Structure/Audio/$(LOCAL_MODULE_STEM)
LOCAL_MODULE_PATH := $(HOST_OUT)/etc
LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Structure/Audio
include $(BUILD_PREBUILT)

##################################################
#Generate audio domains file

include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE := AudioConfigurableDomainsTestHost.xml
LOCAL_MODULE_STEM := AudioConfigurableDomains.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(HOST_OUT)/etc
LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Settings/Audio

include $(CLEAR_PFW_VARS)
PFW_TOPLEVEL_FILE := $(LOCAL_PATH)/AudioParameterFramework.xml
PFW_CRITERIA_FILE := $(LOCAL_PATH)/AudioCriteria.txt
PFW_EDD_FILES := $(LOCAL_PATH)/Settings/Audio/audio.pfw

PFW_COPYBACK := $(LOCAL_PATH)/Settings/Audio/$(LOCAL_MODULE)
include $(BUILD_PFW_SETTINGS)


##################################################
# Audio PF Top File

include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_REQUIRED_MODULES := \
    AudioConfigurableDomainsTestHost.xml \
    AudioClassTestHost.xml \
    AudioSubsystemTestHost.xml
LOCAL_MODULE := AudioParameterFrameworkTestHost.xml
LOCAL_MODULE_STEM := AudioParameterFramework.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := $(LOCAL_MODULE_STEM)
LOCAL_MODULE_PATH := $(HOST_OUT)/etc
LOCAL_MODULE_RELATIVE_PATH := parameter-framework
include $(BUILD_PREBUILT)

##################################################
# Route PF Top File

include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_REQUIRED_MODULES := \
    RouteConfigurableDomainsTestHost.xml \
    RouteClassTestHost.xml \
    RouteSubsystemTestHost.xml
LOCAL_MODULE := RouteParameterFrameworkTestHost.xml
LOCAL_MODULE_STEM := RouteParameterFramework.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := $(LOCAL_MODULE_STEM)
LOCAL_MODULE_PATH := $(HOST_OUT)/etc
LOCAL_MODULE_RELATIVE_PATH := parameter-framework
include $(BUILD_PREBUILT)
