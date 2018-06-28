/*
 * Copyright (C) 2013-2018 Intel Corporation
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
// #define LOG_NDEBUG 0

#include "AudioRouteManager.hpp"
#include "AudioRouteManagerObserver.hpp"
#include "RouteManagerConfig.hpp"
#include "AudioRouteCollection.hpp"
#include "Serializer.hpp"
#include "RoutingStage.hpp"

#include <AudioPlatformState.hpp>
#include <EventThread.h>
#include <property/Property.hpp>
#include <Observer.hpp>
#include <IoStream.hpp>
#include <BitField.hpp>
#include <cutils/bitops.h>
#include <string>
#include <unistd.h>

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

static const char *gConfigFilePathList[] = {
    "/vendor/etc/", "/system/etc/"
};
static const char *gConfigFileName = "audio_policy_configuration.xml";

static const std::string gVoiceVolume = "/Audio/CONFIGURATION/VOICE_VOLUME_CTRL_PARAMETER";

using namespace android;
using namespace std;
using audio_comms::utilities::BitField;
using audio_comms::utilities::Log;
using audio_comms::utilities::Property;
typedef android::RWLock::AutoRLock AutoR;
typedef android::RWLock::AutoWLock AutoW;

static const std::string gEventType {
    "EVENT_TYPE"
};
static const std::string gRecoverUevent {
    gEventType + "=" + "SST_RECOVERY"
};
static const std::string gCrashUevent {
    gEventType + "=" + "SST_CRASHED"
};

namespace intel_audio
{

const int AudioRouteManager::gUEventMsgMaxLeng = 1024;
const int AudioRouteManager::gSocketBufferDefaultSize = 64 * AudioRouteManager::gUEventMsgMaxLeng;
static const std::string gOpenedRouteCriterion[ROUTE_TYPE_NUM] = {
    "OpenedCaptureRoutes",
    "OpenedPlaybackRoutes",
    "OpenedBackendRoutes"
};
static const std::string gRouteCriterionType[ROUTE_TYPE_NUM] = {
    "RouteCaptureType",
    "RoutePlaybackType",
    "RouteBackendType"
};
static const std::string gRoutingStageCriterion = "RoutageState";

AudioRouteManager::AudioRouteManager()
    : mRoutes(new AudioRouteCollection()),
      mEventThread(new CEventThread(this)),
      mPlatformState(new AudioPlatformState())
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
    // Add UEvent to list of Fd to poll BEFORE starting this event thread.
    if (mUEventFd >= 0) {
        Log::Debug() << __FUNCTION__ << ": UEvent fd added to event thread";
        mEventThread->addOpenedFd(FdFromSstDriver, mUEventFd, true);
    }
    // Load configuration file to populate Criterion types, criteria and rogues.
    Criteria mCriteria;
    CriterionTypes mCriterionTypes;
    RouteManagerConfig config(*mRoutes, mCriteria, mCriterionTypes, mParameters,
                              mPlatformState->getConnector<Audio>());
    RouteSerializer serializer;
    status_t status;
    for (const auto &path : gConfigFilePathList) {
        status = serializer.deserialize((string(path) + string(gConfigFileName)).c_str(), config);
        if (status == OK) {
            break;
        }
    }
    AUDIOCOMMS_ASSERT(status == NO_ERROR, "AudioRouteManager: could not parse any config file");

    mPlatformState->setConfig<Audio>(mCriteria, mCriterionTypes, mParameters);
    for (const auto route : *mRoutes) {
        mPlatformState->addCriterionTypeValuePair<Audio>(gRouteCriterionType[route->
                                                                             getRouteType()],
                                                         route->getName(),
                                                         route->getMask());
    }

    /// Construct the platform state component and start it
    status = mPlatformState->start();
    AUDIOCOMMS_ASSERT(status == NO_ERROR, "AudioRouteManager: could not start Platform State");

    // Now that is setup correctly to ensure the route service, start the event thread!
    bool isStarted = mEventThread->start();
    AUDIOCOMMS_ASSERT(isStarted, "AudioRouteManager: Failed to start event thread");

}

AudioRouteManager::~AudioRouteManager()
{
    // Synchronous stop of the event thread must be called with NOT held lock as pending request
    // may need to be served
    mEventThread->stop();

    AutoW lock(mRoutingLock);
    if (mUEventFd >= 0) {
        mEventThread->closeAndRemoveFd(mUEventFd);
    }
    delete mEventThread;
    delete mPlatformState;
    delete mRoutes;
}


void AudioRouteManager::addStream(IoStream &stream)
{
    AutoW lock(mRoutingLock);
    mRoutes->addStream(stream);
}

void AudioRouteManager::removeStream(IoStream &stream)
{
    AutoW lock(mRoutingLock);
    mRoutes->removeStream(stream);
}

IoStream *AudioRouteManager::getVoiceOutputStream()
{
    AutoR lock(mRoutingLock);
    return mRoutes->getVoiceStreamRoute();
}

template <uint32_t type>
inline const std::string AudioRouteManager::routeMaskToString(uint32_t mask) const
{
    return mPlatformState->getFormattedState<Audio>(gRouteCriterionType[type], mask);
}

void AudioRouteManager::reconsiderRouting(bool isSynchronous)
{
    AutoW lock(mRoutingLock);
    reconsiderRoutingUnsafe(isSynchronous);
}

void AudioRouteManager::reconsiderRoutingUnsafe(bool isSynchronous)
{
    AUDIOCOMMS_ASSERT(
        !mEventThread->inThreadContext(), "Failure: not in correct thread context!");

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
                 << routeMaskToString<ROUTE_TYPE_STREAM_CAPTURE>(mRoutes->prevEnabledRouteMask(
                                                        ROUTE_TYPE_STREAM_CAPTURE))
                 << "\n\t-Previously Enabled Route in Output = "
                 << routeMaskToString<ROUTE_TYPE_STREAM_PLAYBACK>(mRoutes->prevEnabledRouteMask(
                                                         ROUTE_TYPE_STREAM_PLAYBACK))
                 << "\n\t-Selected Route in Input = "
                 << routeMaskToString<ROUTE_TYPE_STREAM_CAPTURE>(mRoutes->enabledRouteMask(
                                                        ROUTE_TYPE_STREAM_CAPTURE))
                 << "\n\t-Selected Route in Output = "
                 << routeMaskToString<ROUTE_TYPE_STREAM_PLAYBACK>(mRoutes->enabledRouteMask(
                                                         ROUTE_TYPE_STREAM_PLAYBACK))
                 << "\n\t-Route that need reconfiguration in Input = "
                 << routeMaskToString<ROUTE_TYPE_STREAM_CAPTURE>(mRoutes->needReflowRouteMask(
                                                        ROUTE_TYPE_STREAM_CAPTURE))
                 << "\n\t-Route that need reconfiguration in Output = "
                 << routeMaskToString<ROUTE_TYPE_STREAM_PLAYBACK>(mRoutes->needReflowRouteMask(
                                                         ROUTE_TYPE_STREAM_PLAYBACK))
                 << "\n\t-Route that need rerouting in Input = "
                 << routeMaskToString<ROUTE_TYPE_STREAM_CAPTURE>(mRoutes->needRepathRouteMask(
                                                        ROUTE_TYPE_STREAM_CAPTURE))
                 << "\n\t-Route that need rerouting in Output = "
                 << routeMaskToString<ROUTE_TYPE_STREAM_PLAYBACK>(mRoutes->needRepathRouteMask(
                                                         ROUTE_TYPE_STREAM_PLAYBACK));
    executeRouting();
    Log::Debug() << __FUNCTION__ << ": DONE";
}

void AudioRouteManager::executeRouting()
{
    if (not mAudioSubsystemAvailable) {
        /** If Audio Subsystem is down, disable all stream route until up and running again.
         * Do not invoque Audio PFW since Alsa plugin not aware of audio subsystem down
         */
        mRoutes->disableRoutes();
        mRoutes->postDisableRoutes();
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
    mRoutes->resetAvailability();
}

bool AudioRouteManager::checkAndPrepareRouting()
{
    resetRouting();
    if (mAudioSubsystemAvailable) {
        mRoutes->prepareRouting();
    }
    return mRoutes->routingHasChanged();
}

void AudioRouteManager::executeMuteRoutingStage()
{
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, FlowMask);
    setRouteCriteriaForMute();
    mPlatformState->commitCriteriaAndApplyConfiguration<Audio>();
}

void AudioRouteManager::executeDisableRoutingStage()
{
    mRoutes->disableRoutes();

    setRouteCriteriaForDisable();

    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, PostPathMask);
    mPlatformState->applyConfiguration<Audio>();

    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, StreamPathMask);
    mPlatformState->applyConfiguration<Audio>();

    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, PathMask);
    mPlatformState->applyConfiguration<Audio>();

    mRoutes->postDisableRoutes();

}

void AudioRouteManager::executeConfigureRoutingStage()
{
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, ConfigureMask);
    setRouteCriteriaForConfigure();
    mPlatformState->applyConfiguration<Audio>();
}

void AudioRouteManager::executeEnableRoutingStage()
{
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion, ConfigureMask | PathMask);

    mRoutes->preEnableRoutes();

    mPlatformState->applyConfiguration<Audio>();

    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion,
                                        ConfigureMask | PathMask | StreamPathMask);
    mPlatformState->applyConfiguration<Audio>();

    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion,
                                        ConfigureMask | PathMask | StreamPathMask | PostPathMask);
    mPlatformState->applyConfiguration<Audio>();

    mRoutes->enableRoutes();
}

void AudioRouteManager::executeUnmuteRoutingStage()
{
    mPlatformState->setCriterion<Audio>(gRoutingStageCriterion,
                                        ConfigureMask | PathMask | StreamPathMask | PostPathMask |
                                        FlowMask);
    mPlatformState->applyConfiguration<Audio>();
}

void AudioRouteManager::setRouteCriteriaForConfigure()
{
    for (uint32_t i = 0; i < ROUTE_TYPE_NUM; i++) {
        mPlatformState->setCriterion<Audio>(gOpenedRouteCriterion[i],
                                            mRoutes->enabledRouteMask(i));
    }
}

void AudioRouteManager::setRouteCriteriaForMute()
{
    for (uint32_t i = 0; i < ROUTE_TYPE_NUM; i++) {
        mPlatformState->setCriterion<Audio>(gOpenedRouteCriterion[i],
                                            mRoutes->unmutedRoutes(i));
    }
}

void AudioRouteManager::setRouteCriteriaForDisable()
{
    for (uint32_t i = 0; i < ROUTE_TYPE_NUM; i++) {
        mPlatformState->setCriterion<Audio>(gOpenedRouteCriterion[i],
                                            mRoutes->openedRoutes(i));
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
        char msg[gUEventMsgMaxLeng + 1] = {
            0
        };
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
    const AudioStreamRoute *route = mRoutes->findMatchingRouteForStream(stream);
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
    const AudioStreamRoute *route = mRoutes->findMatchingRouteForStream(stream);
    if (route == NULL) {
        Log::Error() << __FUNCTION__ << ": no route found for stream with flags=0x" << std::hex
                     << stream.getFlagMask() << ", use case =" << stream.getUseCaseMask();
        return 0;
    }
    return route->getLatencyInUs();
}

bool AudioRouteManager::supportStreamConfig(const IoStream &stream) const
{
    return mRoutes->findMatchingRouteForStream(stream) != nullptr;
}

AudioCapabilities AudioRouteManager::getCapabilities(const IoStream &stream) const
{
    auto streamRoute = mRoutes->findMatchingRouteForStream(stream);
    if (streamRoute != nullptr) {
        return streamRoute->getCapabilities();
    }
    return AudioCapabilities();
}

status_t AudioRouteManager::setParameters(const std::string &keyValuePair, bool isSynchronous)
{
    AutoW lock(mRoutingLock);
    bool hasChanged = false;
    status_t ret = mPlatformState->setParameters(keyValuePair, hasChanged);
    KeyValuePairs pairs(keyValuePair);
    bool isSelect = false;
    for (const auto param : mParameters) {
        status_t st = pairs.get<bool>(param->getKey(), isSelect);
        std::string name = param->getRoute();
        if (st == android::OK && !name.empty()) {
            AudioRoute *route = mRoutes->getRoute(name);
            if (route != NULL) {
                Log::Debug() << __FUNCTION__ << "route:" << route->getName() << "setSelcted:" <<
                    isSelect;
                route->setSelected(isSelect);
            }
        }
    }

    // Inconditionnaly reconsider the routing as even if the parameters are the same, concurrent
    // streams with identical settings may have been stopped/started.
    reconsiderRoutingUnsafe(isSynchronous);

    int device;
    status_t status = pairs.get<int>(AUDIO_PARAMETER_DEVICE_CONNECT, device);
    if (status == android::OK) {
        mRoutes->handleDeviceConnectionState(device, true);
    }
    status = pairs.get<int>(AUDIO_PARAMETER_DEVICE_DISCONNECT, device);
    if (status == android::OK) {
        mRoutes->handleDeviceConnectionState(device, false);
    }
    return ret;
}

std::string AudioRouteManager::getParameters(const std::string &keys) const
{
    AutoR lock(mRoutingLock);
    return mPlatformState->getParameters(keys);
}

android::status_t AudioRouteManager::dump(const int fd, int spaces) const
{
    AutoR lock(mRoutingLock);
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    snprintf(buffer, SIZE, "%*sAudio Route Manager:\n", spaces, "");
    result.append(buffer);

    write(fd, result.string(), result.size());
    mRoutes->dump(fd, spaces + 4);
    return android::OK;
}

} // namespace intel_audio
