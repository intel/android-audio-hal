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

#include <stdint.h>
#include <inttypes.h>
#include <string>

class CParameterMgrPlatformConnector;
class ISelectionCriterionInterface;
class CriterionType;

class Criterion
{
public:
    Criterion(const std::string &name,
              CriterionType *criterionType,
              CParameterMgrPlatformConnector *parameterMgrConnector,
              int32_t defaultValue = 0);

    Criterion(const std::string &name,
              CriterionType *criterionType,
              CParameterMgrPlatformConnector *parameterMgrConnector,
              const std::string &defaultLiteralValue);

    /**
     * Get the name of the criterion.
     *
     * @return name of the criterion.
     */
    const std::string &getName() const
    {
        return mName;
    }

    /**
     * Set the local value of the criterion.
     * Upon this call, the value is not written in the parameter manager.
     *
     * @tparam T type of the value to set. uint32_t and std::string specialization provided.
     * @param[in] value to set.
     *
     * @return true if the value was changed, false if same value requested to be set.
     */
    template <typename T>
    bool setValue(const T &value);

    /**
     * Get the local value of the criterion.
     * No call is make on the parameter manager.
     *
     * @return value of the criterion.
     */
    template <typename T>
    T getValue() const;

    /**
     * Set the local value to the parameter manager.
     */
    void setCriterionState();

    /**
     * Set the local value to the parameter manager.
     *
     * @tparam T type of the value to set.
     * @param[in] value value to set to the criterion of the parameter manager.
     *
     * @return true if the value was changed, false if same value requested to be set.
     */
    template <typename T>
    bool setCriterionState(const T &value);

    /**
     * Get the criterion type handler.
     *
     * @return criterion type handle.
     */
    CriterionType *getCriterionType()
    {
        return mCriterionType;
    }

    /**
     * Helper function to retrieve the numerical value from the literal representation of the
     * criterion.
     * Note that the literal value may either be the literal value associated to the criterion
     * or the numerical value converted to string. It may be the case when receiving parameters
     * from the policy like devices.
     *
     * @param[in] literalValue: literal representation of the criterion.
     * @param[out] numerical representation of the criterion.
     *
     * @return true if the literal value was translated to a valid numerical, false otherwise.
     */
    bool getNumericalFromLiteral(const std::string &literalValue, int &numerical) const;

private:
    /**
     * Initialize the criterion, i.e. get the criterion interface and set the criterion
     * init value.
     *
     * @param[in] defaultValue Default numerical value of the criterion.
     */
    void init(int32_t defaultValue);

    /**
     * criterion interface for parameter manager operations.
     */
    ISelectionCriterionInterface *mSelectionCriterionInterface;

    CriterionType *mCriterionType; /**< Criterion type to which this criterion is associated. */

    CParameterMgrPlatformConnector *mParameterMgrConnector; /**< parameter manager connector. */

    std::string mName; /**< name of the criterion. */

    uint32_t mValue; /**< value of the criterion. */
};
