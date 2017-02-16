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
#define LOG_TAG "ParameterHelper/CriterionType"

#include "CriterionType.hpp"
#include "ParameterMgrPlatformConnector.h"
#include <convert.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <boost/tokenizer.hpp>

using std::string;
using audio_comms::utilities::Log;

CriterionType::CriterionType(const string &name,
                             bool isInclusive,
                             CParameterMgrPlatformConnector *parameterMgrConnector)
    : mName(name),
      mIsInclusive(isInclusive),
      mParameterMgrConnector(parameterMgrConnector)
{
    mCriterionTypeInterface = mParameterMgrConnector->createSelectionCriterionType(mIsInclusive);
}

CriterionType::~CriterionType()
{
}

string CriterionType::getFormattedState(uint32_t value) const
{
    return mCriterionTypeInterface->getFormattedState(value);
}

void CriterionType::addValuePair(int numerical, const string &literal)
{
    string result;
    getTypeInterface()->addValuePair(numerical, literal.c_str(), result);
}

bool CriterionType::hasValuePairByName(const std::string &name)
{
    int value = 0;
    return getTypeInterface()->getNumericalValue(name, value);
}

bool CriterionType::getNumericalFromLiteral(const std::string &literalValue, int &numerical) const
{
    boost::char_separator<char> sep("|");
    boost::tokenizer<boost::char_separator<char>> tokenizeLiteral(literalValue, sep);

    for (const auto &value : tokenizeLiteral) {
        int numericalValue = 0;

        // First check if the literal value is a known value from the criterion type
        if (mCriterionTypeInterface->getNumericalValue(value, numericalValue)) {
            numerical += numericalValue;
            continue;
        }
        // The literal value does not belong to the value declared for this criterion type.
        // The literal may represent the numerical converted as a string
        bool isValueProvidedAsHexa = !literalValue.compare(0, 2, "0x");
        if (isValueProvidedAsHexa) {
            uint32_t numericalUValue = 0;
            if (!audio_comms::utilities::convertTo(value, numericalUValue)) {
                return false;
            }
            numerical += numericalUValue;
        } else {
            if (!audio_comms::utilities::convertTo(value, numericalValue)) {
                return false;
            }
            numerical += numericalValue;
        }
    }

    // We succeeded to convert the literal value as an numerical, checking now that this numerical
    // value is a valid value for this criterion type (test limited to exclusive criterion)
    return mCriterionTypeInterface->isTypeInclusive() ? true : isNumericValueValid(numerical);
}

bool CriterionType::isNumericValueValid(int valueToCheck) const
{
    string literalValue;
    return mCriterionTypeInterface->getLiteralValue(valueToCheck, literalValue);
}
