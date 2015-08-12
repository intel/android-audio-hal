/*
 * Copyright (C) 2014-2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <string>

namespace intel_audio
{
#ifndef HAL_CONF_FILE_PATH
#define HAL_CONF_FILE_PATH  "/system/etc"
#endif

static const char *const gAudioHalConfFilePath =
    HAL_CONF_FILE_PATH"/route_criteria.conf";

static const char *const gAudioHalVendorConfFilePath =
    "/vendor/etc/route_criteria.conf";

/**
 * PFW instances tags
 */
static const std::string gAudioConfTag = "Audio";
static const std::string gRouteConfTag = "Route";
static const std::string gCommonConfTag = "Common";

/**
 * PFW elements tags
 */
static const std::string gInclusiveCriterionTypeTag = "InclusiveCriterionType";
static const std::string gExclusiveCriterionTypeTag = "ExclusiveCriterionType";
static const std::string gCriterionTag = "Criterion";
static const std::string gRogueParameterTag = "RogueParameter";

/**
 * PFW Elements specific tags
 */
static const std::string gParameterDefaultTag = "Default";
static const std::string gAndroidParameterTag = "Parameter";
static const std::string gMappingTableTag = "Mapping";
static const std::string gTypeTag = "Type";
static const std::string gPathTag = "Path";
static const std::string gUnsignedIntegerTypeTag = "uint32_t";
static const std::string gStringTypeTag = "string";
static const std::string gDoubleTypeTag = "double";

/**
 * ValueSet tags
 */
static const std::string gValueSet = "ValueSet";
static const std::string gModemValueSet = "ModemValueSet";
static const std::string gInterfaceLibraryName = "InterfaceLibrary";
static const std::string gInterfaceLibraryInstance = "Instance";

} // namespace intel_audio
