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
#define LOG_TAG "RouteManager/Route"

#include "AudioPort.hpp"
#include "AudioRoute.hpp"
#include <AudioCommsAssert.hpp>
#include <utils/Log.h>

using std::string;

AudioRoute::AudioRoute(const string &name, uint32_t routeId)
    : RoutingElement(name, routeId),
      _isUsed(false),
      _previouslyUsed(false),
      _isApplicable(false),
      _forcedRoutingStageRequested(None)
{
    _port[EPortSource] = NULL;
    _port[EPortDest] = NULL;
}

AudioRoute::~AudioRoute()
{
}


void AudioRoute::addPort(AudioPort *port)
{
    AUDIOCOMMS_ASSERT(port != NULL, "Invalid port requested");

    ALOGV("%s: %d to route %s", __FUNCTION__, port->getId(), getName().c_str());

    port->addRouteToPortUsers(this);
    if (!_port[EPortSource]) {

        _port[EPortSource] = port;
    } else {
        _port[EPortDest] = port;
    }
}

void AudioRoute::resetAvailability()
{
    _blocked = false;
    _previouslyUsed = _isUsed;
    _isUsed = false;
}

bool AudioRoute::isApplicable() const
{
    ALOGV("%s %s !isBlocked()=%d && _isApplicable=%d", __FUNCTION__,
          getName().c_str(), !isBlocked(), _isApplicable);
    return !isBlocked() && _isApplicable;
}

void AudioRoute::setUsed(bool isUsed)
{
    if (!isUsed) {

        return;
    }

    AUDIOCOMMS_ASSERT(isBlocked() != true, "Requested route blocked");

    if (!_isUsed) {
        ALOGV("%s: route %s is now in use in %s", __FUNCTION__, getName().c_str(),
              _isOut ? "PLAYBACK" : "CAPTURE");
        _isUsed = true;

        // Propagate the in use attribute to the ports
        // used by this route
        for (int i = 0; i < ENbPorts; i++) {

            if (_port[i]) {

                _port[i]->setUsed(this);
            }
        }
    }
}

void AudioRoute::setBlocked()
{
    if (!_blocked) {

        ALOGV("%s: route %s is now blocked", __FUNCTION__, getName().c_str());
        _blocked = true;
    }
}
