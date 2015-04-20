/*
 * Copyright (C) 2013-2015 Intel Corporation
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

#define LOG_TAG "RouteManager"

#include "AudioRouteManager.hpp"
#include "AudioRouteManagerObserver.hpp"

#include <property/Property.hpp>
#include <Observer.hpp>
#include <IoStream.hpp>
#include <BitField.hpp>
#include <cutils/bitops.h>
#include <string>

#include <utilities/Log.hpp>

using android::status_t;
using namespace std;
using audio_comms::utilities::BitField;
using audio_comms::utilities::Log;
using audio_comms::utilities::Property;
typedef android::RWLock::AutoRLock AutoR;
typedef android::RWLock::AutoWLock AutoW;


namespace intel_audio
{

template <>
struct AudioRouteManager::routingElementSupported<AudioPort> {};
template <>
struct AudioRouteManager::routingElementSupported<AudioPortGroup> {};
template <>
struct AudioRouteManager::routingElementSupported<AudioRoute> {};
template <>
struct AudioRouteManager::routingElementSupported<AudioStreamRoute> {};

AudioRouteManager::AudioRouteManager()
    : mEventThread(new CEventThread(this)),
      mIsStarted(false),
      mPlatformState(NULL)
{
    memset(mRoutes, 0, sizeof(mRoutes[0]) * Direction::gNbDirections);
}

AudioRouteManager::~AudioRouteManager()
{
    AutoW lock(mRoutingLock);

    if (mIsStarted) {
        // Synchronous stop of the event thread must be called with NOT held lock as pending request
        // may need to be served
        mRoutingLock.unlock();
        stopService();
        mRoutingLock.writeLock();
    }
    delete mEventThread;
}

template <Direction::Values dir>
inline const std::string AudioRouteManager::routeMaskToString(uint32_t mask) const
{
    return mPlatformState->getFormattedState<Audio>(gRouteCriterionType[dir], mask);
}

status_t AudioRouteManager::stopService()
{
    AutoW lock(mRoutingLock);
    Log::Debug() << __FUNCTION__;
    if (mIsStarted) {
        delete mPlatformState;
        mPlatformState = NULL;

        // Delete all routes
        for (RouteMapIterator it = mRouteMap.begin(); it != mRouteMap.end(); ++it) {
            delete it->second;
        }
        mRouteMap.clear();
        mStreamRouteMap.clear();

        // Synchronous stop of the event thread must be called with NOT held lock as pending request
        // may need to be served
        mRoutingLock.unlock();
        mEventThread->stop();
        mRoutingLock.writeLock();
        mIsStarted = false;
    }
    return android::OK;
}

status_t AudioRouteManager::startService()
{
    {
        AutoW lock(mRoutingLock);
        AUDIOCOMMS_ASSERT(mIsStarted != true, "Route Manager service already started!");
        AUDIOCOMMS_ASSERT(mEventThread->start(), "failure when starting event thread!");
        mPlatformState = new AudioPlatformState();
    }

    /// Construct the platform state component and start it
    if (mPlatformState->start() != android::OK) {
        Log::Error() << __FUNCTION__ << ": could not start Platform State";
        delete mPlatformState;
        mPlatformState = NULL;
        return android::NO_INIT;
    }
    Log::Debug() << __FUNCTION__ << ": success";

    AutoW lock(mRoutingLock);
    mIsStarted = true;
    return android::OK;
}

void AudioRouteManager::reconsiderRouting(bool isSynchronous)
{
    AutoW lock(mRoutingLock);
    reconsiderRoutingUnsafe(isSynchronous);
}

void AudioRouteManager::reconsiderRoutingUnsafe(bool isSynchronous)
{
    if (!mIsStarted) {
        Log::Warning() << __FUNCTION__
                       << ": Could not serve this request as Route Manager is not started";
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
        mPlatformState->commitCriteriaAndApplyConfiguration<Audio>();
        return;
    }
    Log::Debug() << __FUNCTION__
                 << ": Route state:"
                 << "\n\t-Previously Enabled Route in Input = "
                 << routeMaskToString<Direction::Input>(prevEnabledRoutes(Direction::Input))
                 << "\n\t-Previously Enabled Route in Output = "
                 << routeMaskToString<Direction::Output>(prevEnabledRoutes(Direction::Output))
                 << "\n\t-Selected Route in Input = "
                 << routeMaskToString<Direction::Input>(enabledRoutes(Direction::Input))
                 << "\n\t-Selected Route in Output = "
                 << routeMaskToString<Direction::Output>(enabledRoutes(Direction::Output))
                 << (needReflowRoutes(Direction::Input) ?
                    "\n\t-Route that need reconfiguration in Input = " +
                    routeMaskToString<Direction::Input>(needReflowRoutes(Direction::Input))
                    : "")
                 << (needReflowRoutes(Direction::Output) ?
                    "\n\t-Route that need reconfiguration in Output = "
                    + routeMaskToString<Direction::Output>(needReflowRoutes(Direction::Output))
                    : "")
                 << (needRepathRoutes(Direction::Input) ?
                    "\n\t-Route that need rerouting in Input = " +
                    routeMaskToString<Direction::Input>(needRepathRoutes(Direction::Input))
                    : "")
                 << (needRepathRoutes(Direction::Output) ?
                    "\n\t-Route that need rerouting in Output = "
                    + routeMaskToString<Direction::Output>(needRepathRoutes(Direction::Output))
                    : "");
    executeRouting();
    Log::Debug() << __FUNCTION__ << ": DONE";
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
    for (uint32_t i = 0; i < Direction::gNbDirections; i++) {
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
        setBit(route->needReflow(), route->getMask(), mRoutes[route->isOut()].needReflow);
        setBit(route->needRepath(), route->getMask(), mRoutes[route->isOut()].needRepath);
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
    setBit(isApplicable, route->getMask(), mRoutes[route->isOut()].enabled);
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
                Log::Verbose() << __FUNCTION__
                               << ": stream route " << route->getName() << " is applicable";
                route->setStream(stream);
                return true;
            }
        }
    }

    return false;
}

void AudioRouteManager::executeMuteRoutingStage()
{
    Log::Debug() << "\t\t-" << __FUNCTION__ << "-";

    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, FlowMask);
    setRouteCriteriaForMute();
    mPlatformState->applyConfiguration<Audio>();
}

void AudioRouteManager::executeDisableRoutingStage()
{
    Log::Debug() << "\t\t-" << __FUNCTION__ << "-";
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, PathMask);
    setRouteCriteriaForDisable();
    doDisableRoutes();
    mPlatformState->applyConfiguration<Audio>();
    doPostDisableRoutes();
}

void AudioRouteManager::executeConfigureRoutingStage()
{
    Log::Debug() << "\t\t-" << __FUNCTION__ << "-";
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, ConfigureMask);
    StreamRouteMapIterator routeIt;
    for (routeIt = mStreamRouteMap.begin(); routeIt != mStreamRouteMap.end(); ++routeIt) {

        AudioStreamRoute *streamRoute = routeIt->second;
        if (streamRoute->needReflow()) {

            streamRoute->configure();
        }
    }

    setRouteCriteriaForConfigure();
    mPlatformState->commitCriteriaAndApplyConfiguration<Audio>();
}

void AudioRouteManager::executeEnableRoutingStage()
{
    Log::Debug() << "\t\t-" << __FUNCTION__ << "-";
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, PathMask | ConfigureMask);
    doPreEnableRoutes();
    mPlatformState->applyConfiguration<Audio>();
    doEnableRoutes();
}

void AudioRouteManager::executeUnmuteRoutingStage()
{
    Log::Debug() << "\t\t-" << __FUNCTION__ << "-";
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion,
                                        ConfigureMask | PathMask | FlowMask);
    mPlatformState->applyConfiguration<Audio>();
}

void AudioRouteManager::setRouteCriteriaForConfigure()
{
    for (uint32_t i = 0; i < Direction::gNbDirections; i++) {
        mPlatformState->setCriterion<Audio>(gClosingRouteCriterion[i], 0);
        mPlatformState->setCriterion<Audio>(gOpenedRouteCriterion[i], enabledRoutes(i));
    }
}

void AudioRouteManager::setRouteCriteriaForMute()
{
    for (uint32_t i = 0; i < Direction::gNbDirections; i++) {

        uint32_t unmutedRoutes = prevEnabledRoutes(i) & enabledRoutes(i) & ~needReflowRoutes(i);
        uint32_t routesToMute = (prevEnabledRoutes(i) & ~enabledRoutes(i)) | needReflowRoutes(i);

        mPlatformState->setCriterion<Audio>(gClosingRouteCriterion[i], routesToMute);
        mPlatformState->setCriterion<Audio>(gOpenedRouteCriterion[i], unmutedRoutes);
    }
}

void AudioRouteManager::setRouteCriteriaForDisable()
{
    for (uint32_t i = 0; i < Direction::gNbDirections; i++) {

        uint32_t openedRoutes = prevEnabledRoutes(i) & enabledRoutes(i) & ~needRepathRoutes(i);
        uint32_t routesToDisable = (prevEnabledRoutes(i) & ~enabledRoutes(i)) | needRepathRoutes(i);

        mPlatformState->setCriterion<Audio>(gClosingRouteCriterion[i], routesToDisable);
        mPlatformState->setCriterion<Audio>(gOpenedRouteCriterion[i], openedRoutes);
    }
}

void AudioRouteManager::doDisableRoutes(bool isPostDisable)
{
    StreamRouteMapIterator it;
    for (it = mStreamRouteMap.begin(); it != mStreamRouteMap.end(); ++it) {

        AudioStreamRoute *streamRoute = it->second;

        if ((streamRoute->previouslyUsed() && !streamRoute->isUsed()) ||
            streamRoute->needRepath()) {
            Log::Verbose() << __FUNCTION__
                           << ": Route " << streamRoute->getName() << " to be disabled";
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
            Log::Verbose() << __FUNCTION__
                           << ": Route" << streamRoute->getName() << " to be enabled";
            if (streamRoute->route(isPreEnable) != android::OK) {
                Log::Error() << "\t error while routing " << streamRoute->getName();
            }
        }
    }
}

template <typename T>
bool AudioRouteManager::addElement(const string &key,
                                   const string &name,
                                   map<string, T *> &elementsMap)
{
    routingElementSupported<T>();
    if (elementsMap.find(key) != elementsMap.end()) {
        Log::Warning() << __FUNCTION__ << ": element(" << key << " already added";
        return false;
    }
    elementsMap[key] = new T(name);
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
T *AudioRouteManager::findElementByName(const std::string &name, map<string, T *> &elementsMap)
{
    routingElementSupported<T>();
    typename map<string, T *>::iterator it;
    it = elementsMap.find(name);
    return (it == elementsMap.end()) ? NULL : it->second;
}

template <typename T>
void AudioRouteManager::resetAvailability(map<string, T *> &elementsMap)
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
    Log::Debug() << __FUNCTION__;
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
        Log::Warning() << __FUNCTION__ << ": (" << gain << ") out of range [0.0 .. 1.0]";
        return -ERANGE;
    }
    Log::Debug() << __FUNCTION__ << ": gain=" << gain;
    CParameterHandle *voiceVolumeHandle =
        mPlatformState->getDynamicParameterHandle<Audio>(gVoiceVolume);

    if (!voiceVolumeHandle) {
        Log::Error() << "Could not retrieve volume path handle";
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
        Log::Error() << __FUNCTION__
                     << ": Unable to set value " << gain
                     << ", from parameter path: " << voiceVolumeHandle->getPath()
                     << ", error=" << error;
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
    if (*it == NULL) {
        Log::Error() << __FUNCTION__ << ": current stream NOT FOUND for echo ref";
    }
    return *it;
}

const AudioStreamRoute *AudioRouteManager::findMatchingRouteForStream(const IoStream *stream) const
{
    StreamRouteMapConstIterator it;
    for (it = mStreamRouteMap.begin(); it != mStreamRouteMap.end(); ++it) {
        const AudioStreamRoute *streamRoute = it->second;
        if (streamRoute->isMatchingWithStream(stream)) {
            return streamRoute;
        }
    }
    return NULL;
}

uint32_t AudioRouteManager::getPeriodInUs(const IoStream *stream) const
{
    AutoR lock(mRoutingLock);
    const AudioStreamRoute *route = findMatchingRouteForStream(stream);
    if (route == NULL) {
        Log::Error() << __FUNCTION__ << ": no route found for stream with flags=0x" << std::hex
                     << stream->getFlagMask() << ", use case =" << stream->getUseCaseMask();
        return 0;
    }
    return route->getPeriodInUs();
}

uint32_t AudioRouteManager::getLatencyInUs(const IoStream *stream) const
{
    AutoR lock(mRoutingLock);
    const AudioStreamRoute *route = findMatchingRouteForStream(stream);
    if (route == NULL) {
        Log::Error() << __FUNCTION__ << ": no route found for stream with flags=0x" << std::hex
                     << stream->getFlagMask() << ", use case =" << stream->getUseCaseMask();
        return 0;
    }
    return route->getLatencyInUs();
}

void AudioRouteManager::setBit(bool isSet, uint32_t index, uint32_t &mask)
{
    if (isSet) {
        mask |= index;
    }
}

void AudioRouteManager::setRouteApplicable(const string &name, bool isApplicable)
{
    Log::Verbose() << __FUNCTION__ << ": " << name;
    AutoW lock(mRoutingLock);
    getElement<AudioRoute>(name, mRouteMap)->setApplicable(isApplicable);
}

void AudioRouteManager::setRouteNeedReconfigure(const string &name, bool needReconfigure)
{
    Log::Verbose() << __FUNCTION__ << ": " << name;
    AutoW lock(mRoutingLock);
    getElement<AudioRoute>(name, mRouteMap)->setNeedReconfigure(needReconfigure);
}

void AudioRouteManager::setRouteNeedReroute(const string &name, bool needReroute)
{
    Log::Verbose() << __FUNCTION__ << ": " << name;
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

bool AudioRouteManager::setAudioCriterion(const std::string &name, uint32_t value)
{
    AutoW lock(mRoutingLock);
    return mPlatformState->stageCriterion<Audio>(name, value);

}

static uint32_t count[Direction::gNbDirections] = {
    0, 0
};

template <typename T>
void AudioRouteManager::addRoute(const string &name,
                                 const string &portSrc,
                                 const string &portDst,
                                 bool isOut,
                                 map<string, T *> &elementsMap)
{
    std::string mapKeyName = name + (isOut ? "_Playback" : "_Capture");

    AutoW lock(mRoutingLock);
    if (addElement<T>(mapKeyName, name, elementsMap)) {
        T *route = elementsMap[mapKeyName];
        Log::Debug() << __FUNCTION__ << ": Name=" << mapKeyName
                     << " ports used= " << portSrc << " ," << portDst;
        route->setDirection(isOut);
        route->setMask(1 << count[isOut]);
        if (!portSrc.empty()) {
            route->addPort(findElementByName<AudioPort>(portSrc, mPortMap));
        }
        if (!portDst.empty()) {
            route->addPort(findElementByName<AudioPort>(portDst, mPortMap));
        }
        if (route->isStreamRoute()) {

            // Stream route must also be added in route list as well.
            AUDIOCOMMS_ASSERT(mRouteMap.find(mapKeyName) == mRouteMap.end(),
                              "Fatal: route " << mapKeyName << " already added to route list!");
            mRouteMap[mapKeyName] = route;
        }
        // Populate Route criterion type value pair for Audio PFW (values provided by Route Plugin)
        mPlatformState->addCriterionTypeValuePair<Audio>(gRouteCriterionType[isOut],
                                                         name,
                                                         1 << count[isOut]++);
    }
}

void AudioRouteManager::addPort(const string &name)
{
    AutoW lock(mRoutingLock);
    Log::Debug() << __FUNCTION__ << ": Name=" << name;
    addElement<AudioPort>(name, name, mPortMap);
}

void AudioRouteManager::addPortGroup(const string &name, const string &portMember)
{
    AutoW lock(mRoutingLock);
    if (addElement<AudioPortGroup>(name, name, mPortGroupMap)) {
        Log::Debug() << __FUNCTION__ << ": Group=" << name << " PortMember to add=" << portMember;
        AudioPortGroup *portGroup = mPortGroupMap[name];
        AUDIOCOMMS_ASSERT(portGroup != NULL, "Fatal: invalid port group!");

        AudioPort *port = findElementByName<AudioPort>(portMember, mPortMap);
        portGroup->addPortToGroup(port);
    }
}

status_t AudioRouteManager::setParameters(const std::string &keyValuePair, bool isSynchronous)
{
    AutoW lock(mRoutingLock);
    bool hasChanged = false;
    status_t ret = mPlatformState->setParameters(keyValuePair, hasChanged);
    if (!hasChanged) {
        return ret;
    }
    reconsiderRoutingUnsafe(isSynchronous);
    return ret;
}

std::string AudioRouteManager::getParameters(const std::string &keys) const
{
    AutoR lock(mRoutingLock);
    return mPlatformState->getParameters(keys);
}

} // namespace intel_audio
