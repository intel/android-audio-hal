/*
 * Copyright (C) 2013-2015 Intel Corporation
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
#define LOG_TAG "ParameterHelper"

#include "ParameterMgrHelper.hpp"
#include "ParameterMgrPlatformConnector.h"
#include "SelectionCriterionInterface.h"
#include <utilities/Log.hpp>
#include <string>

using audio_comms::utilities::Log;
using std::map;
using std::string;
using std::vector;
using namespace android;

ParameterMgrHelper::ParameterMgrHelper(CParameterMgrPlatformConnector *pfwConnector)
    : mPfwConnector(pfwConnector)
{
}

ParameterMgrHelper::~ParameterMgrHelper()
{
    ParameterHandleMapIterator it;

    for (it = mParameterHandleMap.begin(); it != mParameterHandleMap.end(); ++it) {

        delete it->second;
    }
    mParameterHandleMap.clear();
}

template <>
bool ParameterMgrHelper::setAsTypedValue<uint32_t>(CParameterHandle *parameterHandle,
                                                   const uint32_t &value, string &error)
{
    bool success;
    if (parameterHandle->isArray()) {
        vector<uint32_t> arrayValue(parameterHandle->getArrayLength(), value);
        success = parameterHandle->setAsIntegerArray(arrayValue, error);
    } else {

        success = parameterHandle->setAsInteger(value, error);
    }

    if (!success) {

        Log::Error() << "Unable to set value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

template <>
bool ParameterMgrHelper::getAsTypedValue<uint32_t>(CParameterHandle *parameterHandle,
                                                   uint32_t &value, string &error)
{
    if (!parameterHandle->getAsInteger(value, error)) {
        Log::Error() << "Unable to get value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

template <>
bool ParameterMgrHelper::setAsTypedValue<vector<uint32_t> >(CParameterHandle *parameterHandle,
                                                            const vector<uint32_t> &value,
                                                            string &error)
{
    if (!parameterHandle->setAsIntegerArray(value, error)) {
        Log::Error() << "Unable to set value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

template <>
bool ParameterMgrHelper::setAsTypedValue<string>(CParameterHandle *parameterHandle,
                                                 const string &value, string &error)
{
    if (!parameterHandle->setAsString(value, error)) {
        Log::Error() << "Unable to get value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

template <>
bool ParameterMgrHelper::getAsTypedValue<string>(CParameterHandle *parameterHandle,
                                                 string &value, string &error)
{
    if (!parameterHandle->getAsString(value, error)) {
        Log::Error() << "Unable to get value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

template <>
bool ParameterMgrHelper::setAsTypedValue<double>(CParameterHandle *parameterHandle,
                                                   const double &value, string &error)
{
    bool success;
    if (parameterHandle->isArray()) {
        vector<double> arrayValue(parameterHandle->getArrayLength(), value);
        success = parameterHandle->setAsDoubleArray(arrayValue, error);
    } else {

        success = parameterHandle->setAsDouble(value, error);
    }

    if (!success) {

        Log::Error() << "Unable to set value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

template <>
bool ParameterMgrHelper::getAsTypedValue<double>(CParameterHandle *parameterHandle,
                                                   double &value, string &error)
{
    if (!parameterHandle->getAsDouble(value, error)) {

        Log::Error() << "Unable to set value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

template <>
bool ParameterMgrHelper::setAsTypedValue<vector<double> >(CParameterHandle *parameterHandle,
                                                            const vector<double> &value,
                                                            string &error)
{
    if (!parameterHandle->setAsDoubleArray(value, error)) {

        Log::Error() << "Unable to set value: " << error
                     << ", from parameter path: " << parameterHandle->getPath();
        return false;
    }
    return true;
}

bool ParameterMgrHelper::getParameterHandle(CParameterMgrPlatformConnector *pfwConnector,
                                            CParameterHandle * &handle,
                                            const string &path)
{
    if (pfwConnector == NULL || !pfwConnector->isStarted()) {
        Log::Error() << __FUNCTION__ << ": PFW connector is NULL or PFW is not started";
        return false;
    }
    string error;
    handle = pfwConnector->createParameterHandle(path, error);
    if (!handle) {
        Log::Error() << __FUNCTION__ << ": Unable to get handle for " << path << "' '" << error;
        return false;
    }
    return true;
}

CParameterHandle *ParameterMgrHelper::getPlatformParameterHandle(const string &paramPath) const
{
    string platformParamPath;

    // First retrieve the platform dependant parameter path
    if (!getParameterValue<string>(mPfwConnector, paramPath, platformParamPath)) {
        Log::Error() << "Could not retrieve parameter path handler";
        return NULL;
    }
    Log::Debug() << __FUNCTION__ << ": Platform specific parameter path=" << platformParamPath;

    // Initialise handle to NULL to avoid KW "false-positive".
    CParameterHandle *handle = NULL;
    if (!getParameterHandle(mPfwConnector, handle, platformParamPath)) {
        return NULL;
    }
    return handle;
}

CParameterHandle *ParameterMgrHelper::getDynamicParameterHandle(const string &dynamicParamPath)
{
    if (mParameterHandleMap.find(dynamicParamPath) == mParameterHandleMap.end()) {
        Log::Debug() << __FUNCTION__ << ": Dynamic parameter " << dynamicParamPath
                     << " not found in map, get a handle and push it in the map";
        mParameterHandleMap[dynamicParamPath] = getPlatformParameterHandle(dynamicParamPath);
    }
    return mParameterHandleMap[dynamicParamPath];
}
