/*
 * INTEL CONFIDENTIAL
 * Copyright © 2013 Intel
 * Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel’s prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#define LOG_TAG "RouteManager/Port"

#include "AudioPort.hpp"
#include "AudioPortGroup.hpp"
#include "AudioRoute.hpp"
#include <AudioCommsAssert.hpp>
#include <utils/Log.h>

using std::string;
using audio_comms::utilities::Direction;

AudioPort::AudioPort(const string &name, uint32_t portIndex)
    : RoutingElement(name, portIndex),
      _portGroupList(0),
      _isBlocked(false),
      _isUsed(false)
{
    _routeAttached[Direction::Output] = NULL;
    _routeAttached[Direction::Input] = NULL;
}

AudioPort::~AudioPort()
{
}

void AudioPort::resetAvailability()
{
    _isUsed = false;
    _isBlocked = false;
    _routeAttached[Direction::Output] = NULL;
    _routeAttached[Direction::Input] = NULL;
}

void AudioPort::addGroupToPort(AudioPortGroup *portGroup)
{
    ALOGV("%s", __FUNCTION__);

    if (portGroup) {
        _portGroupList.push_back(portGroup);
    }

}

void AudioPort::setBlocked(bool blocked)
{
    if (_isBlocked == blocked) {

        return;
    }

    ALOGV("%s: port %s is blocked", __FUNCTION__, getName().c_str());

    _isBlocked = blocked;

    if (blocked) {

        AUDIOCOMMS_ASSERT(_routeAttached != NULL, "Attached route is not initialized");

        // Blocks now all route that use this port
        RouteListIterator it;

        // Find the applicable route for this route request
        for (it = _routeList.begin(); it != _routeList.end(); ++it) {

            AudioRoute *route = *it;
            ALOGV("%s:  blocking %s", __FUNCTION__, route->getName().c_str());
            route->setBlocked();
        }
    }
}

void AudioPort::setUsed(AudioRoute *route)
{
    AUDIOCOMMS_ASSERT(route != NULL, "Invalid route requested");

    if (_isUsed) {

        // Port is already in use, bailing out
        return;
    }

    ALOGV("%s: port %s is in use", __FUNCTION__, getName().c_str());

    _isUsed = true;
    _routeAttached[route->isOut()] = route;

    PortGroupListIterator it;

    for (it = _portGroupList.begin(); it != _portGroupList.end(); ++it) {

        AudioPortGroup *portGroup = *it;

        portGroup->blockMutualExclusivePort(this);
    }

    // Block all route using this port as well (except the route in opposite direction
    // with same name/id as full duplex is allowed)

    // Blocks now all route that use this port except the one that uses the port
    RouteListIterator routeIt;

    // Find the applicable route for this route request
    for (routeIt = _routeList.begin(); routeIt != _routeList.end(); ++routeIt) {

        AudioRoute *routeUsingThisPort = *routeIt;
        if ((routeUsingThisPort == route) || (route->getId() == routeUsingThisPort->getId())) {

            continue;
        }

        ALOGV("%s:  blocking %s", __FUNCTION__, routeUsingThisPort->getName().c_str());
        routeUsingThisPort->setBlocked();
    }
}

void AudioPort::addRouteToPortUsers(AudioRoute *route)
{
    AUDIOCOMMS_ASSERT(route != NULL, "Invalid route requested");

    _routeList.push_back(route);

    ALOGV("%s: added %s route to %s port users", __FUNCTION__, route->getName().c_str(),
          getName().c_str());
}
// namespace android
