/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2014 Intel
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

#include "CriterionParameter.hpp"
#include <CriterionType.hpp>
#include <Criterion.hpp>
#include <ParameterMgrPlatformConnector.h>
#include <StreamInterface.hpp>

bool CriterionParameter::set(const std::string &androidParamValue)
{
    mObserver->parameterHasChanged(getName());
    return true;
}

RouteCriterionParameter::RouteCriterionParameter(ParameterChangedObserver *observer,
                                                 const std::string &key,
                                                 const std::string &name,
                                                 CriterionType *criterionType,
                                                 CParameterMgrPlatformConnector *connector,
                                                 const std::string &defaultValue /* = "" */)
    : CriterionParameter(observer, key, name, defaultValue),
      mCriterion(new Criterion(name, criterionType, connector, defaultValue))
{
}

bool RouteCriterionParameter::setValue(const std::string &value)
{
    std::string literalValue;
    if (!getLiteralValueFromParam(value, literalValue)) {

        ALOGW("%s: unknown parameter value(%s) for %s",
              __FUNCTION__, value.c_str(), getKey().c_str());
        return false;
    }
    ALOGV("%s: %s (%s, %s)", __FUNCTION__, getName().c_str(), value.c_str(),
          literalValue.c_str());
    if (mCriterion->setCriterionState<std::string>(literalValue)) {
        // by construction, only "failing" case happens when the value of the criterion did not
        // change. It is internal choice to report false in this case, no need to propagate to upper
        // layer.
        CriterionParameter::set(literalValue);
    }
    return true;
}

bool RouteCriterionParameter::getValue(std::string &value) const
{
    std::string criterionLiteralValue = mCriterion->getFormattedValue();

    return getParamFromLiteralValue(value, criterionLiteralValue);
}

bool RouteCriterionParameter::sync()
{
    return mCriterion->setCriterionState<std::string>(getDefaultLiteralValue());
}

AudioCriterionParameter::AudioCriterionParameter(ParameterChangedObserver *observer,
                                                 const std::string &key,
                                                 const std::string &name,
                                                 const std::string &typeName,
                                                 IStreamInterface *streamInterface,
                                                 const std::string &defaultValue /* = "" */)
    : CriterionParameter(observer, key, name, defaultValue),
      mStreamInterface(streamInterface)
{
    mStreamInterface->addCriterion(name, typeName, defaultValue);
}

bool AudioCriterionParameter::setValue(const std::string &value)
{
    std::string literalValue;
    if (!getLiteralValueFromParam(value, literalValue)) {
        ALOGW("%s: unknown parameter value(%s) for %s",
              __FUNCTION__, value.c_str(), getKey().c_str());
        return false;
    }
    ALOGV("%s: %s (%s, %s)", __FUNCTION__, getName().c_str(), value.c_str(),
          literalValue.c_str());
    if (mStreamInterface->setAudioCriterion(getName(), literalValue)) {
        CriterionParameter::set(literalValue);
    }
    return true;
}

bool AudioCriterionParameter::getValue(std::string &value) const
{
    std::string criterionLiteralValue;
    return mStreamInterface->getAudioCriterion(getName(), criterionLiteralValue) &&
           getParamFromLiteralValue(value, criterionLiteralValue);
}

bool AudioCriterionParameter::sync()
{
    return mStreamInterface->setAudioCriterion(getName(), getDefaultLiteralValue());
}
