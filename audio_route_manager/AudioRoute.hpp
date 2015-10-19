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

#include <NonCopyable.hpp>
#include "RoutingStage.hpp"
#include <bitset>

namespace intel_audio
{

class AudioPort;

class AudioRoute : public audio_comms::utilities::NonCopyable
{

public:

    /**
     * Used as index and to identify a port as source or destination.
     */
    enum Port
    {

        EPortSource = 0,
        EPortDest,

        ENbPorts
    };

    AudioRoute(const std::string &name, bool isOut, uint32_t mask);
    virtual ~AudioRoute();

    /**
     * Add a port.
     *
     * Called at the construction of the audio platform, it sets the port used by this route.
     *
     * @param[in] port Port that this route is using.
     */
    void addPort(intel_audio::AudioPort &port);

    /**
     * Reset the availability of the route.
     */
    virtual void resetAvailability();

    /**
     * Informs if the route is available for use.
     *
     * A route is available if not already used or if not blocked.
     *
     * @return true if the route can be used, false otherwise.
     */
    bool available() const
    {
        return !isBlocked() && !isUsed();
    }

    /**
     * Sets the route as in use.
     *
     * Calling this API will propagate the in use attribute to the ports belonging to this route.
     *
     */
    virtual void setUsed();

    /**
     * Checks if the route is used.
     *
     * @return true if the route is in use, false otherwise.
     */
    bool isUsed() const
    {
        return mIsUsed;
    }

    /**
     * Checks if the route was previously used.
     *
     * Previously used means if the route was in use before the routing
     * reconsideration of the route manager.
     *
     * @return true if the route was in use, false otherwise.
     */
    bool previouslyUsed() const
    {
        return mPreviouslyUsed;
    }

    /**
     * Checks if a route is applicable.
     *
     * It overrides the applicability of Route Parameter Manager to apply the port strategy.
     *
     * @return true if the route is applicable, false otherwise.
     */
    bool isApplicable() const;

    /**
     * Sets a route applicable.
     *
     * This API is intended to be called by the Route Parameter Manager to declare this route
     * applicable according to the platform settings (XML configuration).
     *
     * @param[in] isApplicable Route applicable boolean.
     */
    void setApplicable(bool isApplicable)
    {
        mIsApplicable = isApplicable;
    }

    inline bool stillUsed() const { return previouslyUsed() && isUsed(); }

    /**
     * Checks if a route needs to be muted / unmuted.
     *
     * It overrides the need reconfigure of Route Parameter Manager to ensure the route was
     * previously used and will be used after the routing reconsideration.
     *
     * @return true if the route needs reconfiguring, false otherwise.
     */
    virtual bool needReflow() const
    {
        return stillUsed() && (mRoutingStageRequested.test(Flow) ||
                               mRoutingStageRequested.test(Path));
    }

    /**
     * Checks if a route needs to be disabled / enabled.
     *
     * It overrides the need reroute of Route Parameter Manager to ensure the route was
     * previously used and will be used after the routing reconsideration.
     *
     * @return true if the route needs rerouting, false otherwise.
     */
    virtual bool needRepath() const
    {
        return stillUsed() && mRoutingStageRequested.test(Path);
    }

    /**
     * Sets need reconfigure flag.
     *
     * This API is intended to be called by the Route Parameter Manager to set the need reconfigure
     * flag of this route according to the platform settings (XML configuration).
     *
     * @param[in] needReconfigure boolean to indicate Route needs reconfigure.
     */
    void setNeedReconfigure(bool needReconfigure)
    {
        mRoutingStageRequested.set(Flow, needReconfigure);
    }

    /**
     * Sets need reroute flag.
     *
     * This API is intended to be called by the Route Parameter Manager to set the need reroute
     * flag of this route according to the platform settings (XML configuration).
     *
     * @param[in] needReroute boolean to indicate Route needs reroute.
     */
    void setNeedReroute(bool needReroute)
    {
        mRoutingStageRequested.set(Path, needReroute);
    }

    /**
     * Block this route.
     *
     * Calling this API will prevent from using this route.
     */
    void setBlocked();

    /**
     * Checks whether this route is blocked or not.
     *
     * @return true if the route is blocked, false otherwise.
     */
    bool isBlocked() const
    {
        return mBlocked;
    }

    /**
     * Gets the direction of the route.
     *
     * @return true if the route an output route, false if input route.
     */
    bool isOut() const
    {
        return mIsOut;
    }

    uint32_t getMask() const { return mMask; }

    /**
     * Returns identifier of current routing element
     *
     * @returns string representing the name of the routing element
     */
    const std::string &getName() const { return mName; }

private:
    AudioPort *mPort[ENbPorts]; /**< Route is connected to 2 ports. NULL if no mutual exclusion*/

    bool mBlocked; /**< Tells whether the route is blocked or not. */

    std::string mName; /**< Unique Identifier of a routing element */

    uint32_t mMask; /**< A route is identified with a mask, it helps for criteria representation. */

    bool mIsOut; /**< Tells whether the route is an output or not. */

protected:
    bool mIsUsed; /**< Route will be used after reconsidering the routing. */

    bool mPreviouslyUsed; /**< Route was used before reconsidering the routing. */

    bool mIsApplicable; /**< Route is applicable according to Route Parameter Mgr settings. */

    /**
     * Routing stage(s) requested by Route Parameter Mgr
     * Bitfield definition from RoutingStage enum.
     */
    std::bitset<gNbRoutingStages> mRoutingStageRequested;
};

} // namespace intel_audio
