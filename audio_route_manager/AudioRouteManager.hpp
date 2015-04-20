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
private:
    typedef std::map<std::string, AudioRoute *>::iterator RouteMapIterator;
    typedef std::map<std::string, AudioRoute *>::const_iterator RouteMapConstIterator;
    typedef std::map<std::string, AudioStreamRoute *>::iterator StreamRouteMapIterator;
    typedef std::map<std::string, AudioStreamRoute *>::const_iterator StreamRouteMapConstIterator;
    typedef std::map<std::string, AudioPort *>::iterator PortMapIterator;
    typedef std::map<std::string, AudioPort *>::const_iterator PortMapConstIterator;
    typedef std::map<std::string, AudioPortGroup *>::iterator PortGroupMapIterator;
    typedef std::map<std::string, AudioPortGroup *>::const_iterator PortGroupMapConstIterator;
    typedef std::list<IoStream *>::iterator StreamListIterator;
    typedef std::list<IoStream *>::const_iterator StreamListConstIterator;
    typedef std::map<std::string, Criterion *>::iterator CriteriaMapIterator;
    typedef std::map<std::string, Criterion *>::const_iterator CriteriaMapConstIterator;
    typedef std::map<std::string, CriterionType *>::iterator CriteriaTypeMapIterator;
    typedef std::map<std::string, CriterionType *>::const_iterator CriteriaTypeMapConstIterator;

public:
    AudioRouteManager();
    virtual ~AudioRouteManager();

private:
    /// From Stream Interface
    virtual android::status_t startService();
    virtual android::status_t stopService();
    virtual void addStream(IoStream *stream);
    virtual void removeStream(IoStream *stream);
    virtual void reconsiderRouting(bool isSynchronous = false);
    virtual android::status_t setVoiceVolume(float gain);
    virtual IoStream *getVoiceOutputStream();
    virtual uint32_t getLatencyInUs(const IoStream *stream) const;
    virtual uint32_t getPeriodInUs(const IoStream *stream) const;
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
        addRoute<AudioRoute>(name, portSrc, portDst, isOut, mRouteMap);
    }

    virtual void addAudioStreamRoute(const std::string &name,
                                     const std::string &portSrc, const std::string &portDst,
                                     bool isOut)
    {
        addRoute<AudioStreamRoute>(name, portSrc, portDst, isOut, mStreamRouteMap);
    }

    virtual void updateStreamRouteConfig(const std::string &name,
                                         const StreamRouteConfig &config);
    virtual void addRouteSupportedEffect(const std::string &name, const std::string &effect);
    virtual void setRouteApplicable(const std::string &name, bool isApplicable);
    virtual void setRouteNeedReconfigure(const std::string &name,
                                         bool needReconfigure);
    virtual void setRouteNeedReroute(const std::string &name,
                                     bool needReroute);
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
     * Add an Audio Route to route manager.
     * Called at audio platform discovery.
     *
     * @tparam T: route type (Audio Route or Audio Stream Route).
     * @param[in] name: route name.
     * @param[in] portSrc: source port used by route, may be null if no protection needed.
     * @param[in] portDst: destination port used by route, may be null if no protection needed.
     * @param[in] isOut: route direction (true for output, false for input).
     * @param[in] elementsMap: list in which this route needs to be added.
     */
    template <typename T>
    void addRoute(const std::string &name,
                  const std::string &portSrc,
                  const std::string &portDst,
                  bool isOut,
                  std::map<std::string, T *> &elementsMap);

    /**
     * Find the most suitable route for a given stream according to its attributes, ie flags,
     * use cases, effects...
     *
     * @param[in] stream for which the matching route request is performed
     *
     * @return valid stream route if found, NULL otherwise.
     */
    const AudioStreamRoute *findMatchingRouteForStream(const IoStream *stream) const;

    /**
     * Sets a bit referred by an index within a mask.
     *
     * @param[in] isSet if true, the bit will be set, if false, nop (bit will not be cleared).
     * @param[in] index bit index to set.
     * @param[in,out] mask in which the bit must be set.
     */
    void setBit(bool isSet, uint32_t index, uint32_t &mask);

    /**
     * Checks if the routing conditions changed in a given direction.
     *
     * @tparam isOut direction of the route to consider.
     *
     * @return  true if previously enabled routes is different from currently enabled routes
     *               or if any route needs to be reconfigured.
     */
    template <bool isOut>
    inline bool routingHasChanged();

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
     * Performs the disabling of the route.
     * It only concerns the action that needs to be done on routes themselves, ie detaching
     * streams, closing alsa devices.
     * Disable Routes that were opened before reconsidering the routing and will be closed after
     * or routes that request to be rerouted.
     *
     * @param[in] bIsPostDisable if set, it indicates that the disable happens after unrouting.
     */
    void doDisableRoutes(bool isPostDisable = false);

    /**
     * Performs the post-disabling of the route.
     * It only concerns the action that needs to be done on routes themselves, ie detaching
     * streams, closing alsa devices. Some platform requires to close stream before unrouting.
     * Behavior is encoded in the route itself.
     *
     */
    inline void doPostDisableRoutes()
    {
        doDisableRoutes(true);
    }

    /**
     * Enable the routes.
     */
    void executeEnableRoutingStage();

    /**
     * Performs the enabling of the routes.
     * It only concerns the action that needs to be done on routes themselves, ie attaching
     * streams, opening alsa devices.
     * Enable Routes that were not enabled and will be enabled after the routing reconsideration
     * or routes that requested to be rerouted.
     *
     * @tparam isOut direction of the routes to disable.
     * @param[in] bIsPreEnable if set, it indicates that the enable happens before routing.
     */
    void doEnableRoutes(bool isPreEnable = false);

    /**
     * Performs the pre-enabling of the routes.
     * It only concerns the action that needs to be done on routes themselves, ie attaching
     * streams, opening alsa devices. Some platform requires to open stream before routing.
     * Behavior is encoded in the route itself.
     */
    inline void doPreEnableRoutes()
    {
        doEnableRoutes(true);
    }

    /**
     * Before routing, prepares the attribute of an audio route.
     *
     * @param[in] route AudioRoute to prepare for routing.
     */
    void prepareRoute(AudioRoute *route);

    /**
     * Find and set a stream for an applicable route.
     * It try to associate a streams that must be started and not already routed, with a stream
     * route according to the applicability mask.
     * This mask depends on the direction of the stream:
     *      -Output stream: output Flags
     *      -Input stream: input source.
     *
     * @param[in] route applicable route to be associated to a stream.
     *
     * @return true if a stream was found and attached to the route, false otherwise.
     */
    bool setStreamForRoute(AudioStreamRoute *route);

    /**
     * Add a routing element referred by its name and id to a map. Routing Elements are ports, port
     * groups, route and stream route. Compile time error generated if called with wrong type.
     *
     * @tparam T type of routing element to add.
     * @param[in] key to be used for indexing the map.
     * @param[in] name of the routing element to add.
     * @param[in] elementsMap maps of routing elements to add to.
     *
     * @return true if added, false otherwise (already added or PFW already started).
     */
    template <typename T>
    bool addElement(const std::string &key,
                    const string &name,
                    std::map<std::string, T *> &elementsMap);

    /**
     * Get a routing element from a map by its name. Routing Elements are ports, port
     * groups, route and stream route. Compile time error generated if called with wrong type.
     *
     * @tparam T type of routing element to search.
     * @param[in] name name of the routing element to find.
     * @param[in] elementsMap maps of routing elements to search into.
     *
     * @return valid pointer on routing element if found, assert if element not found.
     */
    template <typename T>
    T *getElement(const std::string &name, std::map<std::string, T *> &elementsMap);

    /**
     * Find a routing element from a map by its name. Routing Elements are ports, port
     * groups, route and stream route. Compile time error generated if called with wrong type.
     *
     * @tparam T type of routing element to search.
     * @param[in] name name of the routing element to find.
     * @param[in] elementsMap maps of routing elements to search into.
     *
     * @return valid pointer on element if found, NULL otherwise.
     */
    template <typename T>
    T *findElementByName(const std::string &name, std::map<std::string, T *> &elementsMap);

    /**
     * Reset the availability of routing elements belonging to a map. Routing Elements are ports,
     * port, groups, route and stream route. Compile time error generated if called with wrong type.
     *
     * @tparam T type of routing element.
     * @param[in] elementsMap maps of routing elements to search into.
     *
     * @return valid pointer on element if found, NULL otherwise.
     */
    template <typename T>
    void resetAvailability(std::map<std::string, T *> &elementsMap);

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

    /**
     * array of list of streams opened.
     */
    std::list<IoStream *> mStreamsList[Direction::gNbDirections];

    std::map<std::string, AudioRoute *> mRouteMap; /**< map of audio route to manage. */

    std::map<std::string, AudioStreamRoute *> mStreamRouteMap; /**< map of audio stream route. */

    std::map<std::string, AudioPort *> mPortMap; /**< map of audio ports whose state may change. */

    /**
     * map of mutuel exclusive port groups.
     */
    std::map<std::string, AudioPortGroup *> mPortGroupMap;

    CEventThread *mEventThread; /**< worker thread in which routing is running. */

    bool mIsStarted; /**< Started service flag. */

    mutable android::RWLock mRoutingLock; /**< lock to protect the routing. */

    struct
    {
        uint32_t needReflow;  /**< Bitfield of routes that needs to be mute / unmutes. */
        uint32_t needRepath;  /**< Bitfield of routes that needs to be disabled / enabled. */
        uint32_t enabled;     /**< Bitfield of enabled routes. */
        uint32_t prevEnabled; /**< Bitfield of previously enabled routes. */
    } mRoutes[Direction::gNbDirections];

    /**
     * Get the need reflow routes mask.
     *
     * @param[in] isOut direction of the route addressed by the request.
     *
     * @return reflow routes mask in the requested direction.
     */
    inline uint32_t needReflowRoutes(bool isOut) const
    {
        return mRoutes[isOut].needReflow;
    }

    /**
     * Get the need repath routes mask.
     *
     * @param[in] isOut direction of the route addressed by the request.
     *
     * @return repath routes mask in the requested direction.
     */
    inline uint32_t needRepathRoutes(bool isOut) const
    {
        return mRoutes[isOut].needRepath;
    }

    /**
     * Get the enabled routes mask.
     *
     * @param[in] isOut direction of the route addressed by the request.
     *
     * @return enabled routes mask in the requested direction.
     */
    inline uint32_t enabledRoutes(bool isOut) const
    {
        return mRoutes[isOut].enabled;
    }

    /**
     * Get the prevously enabled routes mask.
     *
     * @param[in] isOut direction of the route addressed by the request.
     *
     * @return previously enabled routes mask in the requested direction.
     */
    inline uint32_t prevEnabledRoutes(bool isOut) const
    {
        return mRoutes[isOut].prevEnabled;
    }

    /**
     * provide a compile time error if no specialization is provided for a given type.
     *
     * @tparam T: type of the routingElement. Routing Element supported are:
     *                      - AudioPort
     *                      - AudioPortGroup
     *                      - AudioRoute
     *                      - AudioStreamRoute.
     */
    template <typename T>
    struct routingElementSupported;

    AudioPlatformState *mPlatformState; /**< Platform state handler for Route / Audio PFW. */
};

} // namespace intel_audio
