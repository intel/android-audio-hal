/*
 * Copyright (C) 2013-2016 Intel Corporation
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
#define LOG_TAG "ParameterHelper/Criterion"

#include "Criterion.hpp"
#include "CriterionType.hpp"
#include "ParameterMgrPlatformConnector.h"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using std::string;
using audio_comms::utilities::Log;

Criterion::Criterion(const string &name,
                     CriterionType *criterionType,
                     CParameterMgrPlatformConnector *parameterMgrConnector,
                     int32_t defaultValue)
    : mCriterionType(criterionType),
      mParameterMgrConnector(parameterMgrConnector),
      mName(name)
{
    init(defaultValue);
}

Criterion::Criterion(const string &name,
                     CriterionType *criterionType,
                     CParameterMgrPlatformConnector *parameterMgrConnector,
                     const std::string &defaultLiteralValue)
    : mCriterionType(criterionType),
      mParameterMgrConnector(parameterMgrConnector),
      mName(name)
{
    int defaultNumeric;
    if (!getNumericalFromLiteral(defaultLiteralValue, defaultNumeric)) {
        defaultNumeric = 0;
    }
    init(defaultNumeric);
}

void Criterion::init(int32_t defaultValue)
{
    AUDIOCOMMS_ASSERT(mCriterionType != NULL, "NULL criterion Type");
    mSelectionCriterionInterface =
        mParameterMgrConnector->createSelectionCriterion(mName, mCriterionType->getTypeInterface());
    AUDIOCOMMS_ASSERT(mSelectionCriterionInterface != NULL, "NULL criterion interface");
    mValue = defaultValue;
    setCriterionState();
}

template <>
bool Criterion::setValue<uint32_t>(const uint32_t &value)
{
    if (mValue != value) {

        mValue = value;
        return true;
    }
    return false;
}

template <>
uint32_t Criterion::getValue() const
{
    return mValue;
}

template <>
string Criterion::getValue() const
{
    return mCriterionType->getFormattedState(mValue);
}

template <>
bool Criterion::setValue<std::string>(const std::string &literalValue)
{
    int32_t numericValue = 0;
    if (!getNumericalFromLiteral(literalValue, numericValue)) {
        Log::Error() << __FUNCTION__ << ": Invalid value:" << literalValue;
        return false;
    }
    return setValue<uint32_t>(numericValue);
}

void Criterion::setCriterionState()
{
    mSelectionCriterionInterface->setCriterionState(mValue);
}

template <>
bool Criterion::setCriterionState<int32_t>(const int32_t &value)
{
    if (setValue<uint32_t>(value)) {
        mSelectionCriterionInterface->setCriterionState(mValue);
        return true;
    }
    return false;
}

template <>
bool Criterion::setCriterionState<string>(const string &value)
{
    int32_t numericValue = 0;
    if (!getNumericalFromLiteral(value, numericValue)) {
        Log::Error() << __FUNCTION__ << ": Invalid value: " << value;
        return false;
    }
    return setCriterionState<int32_t>(numericValue);
}

bool Criterion::getNumericalFromLiteral(const std::string &literalValue, int &numerical) const
{
    return mCriterionType->getNumericalFromLiteral(literalValue, numerical);
}
