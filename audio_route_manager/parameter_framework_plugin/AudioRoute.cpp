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
#include "AudioRoute.hpp"
#include "Tokenizer.h"
#include "RouteMappingKeys.hpp"
#include "RouteSubsystem.hpp"
#include <AudioCommsAssert.hpp>

const string AudioRoute::OUTPUT_DIRECTION = "out";
const string AudioRoute::STREAM_TYPE = "streamRoute";
const string AudioRoute::PORT_DELIMITER = "-";
const string AudioRoute::ROUTE_CRITERION_TYPE = "RouteType";

const AudioRoute::Status AudioRoute::DEFAULT_STATUS = {
    isApplicable    : false,
    forcedRoutingStage : 0
};

AudioRoute::AudioRoute(const string &mappingValue,
                       CInstanceConfigurableElement *instanceConfigurableElement,
                       const CMappingContext &context)
    : CFormattedSubsystemObject(instanceConfigurableElement,
                                mappingValue,
                                MappingKeyAmend1,
                                (MappingKeyAmendEnd - MappingKeyAmend1 + 1),
                                context),
      _routeSubsystem(static_cast<const RouteSubsystem *>(
                          instanceConfigurableElement->getBelongingSubsystem())),
      _routeInterface(_routeSubsystem->getRouteInterface()),
      _status(DEFAULT_STATUS),
      _routeId(context.getItemAsInteger(MappingKeyId)),
      _isStreamRoute(context.getItem(MappingKeyType) == STREAM_TYPE),
      _isOut(context.getItem(MappingKeyDirection) == OUTPUT_DIRECTION)
{
    _routeName = getFormattedMappingValue();

    // Add Route criterion type value pair
    _routeInterface->addCriterionType(ROUTE_CRITERION_TYPE, true);
    _routeInterface->addCriterionTypeValuePair(ROUTE_CRITERION_TYPE,
                                               context.getItem(MappingKeyAmend1),
                                               1 << _routeId);

    string ports = context.getItem(MappingKeyPorts);
    Tokenizer mappingTok(ports, PORT_DELIMITER);
    vector<string> subStrings = mappingTok.split();
    AUDIOCOMMS_ASSERT(subStrings.size() <= DUAL_PORTS,
                      "Route cannot be connected to more than 2 ports");

    string portSrc = subStrings.size() >= SINGLE_PORT ? subStrings[0] : string();
    string portDst = subStrings.size() == DUAL_PORTS ? subStrings[1] : string();

    // Append route to RouteMgr
    if (_isStreamRoute) {

        _routeInterface->addAudioStreamRoute(_routeName, 1 << _routeId,
                                             portSrc, portDst, _isOut);
    } else {
        _routeInterface->addAudioRoute(_routeName, 1 << _routeId,
                                       portSrc, portDst, _isOut);
    }
}

bool AudioRoute::receiveFromHW(string &error)
{
    blackboardWrite(&_status, sizeof(_status));

    return true;
}

bool AudioRoute::sendToHW(string &error)
{
    Status status;

    // Retrieve blackboard
    blackboardRead(&status, sizeof(status));

    // Updates applicable status if changed
    if (status.isApplicable != _status.isApplicable) {

        _routeInterface->setRouteApplicable(_routeName, status.isApplicable);
    }

    // Updates forced routing stage if changed
    if (status.forcedRoutingStage != _status.forcedRoutingStage) {

        _routeInterface->setForcedRoutingStageRequested(_routeName,
                                                        toRoutingStage(status.forcedRoutingStage));
    }

    _status = status;

    return true;
}

RoutingStage AudioRoute::toRoutingStage(uint8_t stage)
{
    return stage == Path ? Path : (stage == Flow ? Flow : None);
}
