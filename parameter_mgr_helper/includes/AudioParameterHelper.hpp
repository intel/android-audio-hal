/*
 * INTEL CONFIDENTIAL
 * Copyright © 2013 Intel
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
 * disclosed in any way without Intel’s prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#pragma once

#include <map>
#include <string>
#include <utils/Errors.h>
#include <vector>

class CParameterMgrPlatformConnector;
class CParameterHandle;


class AudioParameterHelper
{
private:
    typedef std::map<std::string, CParameterHandle *>::iterator ParameterHandleMapIterator;

public:
    AudioParameterHelper(CParameterMgrPlatformConnector *audioPFWConnector);

    virtual ~AudioParameterHelper();

    /**
     * unsigned integer parameter value retrieval.
     *
     * @param[in] parameterPath path of the parameter.
     * @param[in] defaultValue default value to use if any error.
     *
     * @return numerical value of the parameter as an unsigned integer.
     */
    uint32_t getIntegerParameterValue(const std::string &parameterPath,
                                      uint32_t defaultValue) const;

    /**
     * unsigned integer parameter value set.
     *
     * @param[in] paramPath path of the parameter.
     * @param[in] value default to set.
     *
     * @return OK if success, error code otherwise.
     */
    android::status_t setIntegerParameterValue(const std::string &paramPath, uint32_t value);

    /**
     * unsigned integer parameter value setter.
     *
     * @param[in] paramPath path of the parameter.
     * @param[in] array value array to write.
     *
     * @return OK if success, error code otherwise.
     */
    android::status_t setIntegerArrayParameterValue(const std::string &paramPath,
                                                    std::vector<uint32_t> &array) const;

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

    /**
     * Get a string value from a parameter path.
     * It returns the value stored in the parameter framework under the given path.
     *
     * @param[in] paramPath path of the parameter.
     * @param[out] value string value stored under this path.
     *
     * @return status_t OK if success, error code otherwise and strValue is kept unchanged.
     */
    android::status_t getStringParameterValue(const std::string &paramPath,
                                              std::string &value) const;

private:
    /**
     * Get a handle on the platform dependent parameter.
     *
     * @param[in] paramPath path of the parameter on which a handler is requested;
     *
     * @return CParameterHandle handle on the parameter, NULL pointer if error.
     */
    CParameterHandle *getParameterHandle(const std::string &paramPath);

    CParameterMgrPlatformConnector *_audioPfwConnector; /** < PFW Connector */

    std::map<std::string, CParameterHandle *> _parameterHandleMap; /**< Parameter handle Map */
};
