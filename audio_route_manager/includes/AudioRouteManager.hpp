/*
 * Copyright (C) 2013-2017 Intel Corporation
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
#pragma once

#include "AudioCapabilities.hpp"
#include <AudioCommsAssert.hpp>
#include <Direction.hpp>
#include <Observable.hpp>
#include <EventListener.h>
#include <AudioNonCopyable.hpp>
#include <utils/RWLock.h>
#include <list>
#include <map>
#include <vector>

class CEventThread;

namespace intel_audio
{

class IoStream;
struct pcm_config;
class AudioPlatformState;
class StreamRouteCollection;

class AudioRouteManager : private IEventListener,
                          private audio_comms::utilities::Observable,
                          private audio_comms::utilities::NonCopyable
{
public:
    AudioRouteManager();
    virtual ~AudioRouteManager();

    /**
     * Adds a streams to the list of opened streams.
     *
     * @param[in] stream: opened stream to be appended.
     */
    void addStream(IoStream &stream);

    /**
     * Removes a streams from the list of opened streams.
     *
     * @param[in] stream: closed stream to be removed.
     */
    void removeStream(IoStream &stream);

    /**
     * Trigs a routing reconsideration.
     *
     * @param[in] synchronous: if set, re routing shall be synchronous.
     */
    void reconsiderRouting(bool isSynchronous = false);

    /**
     * Sets the voice volume.
     * Called from AudioSystem/Policy to apply the volume on the voice call stream which is
     * platform dependent.
     *
     * @param[in] gain the volume to set in float format in the expected range [0 .. 1.0]
     *                 Note that any attempt to set a value outside this range will return -ERANGE.
     *
     * @return OK if success, error code otherwise.
     */
    android::status_t setVoiceVolume(float gain);

    /**
     * Get the voice output stream.
     * The purpose of this function is
     * - to retrieve the instance of the voice output stream in order to write playback frames
     * as echo reference for
     *
     * @return voice output stream
     */
    IoStream *getVoiceOutputStream();

    /**
     * Get the latency.
     * Upon creation, streams need to provide latency. As stream are not attached
     * to any route at creation, they must get a latency dependant of the
     * platform to provide information of latency and buffersize (inferred from ALSA ring buffer).
     * This latency depends on the route that will be assigned to the stream. The route manager
     * will return the route matching those stream attributes. If no route is found, it returns 0.
     *
     * @param[in] stream requesting the latency
     *
     * @return latency in microseconds
     */
    uint32_t getLatencyInUs(const IoStream &stream) const;

    /**
     * Get the period size.
     * Upon creation, streams need to provide buffer size. As stream are not attached
     * to any route at creation, they must get a latency dependant of the
     * platform to provide information of latency and buffersize (inferred from ALSA ring buffer).
     * This period depends on the route that will be assigned to the stream. The route manager
     * will return the route matching those stream attributes. If no route is found, it returns 0.
     *
     * @param[in] stream requesting the period
     *
     * @return period size in microseconds
     */
    uint32_t getPeriodInUs(const IoStream &stream) const;

    /**
     * Checks whether the stream and its audio configuration that it wishes to use match
     * with a stream route.
     *
     * @param[in] stream to be checked for support
     *
     * @return true if the audio configuration of the stream is supported by a route,
     *              false otherwise.
     */
    bool supportStreamConfig(const IoStream &stream) const;


    /**
     * Retrieve the capabilities for a given stream, i.e. what is the list of sample rates, formats
     * and channel masks allowed for a given stream, i.e. for a request to play / capture from a
     * given device, during a given use case (aka input source, not defined for output), in a given
     * manneer (aka with input or output flags).
     *
     * @param[in] stream for which the capabilities are requested
     *
     * @return capabilities supported for this stream.
     */
    AudioCapabilities getCapabilities(const IoStream &stream) const;

    android::status_t setParameters(const std::string &keyValuePair,
                                    bool isSynchronous = false);

    std::string getParameters(const std::string &keys) const;

    /**
     * Print debug information from target debug files
     */
    void printPlatformFwErrorInfo() const {}

    android::status_t dump(const int  fd, int spaces = 0) const;

private:
    /**
     * From worker thread context
     * This function requests to evaluate the routing for all the streams
     * after a mode change, a modem event ...
     */
    void doReconsiderRouting();

    /**
     * Trigs a routing reconsideration. Must be called with Routing Lock held in W Mode
     *
     * @param[in] synchronous: if set, re routing shall be synchronous.
     */
    void reconsiderRoutingUnsafe(bool isSynchronous = false);

    /**
     *
     * Returns true if the routing scheme has changed, false otherwise.
     *
     * This function override the applicability evaluation of the MetaPFW
     * since further checks are required:
     *     -Ports used strategy and mutual exclusive port
     *     -Streams route are considered as enabled only if a started stream is applicable on it.
     *
     * Returns true if previous enabled route is different from current enabled route
     *              or if any route needs to be reconfigured.
     *         false otherwise (the list of enabled route did not change, no route
     *              needs to be reconfigured).
     *
     */
    bool checkAndPrepareRouting();

    /**
     * Execute 5-steps routing.
     */
    void executeRouting();

    /**
     * Mute the routes.
     * Mute action will be applied on route pointed by ClosingRoutes criterion.
     */
    void executeMuteRoutingStage();

    /**
     * Computes the PFW route criteria to mute the routes.
     * It sets the ClosingRoutes criteria as the routes that were opened before reconsidering
     * the routing, and either will be closed or need reconfiguration.
     * It sets the OpenedRoutes criteria as the routes that were opened before reconsidering the
     * routing, and will remain enabled and do not need to be reconfigured.
     */
    void setRouteCriteriaForMute();

    /**
     * Unmute the routes
     */
    void executeUnmuteRoutingStage();

    /**
     * Performs the configuration of the routes.
     * Change here the devices, the mode, ... all the criteria required for the routing.
     */
    void executeConfigureRoutingStage();

    /**
     * Computes the PFW route criteria to configure the routes.
     * It sets the OpenedRoutes criteria: Routes that will be opened after reconsidering the routing
     * and ClosingRoutes criteria: reset here, as no more closing action required.
     */
    void setRouteCriteriaForConfigure();

    /**
     * Disable the routes
     */
    void executeDisableRoutingStage();

    /**
     * Computes the PFW route criteria to disable the routes.
     * It appends to the closing route criterion the route that were opened before the routing
     * reconsideration and that will not be used anymore after. It also removes from opened routes
     * criterion these routes.
     *
     */
    void setRouteCriteriaForDisable();

    /**
     * Enable the routes.
     */
    void executeEnableRoutingStage();

    /**
     * Returns the formatted state of the route criterion according to the mask.
     *
     * @tparam dir Direction of the route for which the translation is requested
     * @param[in] mask of the route(s) to convert into a formatted state.
     *
     * @return litteral values of the route separated by a "|".
     */
    template <Direction::Values dir>
    inline const std::string routeMaskToString(uint32_t mask) const;

    /**
     * Reset the routing conditions.
     * It backup the enabled routes, resets the route criteria, resets the needReconfigure flags,
     * resets the availability of the routes and ports.
     */
    void resetRouting();

    /// from IEventListener
    virtual bool onEvent(int);
    virtual bool onError(int);
    virtual bool onHangup(int);
    virtual void onAlarm();
    virtual void onPollError();
    virtual bool onProcess(void *, uint32_t);

    StreamRouteCollection *mStreamRoutes; /**< map of audio stream route. */


    CEventThread *mEventThread; /**< worker thread in which routing is running. */

    mutable android::RWLock mRoutingLock; /**< lock to protect the routing. */

    AudioPlatformState *mPlatformState; /**< Platform state handler for Route / Audio PFW. */

    /**Socket Id enumerator */
    enum UeventSockDesc
    {
        FdFromSstDriver,
        NbSockDesc
    };

    /** Uevent Socket Fd */
    int mUEventFd;

    /** Uevent message max length */
    static const int gUEventMsgMaxLeng;

    /** Socket buffer default size (64K) */
    static const int gSocketBufferDefaultSize;

    bool mAudioSubsystemAvailable = true;
};

} // namespace intel_audio
