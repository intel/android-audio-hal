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
#include <boost/tokenizer.hpp>
#include <property/Property.hpp>

using std::string;
using audio_comms::utilities::Log;
using audio_comms::utilities::Property;

namespace intel_audio
{

Parameter::Parameter(const std::string &key,
          const std::string &name,
          const std::string &defaultValue,
          const std::string &androidProperty)
    : mDefaultLiteralValue(androidProperty.empty() ? defaultValue : Property<string>(androidProperty, defaultValue).getValue()),
      mAndroidParameterKey(key),
      mAndroidParameter(name),
      mAndroidProperty(androidProperty)
{
}

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
    // More than one android param may be sent with pipe delimiter
    boost::char_separator<char> sep("|");
    boost::tokenizer<boost::char_separator<char>> tokenizeParam(androidParam, sep);

    for (const auto &param : tokenizeParam) {
        MappingValuesMapConstIterator it = mMappingValuesMap.find(param);
        if (it == mMappingValuesMap.end()) {
            Log::Verbose() << __FUNCTION__ << ": unknown parameter value \"" << param
                           << "\" for " << mAndroidParameterKey;
            return false;
        }
        literalValue += (literalValue.empty() ? "" : "|") + it->second;
    }
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

    boost::char_separator<char> sep("|");
    boost::tokenizer<boost::char_separator<char>> tokenizeLiteral(literalValue, sep);

    for (const auto &literal : tokenizeLiteral) {
        for (MappingValuesMapConstIterator it = mMappingValuesMap.begin();
             it != mMappingValuesMap.end();
             ++it) {
            if (it->second == literal) {
                androidParam += (androidParam.empty() ? "" : "|") + it->first;
            }
        }
    }
    return true;
}

} // namespace intel_audio
