/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (c) 2013-2014 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors.
 *
 * Title to the Material remains with Intel Corporation or its suppliers and
 * licensors. The Material contains trade secrets and proprietary and
 * confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and treaty
 * provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or disclosed
 * in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

#include "RouteSubsystem.hpp"
#include "SubsystemObjectFactory.h"
#include "RouteMappingKeys.hpp"
#include "AudioRoute.hpp"
#include "AudioPort.hpp"
#include "AudioStreamRoute.hpp"
#include "Criterion.hpp"
#include "InterfaceProviderLib.h"
#include "Property.h"
#include <RouteInterface.hpp>

using namespace NInterfaceProvider;
using intel_audio::IRouteInterface;

const char *const RouteSubsystem::ROUTE_LIB_PROP_NAME = "audiocomms.routeLib";
const char *const RouteSubsystem::ROUTE_LIBRARY_NAME = "audio.routemanager.so";

const char *const RouteSubsystem::KEY_NAME = "Name";
const char *const RouteSubsystem::KEY_ID = "Id";
const char *const RouteSubsystem::KEY_DIRECTION = "Direction";
const char *const RouteSubsystem::KEY_TYPE = "Type";
const char *const RouteSubsystem::KEY_CARD = "Card";
const char *const RouteSubsystem::KEY_DEVICE = "Device";
const char *const RouteSubsystem::KEY_PORT = "Ports";
const char *const RouteSubsystem::KEY_GROUPS = "Groups";
const char *const RouteSubsystem::KEY_INCLUSIVE = "Inclusive";
const char *const RouteSubsystem::KEY_AMEND_1 = "Amend1";
const char *const RouteSubsystem::KEY_AMEND_2 = "Amend2";
const char *const RouteSubsystem::KEY_AMEND_3 = "Amend3";

const char *const RouteSubsystem::PORT_COMPONENT_NAME = "Port";
const char *const RouteSubsystem::ROUTE_COMPONENT_NAME = "Route";
const char *const RouteSubsystem::STREAM_ROUTE_COMPONENT_NAME = "StreamRoute";
const char *const RouteSubsystem::CRITERION_COMPONENT_NAME = "Criterion";

RouteSubsystem::RouteSubsystem(const string &strName) : CSubsystem(strName),
                                                        _routeInterface(NULL)
{
    // Try to connect a Route Interface from RouteManager
    IInterfaceProvider *interfaceProvider =
        getInterfaceProvider(TProperty<string>(ROUTE_LIB_PROP_NAME,
                                               ROUTE_LIBRARY_NAME).getValue().c_str());

    if (interfaceProvider) {

        // Retrieve the Route Interface
        _routeInterface = interfaceProvider->queryInterface<IRouteInterface>();
    }

    // Provide mapping keys to upper layer
    addContextMappingKey(KEY_NAME);
    addContextMappingKey(KEY_ID);
    addContextMappingKey(KEY_DIRECTION);
    addContextMappingKey(KEY_TYPE);
    addContextMappingKey(KEY_CARD);
    addContextMappingKey(KEY_DEVICE);
    addContextMappingKey(KEY_PORT);
    addContextMappingKey(KEY_GROUPS);
    addContextMappingKey(KEY_INCLUSIVE);
    addContextMappingKey(KEY_AMEND_1);
    addContextMappingKey(KEY_AMEND_2);
    addContextMappingKey(KEY_AMEND_3);

    // Provide creators to upper layer
    addSubsystemObjectFactory(
        new TSubsystemObjectFactory<AudioPort>(
            PORT_COMPONENT_NAME,
            (1 << MappingKeyId) | (1 << MappingKeyAmend1) | (1 << MappingKeyGroups))
        );
    addSubsystemObjectFactory(
        new TSubsystemObjectFactory<AudioRoute>(
            ROUTE_COMPONENT_NAME,
            (1 << MappingKeyId) |
            (1 << MappingKeyType) | (1 << MappingKeyDirection) | (1 << MappingKeyPorts) |
            (1 << MappingKeyAmend1))
        );
    addSubsystemObjectFactory(
        new TSubsystemObjectFactory<AudioStreamRoute>(
            STREAM_ROUTE_COMPONENT_NAME,
            (1 << MappingKeyId) | (1 << MappingKeyDirection) |
            (1 << MappingKeyCard) | (1 << MappingKeyDevice) | (1 << MappingKeyPorts) |
            (1 << MappingKeyAmend1))
        );

    addSubsystemObjectFactory(
        new TSubsystemObjectFactory<Criterion>(
            CRITERION_COMPONENT_NAME,
            (1 << MappingKeyName) | (1 << MappingKeyType) | (1 << MappingKeyInclusive))
        );
}

// Retrieve Route interface
IRouteInterface *RouteSubsystem::getRouteInterface() const
{
    return _routeInterface;
}
