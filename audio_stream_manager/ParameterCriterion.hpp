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
#pragma once

#include <Criterion.hpp>
#include <CriterionType.hpp>
#include <AudioCommsAssert.hpp>
#include <map>
#include <stdint.h>
#include <string>
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
          mDefaultValue(defaultValue),
          mKey(key)
    {}

    const char *getKey() const
    {
        return mKey.c_str();
    }

    void setParamToCriterionPair(const std::string &name, uint32_t value)
    {
        AUDIOCOMMS_ASSERT(mParamToCriterionMap.find(name) == mParamToCriterionMap.end(),
                          "parameter value " << name.c_str() << " already appended");
        mParamToCriterionMap[name] = value;
    }

    bool setParameter(const std::string &value)
    {
        if (!isParameterValueValid(value)) {

            ALOGD("%s: unknown parameter value(%s) for %s, setting default value (%d)",
                  __FUNCTION__, value.c_str(), mKey.c_str(), mDefaultValue);
            return setCriterionState(mDefaultValue);
        }
        ALOGD("%s: %s (%s, %d)", __FUNCTION__, getName().c_str(), value.c_str(),
              mParamToCriterionMap[value]);
        return setCriterionState(mParamToCriterionMap[value]);
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
        return mParamToCriterionMap.find(value) != mParamToCriterionMap.end();
    }

    /**
     * associate parameter value to criteria value.
     */
    std::map<std::string, int> mParamToCriterionMap;

    int32_t mDefaultValue; /**< default numerical value. */

    std::string mKey; /**< key name of the parameter. */
};
