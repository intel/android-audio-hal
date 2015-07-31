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

#define LOG_TAG "AudioIntelHal/AudioPlatformState/Pfw"

#include "Pfw.hpp"
#include "AudioPlatformState.hpp"
#include "AudioHalConf.hpp"
#include "CriterionParameter.hpp"
#include "RogueParameter.hpp"
#include "ParameterMgrPlatformConnector.h"
#include <Criterion.hpp>
#include <CriterionType.hpp>
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

template <>
class PfwTrait<Route>
{
public:
    static const char *const mConfFileNamePropName;
    static const char *const mDefaultConfFileName;
    static const char *const mTag;
};

const char *const PfwTrait<Audio>::mConfFileNamePropName = "persist.audio.audioConf";
const char *const PfwTrait<Audio>::mDefaultConfFileName =  "AudioParameterFramework.xml";
const char *const PfwTrait<Audio>::mTag = "Audio";

const char *const PfwTrait<Route>::mConfFileNamePropName = "persist.audio.routeConf";
const char *const PfwTrait<Route>::mDefaultConfFileName = "RouteParameterFramework.xml";
const char *const PfwTrait<Route>::mTag = "Route";

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
    for (CriterionTypeMapIterator it = mCriterionTypes.begin(); it != mCriterionTypes.end(); ++it) {
        delete it->second;
    }

    for (CriterionMapIterator it = mCriteria.begin(); it != mCriteria.end(); ++it) {
        delete it->second;
    }
    delete mParameterHelper;
    mConnector->setLogger(NULL);
    delete mConnectorLogger;
    delete mConnector;
}

template <class Trait>
void Pfw<Trait>::commitCriteriaAndApplyConfiguration()
{
    CriterionMapIterator it;
    for (it = mCriteria.begin(); it != mCriteria.end(); ++it) {
        it->second->setCriterionState();
    }
    applyConfiguration();
}

template <class Trait>
void Pfw<Trait>::applyConfiguration()
{
    mConnector->applyConfigurations();
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
bool Pfw<Trait>::addCriterionType(const string &typeName, bool isInclusive)
{
    if (collectionHasElement<CriterionType *>(typeName, mCriterionTypes)) {
        Log::Verbose() << __FUNCTION__ << ": already added " << typeName << " criterion ["
                       << (isInclusive ? "inclusive" : "exclusive") << "]";
        return true;
    }
    Log::Verbose() << __FUNCTION__ << ": Adding " << typeName << " for " << mTag << " PFW";
    mCriterionTypes[typeName] = new CriterionType(typeName, isInclusive, mConnector);
    return false;
}

template <class Trait>
void Pfw<Trait>::addCriterionTypeValuePair(const string &typeName, uint32_t numericValue,
                                           const string &literalValue)
{
    AUDIOCOMMS_ASSERT(collectionHasElement<CriterionType *>(typeName, mCriterionTypes),
                      "CriterionType " << typeName.c_str() << " not found");
    CriterionType *criterionType = mCriterionTypes[typeName];
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
void Pfw<Trait>::loadCriterionType(cnode &root, bool isInclusive)
{
    cnode *node;
    for (node = root.first_child; node != NULL; node = node->next) {
        const char *typeName = node->name;
        char *valueNames = strndup(node->value, strlen(node->value));

        addCriterionType(typeName, isInclusive);

        uint32_t index = 0;
        char *ctx;
        char *valueName = strtok_r(valueNames, ",", &ctx);
        while (valueName != NULL) {
            if (strlen(valueName) != 0) {

                // Conf file may use or not pair, if no pair, use incremental index, else
                // use provided index.
                if (strchr(valueName, ':') != NULL) {

                    char *first = strtok(valueName, ":");
                    char *second = strtok(NULL, ":");
                    AUDIOCOMMS_ASSERT((first != NULL) && (strlen(first) != 0) &&
                                      (second != NULL) && (strlen(second) != 0),
                                      "invalid value pair");

                    bool isValueProvidedAsHexa = !string(first).compare(0, 2, "0x");
                    if (isValueProvidedAsHexa) {
                        if (!convertTo<string, uint32_t>(first, index)) {
                            Log::Error() << __FUNCTION__ << ": Invalid value(" << first << ")";
                        }
                    } else {
                        int32_t signedIndex = 0;
                        if (!convertTo<string, int32_t>(first, signedIndex)) {
                            Log::Error() << __FUNCTION__ << ": Invalid value(" << first << ")";
                        }
                        index = signedIndex;
                    }
                    Log::Verbose() << __FUNCTION__ << ": name=" << typeName << ", index=" << index
                                   << ", value=" << second << " for " << mTag << " PFW";
                    addCriterionTypeValuePair(typeName, index, second);
                } else {

                    uint32_t pfwIndex = isInclusive ? 1 << index : index;
                    Log::Verbose() << __FUNCTION__ << ": name=" << typeName << ", index="
                                   << pfwIndex << ", value=" << valueName;
                    addCriterionTypeValuePair(typeName, pfwIndex, valueName);
                    index += 1;
                }
            }
            valueName = strtok_r(NULL, ",", &ctx);
        }
        free(valueNames);
    }
}

template <class Trait>
void Pfw<Trait>::loadInclusiveCriterionType(cnode &root)
{
    cnode *node = config_find(&root, gInclusiveCriterionTypeTag.c_str());
    if (node == NULL) {
        return;
    }
    loadCriterionType(*node, true);
}

template <class Trait>
void Pfw<Trait>::loadExclusiveCriterionType(cnode &root)
{
    cnode *node = config_find(&root, gExclusiveCriterionTypeTag.c_str());
    if (node == NULL) {
        return;
    }
    loadCriterionType(*node, false);
}

/**
 * This class defines a unary function to be used when looping on the vector of value pairs
 * of a parameter.
 * It will help applying all the pairs (Android-Parameter value - PFW-Parameter value)
 * for the mapping table.
 */
class SetAndroidParamMappingPairHelper
{
public:
    SetAndroidParamMappingPairHelper(Parameter *param)
        : mParamCriterion(param)
    {}

    void operator()(AndroidParamMappingValuePair pair)
    {
        mParamCriterion->setMappingValuePair(pair.first, pair.second);
    }

    Parameter *mParamCriterion;
};

template <class Trait>
void Pfw<Trait>::addParameter(Parameter *param,
                              const std::vector<AndroidParamMappingValuePair> &valuePairs,
                              std::vector<Parameter *> &parameterVector)
{
    for_each(valuePairs.begin(), valuePairs.end(), SetAndroidParamMappingPairHelper(param));
    parameterVector.push_back(param);
}

template <class Trait>
void Pfw<Trait>::addCriterionParameter(const string &typeName,
                                       const string &paramKey,
                                       const string &name,
                                       const string &defaultValue,
                                       const std::vector<AndroidParamMappingValuePair> &valuePairs,
                                       std::vector<Parameter *> &parameterVector)
{
    addCriterion(name, typeName, defaultValue);
    CriterionParameter *paramCriterion = new CriterionParameter(paramKey, name, *mCriteria[name],
                                                                defaultValue);
    addParameter(paramCriterion, valuePairs, parameterVector);
}

template <class Trait>
void Pfw<Trait>::addRogueParameter(const string &typeName, const string &paramKey,
                                   const string &name,
                                   const string &defaultValue,
                                   const std::vector<AndroidParamMappingValuePair> &valuePairs,
                                   std::vector<Parameter *> &parameterVector)
{
    if (typeName == gUnsignedIntegerTypeTag) {
        RogueParameter<uint32_t> *paramRogue =
            new RogueParameter<uint32_t>(paramKey, name, mConnector, defaultValue);
        addParameter(paramRogue, valuePairs, parameterVector);
    } else if (typeName == gStringTypeTag) {
        RogueParameter<string> *paramRogue =
            new RogueParameter<string>(paramKey, name, mConnector, defaultValue);
        addParameter(paramRogue, valuePairs, parameterVector);
    } else if (typeName == gDoubleTypeTag) {
        RogueParameter<double> *paramRogue =
            new RogueParameter<double>(paramKey, name, mConnector, defaultValue);
        addParameter(paramRogue, valuePairs, parameterVector);
    } else {
        Log::Error() << __FUNCTION__ << ": type " << typeName << " not supported ";
        return;
    }
}

template <class Trait>
void Pfw<Trait>::parseChildren(cnode &root, string &path, string &defaultValue, string &key,
                               string &type, std::vector<AndroidParamMappingValuePair> &valuePairs)
{
    cnode *node;
    for (node = root.first_child; node != NULL; node = node->next) {
        if (string(node->name) == gPathTag) {
            path = node->value;
        } else if (string(node->name) == gParameterDefaultTag) {
            defaultValue = node->value;
        } else if (string(node->name) == gAndroidParameterTag) {
            key = node->value;
        } else if (string(node->name) == gMappingTableTag) {
            valuePairs = parseMappingTable(node->value);
        } else if (string(node->name) == gTypeTag) {
            type = node->value;
        } else {
            Log::Error() << __FUNCTION__ << ": Unrecognized " << node->name << " "
                         << node->value << " node ";
        }
    }
    Log::Verbose() << __FUNCTION__ << ": path=" << path << ",  key=" << key << " default="
                   << defaultValue << ", type=" << type;
}

template <class Trait>
void Pfw<Trait>::loadRogueParameterType(cnode &root, std::vector<Parameter *> &parameterVector)
{
    const char *rogueParameterName = root.name;

    std::vector<AndroidParamMappingValuePair> valuePairs;
    string paramKeyName = "";
    string rogueParameterPath = "";
    string typeName = "";
    string defaultValue = "";

    parseChildren(root, rogueParameterPath, defaultValue, paramKeyName, typeName, valuePairs);

    AUDIOCOMMS_ASSERT(!paramKeyName.empty(), "Rogue Parameter " << rogueParameterName <<
                      " not associated to any Android parameter");

    addRogueParameter(typeName, paramKeyName, rogueParameterPath, defaultValue, valuePairs,
                      parameterVector);
}

template <class Trait>
void Pfw<Trait>::loadRogueParameterTypeList(cnode &root, std::vector<Parameter *> &parameterVector)
{
    cnode *node = config_find(&root, gRogueParameterTag.c_str());
    if (node == NULL) {
        Log::Warning() << __FUNCTION__ << ": no rogue parameter type found";
        return;
    }
    for (node = node->first_child; node != NULL; node = node->next) {
        loadRogueParameterType(*node, parameterVector);
    }
}

template <class Trait>
template <typename T>
bool Pfw<Trait>::collectionHasElement(const string &name,
                                      const std::map<string, T> &collection) const
{
    typename std::map<string, T>::const_iterator it = collection.find(name);
    return it != collection.end();
}

template <class Trait>
template <typename T>
T *Pfw<Trait>::getElement(const string &name, std::map<string, T *> &elementsMap)
{
    typename std::map<string, T *>::iterator it = elementsMap.find(name);
    AUDIOCOMMS_ASSERT(it != elementsMap.end(), "Element " << name << " not found");
    return it->second;
}

template <class Trait>
template <typename T>
const T *Pfw<Trait>::getElement(const string &name, const std::map<string, T *> &elementsMap) const
{
    typename std::map<string, T *>::const_iterator it = elementsMap.find(name);
    AUDIOCOMMS_ASSERT(it != elementsMap.end(), "Element " << name << " not found");
    return it->second;
}

template <class Trait>
void Pfw<Trait>::loadCriteria(cnode &root, std::vector<Parameter *> &parameterVector)
{
    cnode *node = config_find(&root, gCriterionTag.c_str());

    if (node == NULL) {
        Log::Warning() << __FUNCTION__ << ": no inclusive criteria found";
        return;
    }
    for (node = node->first_child; node != NULL; node = node->next) {
        loadCriterion(*node, parameterVector);
    }
}

template <class Trait>
std::vector<AndroidParamMappingValuePair> Pfw<Trait>::parseMappingTable(const char *values)
{
    AUDIOCOMMS_ASSERT(values != NULL, "error in parsing file");
    char *mappingPairs = strndup(values, strlen(values));
    char *ctx;
    std::vector<AndroidParamMappingValuePair> valuePairs;

    char *mappingPair = strtok_r(mappingPairs, ",", &ctx);
    while (mappingPair != NULL) {
        if (strlen(mappingPair) != 0) {

            char *first = strtok(mappingPair, ":");
            char *second = strtok(NULL, ":");
            AUDIOCOMMS_ASSERT((first != NULL) && (strlen(first) != 0) &&
                              (second != NULL) && (strlen(second) != 0),
                              "invalid value pair");
            AndroidParamMappingValuePair pair = std::make_pair(first, second);
            valuePairs.push_back(pair);
        }
        mappingPair = strtok_r(NULL, ",", &ctx);
    }
    free(mappingPairs);
    return valuePairs;
}

template <class Trait>
void Pfw<Trait>::addCriterion(const string &name, const string &typeName,
                              const string &defaultValue)
{
    AUDIOCOMMS_ASSERT(!collectionHasElement<Criterion *>(name, mCriteria),
                      "Criterion " << name << " already added for " << mTag << " PFW");
    if (collectionHasElement<CriterionType *>(gStateChangedCriterionType, mCriterionTypes) &&
        name != gStateChangedCriterion) {
        Log::Verbose() << __FUNCTION__ << ": name=" << gStateChangedCriterionType << ", index="
                       << 1 << mCriteria.size() << ", value=" << name;
        mCriterionTypes[gStateChangedCriterionType]->addValuePair(1 << mCriteria.size(), name);
    }
    Log::Verbose() << __FUNCTION__ << ": name=" << name << " criterionType=" << typeName;
    CriterionType *criterionType = getElement<CriterionType>(typeName, mCriterionTypes);
    mCriteria[name] = new Criterion(name, criterionType, mConnector, defaultValue);
}

template <class Trait>
bool Pfw<Trait>::setCriterion(const string &name, uint32_t value)
{
    if (!collectionHasElement<Criterion *>(name, mCriteria)) {
        Log::Error() << __FUNCTION__ << ": " << name << " is not a member of " << mTag << " PFW";
        return false;
    }
    return mCriteria[name]->template setCriterionState<int32_t>(value);
}

template <class Trait>
bool Pfw<Trait>::stageCriterion(const string &name, uint32_t value)
{
    if (!collectionHasElement<Criterion *>(name, mCriteria)) {
        Log::Error() << __FUNCTION__ << ": " << name << " is not a member of " << mTag << " PFW";
        return false;
    }
    return mCriteria[name]->template setValue<uint32_t>(value);
}


template <class Trait>
uint32_t Pfw<Trait>::getCriterion(const string &name) const
{
    if (!collectionHasElement<Criterion *>(name, mCriteria)) {
        Log::Error() << __FUNCTION__ << ": " << name << " is not a member of " << mTag << " PFW";
        return 0;
    }
    const Criterion *criterion = getElement<Criterion>(name, mCriteria);
    return criterion->getValue<uint32_t>();
}

template <class Trait>
void Pfw<Trait>::loadCriterion(cnode &root, std::vector<Parameter *> &parameterVector)
{
    const char *criterionName = root.name;

    std::vector<AndroidParamMappingValuePair> valuePairs;
    string paramKeyName = "";
    string path = "";
    string typeName = "";
    string defaultValue = "";

    parseChildren(root, path, defaultValue, paramKeyName, typeName, valuePairs);

    if (!paramKeyName.empty()) {
        /**
         * If a parameter key is found, this criterion is linked to a parameter received from
         * AudioSystem::setParameters.
         */
        addCriterionParameter(typeName, paramKeyName, criterionName, defaultValue, valuePairs,
                              parameterVector);
    } else {
        addCriterion(criterionName, typeName, defaultValue);
    }
}

template <class Trait>
void Pfw<Trait>::loadConfig(cnode &root, std::vector<Parameter *> &parameterVector)
{
    // Must load criterion type for both common and specific PFW tag before parsing criteria.
    cnode *node = config_find(&root, gCommonConfTag.c_str());
    if (node != NULL) {
        Log::Verbose() << __FUNCTION__ << " Load common conf types for " << mTag;
        loadInclusiveCriterionType(*node);
        loadExclusiveCriterionType(*node);
    }
    node = config_find(&root, mTag.c_str());
    if (node != NULL) {
        Log::Verbose() << __FUNCTION__ << " Load specific conf types for " << mTag;
        loadInclusiveCriterionType(*node);
        loadExclusiveCriterionType(*node);
    }

    node = config_find(&root, gCommonConfTag.c_str());
    if (node != NULL) {
        Log::Verbose() << __FUNCTION__ << " Load common conf for " << mTag;
        loadCriteria(*node, parameterVector);
        loadRogueParameterTypeList(*node, parameterVector);
    }
    node = config_find(&root, mTag.c_str());
    if (node != NULL) {
        Log::Verbose() << __FUNCTION__ << " Load specific conf for " << mTag;
        loadCriteria(*node, parameterVector);
        loadRogueParameterTypeList(*node, parameterVector);
    }
}

template <class Trait>
string Pfw<Trait>::getFormattedState(const string &typeName, uint32_t numeric) const
{
    if (!collectionHasElement<CriterionType *>(typeName, mCriterionTypes)) {
        return "";
    }
    const CriterionType *criterionType = getElement<CriterionType>(typeName, mCriterionTypes);
    return criterionType->getFormattedState(numeric);
}

template <class Trait>
bool Pfw<Trait>::getNumericalValue(const string &criterionName, const string &literal,
                                   int &numeric) const
{
    if (!collectionHasElement<Criterion *>(criterionName, mCriteria)) {
        return false;
    }
    const Criterion *criterion = getElement<Criterion>(criterionName, mCriteria);
    return criterion->getNumericalFromLiteral(literal, numeric);
}

template <class Trait>
CParameterHandle *Pfw<Trait>::getDynamicParameterHandle(const string &dynamicParamPath)
{
    return mParameterHelper->getDynamicParameterHandle(dynamicParamPath);
}


template class Pfw<PfwTrait<Audio> >;
template class Pfw<PfwTrait<Route> >;

} // namespace intel_audio
