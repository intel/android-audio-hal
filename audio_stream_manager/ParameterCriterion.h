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

#include <Criterion.h>
#include <CriterionType.h>
#include <map>
#include <stdint.h>
#include <string>
#include <AudioCommsAssert.hpp>
#include <utils/Errors.h>
#include <utils/Log.h>

class ParameterCriterion : public Criterion
{
public:
    ParameterCriterion(const std::string &key,
                       const std::string &name,
                       CriterionType *criterionType,
                       CParameterMgrPlatformConnector *parameterMgrConnector,
                       int32_t defaultValue = 0)
        : Criterion(name, criterionType, parameterMgrConnector, defaultValue),
          _defaultValue(defaultValue),
          _key(key)
    {}

    const char *getKey() const
    {
        return _key.c_str();
    }

    void setParamToCriterionPair(const std::string &name, uint32_t value)
    {
        AUDIOCOMMS_ASSERT(_paramToCriterionMap.find(name) == _paramToCriterionMap.end(),
                          "parameter value " << name.c_str() << " already appended");
        _paramToCriterionMap[name] = value;
    }

    bool setParameter(const std::string &value)
    {
        if (!isParameterValueValid(value)) {

            ALOGD("%s: unknown parameter value(%s) for %s, setting default value (%d)",
                  __FUNCTION__, value.c_str(), _key.c_str(), _defaultValue);
            return setCriterionState(_defaultValue);
        }
        ALOGD("%s: %s (%s, %d)", __FUNCTION__, getName().c_str(), value.c_str(),
              _paramToCriterionMap[value]);
        return setCriterionState(_paramToCriterionMap[value]);
    }
private:

    /**
     * Checks the validity of a parameter value.
     *
     * @param[in] value parameter value to check upon validity.
     *
     * @return true if value found in the mapping table, false otherwise.
     */
    bool isParameterValueValid(const std::string value) const
    {
        return _paramToCriterionMap.find(value) != _paramToCriterionMap.end();
    }

    /**
     * associate parameter value to criteria value.
     */
    std::map<std::string, int> _paramToCriterionMap;

    int32_t _defaultValue; /**< default numerical value. */

    std::string _key; /**< key name of the parameter. */
};
