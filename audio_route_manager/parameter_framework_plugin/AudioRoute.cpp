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

#include "AudioRoute.hpp"
#include "Tokenizer.h"
#include "RouteMappingKeys.hpp"
#include "RouteSubsystem.hpp"
#include <AudioCommsAssert.hpp>

using intel_audio::IRouteInterface;
using std::string;

const string AudioRoute::mOutputDirection = "out";
const string AudioRoute::mStreamType = "streamRoute";
const string AudioRoute::mPortDelimiter = "-";

const AudioRoute::Status AudioRoute::mDefaultStatus = {
    .isApplicable = false,
    .needReconfigure = false,
    .needReroute = false
};

AudioRoute::AudioRoute(const std::string &mappingValue,
                       CInstanceConfigurableElement *instanceConfigurableElement,
                       const CMappingContext &context,
                       core::log::Logger &logger)
    : CFormattedSubsystemObject(instanceConfigurableElement,
                                logger,
                                mappingValue,
                                MappingKeyAmend1,
                                (MappingKeyAmendEnd - MappingKeyAmend1 + 1),
                                context),
      mRouteSubsystem(static_cast<const RouteSubsystem *>(
                          instanceConfigurableElement->getBelongingSubsystem())),
      mRouteInterface(mRouteSubsystem->getRouteInterface()),
      mStatus(mDefaultStatus),
      mIsStreamRoute(context.getItem(MappingKeyType) == mStreamType),
      mIsOut(context.getItem(MappingKeyDirection) == mOutputDirection)
{
    mRouteName = getFormattedMappingValue();

    string ports = context.getItem(MappingKeyPorts);
    Tokenizer mappingTok(ports, mPortDelimiter);
    std::vector<string> subStrings = mappingTok.split();
    AUDIOCOMMS_ASSERT(subStrings.size() <= mDualPorts,
                      "Route cannot be connected to more than 2 ports");

    string portSrc = subStrings.size() >= mSinglePort ? subStrings[0] : string();
    string portDst = subStrings.size() == mDualPorts ? subStrings[1] : string();

    // Append route to RouteMgr with route root name
    if (mIsStreamRoute) {
        mRouteInterface->addAudioStreamRoute(context.getItem(MappingKeyAmend1),
                                             portSrc, portDst, mIsOut);
    } else {
        mRouteInterface->addAudioRoute(context.getItem(MappingKeyAmend1), portSrc, portDst, mIsOut);
    }
}

bool AudioRoute::sendToHW(string & /*error*/)
{
    Status status;

    // Retrieve blackboard
    blackboardRead(&status, sizeof(status));

    // Updates applicable status if changed
    if (status.isApplicable != mStatus.isApplicable) {

        mRouteInterface->setRouteApplicable(mRouteName, status.isApplicable);
    }

    // Updates reconfigure flag if changed
    if (status.needReconfigure != mStatus.needReconfigure) {

        mRouteInterface->setRouteNeedReconfigure(mRouteName, status.needReconfigure);
    }

    // Updates reroute flag if changed
    if (status.needReroute != mStatus.needReroute) {

        mRouteInterface->setRouteNeedReroute(mRouteName, status.needReroute);
    }

    mStatus = status;

    return true;
}
