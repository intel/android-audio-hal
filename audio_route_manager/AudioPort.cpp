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
#define LOG_TAG "RouteManager/Port"

#include "AudioPort.hpp"
#include "AudioPortGroup.hpp"
#include "AudioRoute.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using std::string;
using audio_comms::utilities::Log;

namespace intel_audio
{

AudioPort::AudioPort(const string &name)
    : RoutingElement(name),
      mPortGroupList(0),
      mIsBlocked(false),
      mIsUsed(false)
{
    mRouteAttached[Direction::Output] = NULL;
    mRouteAttached[Direction::Input] = NULL;
}

AudioPort::~AudioPort()
{
}

void AudioPort::resetAvailability()
{
    mIsUsed = false;
    mRouteAttached[Direction::Output] = NULL;
    mRouteAttached[Direction::Input] = NULL;
}

void AudioPort::addGroupToPort(AudioPortGroup &portGroup)
{
    Log::Verbose() << __FUNCTION__;
    mPortGroupList.push_back(&portGroup);
}

void AudioPort::setBlocked(bool blocked)
{
    if (mIsBlocked == blocked) {

        return;
    }
    Log::Verbose() << __FUNCTION__ << ": port " << getName() << " is blocked";

    mIsBlocked = blocked;

    if (blocked) {
        // Blocks now all route that use this port, whether this port is attached or not to a route.
        RouteListIterator it;

        // Find the applicable route for this route request
        for (it = mRouteList.begin(); it != mRouteList.end(); ++it) {

            AudioRoute *route = *it;
            Log::Verbose() << __FUNCTION__ << ":  blocking " << route->getName();
            route->setBlocked();
        }
    }
}

void AudioPort::setUsed(AudioRoute &route)
{
    if (mIsUsed) {

        // Port is already in use, bailing out
        return;
    }
    Log::Verbose() << __FUNCTION__ << ": port " << getName() << " is in use";

    mIsUsed = true;
    mRouteAttached[route.isOut()] = &route;

    PortGroupListIterator it;

    for (it = mPortGroupList.begin(); it != mPortGroupList.end(); ++it) {

        AudioPortGroup *portGroup = *it;

        portGroup->blockMutualExclusivePort(*this);
    }

    // Block all route using this port as well (except the route in opposite direction
    // with same name/id as full duplex is allowed)

    // Blocks now all route that use this port except the one that uses the port
    RouteListIterator routeIt;

    // Find the applicable route for this route request
    for (routeIt = mRouteList.begin(); routeIt != mRouteList.end(); ++routeIt) {

        AudioRoute *routeUsingThisPort = *routeIt;
        if ((routeUsingThisPort == &route) || (route.getName() == routeUsingThisPort->getName())) {

            continue;
        }
        Log::Verbose() << __FUNCTION__ << ":  blocking " << routeUsingThisPort->getName();
        routeUsingThisPort->setBlocked();
    }
}

void AudioPort::addRouteToPortUsers(AudioRoute &route)
{
    mRouteList.push_back(&route);

    Log::Verbose() << __FUNCTION__ << ": added " << route.getName()
                   << " route to " << getName() << " port users";
}

} // namespace intel_audio
