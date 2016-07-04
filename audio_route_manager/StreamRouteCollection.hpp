/*
 * Copyright (C) 2015-2016 Intel Corporation
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

#include "RouteCollection.hpp"
#include "AudioStreamRoute.hpp"
#include <IoStream.hpp>
#include <list>

namespace intel_audio
{

class StreamRouteCollection : public RouteCollection<AudioStreamRoute>
{
public:
    virtual void reset()
    {
        mElements.clear();
    }

    void addStream(IoStream &stream)
    {
        // Note: Priority shall be given to direct stream first when routing streams.
        if (stream.isDirect()) {
            mOrderedStreamList[stream.isOut()].push_front(&stream);
        } else {
            mOrderedStreamList[stream.isOut()].push_back(&stream);
        }
    }

    void removeStream(IoStream &streamToRemove)
    {
        mOrderedStreamList[streamToRemove.isOut()].remove(&streamToRemove);
    }

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
    bool setStreamForRoute(AudioStreamRoute &route)
    {
        for (auto stream : mOrderedStreamList[route.isOut()]) {

            if (stream->isStarted() && stream->isRoutedByPolicy() &&
                    !stream->isNewRouteAvailable()) {
                if (route.isMatchingWithStream(*stream)) {
                    return route.setStream(*stream);
                }
            }
        }
        return false;
    }

    IoStream *getVoiceStreamRoute()
    {
        // We take the first stream that corresponds to the primary output.
        auto it = mOrderedStreamList[Direction::Output].begin();
        if (*it == NULL) {
            audio_comms::utilities::Log::Error() << __FUNCTION__
                                                 << ": current stream NOT FOUND for echo ref";
        }
        return *it;
    }

    void updateStreamRouteConfig(const std::string &name, const StreamRouteConfig &config)
    {
        AudioStreamRoute *route = getElement(name);
        if (!route) {
            audio_comms::utilities::Log::Error() << __FUNCTION__ << ": invalid route " << name;
            return;
        }
        route->updateStreamRouteConfig(config);
    }

    void addRouteSupportedEffect(const std::string &name, const std::string &effect)
    {
        AudioStreamRoute *route = getElement(name);
        if (!route) {
            audio_comms::utilities::Log::Error() << __FUNCTION__ << ": invalid route " << name;
            return;
        }
        route->addEffectSupported(effect);
    }

    void prepareRouting()
    {
        for (auto it : Base::mElements) {
            auto route = it.second;
            // The stream route collection must not only ensure that the route is applicable
            // but also that a stream matches the route.
            if (route && route->isApplicable() && setStreamForRoute(*route)) {
                route->setUsed();
            }
        }
    }

    /**
     * Performs the disabling of the route.
     * It only concerns the action that needs to be done on routes themselves, ie detaching
     * streams, closing alsa devices.
     * Disable Routes that were opened before reconsidering the routing and will be closed after
     * or routes that request to be rerouted.
     *
     * @param[in] bIsPostDisable if set, it indicates that the disable happens after unrouting.
     */
    void disableRoutes(bool isPostDisable = false)
    {
        for (auto it : Base::mElements) {
            auto route = it.second;

            if (route && ((route->previouslyUsed() && !route->isUsed()) || route->needRepath())) {
                audio_comms::utilities::Log::Verbose() << __FUNCTION__
                                                       << ": Route " << route->getName()
                                                       << " to be disabled";
                route->unroute(isPostDisable);
            }
        }
    }

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
    void enableRoutes(bool isPreEnable = false)
    {
        for (auto it : Base::mElements) {
            auto route = it.second;

            if (route && ((!route->previouslyUsed() && route->isUsed()) || route->needRepath())) {
                audio_comms::utilities::Log::Verbose() << __FUNCTION__
                                                       << ": Route" << route->getName()
                                                       << " to be enabled";
                if (route->route(isPreEnable) != android::OK) {
                    audio_comms::utilities::Log::Error() << "\t error while routing "
                                                         << route->getName();
                }
            }
        }
    }

    /**
     * Find the most suitable route for a given stream according to its attributes, ie flags,
     * use cases, effects...
     *
     * @param[in] stream for which the matching route request is performed
     *
     * @return valid stream route if found, NULL otherwise.
     */
    const AudioStreamRoute *findMatchingRouteForStream(const IoStream &stream) const
    {
        for (auto it : Base::mElements) {
            const auto streamRoute = it.second;
            if (streamRoute->isMatchingWithStream(stream)) {
                return streamRoute;
            }
        }
        return NULL;
    }

    /**
     * Handle the change of state of a device to whom it concerns by loading / resetting
     * capabilities of route(s) supporting this device.
     * @param[in] device that has been connected / disconnected
     * @param[in] state of the device.
     */
    void handleDeviceConnectionState(audio_devices_t device, bool isConnected)
    {
        for (auto it : Base::mElements) {
            auto route = it.second;
            if ((route->getSupportedDeviceMask() & device) == device) {
                if (isConnected) {
                    route->loadCapabilities();
                } else {
                    route->resetCapabilities();
                }
            }
        }
    }

    /**
     * Performs the post-disabling of the route.
     * It only concerns the action that needs to be done on routes themselves, ie detaching
     * streams, closing alsa devices. Some platform requires to close stream before unrouting.
     * Behavior is encoded in the route itself.
     *
     */
    inline void postDisableRoutes()
    {
        disableRoutes(true);
    }

    /**
     * Performs the pre-enabling of the routes.
     * It only concerns the action that needs to be done on routes themselves, ie attaching
     * streams, opening alsa devices. Some platform requires to open stream before routing.
     * Behavior is encoded in the route itself.
     */
    inline void preEnableRoutes()
    {
        enableRoutes(true);
    }

    /**
     * array of list of streams opened.
     */
    std::list<IoStream *> mOrderedStreamList[Direction::gNbDirections];
};

} // namespace intel_audio
