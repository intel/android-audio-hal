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

#include "AudioRouteManager.hpp"
#include "AudioRouteManagerObserver.hpp"
#include "InterfaceProviderLib.h"
#include "Property.h"
#include <Observer.hpp>
#include <IoStream.hpp>
#include <BitField.hpp>
#include <cutils/bitops.h>
#include <string>
#include <utils/Log.h>

using android::status_t;
using namespace std;
using NInterfaceProvider::CInterfaceProviderImpl;
using audio_comms::utilities::BitField;
using audio_comms::utilities::Direction;

typedef android::RWLock::AutoRLock AutoR;
typedef android::RWLock::AutoWLock AutoW;

namespace intel_audio
{

const char *const AudioRouteManager::mVoiceVolume =
    "/Audio/CONFIGURATION/VOICE_VOLUME_CTRL_PARAMETER";

const char *const AudioRouteManager::mClosingRouteCriterion[Direction::_nbDirections] = {
    "ClosingCaptureRoutes", "ClosingPlaybackRoutes"
};
const char *const AudioRouteManager::mOpenedRouteCriterion[Direction::_nbDirections] = {
    "OpenedCaptureRoutes", "OpenedPlaybackRoutes"
};
const char *const AudioRouteManager::mRouteCriterionType = "RouteType";
const char *const AudioRouteManager::mRoutingStage = "RoutageState";

const char *const AudioRouteManager::mAudioPfwConfFilePropName = "AudioComms.PFW.ConfPath";

const char *const AudioRouteManager::mAudioPfwDefaultConfFileName =
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
const pair<int, const char *> AudioRouteManager::mRoutingStageValuePairs[] = {
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
template <>
struct AudioRouteManager::routingElementSupported<Criterion> {};

AudioRouteManager::AudioRouteManager()
    : mRouteInterface(this),
      mStreamInterface(this),
      mAudioPfwConnectorLogger(new CParameterMgrPlatformConnectorLogger),
      mEventThread(new CEventThread(this)),
      mIsStarted(false)
{
    memset(mRoutes, 0, sizeof(mRoutes[0]) * Direction::_nbDirections);

    /// Connector
    // Fetch the name of the PFW configuration file: this name is stored in an Android property
    // and can be different for each hardware
    string audioPfwConfigurationFilePath = TProperty<string>(mAudioPfwConfFilePropName,
                                                             mAudioPfwDefaultConfFileName);
    ALOGI("%s: audio PFW using configuration file: %s", __FUNCTION__,
          audioPfwConfigurationFilePath.c_str());

    // Actually create the Connector
    mAudioPfwConnector = new CParameterMgrPlatformConnector(audioPfwConfigurationFilePath);

    mParameterHelper = new ParameterMgrHelper(mAudioPfwConnector);

    // Logger
    mAudioPfwConnector->setLogger(mAudioPfwConnectorLogger);
}

AudioRouteManager::~AudioRouteManager()
{
    AutoW lock(mRoutingLock);

    if (mIsStarted) {

        mEventThread->stop();
    }
    delete mEventThread;

    // Delete all routes
    RouteMapIterator it;
    for (it = mRouteMap.begin(); it != mRouteMap.end(); ++it) {

        delete it->second;
    }

    // Unset logger
    mAudioPfwConnector->setLogger(NULL);
    delete mParameterHelper;

    // Remove logger
    delete mAudioPfwConnectorLogger;

    // Remove connector
    delete mAudioPfwConnector;
}

// Interface populate
void AudioRouteManager::getImplementedInterfaces(CInterfaceProviderImpl &interfaceProvider)
{
    interfaceProvider.addInterface(&mRouteInterface);
    interfaceProvider.addInterface(&mStreamInterface);
}

status_t AudioRouteManager::startService()
{
    AutoW lock(mRoutingLock);

    AUDIOCOMMS_ASSERT(mIsStarted != true, "Route Manager service already started!");

    // Routes Criterion Type
    AUDIOCOMMS_ASSERT(mCriterionTypesMap.find(mRouteCriterionType) != mCriterionTypesMap.end(),
                      "Route CriterionType not found");

    CriterionType *routeCriterionType = mCriterionTypesMap[mRouteCriterionType];

    // Route Criteria
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        mSelectedOpenedRoutes[i] = new Criterion(mOpenedRouteCriterion[i],
                                                 routeCriterionType,
                                                 mAudioPfwConnector);
        mSelectedClosingRoutes[i] = new Criterion(mClosingRouteCriterion[i],
                                                  routeCriterionType,
                                                  mAudioPfwConnector);
    }

    CriterionType *routageStageCriterionType = new CriterionType(mRoutingStage, true,
                                                                 mAudioPfwConnector);
    mCriterionTypesMap[mRoutingStage] = routageStageCriterionType;
    routageStageCriterionType->addValuePairs(mRoutingStageValuePairs,
                                             sizeof(mRoutingStageValuePairs) /
                                             sizeof(mRoutingStageValuePairs[0]));
    // Routing stage criterion is initialised to Configure | Path | Flow to apply all pending
    // configuration for init and minimalize cold latency at first playback / capture
    mRoutingStageCriterion = new Criterion(mRoutingStage, routageStageCriterionType,
                                           mAudioPfwConnector, Configure | Path | Flow);
    // Start Event thread
    bool started = mEventThread->start();
    AUDIOCOMMS_ASSERT(started, "failure when starting event thread!");

    // Start PFW
    std::string strError;
    if (!mAudioPfwConnector->start(strError)) {

        ALOGE("parameter-manager start error: %s", strError.c_str());
        mEventThread->stop();
        return android::NO_INIT;
    }

    mIsStarted = true;

    ALOGD("%s: parameter-manager successfully started!", __FUNCTION__);

    return android::OK;
}

void AudioRouteManager::reconsiderRouting(bool isSynchronous)
{
    AutoW lock(mRoutingLock);

    if (!mIsStarted) {
        ALOGW("%s Could not serve this request as Route Manager is not started", __FUNCTION__);
        return;
    }

    AUDIOCOMMS_ASSERT(!mEventThread->inThreadContext(), "Failure: not in correct thread context!");

    if (!isSynchronous) {

        // Trigs the processing of the list
        mEventThread->trig(NULL);
    } else {

        // Create a route manager observer
        AudioRouteManagerObserver obs;

        // Add the observer to the route manager
        addObserver(&obs);

        // Trig the processing of the list
        mEventThread->trig(NULL);

        // Unlock to allow for sem wait
        mRoutingLock.unlock();

        // Wait a notification from the route manager
        obs.waitNotification();

        // Relock
        mRoutingLock.writeLock();

        // Remove the observer from Route Manager
        removeObserver(&obs);
    }
}

void AudioRouteManager::doReconsiderRouting()
{
    if (!checkAndPrepareRouting()) {

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
    return mAudioPfwConnector && mAudioPfwConnector->isStarted();
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
        mRoutes[i].prevEnabled = mRoutes[i].enabled;
        mRoutes[i].enabled = 0;
        mRoutes[i].needReflow = 0;
        mRoutes[i].needRepath = 0;
    }

    resetAvailability<AudioRoute>(mRouteMap);
    resetAvailability<AudioPort>(mPortMap);
}

void AudioRouteManager::addStream(IoStream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Failure, invalid stream parameter");

    AutoW lock(mRoutingLock);
    mStreamsList[stream->isOut()].push_back(stream);
}

void AudioRouteManager::removeStream(IoStream *streamToRemove)
{
    AUDIOCOMMS_ASSERT(streamToRemove != NULL, "Failure, invalid stream parameter");

    AutoW lock(mRoutingLock);
    mStreamsList[streamToRemove->isOut()].remove(streamToRemove);
}

bool AudioRouteManager::checkAndPrepareRouting()
{
    resetRouting();

    RouteMapIterator it;
    for (it = mRouteMap.begin(); it != mRouteMap.end(); ++it) {

        AudioRoute *route =  it->second;
        prepareRoute(route);
        setBit(route->needReflow(), route->getId(), mRoutes[route->isOut()].needReflow);
        setBit(route->needRepath(), route->getId(), mRoutes[route->isOut()].needRepath);
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
    setBit(isApplicable, route->getId(), mRoutes[route->isOut()].enabled);
}

bool AudioRouteManager::setStreamForRoute(AudioStreamRoute *route)
{
    AUDIOCOMMS_ASSERT(route != NULL, "Failure, invalid route parameter");

    bool isOut = route->isOut();

    StreamListIterator it;
    for (it = mStreamsList[isOut].begin(); it != mStreamsList[isOut].end(); ++it) {

        IoStream *stream = *it;
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

    mRoutingStageCriterion->setCriterionState<int32_t>(FlowMask);
    setRouteCriteriaForMute();
    mAudioPfwConnector->applyConfigurations();
}

void AudioRouteManager::executeDisableRoutingStage()
{
    ALOGD("\t\t-%s-", __FUNCTION__);

    mRoutingStageCriterion->setCriterionState<int32_t>(PathMask);
    setRouteCriteriaForDisable();
    doDisableRoutes();
    mAudioPfwConnector->applyConfigurations();
    doPostDisableRoutes();
}

void AudioRouteManager::executeConfigureRoutingStage()
{
    ALOGD("\t\t-%s-", __FUNCTION__);

    mRoutingStageCriterion->setCriterionState<int32_t>(ConfigureMask);

    StreamRouteMapIterator routeIt;
    for (routeIt = mStreamRouteMap.begin(); routeIt != mStreamRouteMap.end(); ++routeIt) {

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

    mRoutingStageCriterion->setCriterionState<int32_t>(PathMask | ConfigureMask);
    doPreEnableRoutes();
    mAudioPfwConnector->applyConfigurations();
    doEnableRoutes();
}

void AudioRouteManager::executeUnmuteRoutingStage()
{
    ALOGD("\t\t-%s", __FUNCTION__);

    mRoutingStageCriterion->setCriterionState<int32_t>(ConfigureMask | PathMask | FlowMask);
    mAudioPfwConnector->applyConfigurations();
}

void AudioRouteManager::setRouteCriteriaForConfigure()
{
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        mSelectedClosingRoutes[i]->setCriterionState<int32_t>(0);
        mSelectedOpenedRoutes[i]->setCriterionState<int32_t>(enabledRoutes(i));
    }
}

void AudioRouteManager::setRouteCriteriaForMute()
{
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        uint32_t unmutedRoutes = prevEnabledRoutes(i) & enabledRoutes(i) & ~needReflowRoutes(i);
        uint32_t routesToMute = (prevEnabledRoutes(i) & ~enabledRoutes(i)) | needReflowRoutes(i);

        mSelectedClosingRoutes[i]->setCriterionState<int32_t>(routesToMute);
        mSelectedOpenedRoutes[i]->setCriterionState<int32_t>(unmutedRoutes);
    }
}

void AudioRouteManager::setRouteCriteriaForDisable()
{
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        uint32_t openedRoutes = prevEnabledRoutes(i) & enabledRoutes(i) & ~needRepathRoutes(i);
        uint32_t routesToDisable = (prevEnabledRoutes(i) & ~enabledRoutes(i)) | needRepathRoutes(i);

        mSelectedClosingRoutes[i]->setCriterionState<int32_t>(routesToDisable);
        mSelectedOpenedRoutes[i]->setCriterionState<int32_t>(openedRoutes);
    }
}

void AudioRouteManager::doDisableRoutes(bool isPostDisable)
{
    StreamRouteMapIterator it;
    for (it = mStreamRouteMap.begin(); it != mStreamRouteMap.end(); ++it) {

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
    for (it = mStreamRouteMap.begin(); it != mStreamRouteMap.end(); ++it) {

        AudioStreamRoute *streamRoute = it->second;

        if ((!streamRoute->previouslyUsed() && streamRoute->isUsed()) ||
            streamRoute->needRepath()) {

            ALOGV("%s: Route %s to be enabled", __FUNCTION__, streamRoute->getName().c_str());
            if (streamRoute->route(isPreEnable) != android::OK) {

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
    AUDIOCOMMS_ASSERT(it != elementsMap.end(), "Element " << name << " not found");
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

bool AudioRouteManager::onEvent(int)
{
    return false;
}

bool AudioRouteManager::onError(int)
{
    return false;
}

bool AudioRouteManager::onHangup(int)
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

bool AudioRouteManager::onProcess(void *, uint32_t)
{
    AutoW lock(mRoutingLock);
    doReconsiderRouting();

    // Notify all potential observer of Route Manager Subject
    notify();

    return false;
}

status_t AudioRouteManager::setVoiceVolume(float gain)
{
    AutoR lock(mRoutingLock);
    string error;
    bool ret;

    if ((gain < 0.0) || (gain > 1.0)) {

        ALOGW("%s(%f) out of range [0.0 .. 1.0]", __FUNCTION__, gain);
        return -ERANGE;
    }

    ALOGD("%s gain=%f", __FUNCTION__, gain);
    CParameterHandle *voiceVolumeHandle = mParameterHelper->getDynamicParameterHandle(mVoiceVolume);

    if (!voiceVolumeHandle) {

        ALOGE("Could not retrieve volume path handle");
        return android::INVALID_OPERATION;
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
        return android::INVALID_OPERATION;
    }
    return android::OK;
}

IoStream *AudioRouteManager::getVoiceOutputStream()
{
    AutoW lock(mRoutingLock);

    StreamListIterator it;
    // We take the first stream that corresponds to the primary output.
    it = mStreamsList[Direction::Output].begin();
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
    for (it = mStreamRouteMap.begin(); it != mStreamRouteMap.end(); ++it) {

        const AudioStreamRoute *streamRoute = it->second;
        if ((streamRoute->isOut() == isOut) && (mask & streamRoute->getApplicableMask())) {

            return streamRoute;
        }
    }
    return NULL;
}

uint32_t AudioRouteManager::getPeriodInUs(bool isOut, uint32_t flags) const
{
    AutoR lock(mRoutingLock);
    const AudioStreamRoute *route = findMatchingRoute(isOut, flags);
    if (route != NULL) {

        return route->getPeriodInUs();
    }
    ALOGE("%s not route found, audio might not be functional", __FUNCTION__);
    return 0;
}

uint32_t AudioRouteManager::getLatencyInUs(bool isOut, uint32_t flags) const
{
    AutoR lock(mRoutingLock);
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
    AutoW lock(mRoutingLock);
    getElement<AudioRoute>(name, mRouteMap)->setApplicable(isApplicable);
}

void AudioRouteManager::setRouteNeedReconfigure(const string &name, bool needReconfigure)
{
    ALOGV("%s: %s", __FUNCTION__, name.c_str());
    AutoW lock(mRoutingLock);
    getElement<AudioRoute>(name, mRouteMap)->setNeedReconfigure(needReconfigure);
}

void AudioRouteManager::setRouteNeedReroute(const string &name, bool needReroute)
{
    ALOGV("%s: %s", __FUNCTION__, name.c_str());
    AutoW lock(mRoutingLock);
    getElement<AudioRoute>(name, mRouteMap)->setNeedReroute(needReroute);
}

void AudioRouteManager::updateStreamRouteConfig(const string &name,
                                                const StreamRouteConfig &config)
{
    AutoW lock(mRoutingLock);
    getElement<AudioStreamRoute>(name, mStreamRouteMap)->updateStreamRouteConfig(config);
}

void AudioRouteManager::addRouteSupportedEffect(const string &name, const string &effect)
{
    getElement<AudioStreamRoute>(name, mStreamRouteMap)->addEffectSupported(effect);
}

void AudioRouteManager::setPortBlocked(const string &name, bool isBlocked)
{
    AutoW lock(mRoutingLock);
    getElement<AudioPort>(name, mPortMap)->setBlocked(isBlocked);
}

template <bool isOut>
bool AudioRouteManager::routingHasChanged()
{
    return (prevEnabledRoutes(isOut) != enabledRoutes(isOut)) ||
           (needReflowRoutes(isOut) != 0) || (needRepathRoutes(isOut) != 0);
}

bool AudioRouteManager::addCriterionType(const std::string &name, bool isInclusive)
{
    AutoW lock(mRoutingLock);
    CriteriaTypeMapIterator it;
    if ((it = mCriterionTypesMap.find(name)) == mCriterionTypesMap.end()) {

        ALOGV("%s: adding  %s criterion [%s]", __FUNCTION__, name.c_str(),
              isInclusive ? "inclusive" : "exclusive");
        mCriterionTypesMap[name] = new CriterionType(name, isInclusive, mAudioPfwConnector);
        return false;
    }
    ALOGV("%s: already added %s criterion [%s]", __FUNCTION__, name.c_str(),
          isInclusive ? "inclusive" : "exclusive");
    return true;
}

void AudioRouteManager::addCriterionTypeValuePair(const string &name,
                                                  const string &literal,
                                                  uint32_t value)
{
    AutoW lock(mRoutingLock);
    AUDIOCOMMS_ASSERT(mCriterionTypesMap.find(name) != mCriterionTypesMap.end(),
                      "CriterionType " << name << "not found");

    CriterionType *criterionType = mCriterionTypesMap[name];

    if (criterionType->hasValuePairByName(literal)) {

        ALOGV("%s: value pair already added", __FUNCTION__);
        return;
    }
    ALOGV("%s: appending new value pair (%s,%d)of criterion type %s",
          __FUNCTION__, literal.c_str(), value, name.c_str());
    criterionType->addValuePair(value, literal);
}

void AudioRouteManager::addCriterion(const string &name, const string &criterionTypeName,
                                     const string &defaultLiteralValue /* = "" */)
{
    AutoW lock(mRoutingLock);
    ALOGV("%s: name=%s criterionType=%s", __FUNCTION__, name.c_str(), criterionTypeName.c_str());
    AUDIOCOMMS_ASSERT(mCriteriaMap.find(name) == mCriteriaMap.end(),
                      "Criterion " << name << "of type " << criterionTypeName << " already added");

    // Retrieve criteria Type object
    CriteriaTypeMapIterator it = mCriterionTypesMap.find(criterionTypeName);

    AUDIOCOMMS_ASSERT(it != mCriterionTypesMap.end(),
                      "type " << criterionTypeName << "not found for " << name << " criterion");

    mCriteriaMap[name] = new Criterion(name, it->second, mAudioPfwConnector, defaultLiteralValue);
}

template <typename T>
bool AudioRouteManager::setAudioCriterion(const std::string &name, const T &value)
{
    AutoW lock(mRoutingLock);
    return getElement<Criterion>(name, mCriteriaMap)->setValue<T>(value);
}

bool AudioRouteManager::getAudioCriterion(const std::string &name, std::string &value) const
{
    AutoR lock(mRoutingLock);
    ALOGV("%s: (%s, %d)", __FUNCTION__, name.c_str(), value);
    CriteriaMapConstIterator it = mCriteriaMap.find(name);
    if (it == mCriteriaMap.end()) {
        ALOGW("%s Criterion %s does not exist", __FUNCTION__, name.c_str());
        return false;
    }
    value = it->second->getFormattedValue();
    return true;
}

template <typename T>
bool AudioRouteManager::setAudioParameter(const std::string &paramPath, const T &value)
{
    AutoW lock(mRoutingLock);
    return ParameterMgrHelper::setParameterValue<T>(mAudioPfwConnector, paramPath, value);
}

template <typename T>
bool AudioRouteManager::getAudioParameter(const std::string &paramPath, T &value) const
{
    AutoW lock(mRoutingLock);
    return ParameterMgrHelper::getParameterValue<T>(mAudioPfwConnector, paramPath, value);
}

void AudioRouteManager::commitCriteriaAndApply()
{
    CriteriaMapIterator it;
    for (it = mCriteriaMap.begin(); it != mCriteriaMap.end(); ++it) {

        it->second->setCriterionState();
    }

    mAudioPfwConnector->applyConfigurations();
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
            route->addPort(findElementByName<AudioPort>(portSrc, mPortMap));
        }
        if (!portDst.empty()) {
            route->addPort(findElementByName<AudioPort>(portDst, mPortMap));
        }
        if (route->isStreamRoute()) {

            // Stream route must also be added in route list as well.
            AUDIOCOMMS_ASSERT(mRouteMap.find(name) == mRouteMap.end(),
                              "Fatal: route " << name << " already added to route list!");
            mRouteMap[name] = route;
        }
    }
}

void AudioRouteManager::addPort(const string &name, uint32_t portId)
{
    ALOGD("%s Name=%s", __FUNCTION__, name.c_str());
    addElement<AudioPort>(name, portId, mPortMap);
}

void AudioRouteManager::addPortGroup(const string &name, int32_t groupId, const string &portMember)
{
    ALOGD("%s: GroupName=%s PortMember to add=%s", __FUNCTION__, name.c_str(), portMember.c_str());

    addElement<AudioPortGroup>(name, groupId, mPortGroupMap);

    AudioPortGroup *portGroup = mPortGroupMap[name];
    AUDIOCOMMS_ASSERT(portGroup != NULL, "Fatal: invalid port group!");

    AudioPort *port = findElementByName<AudioPort>(portMember, mPortMap);
    portGroup->addPortToGroup(port);
}

} // namespace intel_audio
