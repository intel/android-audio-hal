/*
 * Copyright (C) 2014-2017 Intel Corporation
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
#if defined (HAVE_BOOST)
#include <boost/tokenizer.hpp>
#endif

using std::string;
using audio_comms::utilities::Log;

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

#if defined (HAVE_BOOST)
    // More than one android param may be sent with pipe delimiter
    boost::char_separator<char> sep("|");
    boost::tokenizer < boost::char_separator < char >> tokenizeParam(androidParam, sep);
    for (const auto &param : tokenizeParam) {
#else
    char *ctx;
    char *androidParamsDup = strndup(androidParam.c_str(), strlen(androidParam.c_str()));
    for (char *param = strtok_r(androidParamsDup, "|", &ctx);
         param != NULL;
         param = strtok_r(NULL, ",", &ctx)) {
#endif
        MappingValuesMapConstIterator it = mMappingValuesMap.find(param);
        if (it == mMappingValuesMap.end()) {
            Log::Verbose() << __FUNCTION__ << ": unknown parameter value \"" << param
                           << "\" for " << mAndroidParameterKey;
#if (not defined (HAVE_BOOST))
            free(androidParamsDup);
#endif
            return false;
        }
        literalValue += (literalValue.empty() ? "" : "|") + it->second;
    }
#if (not defined (HAVE_BOOST))
    free(androidParamsDup);
#endif
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
#if defined (HAVE_BOOST)
    boost::char_separator<char> sep("|");
    boost::tokenizer < boost::char_separator < char >> tokenizeLiteral(literalValue, sep);
    for (const auto &literal : tokenizeLiteral) {
#else
    char *ctx;
    char *literalValueDup = strndup(literalValue.c_str(), strlen(literalValue.c_str()));
    for (char *literal = strtok_r(literalValueDup, "|", &ctx);
         literal != NULL;
         literal = strtok_r(NULL, ",", &ctx)) {
#endif
        for (const auto &mappingValue : mMappingValuesMap) {
            if (mappingValue.second == literal) {
                androidParam += (androidParam.empty() ? "" : "|") + mappingValue.first;
            }
        }
    }
#if (not defined (HAVE_BOOST))
    free(literalValueDup);
#endif
    return true;
}
