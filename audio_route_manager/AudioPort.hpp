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

#include "RoutingElement.hpp"
#include <Direction.hpp>
#include <list>
#include <stdint.h>

namespace intel_audio
{

class AudioPortGroup;
class AudioRoute;

class AudioPort : public RoutingElement
{
private:
    typedef std::list<AudioPortGroup *>::iterator PortGroupListIterator;
    typedef std::list<AudioPortGroup *>::const_iterator PortGroupListConstIterator;
    typedef std::list<AudioRoute *>::iterator RouteListIterator;
    typedef std::list<AudioRoute *>::const_iterator RouteListConstIterator;

public:
    AudioPort(const std::string &name);
    virtual ~AudioPort();

    /**
     * Blocks a port.
     *
     * Calling this API will prevent from using this port and all routes that
     * use this port as well.
     *
     * @param[in] blocked true if the port needs to be blocked, false otherwise.
     */
    void setBlocked(bool blocked);

    /**
     * Sets a port in use.
     *
     * An audio route is now in use and propagate the in use attribute to the ports that this
     * audio route is using.
     * It parses the list of Group Port in which this port is involved
     * and blocks all ports within these port group that are mutual exclusive with this one.
     *
     * @param[in] route Route that is in use and request to use the port.
     */
    void setUsed(AudioRoute *route);

    /**
     * Resets the availability of the port.
     *
     * It not only resets the used attribute of the port
     * but also the pointers on the routes using this port.
     * It does not reset exclusive access variable of the route parameter manager (as blocked
     * variable).
     */
    virtual void resetAvailability();

    /**
     * Add a route as a potential user of this port.
     *
     * Called at the construction of the audio platform, it pushes a route to the list of route
     * using this port. It will help propagating the blocking attribute when this port is declared
     * as blocked.
     *
     * @param[in] route Route using this port.
     */
    void addRouteToPortUsers(AudioRoute *route);

    /**
     * Add a group to the port.
     *
     * Called at the construction of the audio platform, it pushes a group in the list of groups
     * this port is belonging to. It will help propagating the blocking attribute when this port is
     * declared as blocked.
     *
     * @param[in] portGroup Group to add.
     */
    void addGroupToPort(AudioPortGroup *portGroup);

private:
    std::list<AudioPortGroup *> mPortGroupList; /**< list of groups this port belongs to. */

    /**
     * routes attached to this port.
     */
    AudioRoute *mRouteAttached[Direction::gNbDirections];

    std::list<AudioRoute *> mRouteList; /**< list of routes using potentially this port. */

    bool mIsBlocked; /**< blocked attribute, exclusive access to route parameter manager plugin. */
    bool mIsUsed; /**< used attribute. */
};

} // namespace intel_audio
