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
#pragma once

#include <map>
#include <string>
#include <utils/Errors.h>
#include "ParameterMgrPlatformConnector.h"
#include <vector>

class CParameterMgrPlatformConnector;
class CParameterHandle;


class ParameterMgrHelper
{
private:
    typedef std::map<std::string, CParameterHandle *>::iterator ParameterHandleMapIterator;

public:
    ParameterMgrHelper(CParameterMgrPlatformConnector *pfwConnector);

    virtual ~ParameterMgrHelper();

    /**
     * Get a value from a parameter path.
     * It returns the value stored in the parameter framework under the given path.
     *
     * @param[in] connector to Parameter Framework.
     * @param[in] path of the parameter.
     * @param[out] value stored under this path.
     *
     * @return status_t OK if success, error code otherwise and value is kept unchanged.
     */
    template <typename T>
    static bool getParameterValue(CParameterMgrPlatformConnector *connector,
                                  const std::string &path, T &value)
    {
        std::string error;
        // Initialize handle to NULL to avoid KW "false-positive".
        CParameterHandle *handle = NULL;
        if (!getParameterHandle(connector, handle, path)) {
            return false;
        }
        bool ret = getAsTypedValue<T>(handle, value, error);
        delete handle;
        return ret;
    }

    /**
     * Set a parameter with a typed value.
     *
     * @param[in] connector to Parameter Framework.
     * @param[in] path of the parameter.
     * @param[in] value default to set.
     *
     * @return OK if success, error code otherwise.
     */
    template <typename T>
    static bool setParameterValue(CParameterMgrPlatformConnector *connector,
                                  const std::string &path, const T &value)
    {
        std::string error;
        // Initialize handle to NULL to avoid KW "false-positive".
        CParameterHandle *handle = NULL;
        if (!getParameterHandle(connector, handle, path)) {
            return false;
        }
        bool ret = setAsTypedValue<T>(handle, value, error);
        delete handle;
        return ret;
    }

    /**
     * Get a typed value from a parameter path.
     * It returns the value stored in the parameter framework under the given path.
     *
     * @tparam T type of the value to be retrieved
     * @param[in] parameterHandle handle on the parameter.
     * @param[out] value read from this path, valid only if true is returned.
     * @param[out] error human readable error, set only if false is returned.
     *
     * @return true if success, false otherwise and error is set.
     */
    template <typename T>
    static bool getAsTypedValue(CParameterHandle *parameterHandle, T &value,
                                std::string &error);

    /**
     * Set a typed value to a parameter path.
     *
     * @tparam T type of the value to be retrieved
     * @param[in] parameterHandle handle on the parameter.
     * @param[in] value to set to this path.
     * @param[out] error human readable error, set only if false is returned.
     *
     * @return true if success, false otherwise and error is set.
     */
    template <typename T>
    static bool setAsTypedValue(CParameterHandle *parameterHandle, const T &value,
                                std::string &error);

    /**
     * Get a handle on the platform dependent parameter.
     * It first reads the string value of the dynamic parameter which represents the path of the
     * platform dependent parameter.
     * Then it gets a handle on this platform dependent parameter.
     *
     * @param[in] dynamicParamPath path of the dynamic parameter (platform agnostic).
     *
     * @return CParameterHandle handle on the parameter, NULL pointer if error.
     */
    CParameterHandle *getDynamicParameterHandle(const std::string &dynamicParamPath);

private:
    /**
     * Helper function to retrieve a handle on a parameter.
     *
     * @param[in] pfwConnector Parameter Manager Connector object.
     * @param[out] handle on the parameter.
     * @param[in] paramPath of the parameter on which a handler is requested;
     *
     * @return CParameterHandle handle on the parameter, NULL pointer if error.
     */
    static bool getParameterHandle(CParameterMgrPlatformConnector *pfwConnector,
                                   CParameterHandle * &handle,
                                   const std::string &paramPath);

    /**
     * Get a handle on the platform dependent parameter.
     *
     * @param[in] path of the parameter on which a handler is requested;
     *
     * @return CParameterHandle handle on the parameter, NULL pointer if error.
     */
    CParameterHandle *getPlatformParameterHandle(const std::string &path) const;

    CParameterMgrPlatformConnector *mPfwConnector; /** < PFW Connector */

    std::map<std::string, CParameterHandle *> mParameterHandleMap; /**< Parameter handle Map */
};
