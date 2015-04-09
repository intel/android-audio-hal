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

#include "Parameter.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using std::string;
using audio_comms::utilities::Log;

namespace intel_audio
{

void Parameter::setMappingValuePair(const string &name, const string &value)
{
    if (mMappingValuesMap.find(name) != mMappingValuesMap.end()) {
        Log::Warning() << __FUNCTION__ << ": parameter value " << name << " already appended";
        return;
    }
    mMappingValuesMap[name] = value;
}

bool Parameter::getLiteralValueFromParam(const string &androidParam, string &literalValue) const
{
    if (mMappingValuesMap.empty()) {

        // No Mapping table has been provided, all value that may take the Android Param
        // will be propagated to the PFW Parameter
        literalValue = androidParam;
        return true;
    }
    if (!isAndroidParameterValueValid(androidParam)) {

        Log::Verbose() << __FUNCTION__ << ": unknown parameter value(" << androidParam
                       << ") for " << mAndroidParameterKey << "";
        return false;
    }
    MappingValuesMapConstIterator it = mMappingValuesMap.find(androidParam);
    AUDIOCOMMS_ASSERT(it != mMappingValuesMap.end(), "Value not valid");
    literalValue = it->second;
    return true;
}

bool Parameter::getParamFromLiteralValue(string &androidParam, const string &literalValue) const
{
    // No Mapping table has been provided, all value that may take the PFW Param
    // will be propagated to the Android Parameter
    if (mMappingValuesMap.empty()) {

        androidParam = literalValue;
        return true;
    }
    for (MappingValuesMapConstIterator it = mMappingValuesMap.begin();
         it != mMappingValuesMap.end();
         ++it) {

        if (it->second == literalValue) {
            androidParam = it->first;
            return true;
        }
    }
    return false;
}

} // namespace intel_audio
