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

#include "AudioPort.hpp"
#include "Tokenizer.h"
#include "RouteMappingKeys.hpp"
#include "InstanceConfigurableElement.h"
#include "RouteSubsystem.hpp"

using intel_audio::IRouteInterface;
using std::string;

const string AudioPort::mDelimiter = "-";

AudioPort::AudioPort(const std::string &mappingValue,
                     CInstanceConfigurableElement *instanceConfigurableElement,
                     const CMappingContext &context,
                     core::log::Logger &logger)
    : CFormattedSubsystemObject(instanceConfigurableElement,
                                logger,
                                mappingValue,
                                MappingKeyAmend1,
                                (MappingKeyAmendEnd - MappingKeyAmend1 + 1),
                                context),
      mName(getFormattedMappingValue()),
      mIsBlocked(false),
      mRouteSubsystem(static_cast<const RouteSubsystem *>(
                          instanceConfigurableElement->getBelongingSubsystem()))
{
    mRouteInterface = mRouteSubsystem->getRouteInterface();

    // Adds port first to the route manager and then the links that may exist between these
    // ports (i.e. port groups that represent mutual exclusive ports).
    mRouteInterface->addPort(mName);

    string portGroups = context.getItem(MappingKeyGroups);
    Tokenizer mappingTok(portGroups, mDelimiter);
    std::vector<string> subStrings = mappingTok.split();
    for (uint32_t i = 0; i < subStrings.size(); i++) {

        mRouteInterface->addPortGroup(subStrings[i], mName);
    }
}

bool AudioPort::sendToHW(string & /*error*/)
{
    // Retrieve blackboard
    blackboardRead(&mIsBlocked, sizeof(mIsBlocked));

    mRouteInterface->setPortBlocked(mName, mIsBlocked);

    return true;
}
