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
#include "Stream.hpp"
#include "ParameterCriterion.hpp"
#include "ParameterMgrPlatformConnector.h"
#include "SelectionCriterionInterface.h"
#include <Criterion.hpp>
#include <CriterionType.hpp>
#include <AudioCommsAssert.hpp>
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

const char *const AudioPlatformState::mRouteCriterionConfFilePath =
    "/system/etc/route_criteria.conf";
const char *const AudioPlatformState::mRouteCriterionVendorConfFilePath =
    "/vendor/etc/route_criteria.conf";
const char *const AudioPlatformState::mInclusiveCriterionTypeTag = "InclusiveCriterionType";
const char *const AudioPlatformState::mExclusiveCriterionTypeTag = "ExclusiveCriterionType";
const char *const AudioPlatformState::mCriterionTag = "Criterion";
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

AudioPlatformState::AudioPlatformState()
    : mRoutePfwConnectorLogger(new ParameterMgrPlatformConnectorLogger)
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

    if (loadRouteCriterionConfig(mRouteCriterionVendorConfFilePath) != OK) {
        if (loadRouteCriterionConfig(mRouteCriterionConfFilePath) != OK) {

            ALOGE("Neither vendor conf file (%s) nor system conf file (%s) could be found",
                  mRouteCriterionVendorConfFilePath, mRouteCriterionConfFilePath);
        }
    }

    /// Start PFW
    std::string strError;
    if (!mRoutePfwConnector->start(strError)) {

        ALOGE("Route PFW start error: %s", strError.c_str());
        return;
    }
    ALOGD("%s: Route PFW successfully started!", __FUNCTION__);
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

void AudioPlatformState::loadCriterionType(cnode *root, bool isInclusive)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = root->first_child;

    while (node) {

        char *criterionTypeName = (char *)node->name;
        char *valueNames = (char *)node->value;
        AUDIOCOMMS_ASSERT(mCriterionTypeMap.find(criterionTypeName) == mCriterionTypeMap.end(),
                          "CriterionType " << criterionTypeName << " already added");

        CriterionType *criterionType = new CriterionType(criterionTypeName,
                                                         isInclusive,
                                                         mRoutePfwConnector);
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

                    if (!convertTo<uint32_t>(first, index)) {

                        ALOGE("%s: Invalid index(%s) found", __FUNCTION__, first);
                    }
                    ALOGV("%s: name=%s, index=0x%X, value=%s", __FUNCTION__, criterionTypeName,
                          index, second);
                    criterionType->addValuePair(index, second);
                } else {

                    ALOGV("%s: name=%s, index=0x%X, value=%s", __FUNCTION__, criterionTypeName,
                          isInclusive ? 1 << index : index, valueName);
                    criterionType->addValuePair(isInclusive ? 1 << index : index, valueName);
                    index += 1;
                }
            }
            valueName = strtok_r(NULL, ",", &ctx);
        }
        mCriterionTypeMap[criterionTypeName] = criterionType;
        node = node->next;
    }
}

void AudioPlatformState::loadInclusiveCriterionType(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, mInclusiveCriterionTypeTag);
    if (node == NULL) {
        return;
    }
    loadCriterionType(node, true);
}

void AudioPlatformState::loadExclusiveCriterionType(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, mExclusiveCriterionTypeTag);
    if (node == NULL) {
        return;
    }
    loadCriterionType(node, false);
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

void AudioPlatformState::loadCriteria(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, mCriterionTag);
    if (node != NULL) {

        node = node->first_child;
        while (node) {

            loadCriterion(node);
            node = node->next;
        }
    }
}

vector<AudioPlatformState::ParamToCriterionValuePair> AudioPlatformState::parseMappingTable(
    const char *values)
{
    AUDIOCOMMS_ASSERT(values != NULL, "error in parsing file");
    char *mappingPairs = (char *)(values);
    char *ctx;
    vector<ParamToCriterionValuePair> valuePairs;

    char *mappingPair = strtok_r(mappingPairs, ",", &ctx);
    while (mappingPair != NULL) {
        if (strlen(mappingPair) != 0) {

            char *first = strtok(mappingPair, ":");
            char *second = strtok(NULL, ":");
            AUDIOCOMMS_ASSERT((first != NULL) && (strlen(first) != 0) &&
                              (second != NULL) && (strlen(second) != 0),
                              "invalid value pair");
            ParamToCriterionValuePair pair = make_pair(first, second);
            valuePairs.push_back(pair);
        }
        mappingPair = strtok_r(NULL, ",", &ctx);
    }
    return valuePairs;
}

class AudioPlatformState::SetParamToCriterionPair
{

public:
    SetParamToCriterionPair(ParameterCriterion *paramCriterion)
        : mParamCriterion(paramCriterion)
    {}

    void operator()(ParamToCriterionValuePair pair)
    {

        int numerical = 0;
        if (!mParamCriterion->getCriterionType()->getTypeInterface()->getNumericalValue(pair.second,
                                                                                        numerical))
        {

            ALOGW("%s: could not retrieve numerical value for %s", __FUNCTION__, pair.second);
        }
        mParamCriterion->setParamToCriterionPair(pair.first, numerical);
    }

    ParameterCriterion *mParamCriterion;
};

void AudioPlatformState::loadCriterion(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    vector<ParamToCriterionValuePair> valuePairs;
    const char *paramKeyName = "";

    const char *criterionName = root->name;
    const char *typeName = "";
    const char *defaultValue = "";
    int defaultNumValue = 0;
    CriterionType *criterionType = NULL;

    AUDIOCOMMS_ASSERT(mCriterionMap.find(criterionName) == mCriterionMap.end(),
                      "Criterion " << criterionName << " already added");

    cnode *node = root->first_child;
    while (node) {

        if (!strcmp(node->name, "Default")) {

            defaultValue = node->value;
        } else if (!strcmp(node->name, "Parameter")) {

            paramKeyName = node->value;
        } else if (!strcmp(node->name, "Mapping")) {

            valuePairs = parseMappingTable(node->value);
        } else if (!strcmp(node->name, "Type")) {

            typeName = node->value;
        } else {

            ALOGE("%s: Unrecognized %s %s node ", __FUNCTION__, node->name, node->value);
        }
        node = node->next;
    }
    criterionType = getElement<CriterionType>(typeName, mCriterionTypeMap);
    if (!criterionType->getTypeInterface()->getNumericalValue(defaultValue, defaultNumValue)) {

        ALOGE("%s: could not retrieve numerical value for %s", __FUNCTION__, defaultValue);
    }

    ALOGV("%s: criterion name=%s,type=%s paramKey=%s default=%s,%d",
          __FUNCTION__, criterionName, typeName, paramKeyName, defaultValue, defaultNumValue);

    if (strlen(paramKeyName) != 0) {

        ParameterCriterion *paramCriterion = new ParameterCriterion(paramKeyName,
                                                                    criterionName,
                                                                    criterionType,
                                                                    mRoutePfwConnector,
                                                                    defaultNumValue);

        for_each(valuePairs.begin(), valuePairs.end(), SetParamToCriterionPair(paramCriterion));

        mCriterionMap[criterionName] = paramCriterion;
        mParameterCriteriaVector.push_back(paramCriterion);
    } else {

        mCriterionMap[criterionName] = new Criterion(criterionName,
                                                     criterionType,
                                                     mRoutePfwConnector,
                                                     defaultNumValue);
    }
}

status_t AudioPlatformState::loadRouteCriterionConfig(const char *path)
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

    loadInclusiveCriterionType(root);
    loadExclusiveCriterionType(root);
    loadCriteria(root);

    config_free(root);
    free(root);
    free(data);

    ALOGD("%s: loaded %s", __FUNCTION__, path);

    return NO_ERROR;
}

class AudioPlatformState::CheckAndSetParameter
{

public:
    CheckAndSetParameter(AudioParameter *param, AudioPlatformState *ctx)
        : mParam(param),
          mCtx(ctx)
    {}

    void operator()(ParameterCriterion *paramCriterion)
    {

        String8 key = String8(paramCriterion->getKey());
        String8 value;
        if (mParam->get(key, value) == NO_ERROR) {

            if (paramCriterion->setParameter(value.string())) {

                mCtx->setPlatformStateEvent(paramCriterion->getName().c_str());
            }
            mParam->remove(key);
        }
    }

    AudioParameter *mParam;
    AudioPlatformState *mCtx;
};

status_t AudioPlatformState::setParameters(const android::String8 &keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);

    std::for_each(mParameterCriteriaVector.begin(), mParameterCriteriaVector.end(),
                  CheckAndSetParameter(&param, this));

    if (param.size()) {

        ALOGW("%s: Unhandled argument: %s", __FUNCTION__, param.toString().string());
    }
    return OK;
}

bool AudioPlatformState::hasPlatformStateChanged(int iEvents) const
{
    CriterionMapConstIterator it = mCriterionMap.find(mStateChanged);
    AUDIOCOMMS_ASSERT(it != mCriterionMap.end(), "state " << mStateChanged << " not found");

    return (it->second->getValue() & iEvents) != 0;
}

void AudioPlatformState::setPlatformStateEvent(const string &eventStateName)
{
    CriterionMapIterator it = mCriterionMap.find(mStateChanged);
    AUDIOCOMMS_ASSERT(it != mCriterionMap.end(), "state " << mStateChanged << " not found");

    // Checks if eventState name is a possible value of HasChanged criteria
    int eventId = 0;
    if (!it->second->getCriterionType()->getTypeInterface()->getNumericalValue(
            eventStateName, eventId)) {

        ALOGD("%s: event %s is not part of knowed state", __FUNCTION__, eventStateName.c_str());
        return;
    }
    int32_t platformEventChanged = it->second->getValue() | eventId;
    it->second->setValue(platformEventChanged);
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
    mCriterionMap[mStateChanged]->setValue(0);
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
