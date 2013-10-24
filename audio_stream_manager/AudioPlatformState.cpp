/*
 * INTEL CONFIDENTIAL
 * Copyright © 2013 Intel
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
 * disclosed in any way without Intel’s prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#define LOG_TAG "AudioIntelHAL/AudioPlatformState"

#include "AudioPlatformState.h"
#include "AudioStream.h"
#include "ParameterCriterion.h"
#include "ParameterMgrPlatformConnector.h"
#include "SelectionCriterionInterface.h"
#include <Criterion.h>
#include <CriterionType.h>
#include <AudioCommsAssert.hpp>
#include <Property.h>
#include <algorithm>
#include <convert.hpp>
#include <cutils/bitops.h>
#include <cutils/config_utils.h>
#include <cutils/misc.h>
#include <hardware_legacy/AudioHardwareBase.h>
#include <media/AudioParameter.h>
#include <utils/Log.h>

#define DIRECT_STREAM_FLAGS (AUDIO_OUTPUT_FLAG_DIRECT | AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD)

using std::string;
using audio_comms::utilities::convertTo;

namespace android_audio_legacy
{

const char *const AudioPlatformState::ROUTE_PFW_CONF_FILE_NAME_PROP_NAME =
    "AudioComms.RoutePFW.ConfPath";

const char *const AudioPlatformState::ROUTE_PFW_DEFAULT_CONF_FILE_NAME =
    "/etc/parameter-framework/ParameterFrameworkConfigurationRoute.xml";

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

const char *const AudioPlatformState::AUDIO_CRITERION_CONF_FILE_PATH =  "/etc/audio_criteria.conf";
const char *const AudioPlatformState::INCLUSIVE_CRITERION_TYPE_TAG = "InclusiveCriterionType";
const char *const AudioPlatformState::EXCLUSIVE_CRITERION_TYPE_TAG = "ExclusiveCriterionType";
const char *const AudioPlatformState::CRITERION_TAG = "Criterion";
const char *const AudioPlatformState::OUTPUT_DEVICE = "OutputDevices";
const char *const AudioPlatformState::INPUT_DEVICE = "InputDevices";
const char *const AudioPlatformState::INPUT_SOURCES = "InputSources";
const char *const AudioPlatformState::OUTPUT_FLAGS = "OutputFlags";
const char *const AudioPlatformState::MODEM_AUDIO_STATUS = "ModemAudioStatus";
const char *const AudioPlatformState::ANDROID_MODE = "AndroidMode";
const char *const AudioPlatformState::HAS_MODEM = "HasModem";
const char *const AudioPlatformState::MODEM_STATE = "ModemState";
const char *const AudioPlatformState::STATE_CHANGED = "StatesChanged";
const char *const AudioPlatformState::CSV_BAND = "CsvBandType";
const char *const AudioPlatformState::VOIP_BAND = "VoIPBandType";
const char *const AudioPlatformState::MIC_MUTE = "MicMute";

template <>
struct AudioPlatformState::parameterManagerElementSupported<Criterion> {};
template <>
struct AudioPlatformState::parameterManagerElementSupported<CriterionType> {};

AudioPlatformState::AudioPlatformState()
    : _directStreamsRefCount(0),
      _routePfwConnectorLogger(new ParameterMgrPlatformConnectorLogger)
{
    /// Connector
    // Fetch the name of the PFW configuration file: this name is stored in an Android property
    // and can be different for each hardware
    string routePfwConfFilePath = TProperty<string>(ROUTE_PFW_CONF_FILE_NAME_PROP_NAME,
                                                    ROUTE_PFW_DEFAULT_CONF_FILE_NAME);
    ALOGI("Route-PFW: using configuration file: %s", routePfwConfFilePath.c_str());

    _routePfwConnector = new CParameterMgrPlatformConnector(routePfwConfFilePath);

    // Logger
    _routePfwConnector->setLogger(_routePfwConnectorLogger);

    loadAudioCriterionConfig(AUDIO_CRITERION_CONF_FILE_PATH);

    /// Start PFW
    std::string strError;
    if (!_routePfwConnector->start(strError)) {

        ALOGE("Route PFW start error: %s", strError.c_str());
        return;
    }
    ALOGD("%s: Route PFW successfully started!", __FUNCTION__);
}

AudioPlatformState::~AudioPlatformState()
{
    // Delete All criterion
    CriterionMapIterator it;
    for (it = _criterionMap.begin(); it != _criterionMap.end(); ++it) {

        delete it->second;
    }

    // Delete All criterion type
    CriterionTypeMapIterator itType;
    for (itType = _criterionTypeMap.begin(); itType != _criterionTypeMap.end(); ++itType) {

        delete itType->second;
    }

    // Unset logger
    _routePfwConnector->setLogger(NULL);
    // Remove logger
    delete _routePfwConnectorLogger;
    // Remove connector
    delete _routePfwConnector;
}

void AudioPlatformState::loadCriterionType(cnode *root, bool isInclusive)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = root->first_child;

    while (node) {

        char *criterionTypeName = (char *)node->name;
        char *valueNames = (char *)node->value;
        AUDIOCOMMS_ASSERT(_criterionTypeMap.find(criterionTypeName) == _criterionTypeMap.end(),
                          "CriterionType " << criterionTypeName << " already added");

        CriterionType *criterionType = new CriterionType(criterionTypeName,
                                                         isInclusive,
                                                         _routePfwConnector);
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
        _criterionTypeMap[criterionTypeName] = criterionType;
        node = node->next;
    }
}

void AudioPlatformState::loadInclusiveCriterionType(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, INCLUSIVE_CRITERION_TYPE_TAG);
    if (node == NULL) {
        return;
    }
    loadCriterionType(node, true);
}

void AudioPlatformState::loadExclusiveCriterionType(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, EXCLUSIVE_CRITERION_TYPE_TAG);
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
    cnode *node = config_find(root, CRITERION_TAG);
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
        : _paramCriterion(paramCriterion)
    {}

    void operator()(ParamToCriterionValuePair pair)
    {

        int numerical = 0;
        if (!_paramCriterion->getCriterionType()->getTypeInterface()->getNumericalValue(pair.second,
                                                                                        numerical))
        {

            ALOGW("%s: could not retrieve numerical value for %s", __FUNCTION__, pair.second);
        }
        _paramCriterion->setParamToCriterionPair(pair.first, numerical);
    }

    ParameterCriterion *_paramCriterion;
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

    AUDIOCOMMS_ASSERT(_criterionMap.find(criterionName) == _criterionMap.end(),
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
    ALOGV("%s: criterion name=%s,type=%s paramKey=%s default=%s,%d",
          __FUNCTION__, criterionName, typeName, paramKeyName, defaultValue, defaultNumValue);

    criterionType = getElement<CriterionType>(typeName, _criterionTypeMap);
    if (!criterionType->getTypeInterface()->getNumericalValue(defaultValue, defaultNumValue)) {

        ALOGE("%s: could not retrieve numerical value for %s", __FUNCTION__, defaultValue);
    }

    if (strlen(paramKeyName) != 0) {

        ParameterCriterion *paramCriterion = new ParameterCriterion(paramKeyName,
                                                                    criterionName,
                                                                    criterionType,
                                                                    _routePfwConnector,
                                                                    defaultNumValue);

        for_each(valuePairs.begin(), valuePairs.end(), SetParamToCriterionPair(paramCriterion));

        _criterionMap[criterionName] = paramCriterion;
        _parameterCriteriaVector.push_back(paramCriterion);
    } else {

        _criterionMap[criterionName] = new Criterion(criterionName,
                                                     criterionType,
                                                     _routePfwConnector,
                                                     defaultNumValue);
    }
}

status_t AudioPlatformState::loadAudioCriterionConfig(const char *path)
{
    AUDIOCOMMS_ASSERT(path != NULL, "error in parsing file: empty path");
    cnode *root;
    char *data;
    ALOGD("loadAudioCriterionConfig() ");
    data = (char *)load_file(path, NULL);
    if (data == NULL) {
        return -ENODEV;
    }
    root = config_node("", "");
    config_load(root, data);

    loadInclusiveCriterionType(root);
    loadExclusiveCriterionType(root);
    loadCriteria(root);

    config_free(root);
    free(root);
    free(data);

    ALOGI("loadAudioPolicyConfig() loaded %s\n", path);

    return NO_ERROR;
}

class AudioPlatformState::CheckAndSetParameter
{

public:
    CheckAndSetParameter(AudioParameter *param, AudioPlatformState *ctx)
        : _param(param),
          _ctx(ctx)
    {}

    void operator()(ParameterCriterion *paramCriterion)
    {

        String8 key = String8(paramCriterion->getKey());
        String8 value;
        if (_param->get(key, value) == NO_ERROR) {

            if (paramCriterion->setParameter(value.string())) {

                _ctx->setPlatformStateEvent(paramCriterion->getName().c_str());
            }
            _param->remove(key);
        }
    }

    AudioParameter *_param;
    AudioPlatformState *_ctx;
};

status_t AudioPlatformState::setParameters(const android::String8 &keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);

    std::for_each(_parameterCriteriaVector.begin(), _parameterCriteriaVector.end(),
                  CheckAndSetParameter(&param, this));

    if (param.size()) {

        ALOGW("%s: Unhandled argument: %s", __FUNCTION__, param.toString().string());
    }
    return OK;
}

bool AudioPlatformState::hasPlatformStateChanged(int iEvents) const
{
    CriterionMapConstIterator it = _criterionMap.find(STATE_CHANGED);
    AUDIOCOMMS_ASSERT(it != _criterionMap.end(), "state " << STATE_CHANGED << " not found");

    ALOGV("%s 0x%X 0x%X", __FUNCTION__, _criterionMap[STATE_CHANGED]->getValue(), iEvents);
    return (it->second->getValue() & iEvents) != 0;
}

void AudioPlatformState::setPlatformStateEvent(const string &eventStateName)
{
    CriterionMapIterator it = _criterionMap.find(STATE_CHANGED);
    AUDIOCOMMS_ASSERT(it != _criterionMap.end(), "state " << STATE_CHANGED << " not found");

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

void AudioPlatformState::updateActiveInputSources()
{
    uint32_t inputSources = updateStreamsMask(false);
    setValue(inputSources, INPUT_SOURCES);
}

void AudioPlatformState::updateActiveOutputFlags()
{
    uint32_t outputFlags = updateStreamsMask(true);
    setValue(outputFlags, OUTPUT_FLAGS);
}

uint32_t AudioPlatformState::updateStreamsMask(bool isOut)
{
    StreamListConstIterator it;
    uint32_t streamsMask = 0;

    for (it = _activeStreamsList[isOut].begin(); it != _activeStreamsList[isOut].end(); ++it) {

        const AudioStream *stream = *it;
        streamsMask |= stream->getApplicabilityMask();
    }
    return streamsMask;
}

void AudioPlatformState::startStream(const AudioStream *startedStream)
{
    AUDIOCOMMS_ASSERT(startedStream != NULL, "NULL stream");
    bool isOut = startedStream->isOut();
    _activeStreamsList[isOut].push_back(startedStream);
    isOut ? updateActiveOutputFlags() : updateActiveInputSources();
}

void AudioPlatformState::stopStream(const AudioStream *stoppedStream)
{
    AUDIOCOMMS_ASSERT(stoppedStream != NULL, "NULL stream");
    bool isOut = stoppedStream->isOut();
    _activeStreamsList[isOut].remove(stoppedStream);
    isOut ? updateActiveOutputFlags() : updateActiveInputSources();
}

void AudioPlatformState::clearPlatformStateEvents()
{
    _criterionMap[STATE_CHANGED]->setValue(0);
}

bool AudioPlatformState::isStarted()
{
    ALOGD("%s: %s", __FUNCTION__,
          _routePfwConnector && _routePfwConnector->isStarted() ? "true" : "false");
    return _routePfwConnector && _routePfwConnector->isStarted();
}

void AudioPlatformState::applyPlatformConfiguration()
{
    _criterionMap[STATE_CHANGED]->setCriterionState();
    _routePfwConnector->applyConfigurations();
    clearPlatformStateEvents();
}

void AudioPlatformState::setValue(int value, const char *stateName)
{
    if (getElement<Criterion>(stateName, _criterionMap)->setCriterionState(value)) {

        setPlatformStateEvent(stateName);
    }
}

int AudioPlatformState::getValue(const char *stateName) const
{
    return getElement<Criterion>(stateName, _criterionMap)->getValue();
}
}       // namespace android
