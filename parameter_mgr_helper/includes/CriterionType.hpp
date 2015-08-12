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

#include <AudioCommsAssert.hpp>
#include <NonCopyable.hpp>
#include <stdint.h>
#include <inttypes.h>
#include <string>

class CParameterMgrPlatformConnector;
class ISelectionCriterionTypeInterface;

class CriterionType : public audio_comms::utilities::NonCopyable
{
public:
    typedef std::pair<int, const char *> ValuePair;

    CriterionType(const std::string &name,
                  bool isInclusive,
                  CParameterMgrPlatformConnector *parameterMgrConnector);
    virtual ~CriterionType();

    /**
     * Get the name of the criterion type.
     *
     * @return name of the criterion type.
     */
    const std::string &getName() const { return mName; }

    /**
     * Adds a value pair for this criterion type to the parameter manager.
     *
     * @param[in] numerical part of the value pair.
     * @param[in] literal part of the value pair.
     */
    void addValuePair(int numerical, const std::string &literal);

    /**
     * Adds value pairs for this criterion type to the parameter manager.
     *
     * @param[in] pairs array of value pairs.
     * @param[in] nbPairs number of value pairs to add.
     */
    void addValuePairs(const ValuePair *pairs, uint32_t nbPairs);

    /**
     * Checks if a literal value belongs to the criterion type value pairs.
     *
     * @param[in] name to check if belongs to value pairs.
     *
     * @return true if literal is belonging to the value pairs of the criterion type,
     *              false otherwise.
     */
    bool hasValuePairByName(const std::string &name);

    /**
     * Get the criterion type interface.
     *
     * @return criterion type interface.
     */
    ISelectionCriterionTypeInterface *getTypeInterface()
    {
        AUDIOCOMMS_ASSERT(mCriterionTypeInterface != NULL, "Invalid Interface");
        return mCriterionTypeInterface;
    }

    /**
     * Get the literal value of the criterion.
     *
     * @return the literal value associated to the numerical local value.
     */
    std::string getFormattedState(uint32_t value);

    /**
     * Get the numerical value of the criterion.
     *
     * @param[in] literalValue to convert
     * @param[out] numerical representation of the criterion.
     *
     * @return true if the literal value was translated to a valid numerical, false otherwise.
     */
    bool getNumericalFromLiteral(const std::string &literalValue, int &numerical) const;

    /**
     * Check the validity of a numerical value, i.e. this value belongs to the possible values
     * declared for this criterion type.
     *
     * @param[out] numerical value to check for this criterion type.
     *
     * @return true if the numerical value is valid, false otherwise.
     */
    bool isNumericValueValid(int valueToCheck) const;

private:
    /**
     * criterion type interface for parameter manager operations.
     */
    ISelectionCriterionTypeInterface *mCriterionTypeInterface;

    std::string mName; /**< criterion type name. */
    bool mIsInclusive; /**< inclusive attribute. */
    CParameterMgrPlatformConnector *mParameterMgrConnector; /**< parameter manager connector. */
};
