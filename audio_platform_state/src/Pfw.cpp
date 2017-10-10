/*
 * Copyright (C) 2013-2017 Intel Corporation
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

#define LOG_TAG "AudioIntelHal/AudioPlatformState/Pfw"

#include "Pfw.hpp"
#include "AudioPlatformState.hpp"
#include "ParameterMgrPlatformConnector.h"
#include <ParameterMgrHelper.hpp>
#include <property/Property.hpp>
#include <algorithm>
#include <convert.hpp>
#include <cutils/bitops.h>
#include <cutils/config_utils.h>
#include <cutils/misc.h>
#include <utilities/Log.hpp>

#ifndef PFW_CONF_FILE_PATH
#define PFW_CONF_FILE_PATH  "/etc/parameter-framework/"
#endif

using namespace std;
using audio_comms::utilities::Log;
using audio_comms::utilities::Property;
using audio_comms::utilities::convertTo;

namespace intel_audio
{

template <>
class PfwTrait<Audio>
{
public:
    static const char *const mConfFileNamePropName;
    static const char *const mDefaultConfFileName;
    static const char *const mTag;
};

const char *const PfwTrait<Audio>::mConfFileNamePropName = "persist.audio.audioConf";
const char *const PfwTrait<Audio>::mDefaultConfFileName =  "AudioParameterFramework.xml";
const char *const PfwTrait<Audio>::mTag = "Audio";

/// PFW related definitions
// Logger
class ParameterMgrPlatformConnectorLogger : public CParameterMgrPlatformConnector::ILogger
{
private:
    string mVerbose;
    const string mTag;

public:
    ParameterMgrPlatformConnectorLogger(const string &tagName)
        : mVerbose(Property<string>("persist.media.pfw.verbose", "false").getValue()),
          mTag(tagName)
    {}

    virtual void info(const string &log)
    {
        const static string format = "-Instance: ";

        if (mVerbose == "true") {
            Log::Debug() << mTag << format << log;
        }
    }

    virtual void warning(const string &log)
    {
        const static string format = "-Instance: ";

        Log::Warning() << mTag << format << log;
    }
};

template <class Trait>
Pfw<Trait>::Pfw()
    : mConnectorLogger(new ParameterMgrPlatformConnectorLogger(Trait::mTag)),
      mTag(Trait::mTag)
{
    // Fetch the name of the PFW configuration file: this name is stored in an Android property
    // and can be different for each hardware
    string pfwConfigurationFilePath = PFW_CONF_FILE_PATH;
    pfwConfigurationFilePath +=
        Property<string>(Trait::mConfFileNamePropName, Trait::mDefaultConfFileName).getValue();

    Log::Info() << __FUNCTION__ << ": " << mTag << " conf file: " << pfwConfigurationFilePath;

    mConnector = new CParameterMgrPlatformConnector(pfwConfigurationFilePath);
    mConnector->setLogger(mConnectorLogger);
    string error;
    // PFW fail safe mode: a missing subsystem will fallback on virtual subsystem
    bool isFailSafeActive(Property<bool>("persist.media.pfw.failsafe", true).getValue());
    if (!mConnector->setFailureOnMissingSubsystem(!isFailSafeActive, error)) {
        Log::Error() << __FUNCTION__ << ": Failure " <<
            (isFailSafeActive ? "activated" : "deactivated")
                     << " on missing subsystem, (error = " << error << ")";
    } else {
        Log::Warning() << __FUNCTION__ << ": Fail safe on missing subsystem is "
                       << (isFailSafeActive ? "active" : "inactive");
    }
    mParameterHelper = new ParameterMgrHelper(mConnector);
}

template <class Trait>
Pfw<Trait>::~Pfw()
{
    for (auto criterionType : mCriterionTypes) {
        delete criterionType;
    }
    for (auto criterion : mCriteria) {
        delete criterion;
    }
    delete mParameterHelper;
    mConnector->setLogger(NULL);
    delete mConnectorLogger;
    delete mConnector;
}

template <class Trait>
void Pfw<Trait>::commitCriteriaAndApplyConfiguration()
{
    for (auto criterion : mCriteria) {
        criterion->setCriterionState();
    }
    applyConfiguration();
}

template <class Trait>
void Pfw<Trait>::applyConfiguration()
{
    mConnector->applyConfigurations();
}

template <class Trait>
void Pfw<Trait>::addCriterionTypeValuePair(const string &typeName, uint32_t numericValue,
                                           const string &literalValue)
{
    CriterionType *criterionType = mCriterionTypes.getByName(typeName);
    AUDIOCOMMS_ASSERT(criterionType != nullptr,
                      "CriterionType " << typeName.c_str() << " not found");
    if (criterionType->hasValuePairByName(literalValue)) {
        Log::Verbose() << __FUNCTION__ << ": value pair already added";
        return;
    }
    Log::Verbose() << __FUNCTION__ << ": Adding new value pair (" << numericValue
                   << "," << literalValue << ") for criterionType "
                   << typeName << " for " << mTag << " PFW";
    criterionType->addValuePair(numericValue, literalValue);
}


template <class Trait>
android::status_t Pfw<Trait>::start()
{
    string strError;
    if (!mConnector->start(strError)) {
        Log::Error() << ": " << mTag << " PFW start error: " << strError;
        return android::NO_INIT;
    }

    Log::Debug() << __FUNCTION__ << ": " << mTag << " PFW successfully started!";
    return android::OK;
}

template <class Trait>
bool Pfw<Trait>::setCriterion(const string &name, uint32_t value)
{
    Criterion *criterion = mCriteria.getByName(name);
    if (criterion == nullptr) {
        Log::Error() << __FUNCTION__ << ": " << name << " is not a member of " << mTag << " PFW";
        return false;
    }
    return criterion->setCriterionState<int32_t>(value);
}

template <class Trait>
bool Pfw<Trait>::stageCriterion(const string &name, uint32_t value)
{
    Criterion *criterion = mCriteria.getByName(name);
    if (criterion == nullptr) {
        Log::Error() << __FUNCTION__ << ": " << name << " is not a member of " << mTag << " PFW";
        return false;
    }
    return criterion->setValue<uint32_t>(value);
}


template <class Trait>
bool Pfw<Trait>::commitCriterion(const string &name)
{
    Criterion *criterion = mCriteria.getByName(name);
    if (criterion == nullptr) {
        Log::Error() << __FUNCTION__ << ": " << name << " is not a member of " << mTag << " PFW";
        return false;
    }
    criterion->setCriterionState();
    return true;
}


template <class Trait>
uint32_t Pfw<Trait>::getCriterion(const string &name) const
{
    const Criterion *criterion = mCriteria.getByName(name);
    if (criterion == nullptr) {
        Log::Error() << __FUNCTION__ << ": " << name << " is not a member of " << mTag << " PFW";
        return false;
    }
    return criterion->getValue<uint32_t>();
}

template <class Trait>
string Pfw<Trait>::getFormattedState(const string &typeName, uint32_t numeric) const
{
    const CriterionType *criterionType = mCriterionTypes.getByName(typeName);
    if (criterionType == nullptr) {
        Log::Error() << __FUNCTION__ << ": " << typeName << " is not a member of " << mTag <<
            " PFW";
        return "";
    }
    return criterionType->getFormattedState(numeric);
}

template <class Trait>
bool Pfw<Trait>::getNumericalValue(const string &criterionName, const string &literal,
                                   int &numeric) const
{
    const Criterion *criterion = mCriteria.getByName(criterionName);
    if (criterion == nullptr) {
        Log::Error() << __FUNCTION__ << ": " << criterionName << " is not a member of " << mTag <<
            " PFW";
        return false;
    }
    return criterion->getNumericalFromLiteral(literal, numeric);
}

template <class Trait>
CParameterHandle *Pfw<Trait>::getDynamicParameterHandle(const string &dynamicParamPath)
{
    return mParameterHelper->getDynamicParameterHandle(dynamicParamPath);
}


template class Pfw<PfwTrait<Audio> >;

} // namespace intel_audio
