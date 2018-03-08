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
#pragma once

#include "MixPortConfig.hpp"
#include "AudioPort.hpp"
#include "AudioCapabilities.hpp"
#include <AudioUtils.hpp>
#include <SampleSpec.hpp>
#include <IoStream.hpp>
#include <list>
#include <utils/Errors.h>
#include <Direction.hpp>

namespace intel_audio
{

/**
 * enum route type
 * ROUTE_TYPE_STREAM_CAPTURE:  the type of stream routes for capture
 * ROUTE_TYPE_STREAM_PLAYBACK: the type of stream routes for playback
 * ROUTE_TYPE_BACKEND: the type of backend routes
 * ROUTE_TYPE_NUM: the number of route type
 * ROUTE_TYPE_STREAM_NUM: the number of stream route type
 */
enum RouteType
{
    ROUTE_TYPE_STREAM_CAPTURE,
    ROUTE_TYPE_STREAM_PLAYBACK,
    ROUTE_TYPE_BACKEND,
    ROUTE_TYPE_NUM,
    ROUTE_TYPE_STREAM_NUM = ROUTE_TYPE_BACKEND
};

/**
 * The count of routes of different types.
 */
static uint32_t count[ROUTE_TYPE_NUM] = {
    0, 0, 0
};

/**
 * AudioRoute is the abstraction of an route from
 * device to device or stream to device.
 * The AudioRoute instance is parsed and created
 * from the route element of audio_policy_configuration.xml
 */
class AudioRoute
{
public:
    AudioRoute(std::string name, AudioPorts &sinks, AudioPorts &sources, uint32_t type)
        : mMask(1 << count[type]++), // generate the bit mask of route.
          mIsUsed(false),
          mPreviouslyUsed(false),
          mSinks(sinks),
          mSources(sources),
          mType(type),
          mName(name),
          mIsSelected(false)
    {
    }

    virtual ~AudioRoute() {}



    /**
     * check whether the route includes a mixport. If yes, it is a
     * AudioStreamRoute, otherwise it is AudioBackendRoute
     *
     */
    virtual bool isMixRoute() = 0;

    /**
     * route hook point. It is used to open PCM device for
     * AudioStreamRoute, and isn't used for AudioBackendRoute.
     * Called by the route manager at enable step.
     */
    virtual android::status_t route(bool isPreEnable) { return android::OK; }

    /**
     * unroute hook point. It is used to close PCM device for
     * AudioStreamRoute, and isn't used for AudiobackendRoute.
     * Called by the route manager at disable step.
     */
    virtual void unroute(bool isPostDisable) {}

    /**
     * Reset the availability of the route.
     */
    virtual void resetAvailability() = 0;

    /**
     * get the type of route.
     */
    uint32_t getRouteType() { return mType; }

    /**
     * Sets the route as in use.
     *
     * Calling this API will propagate the in use attribute to the ports belonging to this route.
     *
     */
    void setUsed(bool isUsed) { mIsUsed = isUsed; }

    /**
     * check if the route is used.
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

    void setPreUsed(bool isUsed)
    {
        mPreviouslyUsed = isUsed;
    }

    /**
     * set the route mIsSelected  as isSelect
     *
     * set true, if the route is selected to enable.
     * set false, if the route isn't selected. It means
     * the route is disabled during the reroute.
     *
     */
    void setSelected(bool isSelect)
    {
        mIsSelected = isSelect;
    }

    /**
     * Checks if the route is selected.
     *
     * @return true if the route is selected, false otherwise.
     */

    bool isSelected()
    {
        return mIsSelected;
    }

    inline bool stillUsed() const { return previouslyUsed() && isUsed(); }

    /**
     * Get the bit mask of the route
     * @return mMask
     */
    uint32_t getMask() const { return mMask; }
    /**
     * get the route name
     */
    std::string getName() const { return mName; }
    /**
     * Checks if a route needs to be muted / unmuted.
     *
     * It overrides the need reconfigure of Route Parameter Manager to ensure the route was
     * previously used and will be used after the routing reconsideration.
     *
     * @return true if the route needs reconfiguring, false otherwise.
     */
    virtual bool needReflow() = 0;

    /**
     * Checks if a route needs to be disabled / enabled.
     *
     * It overrides the need reroute of Route Parameter Manager to ensure the route was
     * previously used and will be used after the routing reconsideration.
     *
     * @return true if the route needs rerouting, false otherwise.
     */
    virtual bool needRepath() const = 0;

    /**
     * Load the capabilities of the PCM device of stream route.
     * Backend routes don't need implement, so add the
     * default implementation.
     */
    void loadCapabilities() {}

    /**
     * Reset the capabilities of stream route
     * Backend routes don't need implement, so add the
     * default implementation.
     */
    void resetCapabilities() {}

    /**
     * Get the supported devices of the route.
     * @return devices mask of android
     */
    virtual uint32_t getSupportedDeviceMask() const = 0;
    virtual android::status_t dump(const int fd, int spaces = 0) const = 0;

private:

    /**
     * mMask is the bit mask of the route, it helps for
     * criteria representation and routes calculating.
     */
    uint32_t mMask;
    bool mIsUsed; /**< Route will be used after reconsidering the routing. */
    bool mPreviouslyUsed; /**< Route was used before reconsidering the routing. */
    AudioPorts mSinks;
    AudioPorts mSources;
    uint32_t mType;
    std::string mName;
    bool mIsSelected;
};

} // namespace intel_audio
