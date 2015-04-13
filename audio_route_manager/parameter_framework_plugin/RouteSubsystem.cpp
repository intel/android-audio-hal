/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (c) 2013-2015 Intel Corporation All Rights Reserved.
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
#include <RouteManagerInstance.hpp>
#include <RouteInterface.hpp>
#include <AudioCommsAssert.hpp>

using intel_audio::IRouteInterface;

const char *const RouteSubsystem::mKeyName = "Name";
const char *const RouteSubsystem::mKeyDirection = "Direction";
const char *const RouteSubsystem::mKeyType = "Type";
const char *const RouteSubsystem::mKeyCard = "Card";
const char *const RouteSubsystem::mKeyDevice = "Device";
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

RouteSubsystem::RouteSubsystem(const std::string &name)
    : CSubsystem(name),
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
