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

AudioRouteManager::AudioRouteManager()
    : mEventThread(new CEventThread(this)),
      mIsStarted(false),
      mPlatformState(NULL)
{
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

        mStreamRouteMap.reset();
        mRouteMap.reset();
        mPortMap.reset();

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
        if (mIsStarted) {
            Log::Warning() << "Route Manager service already started.";
            /* Ignore the start; consider this case is not critical */
            return android::OK;
        }
        if (!mEventThread->start()) {
            Log::Error() << "Failed to start event thread.";
            return android::NO_INIT;
        }
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
    Log::Debug() << __FUNCTION__ << ": Route state:"
         << "\n\t-Previously Enabled Route in Input = "
         << routeMaskToString<Direction::Input>(mRouteMap.prevEnabledRouteMask(Direction::Input))
         << "\n\t-Previously Enabled Route in Output = "
         << routeMaskToString<Direction::Output>(mRouteMap.prevEnabledRouteMask(Direction::Output))
         << "\n\t-Selected Route in Input = "
         << routeMaskToString<Direction::Input>(mRouteMap.enabledRouteMask(Direction::Input))
         << "\n\t-Selected Route in Output = "
         << routeMaskToString<Direction::Output>(mRouteMap.enabledRouteMask(Direction::Output))
         << (mRouteMap.needReflowRouteMask(Direction::Input) ?
            "\n\t-Route that need reconfiguration in Input = " +
            routeMaskToString<Direction::Input>(mRouteMap.needReflowRouteMask(Direction::Input))
            : "")
         << (mRouteMap.needReflowRouteMask(Direction::Output) ?
            "\n\t-Route that need reconfiguration in Output = "
            + routeMaskToString<Direction::Output>(mRouteMap.needReflowRouteMask(Direction::Output))
            : "")
         << (mRouteMap.needRepathRouteMask(Direction::Input) ?
            "\n\t-Route that need rerouting in Input = " +
            routeMaskToString<Direction::Input>(mRouteMap.needRepathRouteMask(Direction::Input))
            : "")
         << (mRouteMap.needRepathRouteMask(Direction::Output) ?
            "\n\t-Route that need rerouting in Output = "
            + routeMaskToString<Direction::Output>(mRouteMap.needRepathRouteMask(Direction::Output))
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
    mRouteMap.resetAvailability();
    mPortMap.resetAvailability();
}

bool AudioRouteManager::checkAndPrepareRouting()
{
    resetRouting();
    mStreamRouteMap.prepareRouting();
    mRouteMap.prepareRouting();
    return mRouteMap.routingHasChanged();
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
    mStreamRouteMap.disableRoutes();
    mPlatformState->applyConfiguration<Audio>();
    mStreamRouteMap.postDisableRoutes();
}

void AudioRouteManager::executeConfigureRoutingStage()
{
    Log::Debug() << "\t\t-" << __FUNCTION__ << "-";
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, ConfigureMask);
    mStreamRouteMap.configureRoutes();
    setRouteCriteriaForConfigure();
    mPlatformState->commitCriteriaAndApplyConfiguration<Audio>();
}

void AudioRouteManager::executeEnableRoutingStage()
{
    Log::Debug() << "\t\t-" << __FUNCTION__ << "-";
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, PathMask | ConfigureMask);
    mStreamRouteMap.preEnableRoutes();
    mPlatformState->applyConfiguration<Audio>();
    mStreamRouteMap.enableRoutes();
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
        Direction::Values dir = static_cast<Direction::Values>(i);
        mPlatformState->setCriterion<Audio>(gClosingRouteCriterion[i], 0);
        mPlatformState->setCriterion<Audio>(gOpenedRouteCriterion[i],
                                            mRouteMap.enabledRouteMask(dir));
    }
}

void AudioRouteManager::setRouteCriteriaForMute()
{
    for (uint32_t i = 0; i < Direction::gNbDirections; i++) {
        Direction::Values dir = static_cast<Direction::Values>(i);
        mPlatformState->setCriterion<Audio>(gClosingRouteCriterion[i], mRouteMap.routesToMute(dir));
        mPlatformState->setCriterion<Audio>(gOpenedRouteCriterion[i], mRouteMap.unmutedRoutes(dir));
    }
}

void AudioRouteManager::setRouteCriteriaForDisable()
{
    for (uint32_t i = 0; i < Direction::gNbDirections; i++) {
        Direction::Values dir = static_cast<Direction::Values>(i);
        mPlatformState->setCriterion<Audio>(gClosingRouteCriterion[i],
                                            mRouteMap.routesToDisable(dir));
        mPlatformState->setCriterion<Audio>(gOpenedRouteCriterion[i], mRouteMap.openedRoutes(dir));
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

uint32_t AudioRouteManager::getPeriodInUs(const IoStream &stream) const
{
    AutoR lock(mRoutingLock);
    const AudioStreamRoute *route = mStreamRouteMap.findMatchingRouteForStream(stream);
    if (route == NULL) {
        Log::Error() << __FUNCTION__ << ": no route found for stream with flags=0x" << std::hex
                     << stream.getFlagMask() << ", use case =" << stream.getUseCaseMask();
        return 0;
    }
    return route->getPeriodInUs();
}

uint32_t AudioRouteManager::getLatencyInUs(const IoStream &stream) const
{
    AutoR lock(mRoutingLock);
    const AudioStreamRoute *route = mStreamRouteMap.findMatchingRouteForStream(stream);
    if (route == NULL) {
        Log::Error() << __FUNCTION__ << ": no route found for stream with flags=0x" << std::hex
                     << stream.getFlagMask() << ", use case =" << stream.getUseCaseMask();
        return 0;
    }
    return route->getLatencyInUs();
}

void AudioRouteManager::setPortBlocked(const string &name, bool isBlocked)
{
    AudioPort *port = mPortMap.getElement(name);
    if (port == NULL) {
        return;
    }
    port->setBlocked(isBlocked);
}

bool AudioRouteManager::setAudioCriterion(const std::string &name, uint32_t value)
{
    return mPlatformState->stageCriterion<Audio>(name, value);
}

static uint32_t count[Direction::gNbDirections] = {
    0, 0
};

void AudioRouteManager::addPort(const string &name)
{
    Log::Debug() << __FUNCTION__ << ": Name=" << name;
    AudioPort *port = new AudioPort(name);
    if (!mPortMap.addElement(name, port)) {
        delete port;
    }
}

void AudioRouteManager::addPortGroup(const string &name, const string &portMember)
{
    AudioPortGroup *portGroup = new AudioPortGroup(name);
    if (!mPortGroupMap.addElement(name, portGroup)) {
        delete portGroup;
        return;
    }

    if (portGroup != NULL) {
        Log::Debug() << __FUNCTION__ << ": Group=" << name << " PortMember to add=" << portMember;

        AudioPort *port = mPortMap.getElement(portMember);
        if (port != NULL) {
            portGroup->addPortToGroup(*port);
        }
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

template <typename T>
T *AudioRouteManager::instantiateRoute(const string &name, const string &portSrc,
                                       const string &portDst, bool isOut)
{
    T *route = new T(name, isOut, 1 << count[isOut]);
    if (!portSrc.empty()) {
        route->addPort(*mPortMap.getElement(portSrc));
    }
    if (!portDst.empty()) {
        route->addPort(*mPortMap.getElement(portDst));
    }
    // Populate Route criterion type value pair for Audio PFW (values provided by Route Plugin)
    mPlatformState->addCriterionTypeValuePair<Audio>(gRouteCriterionType[isOut], name,
                                                     1 << count[isOut]++);
    return route;
}

} // namespace intel_audio
