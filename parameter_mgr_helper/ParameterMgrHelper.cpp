/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
 * Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#define LOG_TAG "ParameterHelper"

#include "ParameterMgrHelper.hpp"
#include "ParameterMgrPlatformConnector.h"
#include "SelectionCriterionInterface.h"
#include <string>
#include <utils/Log.h>

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

uint32_t ParameterMgrHelper::getIntegerParameterValue(const string &paramPath,
                                                      uint32_t defaultValue) const
{
    ALOGV("%s in", __FUNCTION__);

    if (!mPfwConnector->isStarted()) {

        return defaultValue;
    }

    string error;
    // Get handle
    CParameterHandle *parameterHandle = mPfwConnector->createParameterHandle(paramPath,
                                                                             error);
    if (!parameterHandle) {

        ALOGE("Unable to get parameter handle: '%s' '%s'", paramPath.c_str(), error.c_str());
        ALOGV("%s returning %d", __FUNCTION__, defaultValue);

        return defaultValue;
    }

    // Retrieve value
    uint32_t value;
    error.clear();
    if (!parameterHandle->getAsInteger(value, error)) {

        ALOGE("Unable to get value: %s, from parameter path: %s", error.c_str(), paramPath.c_str());
        ALOGV("%s returning %d", __FUNCTION__, defaultValue);
        value = defaultValue;
    }

    // Remove handle
    delete parameterHandle;

    ALOGV("%s: %s is %d", __FUNCTION__, paramPath.c_str(), value);

    return value;
}

status_t ParameterMgrHelper::getStringParameterValue(const string &paramPath,
                                                     string &value) const
{
    if (!mPfwConnector->isStarted()) {

        return DEAD_OBJECT;
    }

    string error;
    // Get handle
    CParameterHandle *parameterHandle = mPfwConnector->createParameterHandle(paramPath, error);

    if (!parameterHandle) {

        ALOGE("Unable to get parameter handle: '%s' '%s'", paramPath.c_str(), error.c_str());

        return DEAD_OBJECT;
    }

    // Retrieve value
    status_t ret = OK;
    error.clear();
    if (!parameterHandle->getAsString(value, error)) {

        ALOGE("Unable to get value: %s, from parameter path: %s", error.c_str(), paramPath.c_str());
        ret = BAD_VALUE;
    }

    // Remove handle
    delete parameterHandle;

    ALOGV_IF(!ret, "%s: %s is %s", __FUNCTION__, paramPath.c_str(), value.c_str());

    return ret;
}

status_t ParameterMgrHelper::setIntegerParameterValue(const string &paramPath, uint32_t value)
{
    if (!mPfwConnector->isStarted()) {

        return INVALID_OPERATION;
    }

    string error;
    // Get handle
    CParameterHandle *parameterHandle = mPfwConnector->createParameterHandle(paramPath, error);

    if (!parameterHandle) {

        ALOGE("Unable to get parameter handle: '%s' '%s'", paramPath.c_str(), error.c_str());

        return NAME_NOT_FOUND;
    }

    // set value
    status_t ret = OK;
    error.clear();
    if (!parameterHandle->setAsInteger(value, error)) {

        ALOGE("Unable to set value: %s, from parameter path: %s", error.c_str(), paramPath.c_str());
        ret = NAME_NOT_FOUND;
    }

    // Remove handle
    delete parameterHandle;

    ALOGV_IF(!ret, "%s:  %s set to %d", __FUNCTION__, paramPath.c_str(), value);

    return ret;
}

status_t ParameterMgrHelper::setIntegerArrayParameterValue(const string &paramPath,
                                                           vector<uint32_t> &array) const
{
    if (!mPfwConnector->isStarted()) {

        return INVALID_OPERATION;
    }

    string error;
    // Get handle
    CParameterHandle *parameterHandle = mPfwConnector->createParameterHandle(paramPath, error);

    if (!parameterHandle) {

        ALOGE("Unable to get parameter handle: '%s' '%s'", paramPath.c_str(), error.c_str());

        return NAME_NOT_FOUND;
    }

    // set value
    status_t ret = OK;
    error.clear();
    if (!parameterHandle->setAsIntegerArray(array, error)) {

        ALOGE("Unable to set value: %s, from parameter path: %s", error.c_str(), paramPath.c_str());
        ret = INVALID_OPERATION;
    }

    // Remove handle
    delete parameterHandle;

    return ret;
}

CParameterHandle *ParameterMgrHelper::getParameterHandle(const string &paramPath)
{
    string parameter;

    // First retrieve the platform dependant parameter path
    status_t ret = getStringParameterValue(paramPath, parameter);
    if (ret != NO_ERROR) {

        ALOGE("Could not retrieve parameter path handler err=%d", ret);
        return NULL;
    }
    ALOGD("%s  Platform specific parameter path=%s", __FUNCTION__, parameter.c_str());

    string error;
    CParameterHandle *handle = mPfwConnector->createParameterHandle(parameter, error);
    if (!handle) {

        ALOGE("%s: Unable to get parameter handle: '%s' '%s'", __FUNCTION__,
              paramPath.c_str(), error.c_str());
    }
    return handle;
}

CParameterHandle *ParameterMgrHelper::getDynamicParameterHandle(const string &dynamicParamPath)
{
    if (mParameterHandleMap.find(dynamicParamPath) == mParameterHandleMap.end()) {
        ALOGD("Dynamic parameter %s not found in map, get a handle and push it in the map",
              dynamicParamPath.c_str());
        mParameterHandleMap[dynamicParamPath] = getParameterHandle(dynamicParamPath);
    }
    return mParameterHandleMap[dynamicParamPath];
}
