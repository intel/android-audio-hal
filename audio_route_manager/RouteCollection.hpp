/*
 * Copyright (C) 2015 Intel Corporation
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

#include "ElementCollection.hpp"
#include "AudioRoute.hpp"

namespace intel_audio
{

template <class RouteType = AudioRoute>
class RouteCollection : public ElementCollection<RouteType>
{
protected:
    typedef ElementCollection<RouteType> Base;

public:
    virtual void resetAvailability()
    {
        for (uint32_t i = 0; i < Direction::gNbDirections; i++) {
            mRoutes[i].reset();
        }
        Base::resetAvailability();
    }

    void prepareRouting()
    {
        for (auto it : Base::mElements) {
            auto route = it.second;
            if (!route) {
                continue;
            }
            if (route->isApplicable()) {
                route->setUsed();
            }
            if (route->isUsed()) {
                mRoutes[route->isOut()].setEnabledRoute(route->getMask());
                if (route->needReflow())
                    mRoutes[route->isOut()].setNeedReflowRoute(route->getMask());
                if (route->needRepath())
                    mRoutes[route->isOut()].setNeedRepathRoute(route->getMask());
            }
        }
    }

    bool routingHasChanged() const
    {
        return mRoutes[Direction::Output].routingHasChanged()
               || mRoutes[Direction::Input].routingHasChanged();
    }

    void setRouteApplicable(const std::string &name, bool isApplicable)
    {
        AudioRoute *route = Base::getElement(name);
        if (!route) {
            audio_comms::utilities::Log::Error() << __FUNCTION__ << ": invalid route " << name;
            return;
        }
        route->setApplicable(isApplicable);
    }

    void setRouteNeedReconfigure(const std::string &name, bool needReconfigure)
    {
        AudioRoute *route = Base::getElement(name);
        if (!route) {
            audio_comms::utilities::Log::Error() << __FUNCTION__ << ": invalid route " << name;
            return;
        }
        route->setNeedReconfigure(needReconfigure);
    }

    void setRouteNeedReroute(const std::string &name, bool needReroute)
    {
        AudioRoute *route = Base::getElement(name);
        if (!route) {
            audio_comms::utilities::Log::Error() << __FUNCTION__ << ": invalid route " << name;
            return;
        }
        route->setNeedReroute(needReroute);
    }

    /**
     * Get the need repath routes mask.
     *
     * @return repath routes mask in the requested direction.
     */
    inline uint32_t needRepathRouteMask(Direction::Values dir) const
    {
        return mRoutes[dir].needRepathRoutes();
    }

    /**
     * Get the need repath routes mask.
     *
     * @return repath routes mask in the requested direction.
     */
    inline uint32_t needReflowRouteMask(Direction::Values dir) const
    {
        return mRoutes[dir].needReflowRoutes();
    }

    /**
     * Get the enabled routes mask.
     *
     * @return enabled routes mask in the requested direction.
     */
    inline uint32_t enabledRouteMask(Direction::Values dir) const
    {
        return mRoutes[dir].enabledRoutes();
    }

    /**
     * Get the prevously enabled routes mask.
     *
     * @return previously enabled routes mask in the requested direction.
     */
    inline uint32_t prevEnabledRouteMask(Direction::Values dir) const
    {
        return mRoutes[dir].prevEnabledRoutes();
    }

    inline uint32_t unmutedRoutes(Direction::Values dir) const
    {
        return mRoutes[dir].unmutedRoutes();
    }

    inline uint32_t routesToMute(Direction::Values dir) const
    {
        return mRoutes[dir].routesToMute();
    }

    inline uint32_t openedRoutes(Direction::Values dir) const
    {
        return mRoutes[dir].openedRoutes();
    }

    inline uint32_t routesToDisable(Direction::Values dir) const
    {
        return mRoutes[dir].routesToDisable();
    }

private:
    class RouteMasks
    {
    private:
        uint32_t needReflow;  /**< Bitfield of routes that needs to be mute / unmutes. */
        uint32_t needRepath;  /**< Bitfield of routes that needs to be disabled / enabled. */
        uint32_t enabled;     /**< Bitfield of enabled routes. */
        uint32_t prevEnabled; /**< Bitfield of previously enabled routes. */

    public:
        RouteMasks()
            : needReflow(0), needRepath(0), enabled(0), prevEnabled(0)
        {}

        /**
         * Get the need reflow routes mask.
         *
         * @return reflow routes mask in the requested direction.
         */
        inline uint32_t needReflowRoutes() const
        {
            return needReflow;
        }

        inline void setNeedReflowRoute(uint32_t index)
        {
            setBit(index, needReflow);
        }

        inline void setNeedRepathRoute(uint32_t index)
        {
            setBit(index, needRepath);
        }

        inline void setEnabledRoute(uint32_t index)
        {
            setBit(index, enabled);
        }

        /**
         * Sets a bit referred by an index within a mask.
         *
         * @param[in] index bit index to set.
         * @param[in,out] mask in which the bit must be set.
         */
        inline void setBit(uint32_t index, uint32_t &mask)
        {
            mask |= index;
        }

        /**
         * Get the need repath routes mask.
         *
         * @return repath routes mask in the requested direction.
         */
        inline uint32_t needRepathRoutes() const
        {
            return needRepath;
        }

        /**
         * Get the enabled routes mask.
         *
         * @return enabled routes mask in the requested direction.
         */
        inline uint32_t enabledRoutes() const
        {
            return enabled;
        }

        /**
         * Get the prevously enabled routes mask.
         *
         * @return previously enabled routes mask in the requested direction.
         */
        inline uint32_t prevEnabledRoutes() const
        {
            return prevEnabled;
        }

        /**
         * Checks if the routing conditions changed in a given direction.
         *
         * @tparam isOut direction of the route to consider.
         *
         * @return  true if previously enabled routes is different from currently enabled routes
         *               or if any route needs to be reconfigured.
         */
        bool routingHasChanged() const
        {
            return prevEnabledRoutes() != enabledRoutes()
                   || needReflowRoutes() != 0
                   || needRepathRoutes() != 0;
        }

        void reset()
        {
            prevEnabled = enabled;
            enabled = 0;
            needReflow = 0;
            needRepath = 0;
        }

        uint32_t unmutedRoutes() const
        {
            return prevEnabledRoutes() & enabledRoutes() & ~needReflowRoutes();
        }

        uint32_t routesToMute() const
        {
            return (prevEnabledRoutes() & ~enabledRoutes()) | needReflowRoutes();
        }

        uint32_t openedRoutes() const
        {
            return prevEnabledRoutes() & enabledRoutes() & ~needRepathRoutes();
        }

        uint32_t routesToDisable() const
        {
            return (prevEnabledRoutes() & ~enabledRoutes()) | needRepathRoutes();
        }
    } mRoutes[Direction::gNbDirections];

};

} // namespace intel_audio
