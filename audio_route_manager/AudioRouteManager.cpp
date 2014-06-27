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
#define LOG_TAG "RouteManager"

#include "AudioRouteManagerObserver.hpp"
#include "AudioPort.hpp"
#include "AudioPortGroup.hpp"
#include "AudioRouteManager.hpp"
#include "AudioStreamRoute.hpp"
#include "EventThread.h"
#include "InterfaceProviderLib.h"
#include "ParameterMgrPlatformConnector.h"
#include "Property.h"
#include <Observer.hpp>
#include <AudioParameterHelper.hpp>
#include <Criterion.hpp>
#include <CriterionType.hpp>
#include <Stream.hpp>
#include <AudioCommsAssert.hpp>
#include <BitField.hpp>
#include <cutils/bitops.h>
#include <hardware_legacy/AudioSystemLegacy.h>
#include <string>
#include <utils/Log.h>

using namespace android;
using android_audio_legacy::AudioSystem;
using namespace std;
using NInterfaceProvider::CInterfaceProviderImpl;
using audio_comms::utilities::BitField;
using audio_comms::utilities::Direction;

typedef RWLock::AutoRLock AutoR;
typedef RWLock::AutoWLock AutoW;

const char *const AudioRouteManager::_voiceVolume =
    "/Audio/CONFIGURATION/VOICE_VOLUME_CTRL_PARAMETER";

const char *const AudioRouteManager::_closingRouteCriterion[Direction::_nbDirections] = {
    "ClosingCaptureRoutes", "ClosingPlaybackRoutes"
};
const char *const AudioRouteManager::_openedRouteCriterion[Direction::_nbDirections] = {
    "OpenedCaptureRoutes", "OpenedPlaybackRoutes"
};
const char *const AudioRouteManager::_routeCriterionType = "RouteType";
const char *const AudioRouteManager::_routingStage = "RoutageState";

const char *const AudioRouteManager::_audioPfwConfFilePropName = "AudioComms.PFW.ConfPath";

const char *const AudioRouteManager::_audioPfwDefaultConfFileName =
    "/etc/parameter-framework/ParameterFrameworkConfiguration.xml";

class CParameterMgrPlatformConnectorLogger : public CParameterMgrPlatformConnector::ILogger
{
public:
    CParameterMgrPlatformConnectorLogger() {}

    virtual void log(bool isWarning, const string &log)
    {
        const static char format[] = "audio-parameter-manager: %s";

        if (isWarning) {

            ALOGW(format, log.c_str());
        } else {

            ALOGD(format, log.c_str());
        }
    }
};

/**
 * Routing stage criteria.
 *
 * A Flow stage on ClosingRoutes leads to Mute.
 * A Flow stage on OpenedRoutes leads to Unmute.
 * A Path stage on ClosingRoutes leads to Disable.
 * A Path stage on OpenedRoutes leads to Enable.
 * A Configure stage on ClosingRoutes leads to resetting the configuration.
 * A Configure stage on OpenedRoute lead to setting the configuration.
 */
const pair<int, const char *> AudioRouteManager::_routingStageValuePairs[] = {
    make_pair(FlowMask,       "Flow"),
    make_pair(PathMask,       "Path"),
    make_pair(ConfigureMask,  "Configure")
};

template <>
struct AudioRouteManager::routingElementSupported<AudioPort> {};
template <>
struct AudioRouteManager::routingElementSupported<AudioPortGroup> {};
template <>
struct AudioRouteManager::routingElementSupported<AudioRoute> {};
template <>
struct AudioRouteManager::routingElementSupported<AudioStreamRoute> {};

AudioRouteManager::AudioRouteManager()
    : _routeInterface(this),
      _streamInterface(this),
      _audioPfwConnectorLogger(new CParameterMgrPlatformConnectorLogger),
      _eventThread(new CEventThread(this)),
      _isStarted(false)
{
    memset(_routes, 0, sizeof(_routes[0]) * Direction::_nbDirections);

    /// Connector
    // Fetch the name of the PFW configuration file: this name is stored in an Android property
    // and can be different for each hardware
    string audioPfwConfigurationFilePath = TProperty<string>(_audioPfwConfFilePropName,
                                                             _audioPfwDefaultConfFileName);
    ALOGI("%s: audio PFW using configuration file: %s", __FUNCTION__,
          audioPfwConfigurationFilePath.c_str());

    // Actually create the Connector
    _audioPfwConnector = new CParameterMgrPlatformConnector(audioPfwConfigurationFilePath);

    _parameterHelper = new AudioParameterHelper(_audioPfwConnector);

    // Logger
    _audioPfwConnector->setLogger(_audioPfwConnectorLogger);
}

AudioRouteManager::~AudioRouteManager()
{
    AutoW lock(_routingLock);

    if (_isStarted) {

        _eventThread->stop();
    }
    delete _eventThread;

    // Delete all routes
    RouteMapIterator it;
    for (it = _routeMap.begin(); it != _routeMap.end(); ++it) {

        delete it->second;
    }

    // Unset logger
    _audioPfwConnector->setLogger(NULL);
    delete _parameterHelper;

    // Remove logger
    delete _audioPfwConnectorLogger;

    // Remove connector
    delete _audioPfwConnector;
}

// Interface populate
void AudioRouteManager::getImplementedInterfaces(CInterfaceProviderImpl &interfaceProvider)
{
    interfaceProvider.addInterface(&_routeInterface);
    interfaceProvider.addInterface(&_streamInterface);
}

status_t AudioRouteManager::startService()
{
    AutoW lock(_routingLock);

    AUDIOCOMMS_ASSERT(_isStarted != true, "Route Manager service already started!");

    // Routes Criterion Type
    AUDIOCOMMS_ASSERT(_criterionTypesMap.find(_routeCriterionType) != _criterionTypesMap.end(),
                      "Route CriterionType not found");

    CriterionType *routeCriterionType = _criterionTypesMap[_routeCriterionType];

    // Route Criteria
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        _selectedOpenedRoutes[i] = new Criterion(_openedRouteCriterion[i],
                                                 routeCriterionType,
                                                 _audioPfwConnector);
        _selectedClosingRoutes[i] = new Criterion(_closingRouteCriterion[i],
                                                  routeCriterionType,
                                                  _audioPfwConnector);
    }

    CriterionType *routageStageCriterionType = new CriterionType(_routingStage, true,
                                                                 _audioPfwConnector);
    _criterionTypesMap[_routingStage] = routageStageCriterionType;
    routageStageCriterionType->addValuePairs(_routingStageValuePairs,
                                             sizeof(_routingStageValuePairs) /
                                             sizeof(_routingStageValuePairs[0]));
    _routingStageCriterion = new Criterion(_routingStage, routageStageCriterionType,
                                           _audioPfwConnector);
    // Start Event thread
    bool started = _eventThread->start();
    AUDIOCOMMS_ASSERT(started, "failure when starting event thread!");

    // Start PFW
    std::string strError;
    if (!_audioPfwConnector->start(strError)) {

        ALOGE("parameter-manager start error: %s", strError.c_str());
        _eventThread->stop();
        return NO_INIT;
    }
    initRouting();

    _isStarted = true;

    ALOGD("%s: parameter-manager successfully started!", __FUNCTION__);

    return NO_ERROR;
}


void AudioRouteManager::initRouting()
{
    _routingStageCriterion->setCriterionState(Configure | Path | Flow);
    _audioPfwConnector->applyConfigurations();
}

void AudioRouteManager::reconsiderRouting(bool isSynchronous, bool forceResync)
{
    AutoW lock(_routingLock);

    AUDIOCOMMS_ASSERT(_isStarted && !_eventThread->inThreadContext(),
                      "Failure: service not started or not in correct thread context!");

    if (!isSynchronous) {

        // Trigs the processing of the list
        _eventThread->trig(forceResync ? FORCE_RESYNC : 0);
    } else {

        // Create a route manager observer
        AudioRouteManagerObserver obs;

        // Add the observer to the route manager
        addObserver(&obs);

        // Trig the processing of the list
        _eventThread->trig(forceResync ? FORCE_RESYNC : 0);

        // Unlock to allow for sem wait
        _routingLock.unlock();

        // Wait a notification from the route manager
        obs.waitNotification();

        // Relock
        _routingLock.writeLock();

        // Remove the observer from Route Manager
        removeObserver(&obs);
    }
}

void AudioRouteManager::doReconsiderRouting(bool forceResync)
{
    if (!checkAndPrepareRouting() && !forceResync) {

        // No need to reroute. Some criterion might have changed, update all criteria and apply
        // the conf in order to take for example tuning configuration that are glitch free and do
        // not need to go through the 5-steps routing.
        commitCriteriaAndApply();
        return;
    }
    ALOGD("%s: Route state:", __FUNCTION__);
    ALOGD("\t-Previously Enabled Route in Input = %s",
          routeCriterionType()->getFormattedState(prevEnabledRoutes(Direction::Input)).c_str());
    ALOGD("\t-Previously Enabled Route in Output = %s",
          routeCriterionType()->getFormattedState(prevEnabledRoutes(Direction::Output)).c_str());
    ALOGD("\t-Selected Route in Input = %s",
          routeCriterionType()->getFormattedState(enabledRoutes(Direction::Input)).c_str());
    ALOGD("\t-Selected Route in Output = %s",
          routeCriterionType()->getFormattedState(enabledRoutes(Direction::Output)).c_str());
    ALOGD_IF(needReflowRoutes(Direction::Input), "\t-Route that need reconfiguration in Input = %s",
             routeCriterionType()->getFormattedState(needReflowRoutes(Direction::Input)).c_str());
    ALOGD_IF(needReflowRoutes(Direction::Output),
             "\t-Route that need reconfiguration in Output = %s",
             routeCriterionType()->getFormattedState(needReflowRoutes(Direction::Output)).c_str());
    ALOGD_IF(needRepathRoutes(Direction::Input), "\t-Route that need rerouting in Input = %s",
             routeCriterionType()->getFormattedState(needRepathRoutes(Direction::Input)).c_str());
    ALOGD_IF(needRepathRoutes(Direction::Output), "\t-Route that need rerouting in Output = %s",
             routeCriterionType()->getFormattedState(needRepathRoutes(Direction::Output)).c_str());

    executeRouting();
    ALOGD("%s: DONE", __FUNCTION__);
}

bool AudioRouteManager::isStarted() const
{
    return _audioPfwConnector && _audioPfwConnector->isStarted();
}

void AudioRouteManager::executeRouting()
{
    executeMuteRoutingStage();

    executeDisableRoutingStage();

    executeConfigureRoutingStage();

    executeEnableRoutingStage();

    executeUnmuteRoutingStage();
}

void AudioRouteManager::resetRouting()
{
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        _routes[i].prevEnabled = _routes[i].enabled;
        _routes[i].enabled = 0;
        _routes[i].needReflow = 0;
        _routes[i].needRepath = 0;
    }

    resetAvailability<AudioRoute>(_routeMap);
    resetAvailability<AudioPort>(_portMap);
}

void AudioRouteManager::addStream(Stream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Failure, invalid stream parameter");

    AutoW lock(_routingLock);
    _streamsList[stream->isOut()].push_back(stream);
}

void AudioRouteManager::removeStream(Stream *streamToRemove)
{
    AUDIOCOMMS_ASSERT(streamToRemove != NULL, "Failure, invalid stream parameter");

    AutoW lock(_routingLock);
    _streamsList[streamToRemove->isOut()].remove(streamToRemove);
}

bool AudioRouteManager::checkAndPrepareRouting()
{
    resetRouting();

    RouteMapIterator it;
    for (it = _routeMap.begin(); it != _routeMap.end(); ++it) {

        AudioRoute *route =  it->second;
        prepareRoute(route);
        setBit(route->needReflow(), route->getId(), _routes[route->isOut()].needReflow);
        setBit(route->needRepath(), route->getId(), _routes[route->isOut()].needRepath);
    }

    return routingHasChanged<Direction::Output>() | routingHasChanged<Direction::Input>();
}

void AudioRouteManager::prepareRoute(AudioRoute *route)
{
    AUDIOCOMMS_ASSERT(route != NULL, "Failure, invalid route parameter");

    bool isApplicable = route->isStreamRoute() ?
                        setStreamForRoute(static_cast<AudioStreamRoute *>(route)) :
                        route->isApplicable();
    route->setUsed(isApplicable);
    setBit(isApplicable, route->getId(), _routes[route->isOut()].enabled);
}

bool AudioRouteManager::setStreamForRoute(AudioStreamRoute *route)
{
    AUDIOCOMMS_ASSERT(route != NULL, "Failure, invalid route parameter");

    bool isOut = route->isOut();

    StreamListIterator it;
    for (it = _streamsList[isOut].begin(); it != _streamsList[isOut].end(); ++it) {

        Stream *stream = *it;
        if (stream->isStarted() && !stream->isNewRouteAvailable()) {

            if (route->isApplicable(stream)) {

                ALOGV("%s: stream route %s is applicable", __FUNCTION__, route->getName().c_str());
                route->setStream(stream);
                return true;
            }
        }
    }

    return false;
}

void AudioRouteManager::executeMuteRoutingStage()
{
    ALOGD("\t\t-%s-", __FUNCTION__);

    _routingStageCriterion->setCriterionState(FlowMask);
    setRouteCriteriaForMute();
    _audioPfwConnector->applyConfigurations();
}

void AudioRouteManager::executeDisableRoutingStage()
{
    ALOGD("\t\t-%s-", __FUNCTION__);

    _routingStageCriterion->setCriterionState(PathMask);
    setRouteCriteriaForDisable();
    doDisableRoutes();
    _audioPfwConnector->applyConfigurations();
    doPostDisableRoutes();
}

void AudioRouteManager::executeConfigureRoutingStage()
{
    ALOGD("\t\t-%s-", __FUNCTION__);

    _routingStageCriterion->setCriterionState(ConfigureMask);

    StreamRouteMapIterator routeIt;
    for (routeIt = _streamRouteMap.begin(); routeIt != _streamRouteMap.end(); ++routeIt) {

        AudioStreamRoute *streamRoute = routeIt->second;
        if (streamRoute->needReflow()) {

            streamRoute->configure();
        }
    }

    setRouteCriteriaForConfigure();

    commitCriteriaAndApply();
}

void AudioRouteManager::executeEnableRoutingStage()
{
    ALOGD("\t\t-%s-", __FUNCTION__);

    _routingStageCriterion->setCriterionState(PathMask | ConfigureMask);
    doPreEnableRoutes();
    _audioPfwConnector->applyConfigurations();
    doEnableRoutes();
}

void AudioRouteManager::executeUnmuteRoutingStage()
{
    ALOGD("\t\t-%s", __FUNCTION__);

    _routingStageCriterion->setCriterionState(ConfigureMask | PathMask | FlowMask);
    _audioPfwConnector->applyConfigurations();
}

void AudioRouteManager::setRouteCriteriaForConfigure()
{
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        _selectedClosingRoutes[i]->setCriterionState(0);
        _selectedOpenedRoutes[i]->setCriterionState(enabledRoutes(i));
    }
}

void AudioRouteManager::setRouteCriteriaForMute()
{
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        uint32_t unmutedRoutes = prevEnabledRoutes(i) & enabledRoutes(i) & ~needReflowRoutes(i);
        uint32_t routesToMute = (prevEnabledRoutes(i) & ~enabledRoutes(i)) | needReflowRoutes(i);

        _selectedClosingRoutes[i]->setCriterionState(routesToMute);
        _selectedOpenedRoutes[i]->setCriterionState(unmutedRoutes);
    }
}

void AudioRouteManager::setRouteCriteriaForDisable()
{
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        uint32_t openedRoutes = prevEnabledRoutes(i) & enabledRoutes(i) & ~needRepathRoutes(i);
        uint32_t routesToDisable = (prevEnabledRoutes(i) & ~enabledRoutes(i)) | needRepathRoutes(i);

        _selectedClosingRoutes[i]->setCriterionState(routesToDisable);
        _selectedOpenedRoutes[i]->setCriterionState(openedRoutes);
    }
}

void AudioRouteManager::doDisableRoutes(bool isPostDisable)
{
    StreamRouteMapIterator it;
    for (it = _streamRouteMap.begin(); it != _streamRouteMap.end(); ++it) {

        AudioStreamRoute *streamRoute = it->second;

        if ((streamRoute->previouslyUsed() && !streamRoute->isUsed()) ||
            streamRoute->needRepath()) {

            ALOGV("%s: Route %s to be disabled", __FUNCTION__, streamRoute->getName().c_str());
            streamRoute->unroute(isPostDisable);
        }
    }
}

void AudioRouteManager::doEnableRoutes(bool isPreEnable)
{
    StreamRouteMapIterator it;
    for (it = _streamRouteMap.begin(); it != _streamRouteMap.end(); ++it) {

        AudioStreamRoute *streamRoute = it->second;

        if ((!streamRoute->previouslyUsed() && streamRoute->isUsed()) ||
            streamRoute->needRepath()) {

            ALOGV("%s: Route %s to be enabled", __FUNCTION__, streamRoute->getName().c_str());
            if (streamRoute->route(isPreEnable) != OK) {

                ALOGE("\t error while routing %s", streamRoute->getName().c_str());
            }
        }
    }
}

template <typename T>
bool AudioRouteManager::addElement(const string &name, uint32_t id, map<string, T *> &elementsMap)
{
    routingElementSupported<T>();
    if (elementsMap.find(name) != elementsMap.end()) {

        ALOGW("%s: element(%s,=%d) already added", __FUNCTION__, name.c_str(), id);
        return false;
    }
    elementsMap[name] = new T(name, id);
    return true;
}

template <typename T>
T *AudioRouteManager::getElement(const string &name, map<string, T *> &elementsMap)
{
    routingElementSupported<T>();
    typename map<string, T *>::iterator it = elementsMap.find(name);
    LOG_ALWAYS_FATAL_IF(it == elementsMap.end(), "Element %s not found", name.c_str());
    return it->second;
}

template <typename T>
T *AudioRouteManager::findElementByName(const std::string &name, map<string, T *> elementsMap)
{
    routingElementSupported<T>();
    typename map<string, T *>::iterator it;
    it = elementsMap.find(name);
    return (it == elementsMap.end()) ? NULL : it->second;
}

template <typename T>
void AudioRouteManager::resetAvailability(map<string, T *> elementsMap)
{
    routingElementSupported<T>();
    typename map<string, T *>::iterator it;
    for (it = elementsMap.begin(); it != elementsMap.end(); ++it) {

        it->second->resetAvailability();
    }
}

bool AudioRouteManager::onEvent(int fd)
{
    return false;
}

bool AudioRouteManager::onError(int fd)
{
    return false;
}

bool AudioRouteManager::onHangup(int fd)
{
    return false;
}

void AudioRouteManager::onAlarm()
{
    ALOGD("%s", __FUNCTION__);
}

void AudioRouteManager::onPollError()
{
}

bool AudioRouteManager::onProcess(uint16_t event)
{
    AutoW lock(_routingLock);
    doReconsiderRouting(event == FORCE_RESYNC);

    // Notify all potential observer of Route Manager Subject
    notify();

    return false;
}

status_t AudioRouteManager::setVoiceVolume(float gain)
{
    AutoR lock(_routingLock);
    string error;
    bool ret;

    if ((gain < 0.0) || (gain > 1.0)) {

        ALOGW("%s(%f) out of range [0.0 .. 1.0]", __FUNCTION__, gain);
        return -ERANGE;
    }

    ALOGD("%s gain=%f", __FUNCTION__, gain);
    CParameterHandle *voiceVolumeHandle = _parameterHelper->getDynamicParameterHandle(_voiceVolume);

    if (!voiceVolumeHandle) {

        ALOGE("Could not retrieve volume path handle");
        return INVALID_OPERATION;
    }

    if (voiceVolumeHandle->isArray()) {
        vector<double> gains;

        gains.push_back(gain);
        gains.push_back(gain);

        ret = voiceVolumeHandle->setAsDoubleArray(gains, error);
    } else {
        ret = voiceVolumeHandle->setAsDouble(gain, error);
    }

    if (!ret) {
        ALOGE("%s: Unable to set value %f, from parameter path: %s, error=%s",
              __FUNCTION__, gain, voiceVolumeHandle->getPath().c_str(), error.c_str());
        return INVALID_OPERATION;
    }
    return OK;
}

Stream *AudioRouteManager::getVoiceOutputStream()
{
    AutoW lock(_routingLock);

    StreamListIterator it;
    // We take the first stream that corresponds to the primary output.
    it = _streamsList[Direction::Output].begin();
    ALOGE_IF(*it == NULL, "%s current stream NOT FOUND for echo ref", __FUNCTION__);
    return *it;
}

const AudioStreamRoute *AudioRouteManager::findMatchingRoute(bool isOut, uint32_t flags) const
{
    uint32_t mask = !flags ?
                    (isOut ? static_cast<uint32_t>(AUDIO_OUTPUT_FLAG_PRIMARY) :
                     static_cast<uint32_t>(BitField::indexToMask(AUDIO_SOURCE_DEFAULT)))
                    : flags;

    StreamRouteMapConstIterator it;
    for (it = _streamRouteMap.begin(); it != _streamRouteMap.end(); ++it) {

        const AudioStreamRoute *streamRoute = it->second;
        if ((streamRoute->isOut() == isOut) && (mask & streamRoute->getApplicableMask())) {

            return streamRoute;
        }
    }
    return NULL;
}

uint32_t AudioRouteManager::getPeriodInUs(bool isOut, uint32_t flags) const
{
    AutoR lock(_routingLock);
    const AudioStreamRoute *route = findMatchingRoute(isOut, flags);
    if (route != NULL) {

        return route->getPeriodInUs();
    }
    ALOGE("%s not route found, audio might not be functional", __FUNCTION__);
    return 0;
}

uint32_t AudioRouteManager::getLatencyInUs(bool isOut, uint32_t flags) const
{
    AutoR lock(_routingLock);
    const AudioStreamRoute *route = findMatchingRoute(isOut, flags);
    if (route != NULL) {

        return route->getLatencyInUs();
    }
    ALOGE("%s not route found, audio might not be functional", __FUNCTION__);
    return 0;
}

void AudioRouteManager::setBit(bool isSet, uint32_t index, uint32_t &mask)
{
    if (isSet) {
        mask |= index;
    }
}

void AudioRouteManager::setRouteApplicable(const string &name, bool isApplicable)
{
    ALOGV("%s: %s", __FUNCTION__, name.c_str());
    AutoW lock(_routingLock);
    getElement<AudioRoute>(name, _routeMap)->setApplicable(isApplicable);
}

void AudioRouteManager::setRouteNeedReconfigure(const string &name, bool needReconfigure)
{
    ALOGV("%s: %s", __FUNCTION__, name.c_str());
    AutoW lock(_routingLock);
    getElement<AudioRoute>(name, _routeMap)->setNeedReconfigure(needReconfigure);
}

void AudioRouteManager::setRouteNeedReroute(const string &name, bool needReroute)
{
    ALOGV("%s: %s", __FUNCTION__, name.c_str());
    AutoW lock(_routingLock);
    getElement<AudioRoute>(name, _routeMap)->setNeedReroute(needReroute);
}

void AudioRouteManager::updateStreamRouteConfig(const string &name,
                                                const StreamRouteConfig &config)
{
    AutoW lock(_routingLock);
    getElement<AudioStreamRoute>(name, _streamRouteMap)->updateStreamRouteConfig(config);
}

void AudioRouteManager::addRouteSupportedEffect(const string &name, const string &effect)
{
    getElement<AudioStreamRoute>(name, _streamRouteMap)->addEffectSupported(effect);
}

void AudioRouteManager::setPortBlocked(const string &name, bool isBlocked)
{
    AutoW lock(_routingLock);
    getElement<AudioPort>(name, _portMap)->setBlocked(isBlocked);
}

template <bool isOut>
bool AudioRouteManager::routingHasChanged()
{
    return (prevEnabledRoutes(isOut) != enabledRoutes(isOut)) ||
           (needReflowRoutes(isOut) != 0) || (needRepathRoutes(isOut) != 0);
}

bool AudioRouteManager::addCriterionType(const std::string &name, bool isInclusive)
{
    CriteriaTypeMapIterator it;
    if ((it = _criterionTypesMap.find(name)) == _criterionTypesMap.end()) {

        ALOGD("%s: adding  %s criterion [%s]", __FUNCTION__, name.c_str(),
              isInclusive ? "inclusive" : "exclusive");
        _criterionTypesMap[name] = new CriterionType(name, isInclusive, _audioPfwConnector);
        return false;
    }
    return true;
}

void AudioRouteManager::addCriterionTypeValuePair(const string &name,
                                                  const string &literal,
                                                  uint32_t value)
{
    AUDIOCOMMS_ASSERT(_criterionTypesMap.find(name) != _criterionTypesMap.end(),
                      "CriterionType " << name.c_str() << "not found");

    CriterionType *criterionType = _criterionTypesMap[name];

    if (criterionType->hasValuePairByName(literal)) {

        ALOGV("%s: value pair already added", __FUNCTION__);
        return;
    }
    ALOGD("%s: appending new value pair (%s,%d)of criterion type %s",
          __FUNCTION__, literal.c_str(), value, name.c_str());
    criterionType->addValuePair(value, literal);
}

void AudioRouteManager::addCriterion(const string &name, const string &criterionTypeName)
{
    ALOGD("%s: name=%s criterionType=%s", __FUNCTION__, name.c_str(), criterionTypeName.c_str());
    AUDIOCOMMS_ASSERT(_criteriaMap.find(name) == _criteriaMap.end(), "Criterion already added");

    // Retrieve criteria Type object
    CriteriaTypeMapIterator it = _criterionTypesMap.find(criterionTypeName);

    AUDIOCOMMS_ASSERT(it != _criterionTypesMap.end(),
                      "type " << criterionTypeName.c_str() << "not found for " << name.c_str() <<
                      " criteria");

    _criteriaMap[name] = new Criterion(name, it->second, _audioPfwConnector);
}

void AudioRouteManager::setCriterion(const std::string &name, uint32_t value)
{
    ALOGV("%s: (%s, %d)", __FUNCTION__, name.c_str(), value);
    AUDIOCOMMS_ASSERT(_criteriaMap.find(name) != _criteriaMap.end(), "Criterion does not exist");
    _criteriaMap[name]->setValue(value);
}

void AudioRouteManager::commitCriteriaAndApply()
{
    CriteriaMapIterator it;
    for (it = _criteriaMap.begin(); it != _criteriaMap.end(); ++it) {

        it->second->setCriterionState();
    }

    _audioPfwConnector->applyConfigurations();
}

template <typename T>
void AudioRouteManager::addRoute(const string &name,
                                 uint32_t routeId,
                                 const string &portSrc,
                                 const string &portDst,
                                 bool isOut,
                                 map<string, T *> &elementsMap)
{
    if (addElement<T>(name, routeId, elementsMap)) {

        T *route = elementsMap[name];
        ALOGD("%s Name=%s, Id=%d, ports used=%s,%s", __FUNCTION__, name.c_str(),
              routeId, portSrc.c_str(), portDst.c_str());
        route->setDirection(isOut);
        if (!portSrc.empty()) {
            route->addPort(findElementByName<AudioPort>(portSrc, _portMap));
        }
        if (!portDst.empty()) {
            route->addPort(findElementByName<AudioPort>(portDst, _portMap));
        }
        if (route->isStreamRoute()) {

            // Stream route must also be added in route list as well.
            AUDIOCOMMS_ASSERT(_routeMap.find(
                                  name) == _routeMap.end(),
                              "Fatal: route already added to route list!");
            _routeMap[name] = route;
        }
    }
}

void AudioRouteManager::addPort(const string &name, uint32_t portId)
{
    ALOGD("%s Name=%s", __FUNCTION__, name.c_str());
    addElement<AudioPort>(name, portId, _portMap);
}

void AudioRouteManager::addPortGroup(const string &name, int32_t groupId, const string &portMember)
{
    ALOGD("%s: GroupName=%s PortMember to add=%s", __FUNCTION__, name.c_str(), portMember.c_str());

    addElement<AudioPortGroup>(name, groupId, _portGroupMap);

    AudioPortGroup *portGroup = _portGroupMap[name];
    AUDIOCOMMS_ASSERT(portGroup != NULL, "Fatal: invalid port group!");

    AudioPort *port = findElementByName<AudioPort>(portMember, _portMap);
    portGroup->addPortToGroup(port);
}
