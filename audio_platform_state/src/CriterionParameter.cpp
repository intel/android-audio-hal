/*
 * Copyright (C) 2014-2015 Intel Corporation
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

#include "CriterionParameter.hpp"
#include <CriterionType.hpp>
#include <Criterion.hpp>
#include <ParameterMgrPlatformConnector.h>
#include <IStreamInterface.hpp>
#include <convert.hpp>
#include <utilities/Log.hpp>

using audio_comms::utilities::Log;

namespace intel_audio
{

CriterionParameter::CriterionParameter(const std::string &key,
                                       const std::string &name,
                                       CriterionType *criterionType,
                                       CParameterMgrPlatformConnector *connector,
                                       const std::string &defaultValue /* = "" */)
    : Parameter(key, name, defaultValue),
      mCriterion(new Criterion(name, criterionType, connector, defaultValue))
{
    mCriterion->setCriterionState<std::string>(getDefaultLiteralValue());
}

CriterionParameter::~CriterionParameter()
{
    delete mCriterion;
}

bool CriterionParameter::setValue(const std::string &value)
{
    std::string literalValue;
    if (!getLiteralValueFromParam(value, literalValue)) {
        Log::Warning() << __FUNCTION__
                       << ": unknown parameter value(" << value << ") for " << getKey();
        return false;
    }
    Log::Verbose() << __FUNCTION__ << ": " << getName() << " " << value << "=" << literalValue;
    return mCriterion->setValue(literalValue);
}

bool CriterionParameter::getValue(std::string &value) const
{
    std::string criterionLiteralValue = mCriterion->getValue<std::string>();

    return getParamFromLiteralValue(value, criterionLiteralValue);
}

bool CriterionParameter::sync()
{
    return mCriterion->setCriterionState(getDefaultLiteralValue());
}

} // namespace intel_audio
