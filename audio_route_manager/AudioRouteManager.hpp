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
#pragma once

#include "AudioPort.hpp"
#include "AudioPortGroup.hpp"
#include "AudioStreamRoute.hpp"
#include "StreamRouteCollection.hpp"
#include "StreamRouteFactory.hpp"
#include "RouteCollection.hpp"
#include "ElementCollection.hpp"
#include "AudioRoute.hpp"
#include "EventThread.h"
#include "RoutingStage.hpp"
#include "RouteInterface.hpp"
#include "IStreamInterface.hpp"
#include <AudioPlatformState.hpp>
#include <AudioCommsAssert.hpp>
#include "ParameterMgrPlatformConnector.h"
#include <ParameterMgrHelper.hpp>
#include <Criterion.hpp>
#include <CriterionType.hpp>
#include <Direction.hpp>
#include <Observable.hpp>
#include <EventListener.h>
#include <NonCopyable.hpp>
#include <utils/RWLock.h>
#include <list>
#include <map>
#include <vector>

namespace intel_audio
{

class IoStream;
struct pcm_config;

class AudioRouteManager : public IStreamInterface,
                          public IRouteInterface,
                          private IEventListener,
                          private audio_comms::utilities::Observable,
                          private audio_comms::utilities::NonCopyable
{
public:
    AudioRouteManager();
    virtual ~AudioRouteManager();

private:
    /// From Stream Interface
    virtual android::status_t startService();
    virtual android::status_t stopService();
    virtual void addStream(IoStream &stream)
    {
        AutoW lock(mRoutingLock);
        mStreamRouteMap.addStream(stream);
    }
    virtual void removeStream(IoStream &stream)
    {
        AutoW lock(mRoutingLock);
        mStreamRouteMap.removeStream(stream);
    }

    virtual void reconsiderRouting(bool isSynchronous = false);
    virtual android::status_t setVoiceVolume(float gain);
    virtual IoStream *getVoiceOutputStream()
    {
        AutoR lock(mRoutingLock);
        return mStreamRouteMap.getVoiceStreamRoute();
    }
    virtual uint32_t getLatencyInUs(const IoStream &stream) const;
    virtual uint32_t getPeriodInUs(const IoStream &stream) const;
    virtual bool supportStreamConfig(const IoStream &stream) const;
    virtual AudioCapabilities getCapabilities(const IoStream &stream) const;
    virtual android::status_t setParameters(const std::string &keyValuePair,
                                            bool isSynchronous = false);

    virtual std::string getParameters(const std::string &keys) const;
    virtual void printPlatformFwErrorInfo() const {}

    /// From Route Interface
    virtual void addPort(const std::string &name);
    virtual void addPortGroup(const std::string &name,
                              const std::string &portMember);
    virtual void addAudioRoute(const std::string &name,
                               const std::string &portSrc, const std::string &portDst,
                               bool isOut)
    {
        AudioRoute *route = new AudioRoute(name, isOut);
        if (!addRoute(route, portSrc, portDst, isOut)) {
            delete route;
        }
    }

    virtual void addAudioStreamRoute(const std::string &name,
                                     const std::string &portSrc, const std::string &portDst,
                                     bool isOut)
    {
        AudioStreamRoute *route = StreamRouteFactory::createStreamRoute(name, isOut);
        if (!addRoute(route, portSrc, portDst, isOut)) {
            delete route;
            return;
        }
        mStreamRouteMap.addElement(name + (isOut ? "_Playback" : "_Capture"), route);
    }

    virtual void updateStreamRouteConfig(const std::string &name,
                                         const StreamRouteConfig &config)
    {
        mStreamRouteMap.updateStreamRouteConfig(name, config);
    }

    virtual void addRouteSupportedEffect(const std::string &name, const std::string &effect)
    {
        mStreamRouteMap.addRouteSupportedEffect(name, effect);
    }

    virtual void setRouteApplicable(const std::string &name, bool isApplicable)
    {
        mRouteMap.setRouteApplicable(name, isApplicable);
    }

    virtual void setRouteNeedReconfigure(const std::string &name, bool needReconfigure)
    {
        mRouteMap.setRouteNeedReconfigure(name, needReconfigure);
    }

    virtual void setRouteNeedReroute(const std::string &name, bool needReroute)
    {
        mRouteMap.setRouteNeedReroute(name, needReroute);
    }

    virtual void setPortBlocked(const std::string &name, bool isBlocked);
    virtual bool addAudioCriterionType(const std::string &name, bool isInclusive)
    {
        return mPlatformState->addCriterionType<Audio>(name, isInclusive);
    }
    virtual void addAudioCriterionTypeValuePair(const std::string &name, const std::string &literal,
                                                uint32_t value)
    {
        mPlatformState->addCriterionTypeValuePair<Audio>(name, literal, value);
    }
    virtual void addAudioCriterion(const std::string &name, const std::string &criterionType,
                                   const std::string &defaultLiteralValue = "")
    {
        mPlatformState->addCriterion<Audio>(name, criterionType, defaultLiteralValue);
    }
    virtual void setParameter(const std::string &name, uint32_t value)
    {
        setAudioCriterion(name, value);
    }

    /**
     * Sets an audio parameter manager criterion value. Note that it just stage the value.
     * The criterion will be set only at configure stage.
     *
     * @param[in] name: criterion name.
     * @param[in] value: value to set.
     *
     * @return true if operation successful, false otherwise.
     */
    bool setAudioCriterion(const std::string &name, uint32_t value);

    /**
     * Instantiate an Audio Route.
     * Called at audio platform discovery.
     *
     * @param[in]: route (Audio Route or Audio Stream Route).
     * @param[in] portSrc: source port used by route, may be null if no protection needed.
     * @param[in] portDst: destination port used by route, may be null if no protection needed.
     * @param[in] isOut: route direction (true for output, false for input).
     * @return true if route added to collection, false if already added
     */
    bool addRoute(AudioRoute *route, const std::string &portSrc,  const std::string &portDst,
                  bool isOut);

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

    /**
     * Reset the collections managed by route manager service and delete the platform state.
     */
    void reset();

    /// from IEventListener
    virtual bool onEvent(int);
    virtual bool onError(int);
    virtual bool onHangup(int);
    virtual void onAlarm();
    virtual void onPollError();
    virtual bool onProcess(void *, uint32_t);

    RouteCollection<> mRouteMap; /**< map of audio route to manage. */

    StreamRouteCollection mStreamRouteMap; /**< map of audio stream route. */

    ElementCollection<AudioPort> mPortMap; /**< map of audio ports whose state may change. */

    /**
     * map of mutuel exclusive port groups.
     */
    ElementCollection<AudioPortGroup> mPortGroupMap;

    CEventThread *mEventThread; /**< worker thread in which routing is running. */

    bool mIsStarted; /**< Started service flag. */

    mutable android::RWLock mRoutingLock; /**< lock to protect the routing. */

    AudioPlatformState *mPlatformState; /**< Platform state handler for Route / Audio PFW. */
};

} // namespace intel_audio
