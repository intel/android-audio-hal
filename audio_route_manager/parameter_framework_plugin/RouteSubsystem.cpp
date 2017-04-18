/*
 * Copyright (C) 2013-2017 Intel Corporation
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

#include "RouteSubsystem.hpp"
#include "SubsystemObjectFactory.h"
#include "RouteMappingKeys.hpp"
#include "AudioRoute.hpp"
#include "AudioPort.hpp"
#include "AudioStreamRoute.hpp"
#include "Criterion.hpp"
#include <RouteManagerInstance.hpp>
#include <RouteInterface.hpp>
#include <AudioCommsAssert.hpp>

using intel_audio::IRouteInterface;

const char *const RouteSubsystem::mKeyName = "Name";
const char *const RouteSubsystem::mKeyDirection = "Direction";
const char *const RouteSubsystem::mKeyType = "Type";
const char *const RouteSubsystem::mKeyCard = "Card";
const char *const RouteSubsystem::mKeyDevice = "Device";
const char *const RouteSubsystem::mKeyDeviceAddress = "DeviceAddress";
const char *const RouteSubsystem::mKeyPort = "Ports";
const char *const RouteSubsystem::mKeyGroups = "Groups";
const char *const RouteSubsystem::mKeyInclusive = "Inclusive";
const char *const RouteSubsystem::mKeyAmend1 = "Amend1";
const char *const RouteSubsystem::mKeyAmend2 = "Amend2";
const char *const RouteSubsystem::mKeyAmend3 = "Amend3";

const char *const RouteSubsystem::mPortComponentName = "Port";
const char *const RouteSubsystem::mRouteComponentName = "Route";
const char *const RouteSubsystem::mStreamRouteComponentName = "StreamRoute";
const char *const RouteSubsystem::mCriterionComponentName = "Criterion";

RouteSubsystem::RouteSubsystem(const std::string &name, core::log::Logger &logger)
    : CSubsystem(name, logger),
      mRouteInterface(NULL)
{
    mRouteInterface = intel_audio::RouteManagerInstance::getRouteInterface();
    AUDIOCOMMS_ASSERT(mRouteInterface != NULL,
                      "Could not retrieve route interface, plugin not functional");

    // Provide mapping keys to upper layer
    addContextMappingKey(mKeyName);
    addContextMappingKey(mKeyDirection);
    addContextMappingKey(mKeyType);
    addContextMappingKey(mKeyCard);
    addContextMappingKey(mKeyDevice);
    addContextMappingKey(mKeyDeviceAddress);
    addContextMappingKey(mKeyPort);
    addContextMappingKey(mKeyGroups);
    addContextMappingKey(mKeyInclusive);
    addContextMappingKey(mKeyAmend1);
    addContextMappingKey(mKeyAmend2);
    addContextMappingKey(mKeyAmend3);

    // Provide creators to upper layer
    addSubsystemObjectFactory(
        new TSubsystemObjectFactory<AudioPort>(
            mPortComponentName,
            (1 << MappingKeyAmend1) | (1 << MappingKeyGroups))
        );
    addSubsystemObjectFactory(
        new TSubsystemObjectFactory<AudioRoute>(
            mRouteComponentName,
            (1 << MappingKeyType) | (1 << MappingKeyDirection) | (1 << MappingKeyPorts) |
            (1 << MappingKeyAmend1))
        );
    addSubsystemObjectFactory(
        new TSubsystemObjectFactory<AudioStreamRoute>(
            mStreamRouteComponentName,
            (1 << MappingKeyDirection) |
            (1 << MappingKeyCard) | (1 << MappingKeyDevice) | (1 << MappingKeyPorts) |
            (1 << MappingKeyAmend1))
        );

    addSubsystemObjectFactory(
        new TSubsystemObjectFactory<Criterion>(
            mCriterionComponentName,
            (1 << MappingKeyName) | (1 << MappingKeyType) | (1 << MappingKeyInclusive))
        );
}

// Retrieve Route interface
IRouteInterface *RouteSubsystem::getRouteInterface() const
{
    return mRouteInterface;
}
