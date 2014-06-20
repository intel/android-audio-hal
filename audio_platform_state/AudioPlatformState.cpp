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
#define LOG_TAG "AudioIntelHal/AudioPlatformState"

#include "AudioPlatformState.hpp"
#include "AudioHalConf.hpp"
#include "CriterionParameter.hpp"
#include "RogueParameter.hpp"
#include "ParameterMgrPlatformConnector.h"
#include <Stream.hpp>
#include <Criterion.hpp>
#include <CriterionType.hpp>
#include <ParameterMgrHelper.hpp>
#include <Property.h>
#include "NaiveTokenizer.h"
#include <algorithm>
#include <convert.hpp>
#include <cutils/bitops.h>
#include <cutils/config_utils.h>
#include <cutils/misc.h>
#include <hardware_legacy/AudioHardwareBase.h>
#include <media/AudioParameter.h>
#include <utils/Log.h>
#include <fstream>

#define DIRECT_STREAM_FLAGS (AUDIO_OUTPUT_FLAG_DIRECT | AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD)

using std::string;
using audio_comms::utilities::convertTo;

namespace android_audio_legacy
{

const char *const AudioPlatformState::mRoutePfwConfFileNamePropName =
    "AudioComms.RoutePFW.ConfPath";

const char *const AudioPlatformState::mRoutePfwDefaultConfFileName =
    "/etc/parameter-framework/ParameterFrameworkConfigurationRoute.xml";

const std::string AudioPlatformState::mHwDebugFilesPathList =
    "/Route/debug_fs/debug_files/path_list/";

// For debug purposes. This size is enough for dumping relevant informations
const uint32_t AudioPlatformState::mMaxDebugStreamSize = 998;

/// PFW related definitions
// Logger
class ParameterMgrPlatformConnectorLogger : public CParameterMgrPlatformConnector::ILogger
{
public:
    ParameterMgrPlatformConnectorLogger() {}

    virtual void log(bool isWarning, const string &log)
    {
        const static char format[] = "route-parameter-manager: %s";

        if (isWarning) {

            ALOGW(format, log.c_str());
        } else {

            ALOGD(format, log.c_str());
        }
    }
};

const char *const AudioPlatformState::mOutputDevice = "OutputDevices";
const char *const AudioPlatformState::mInputDevice = "InputDevices";
const char *const AudioPlatformState::mInputSources = "InputSources";
const char *const AudioPlatformState::mOutputFlags = "OutputFlags";
const char *const AudioPlatformState::mModemAudioStatus = "ModemAudioStatus";
const char *const AudioPlatformState::mAndroidMode = "AndroidMode";
const char *const AudioPlatformState::mHasModem = "HasModem";
const char *const AudioPlatformState::mModemState = "ModemState";
const char *const AudioPlatformState::mStateChanged = "StatesChanged";
const char *const AudioPlatformState::mCsvBand = "CsvBandType";
const char *const AudioPlatformState::mVoipBand = "VoIPBandType";
const char *const AudioPlatformState::mMicMute = "MicMute";
const char *const AudioPlatformState::mPreProcessorRequestedByActiveInput =
    "PreProcessorRequestedByActiveInput";

template <>
struct AudioPlatformState::parameterManagerElementSupported<Criterion> {};
template <>
struct AudioPlatformState::parameterManagerElementSupported<CriterionType> {};

AudioPlatformState::AudioPlatformState(IStreamInterface *streamInterface)
    : mStreamInterface(streamInterface),
      mRoutePfwConnectorLogger(new ParameterMgrPlatformConnectorLogger),
      mAudioPfwHasChanged(false)
{
    /// Connector
    // Fetch the name of the PFW configuration file: this name is stored in an Android property
    // and can be different for each hardware
    string routePfwConfFilePath = TProperty<string>(mRoutePfwConfFileNamePropName,
                                                    mRoutePfwDefaultConfFileName);
    ALOGI("Route-PFW: using configuration file: %s", routePfwConfFilePath.c_str());

    mRoutePfwConnector = new CParameterMgrPlatformConnector(routePfwConfFilePath);

    // Logger
    mRoutePfwConnector->setLogger(mRoutePfwConnectorLogger);
}

AudioPlatformState::~AudioPlatformState()
{
    // Delete All criterion
    CriterionMapIterator it;
    for (it = mCriterionMap.begin(); it != mCriterionMap.end(); ++it) {

        delete it->second;
    }

    // Delete All criterion type
    CriterionTypeMapIterator itType;
    for (itType = mCriterionTypeMap.begin(); itType != mCriterionTypeMap.end(); ++itType) {

        delete itType->second;
    }

    // Unset logger
    mRoutePfwConnector->setLogger(NULL);
    // Remove logger
    delete mRoutePfwConnectorLogger;
    // Remove connector
    delete mRoutePfwConnector;
}

status_t AudioPlatformState::start()
{
    if ((loadAudioHalConfig(mAudioHalVendorConfFilePath) != OK) &&
        (loadAudioHalConfig(mAudioHalConfFilePath) != OK)) {

        ALOGE("Neither vendor conf file (%s) nor system conf file (%s) could be found",
              mAudioHalVendorConfFilePath, mAudioHalConfFilePath);
        return NO_INIT;
    }

    /// Start PFW
    std::string strError;
    if (!mRoutePfwConnector->start(strError)) {

        ALOGE("Route PFW start error: %s", strError.c_str());
        return NO_INIT;
    }
    ALOGD("%s: Route PFW successfully started!", __FUNCTION__);
    return OK;
}

template <>
void AudioPlatformState::addCriterionType<Audio>(const string &typeName,
                                                 bool isInclusive)
{
    if (mStreamInterface->addCriterionType(typeName, isInclusive)) {
        ALOGV("%s:criterionType %s already added in Audio PFW", __FUNCTION__, typeName.c_str());
    }
}

template <>
void AudioPlatformState::addCriterionType<Route>(const string &typeName,
                                                 bool isInclusive)
{
    AUDIOCOMMS_ASSERT(mCriterionTypeMap.find(typeName) == mCriterionTypeMap.end(),
                      "CriterionType " << typeName << " already added");

    ALOGD("%s: Adding new criterionType %s for Route PFW", __FUNCTION__, typeName.c_str());
    mCriterionTypeMap[typeName] = new CriterionType(typeName,
                                                    isInclusive,
                                                    mRoutePfwConnector);
}

template <>
void AudioPlatformState::addCriterionTypeValuePair<Audio>(
    const string &typeName,
    uint32_t numericValue,
    const string &literalValue)
{
    mStreamInterface->addCriterionTypeValuePair(typeName, literalValue, numericValue);
}

template <>
void AudioPlatformState::addCriterionTypeValuePair<Route>(
    const string &typeName,
    uint32_t numericValue,
    const string &literalValue)
{
    AUDIOCOMMS_ASSERT(mCriterionTypeMap.find(typeName) != mCriterionTypeMap.end(),
                      "CriterionType " << typeName.c_str() << "not found");

    ALOGV("%s: Adding new value pair (%d, %s) for criterionType %s for Route PFW", __FUNCTION__,
          numericValue, literalValue.c_str(), typeName.c_str());
    CriterionType *criterionType = mCriterionTypeMap[typeName];
    criterionType->addValuePair(numericValue, literalValue);
}

template <PfwInstance pfw>
void AudioPlatformState::loadCriterionType(cnode *root, bool isInclusive)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node;
    for (node = root->first_child; node != NULL; node = node->next) {

        AUDIOCOMMS_ASSERT(node != NULL, "error in parsing file");
        const char *typeName = node->name;
        char *valueNames = (char *)node->value;

        addCriterionType<pfw>(typeName, isInclusive);

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

                    if (!convertTo<string, uint32_t>(first, index)) {

                        ALOGE("%s: Invalid index(%s) found", __FUNCTION__, first);
                    }
                    ALOGV("%s: name=%s, index=0x%X, value=%s", __FUNCTION__, typeName,
                          index, second);

                    addCriterionTypeValuePair<pfw>(typeName, index, second);
                } else {

                    uint32_t pfwIndex = isInclusive ? 1 << index : index;
                    ALOGV("%s: name=%s, index=0x%X, value=%s", __FUNCTION__, typeName,
                          pfwIndex, valueName);

                    addCriterionTypeValuePair<pfw>(typeName, pfwIndex, valueName);
                    index += 1;
                }
            }
            valueName = strtok_r(NULL, ",", &ctx);
        }
    }
}

template <PfwInstance pfw>
void AudioPlatformState::loadInclusiveCriterionType(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, mInclusiveCriterionTypeTag.c_str());
    if (node == NULL) {
        return;
    }
    loadCriterionType<pfw>(node, true);
}

template <PfwInstance pfw>
void AudioPlatformState::loadExclusiveCriterionType(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, mExclusiveCriterionTypeTag.c_str());
    if (node == NULL) {
        return;
    }
    loadCriterionType<pfw>(node, false);
}


void AudioPlatformState::addParameter(Parameter *param,
                                      const vector<AndroidParamMappingValuePair> &valuePairs)
{
    for_each(valuePairs.begin(), valuePairs.end(), SetAndroidParamMappingPairHelper(param));
    mParameterVector.push_back(param);
}

template <>
void AudioPlatformState::addParameter<Audio, ParamRogue>(
    const std::string &typeName, const std::string &paramKey, const std::string &name,
    const std::string &defaultValue, const std::vector<AndroidParamMappingValuePair> &valuePairs)
{
    Parameter *rogueParam;
    if (typeName == mUnsignedIntegerTypeTag) {
        rogueParam = new AudioRogueParameter<uint32_t>(this, paramKey,
                                                       name,
                                                       mStreamInterface,
                                                       defaultValue);
    } else if (typeName == mStringTypeTag) {
        rogueParam = new AudioRogueParameter<string>(this, paramKey, name,
                                                     mStreamInterface,
                                                     defaultValue);
    } else {
        ALOGE("%s: type %s not supported ", __FUNCTION__, typeName.c_str());
        return;
    }
    addParameter(rogueParam, valuePairs);
}

template <>
void AudioPlatformState::addParameter<Audio, ParamCriterion>(
    const std::string &typeName, const std::string &paramKey, const std::string &name,
    const std::string &defaultValue,
    const std::vector<AndroidParamMappingValuePair> &valuePairs)
{
    Parameter *paramCriterion = new AudioCriterionParameter(this, paramKey, name, typeName,
                                                            mStreamInterface, defaultValue);
    addParameter(paramCriterion, valuePairs);
}

template <>
void AudioPlatformState::addParameter<Route, ParamCriterion>(
    const std::string &typeName, const std::string &paramKey, const std::string &name,
    const std::string &defaultValue,
    const std::vector<AndroidParamMappingValuePair> &valuePairs)
{
    CriterionType *criterionType = getElement<CriterionType>(typeName, mCriterionTypeMap);
    RouteCriterionParameter *routeParamCriterion = new RouteCriterionParameter(
        this, paramKey, name, criterionType, mRoutePfwConnector, defaultValue);
    mCriterionMap[name] = routeParamCriterion->getCriterion();
    addParameter(routeParamCriterion, valuePairs);
}


template <>
void AudioPlatformState::addParameter<Route, ParamRogue>(
    const std::string &typeName, const std::string &paramKey, const std::string &name,
    const std::string &defaultValue,
    const std::vector<AndroidParamMappingValuePair> &valuePairs)
{
    RogueParameter *paramRogue;
    if (typeName == mUnsignedIntegerTypeTag) {
        paramRogue = new RouteRogueParameter<uint32_t>(this, paramKey, name, mRoutePfwConnector,
                                                       defaultValue);
    } else if (typeName == mStringTypeTag) {
        paramRogue = new RouteRogueParameter<string>(this, paramKey, name, mRoutePfwConnector,
                                                     defaultValue);
    } else {
        ALOGE("%s: type %s not supported ", __FUNCTION__, typeName.c_str());
        return;
    }
    addParameter(paramRogue, valuePairs);
}

void AudioPlatformState::parseChildren(cnode *root,
                                       string &path,
                                       string &defaultValue,
                                       string &key,
                                       string &type,
                                       vector<AndroidParamMappingValuePair> &valuePairs)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node;
    for (node = root->first_child; node != NULL; node = node->next) {
        AUDIOCOMMS_ASSERT(node != NULL, "error in parsing file");

        if (string(node->name) == mPathTag) {
            path = node->value;
        } else if (string(node->name) == mParameterDefaultTag) {
            defaultValue = node->value;
        } else if (string(node->name) == mAndroidParameterTag) {
            key = node->value;
        } else if (string(node->name) == mMappingTableTag) {
            valuePairs = parseMappingTable(node->value);
        } else if (string(node->name) == mTypeTag) {
            type = node->value;
        } else {
            ALOGE("%s: Unrecognized %s %s node ", __FUNCTION__, node->name, node->value);
        }
    }
    ALOGV("%s: path=%s,  key=%s default=%s, type=%s",
          __FUNCTION__, path.c_str(), key.c_str(), defaultValue.c_str(), type.c_str());
}

template <PfwInstance pfw>
void AudioPlatformState::loadRogueParameterType(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");

    const char *rogueParameterName = root->name;

    vector<AndroidParamMappingValuePair> valuePairs;
    string paramKeyName = "";
    string rogueParameterPath = "";
    string typeName = "";
    string defaultValue = "";

    parseChildren(root, rogueParameterPath, defaultValue, paramKeyName, typeName, valuePairs);

    AUDIOCOMMS_ASSERT(!paramKeyName.empty(), "Rogue Parameter " << rogueParameterName <<
                      " not associated to any Android parameter");

    addParameter<pfw, ParamRogue>(typeName,
                                  paramKeyName,
                                  rogueParameterPath,
                                  defaultValue,
                                  valuePairs);
}

template <PfwInstance pfw>
void AudioPlatformState::loadRogueParameterTypeList(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, mRogueParameterTag.c_str());
    if (node == NULL) {
        ALOGW("%s: no rogue parameter type found", __FUNCTION__);
        return;
    }
    for (node = node->first_child; node != NULL; node = node->next) {
        loadRogueParameterType<pfw>(node);
    }
}

template <typename T>
T *AudioPlatformState::getElement(const string &name, map<string, T *> &elementsMap)
{
    parameterManagerElementSupported<T>();
    typename map<string, T *>::iterator it = elementsMap.find(name);
    AUDIOCOMMS_ASSERT(it != elementsMap.end(), "Element " << name.c_str() << " not found");
    return it->second;
}

template <typename T>
const T *AudioPlatformState::getElement(const string &name,
                                        const map<string, T *> &elementsMap) const
{
    parameterManagerElementSupported<T>();
    typename map<string, T *>::const_iterator it = elementsMap.find(name);
    AUDIOCOMMS_ASSERT(it != elementsMap.end(), "Element " << name.c_str() << " not found");
    return it->second;
}

template <PfwInstance pfw>
void AudioPlatformState::loadCriteria(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, mCriterionTag.c_str());

    if (node == NULL) {
        ALOGW("%s: no inclusive criteria found", __FUNCTION__);
        return;
    }
    for (node = node->first_child; node != NULL; node = node->next) {
        loadCriterion<pfw>(node);
    }
}

vector<AudioPlatformState::AndroidParamMappingValuePair> AudioPlatformState::parseMappingTable(
    const char *values)
{
    AUDIOCOMMS_ASSERT(values != NULL, "error in parsing file");
    char *mappingPairs = (char *)(values);
    char *ctx;
    vector<AndroidParamMappingValuePair> valuePairs;

    char *mappingPair = strtok_r(mappingPairs, ",", &ctx);
    while (mappingPair != NULL) {
        if (strlen(mappingPair) != 0) {

            char *first = strtok(mappingPair, ":");
            char *second = strtok(NULL, ":");
            AUDIOCOMMS_ASSERT((first != NULL) && (strlen(first) != 0) &&
                              (second != NULL) && (strlen(second) != 0),
                              "invalid value pair");
            AndroidParamMappingValuePair pair = make_pair(first, second);
            valuePairs.push_back(pair);
        }
        mappingPair = strtok_r(NULL, ",", &ctx);
    }
    return valuePairs;
}

template <>
void AudioPlatformState::addCriterion<Audio>(const string &name,
                                             const string &typeName,
                                             const string &defaultLiteralValue)
{
    mStreamInterface->addCriterion(name, typeName, defaultLiteralValue);
}

template <>
void AudioPlatformState::addCriterion<Route>(const string &name,
                                             const string &typeName,
                                             const string &defaultLiteralValue)
{
    CriterionType *criterionType = getElement<CriterionType>(typeName, mCriterionTypeMap);
    mCriterionMap[name] = new Criterion(name,
                                        criterionType,
                                        mRoutePfwConnector,
                                        defaultLiteralValue);
}

template <PfwInstance pfw>
void AudioPlatformState::loadCriterion(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    const char *criterionName = root->name;

    AUDIOCOMMS_ASSERT(mCriterionMap.find(criterionName) == mCriterionMap.end(),
                      "Criterion " << criterionName << " already added");

    vector<AndroidParamMappingValuePair> valuePairs;
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
        addParameter<pfw, ParamCriterion>(typeName,
                                          paramKeyName,
                                          criterionName,
                                          defaultValue,
                                          valuePairs);
    } else {
        addCriterion<pfw>(criterionName, typeName, defaultValue);
    }
}

template <>
const string &AudioPlatformState::getPfwInstanceName<Audio>() const
{
    return mAudioConfTag;
}

template <>
const string &AudioPlatformState::getPfwInstanceName<Route>() const
{
    return mRouteConfTag;
}

template <PfwInstance pfw>
void AudioPlatformState::loadConfig(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, getPfwInstanceName<pfw>().c_str());
    if (node == NULL) {
        ALOGW("%s Could not find node for pfw=%s", __FUNCTION__, getPfwInstanceName<pfw>().c_str());
        return;
    }
    ALOGD("%s Loading conf for pfw=%s", __FUNCTION__, getPfwInstanceName<pfw>().c_str());

    loadInclusiveCriterionType<pfw>(node);
    loadExclusiveCriterionType<pfw>(node);
    loadCriteria<pfw>(node);
    loadRogueParameterTypeList<pfw>(node);
}

status_t AudioPlatformState::loadAudioHalConfig(const char *path)
{
    AUDIOCOMMS_ASSERT(path != NULL, "error in parsing file: empty path");
    cnode *root;
    char *data;
    ALOGD("%s", __FUNCTION__);
    data = (char *)load_file(path, NULL);
    if (data == NULL) {
        return -ENODEV;
    }
    root = config_node("", "");
    AUDIOCOMMS_ASSERT(root != NULL, "Unable to allocate a configuration node");
    config_load(root, data);

    loadConfig<Audio>(root);
    loadConfig<Route>(root);

    config_free(root);
    free(root);
    free(data);

    ALOGD("%s: loaded %s", __FUNCTION__, path);

    return NO_ERROR;
}

void AudioPlatformState::sync()
{
    std::for_each(mParameterVector.begin(), mParameterVector.end(), SyncParameterHelper());
    applyPlatformConfiguration();
}

void AudioPlatformState::clearParamKeys(AudioParameter *param)
{
    std::for_each(mParameterVector.begin(), mParameterVector.end(),
                  ClearKeyAndroidParameterHelper(param));
    if (param->size()) {

        ALOGW("%s: Unhandled argument: %s", __FUNCTION__, param->toString().string());
    }
}

status_t AudioPlatformState::setParameters(const android::String8 &keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    int errorCount = 0;
    std::for_each(mParameterVector.begin(), mParameterVector.end(),
                  SetFromAndroidParameterHelper(&param, &errorCount));

    clearParamKeys(&param);
    return errorCount == 0 ? OK : BAD_VALUE;
}

void AudioPlatformState::parameterHasChanged(const std::string &event)
{
    setPlatformStateEvent(event);
}

String8 AudioPlatformState::getParameters(const String8 &keys)
{
    AudioParameter param = AudioParameter(keys);
    AudioParameter returnedParam = AudioParameter(keys);

    std::for_each(mParameterVector.begin(), mParameterVector.end(),
                  GetFromAndroidParameterHelper(&param, &returnedParam));

    return returnedParam.toString();
}

bool AudioPlatformState::hasPlatformStateChanged(int iEvents) const
{
    CriterionMapConstIterator it = mCriterionMap.find(mStateChanged);
    AUDIOCOMMS_ASSERT(it != mCriterionMap.end(), "state " << mStateChanged << " not found");

    return (it->second->getValue() & iEvents) != 0 || mAudioPfwHasChanged;
}

void AudioPlatformState::setPlatformStateEvent(const string &eventStateName)
{
    Criterion *stateChangeCriterion = getElement<Criterion>(mStateChanged, mCriterionMap);

    // Checks if eventState name is a possible value of HasChanged criteria
    int eventId = 0;
    if (!stateChangeCriterion->getCriterionType()->getTypeInterface()->getNumericalValue(
            eventStateName, eventId)) {

        // Checks if eventState name is a possible value of HasChanged criteria of Route PFW.
        // If not, consider that this event is related to Audio PFW Instance.
        mAudioPfwHasChanged = true;
    }
    uint32_t platformEventChanged = stateChangeCriterion->getValue() | eventId;
    stateChangeCriterion->setValue<uint32_t>(platformEventChanged);
}

void AudioPlatformState::setVoipBandType(const Stream *activeStream)
{
    CAudioBand::Type band = CAudioBand::EWide;
    if (activeStream->sampleRate() == mVoiceStreamRateForNarrowBandProcessing) {

        band = CAudioBand::ENarrow;
    }
    setValue(band, mVoipBand);
}

void AudioPlatformState::updateParametersFromActiveInput()
{
    StreamListConstIterator it;

    for (it = mActiveStreamsList[false].begin(); it != mActiveStreamsList[false].end(); ++it) {

        const Stream *stream = *it;
        if (stream->getDevices() != 0) {

            ALOGD("%s: found valid input stream, effectResMask=0x%X", __FUNCTION__,
                  stream->getEffectRequested());

            // Set the requested effect from this active input.
            setValue(stream->getEffectRequested(), mPreProcessorRequestedByActiveInput);

            // Set the band type according to this active input.
            setVoipBandType(stream);

            return;
        }
    }
    setValue(0, mPreProcessorRequestedByActiveInput);
}

uint32_t AudioPlatformState::updateStreamsMask(bool isOut)
{
    StreamListConstIterator it;
    uint32_t streamsMask = 0;

    for (it = mActiveStreamsList[isOut].begin(); it != mActiveStreamsList[isOut].end(); ++it) {

        const Stream *stream = *it;
        streamsMask |= stream->getApplicabilityMask();
    }
    return streamsMask;
}

void AudioPlatformState::updateApplicabilityMask(bool isOut)
{
    uint32_t applicabilityMask = updateStreamsMask(isOut);
    setValue(applicabilityMask, isOut ? mOutputFlags : mInputSources);
}

void AudioPlatformState::startStream(const Stream *startedStream)
{
    AUDIOCOMMS_ASSERT(startedStream != NULL, "NULL stream");
    bool isOut = startedStream->isOut();
    mActiveStreamsList[isOut].push_back(startedStream);
    updateApplicabilityMask(isOut);
    if (!isOut) {
        updateParametersFromActiveInput();
    }
}

void AudioPlatformState::stopStream(const Stream *stoppedStream)
{
    AUDIOCOMMS_ASSERT(stoppedStream != NULL, "NULL stream");
    bool isOut = stoppedStream->isOut();
    mActiveStreamsList[isOut].remove(stoppedStream);
    updateApplicabilityMask(isOut);
    if (!isOut) {
        updateParametersFromActiveInput();
    }
}

void AudioPlatformState::clearPlatformStateEvents()
{
    mCriterionMap[mStateChanged]->setValue<uint32_t>(0);
    mAudioPfwHasChanged = false;
}

bool AudioPlatformState::isStarted()
{
    ALOGD("%s: %s", __FUNCTION__,
          mRoutePfwConnector && mRoutePfwConnector->isStarted() ? "true" : "false");
    return mRoutePfwConnector && mRoutePfwConnector->isStarted();
}

void AudioPlatformState::applyPlatformConfiguration()
{
    mCriterionMap[mStateChanged]->setCriterionState();
    mRoutePfwConnector->applyConfigurations();
    clearPlatformStateEvents();
}

void AudioPlatformState::setValue(int value, const char *stateName)
{
    if (getElement<Criterion>(stateName, mCriterionMap)->setCriterionState(value)) {

        setPlatformStateEvent(stateName);
    }
}

int AudioPlatformState::getValue(const char *stateName) const
{
    return getElement<Criterion>(stateName, mCriterionMap)->getValue();
}


void AudioPlatformState::printPlatformFwErrorInfo() const
{

    ALOGE("^^^^  Print platform Audio firmware error info  ^^^^");

    string paramValue;

    /**
     * Get the list of files path we wish to print. This list is represented as a
     * string defined in the route manager RouteDebugFs plugin.
     */
    if (!ParameterMgrHelper::getParameterValue<std::string>(mRoutePfwConnector,
                                                            mHwDebugFilesPathList,
                                                            paramValue)) {
        ALOGE("Could not get path list from XML configuration");
        return;
    }

    vector<std::string> debugFiles;
    char *debugFile;
    string debugFileString;
    char *tokenString = static_cast<char *>(alloca(paramValue.length() + 1));
    vector<std::string>::const_iterator it;

    strncpy(tokenString, paramValue.c_str(), paramValue.length() + 1);

    while ((debugFile = NaiveTokenizer::getNextToken(&tokenString)) != NULL) {
        debugFileString = string(debugFile);
        debugFileString = debugFile;
        debugFiles.push_back(debugFileString);
    }

    for (it = debugFiles.begin(); it != debugFiles.end(); ++it) {
        ifstream debugStream;

        ALOGE("Opening file %s and reading it.", it->c_str());
        debugStream.open(it->c_str(), ifstream::in);

        if (debugStream.fail()) {
            ALOGE("Could not open Hw debug file, error : %s", strerror(errno));
            debugStream.close();
            continue;
        }

        while (debugStream.good()) {
            char dataToRead[mMaxDebugStreamSize];

            debugStream.read(dataToRead, mMaxDebugStreamSize);
            ALOGE("%s", dataToRead);
        }

        debugStream.close();
    }
}

}       // namespace android
