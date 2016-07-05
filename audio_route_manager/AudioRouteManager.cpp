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

// #define EMULATE_UEVENT
#ifdef EMULATE_UEVENT
#include <cutils/sockets.h>
#include <sys/socket.h>

static const uint8_t RECOVER = 8;
static const uint8_t CRASH = 9;
static const char *const uevent_socket_name = "uevent_emulation";

#else
#include <cutils/uevent.h>
#endif

using android::status_t;
using namespace std;
using audio_comms::utilities::BitField;
using audio_comms::utilities::Log;
using audio_comms::utilities::Property;
typedef android::RWLock::AutoRLock AutoR;
typedef android::RWLock::AutoWLock AutoW;

static const std::string gEventType{"EVENT_TYPE"};
static const std::string gRecoverUevent{gEventType + "=" + "SST_RECOVERY"};
static const std::string gCrashUevent{gEventType + "=" + "SST_CRASHED"};

namespace intel_audio
{
const int AudioRouteManager::gUEventMsgMaxLeng = 1024;
const int AudioRouteManager::gSocketBufferDefaultSize = 64 * AudioRouteManager::gUEventMsgMaxLeng;

AudioRouteManager::AudioRouteManager()
    : mEventThread(new CEventThread(this)),
      mIsStarted(false),
      mPlatformState(NULL)
{
#ifdef EMULATE_UEVENT
    mUEventFd = socket_local_server(uevent_socket_name, ANDROID_SOCKET_NAMESPACE_ABSTRACT,
                                    SOCK_STREAM);
    if (mUEventFd == -1) {
        Log::Error() << __FUNCTION__ << "socket_local_server connection to uevent_emulation failed";
    }
#else
    mUEventFd = uevent_open_socket(gSocketBufferDefaultSize, true);
    if (mUEventFd < 0) {
        Log::Error() << __FUNCTION__ << "uevent_open_socket failed, recovery will not work";
    }
#endif
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
        if (mUEventFd >= 0) {
            mEventThread->closeAndRemoveFd(mUEventFd);
        }
    }
    delete mEventThread;
}

template <Direction::Values dir>
inline const std::string AudioRouteManager::routeMaskToString(uint32_t mask) const
{
    return mPlatformState->getFormattedState<Audio>(gRouteCriterionType[dir], mask);
}

void AudioRouteManager::reset()
{
    delete mPlatformState;
    mPlatformState = NULL;

    mStreamRouteMap.reset();
    mRouteMap.reset();
    mPortMap.reset();
}

status_t AudioRouteManager::stopService()
{
    AutoW lock(mRoutingLock);
    Log::Debug() << __FUNCTION__;
    if (mIsStarted) {
        // Synchronous stop of the event thread must be called with NOT held lock as pending request
        // may need to be served (Event Thread waiting to acquire the lock.
        mRoutingLock.unlock();
        mEventThread->stop();
        mRoutingLock.writeLock();

        reset();
        mIsStarted = false;
        if (mUEventFd >= 0) {
            mEventThread->closeAndRemoveFd(mUEventFd);
        }
    }
    return android::OK;
}

status_t AudioRouteManager::startService()
{
    {
        AutoW lock(mRoutingLock);

        // Add UEvent to list of Fd to poll BEFORE starting this event thread.
        if (mUEventFd >= 0) {
            Log::Debug() << __FUNCTION__ << ": UEvent fd added to event thread";
            mEventThread->addOpenedFd(FdFromSstDriver, mUEventFd, true);
        }

        if (mIsStarted) {
            Log::Warning() << "Route Manager service already started.";
            /* Ignore the start; consider this case is not critical */
            return android::OK;
        }
        mPlatformState = new AudioPlatformState();
    }

    /// Construct the platform state component and start it
    if (mPlatformState->start() != android::OK) {
        Log::Error() << __FUNCTION__ << ": could not start Platform State";
        reset();
        return android::NO_INIT;
    }

    AutoW lock(mRoutingLock);
    // Now that is setup correctly to ensure the route service, start the event thread!
    if (!mEventThread->start()) {
        Log::Error() << "Failed to start event thread.";
        reset();
        return android::NO_INIT;
    }
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
        // Note that system Audio PFW Alsa plugin is not aware of availability of audio subsystem,
        // we prevent to use it while audio subsystem is down.
        if (mAudioSubsystemAvailable) {
            mPlatformState->commitCriteriaAndApplyConfiguration<Audio>();
        }
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
    if (not mAudioSubsystemAvailable) {
        /** If Audio Subsystem is down, disable all stream route until up and running again.
         * Do not invoque Audio PFW since Alsa plugin not aware of audio subsystem down
         */
        mStreamRouteMap.disableRoutes();
        mStreamRouteMap.postDisableRoutes();
        return;
    }
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
    if (mAudioSubsystemAvailable) {
        mStreamRouteMap.prepareRouting();
        mRouteMap.prepareRouting();
    }
    return mRouteMap.routingHasChanged();
}

void AudioRouteManager::executeMuteRoutingStage()
{
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, FlowMask);
    setRouteCriteriaForMute();
    mPlatformState->applyConfiguration<Audio>();
}

void AudioRouteManager::executeDisableRoutingStage()
{
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, PathMask);
    setRouteCriteriaForDisable();
    mStreamRouteMap.disableRoutes();
    mPlatformState->applyConfiguration<Audio>();
    mStreamRouteMap.postDisableRoutes();
}

void AudioRouteManager::executeConfigureRoutingStage()
{
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, ConfigureMask);
    setRouteCriteriaForConfigure();
    mPlatformState->commitCriteriaAndApplyConfiguration<Audio>();
}

void AudioRouteManager::executeEnableRoutingStage()
{
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, PathMask | ConfigureMask);
    mStreamRouteMap.preEnableRoutes();
    mPlatformState->applyConfiguration<Audio>();
    mStreamRouteMap.enableRoutes();
}

void AudioRouteManager::executeUnmuteRoutingStage()
{
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

bool AudioRouteManager::onEvent(int fd)
{
    if (fd == mEventThread->getFd(FdFromSstDriver)) {
        bool audioSubsystemAvailable = mAudioSubsystemAvailable;
#ifdef EMULATE_UEVENT
        int clientSocketFd = accept(mUEventFd, NULL, NULL);
        if (clientSocketFd < 0) {
            Log::Error() << __FUNCTION__ << ": accept failed";
            return false;
        }
        uint8_t data;
        uint8_t *pData = &data;
        uint32_t size = sizeof(data);
        while (size) {
            int32_t accessedSize = ::recv(clientSocketFd, pData, size, MSG_NOSIGNAL);
            if (!accessedSize || accessedSize == -1) {
                return false;
            }
            size -= accessedSize;
            pData += accessedSize;
        }
        if (data == RECOVER) {
            Log::Debug() << __FUNCTION__ << ": Audio Subsystem Up and Running again :-)";
            audioSubsystemAvailable = true;
        } else if (data == CRASH) {
            Log::Debug() << __FUNCTION__ << ": Audio Subsystem down :-(";
            audioSubsystemAvailable = false;
        } else {
            Log::Debug() << __FUNCTION__ << ": Unrecognized message...";
            return false;
        }
#else
        char msg[gUEventMsgMaxLeng +1] = {0};
        char *cp;
        int n;

        n = uevent_kernel_multicast_recv(mUEventFd, msg, gUEventMsgMaxLeng);
        if (n <= 0 || n > gUEventMsgMaxLeng) {
            return false;
        }
        msg[n] = '\0';
        cp = msg;
        while (cp < msg + n) {
            if (!strcmp(cp, gRecoverUevent.c_str())) {
                Log::Warning() << __FUNCTION__ << ": Audio Subsystem Up and Running again :-)";
                audioSubsystemAvailable = true;
                break;
            } else if (!strcmp(cp, gCrashUevent.c_str())) {
                Log::Warning() << __FUNCTION__ << ": Audio Subsystem down :-(";
                audioSubsystemAvailable = false;
                break;
            }
            cp += strlen(cp) + 1;
        }
#endif
        AutoW lock(mRoutingLock);
        if (audioSubsystemAvailable != mAudioSubsystemAvailable) {
            mAudioSubsystemAvailable = audioSubsystemAvailable;
            doReconsiderRouting();
        }
    }
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

bool AudioRouteManager::supportStreamConfig(const IoStream &stream) const
{
    return mStreamRouteMap.findMatchingRouteForStream(stream) != nullptr;
}

AudioCapabilities AudioRouteManager::getCapabilities(const IoStream &stream) const
{
    auto streamRoute = mStreamRouteMap.findMatchingRouteForStream(stream);
    if (streamRoute != nullptr) {
        return streamRoute->getCapabilities();
    }
    return AudioCapabilities();
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
    if (hasChanged) {
        reconsiderRoutingUnsafe(isSynchronous);
    }
    KeyValuePairs pairs(keyValuePair);
    int device;
    status_t status = pairs.get<int>(AUDIO_PARAMETER_DEVICE_CONNECT, device);
    if (status == android::OK) {
        mStreamRouteMap.handleDeviceConnectionState(device, true);
    }
    status = pairs.get<int>(AUDIO_PARAMETER_DEVICE_DISCONNECT, device);
    if (status == android::OK) {
        mStreamRouteMap.handleDeviceConnectionState(device, false);
    }
    return ret;
}

std::string AudioRouteManager::getParameters(const std::string &keys) const
{
    AutoR lock(mRoutingLock);
    return mPlatformState->getParameters(keys);
}

bool AudioRouteManager::addRoute(AudioRoute *route, const string &portSrc, const string &portDst,
                                 bool isOut)
{
    if (route == nullptr) {
        return false;
    }
    std::string mapKeyName = route->getName() + (isOut ? "_Playback" : "_Capture");
    if (mRouteMap.getElement(mapKeyName) != NULL) {
        Log::Verbose() << __FUNCTION__ << ": route "
                       << mapKeyName << " already added to route list!";
        return false;
    }
    if (!portSrc.empty()) {
        route->addPort(*mPortMap.getElement(portSrc));
    }
    if (!portDst.empty()) {
        route->addPort(*mPortMap.getElement(portDst));
    }
    // Populate Route criterion type value pair for Audio PFW (values provided by Route Plugin)
    mPlatformState->addCriterionTypeValuePair<Audio>(gRouteCriterionType[isOut], route->getName(),
                                                     route->getMask());
    mRouteMap.addElement(mapKeyName, route);
    return true;
}

} // namespace intel_audio
