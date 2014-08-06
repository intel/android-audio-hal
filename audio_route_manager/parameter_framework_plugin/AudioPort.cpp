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

#include "AudioPort.hpp"
#include "Tokenizer.h"
#include "RouteMappingKeys.hpp"
#include "InstanceConfigurableElement.h"
#include "RouteSubsystem.hpp"

using intel_audio::IRouteInterface;

const string AudioPort::mDelimiter = "-";

AudioPort::AudioPort(const string &mappingValue,
                     CInstanceConfigurableElement *instanceConfigurableElement,
                     const CMappingContext &context)
    : CFormattedSubsystemObject(instanceConfigurableElement,
                                mappingValue,
                                MappingKeyAmend1,
                                (MappingKeyAmendEnd - MappingKeyAmend1 + 1),
                                context),
      mName(getFormattedMappingValue()),
      mId(context.getItemAsInteger(MappingKeyId)),
      mIsBlocked(false),
      mRouteSubsystem(static_cast<const RouteSubsystem *>(
                          instanceConfigurableElement->getBelongingSubsystem()))
{
    mRouteInterface = mRouteSubsystem->getRouteInterface();

    // Adds port first to the route manager and then the links that may exist between these
    // ports (i.e. port groups that represent mutual exclusive ports).
    mRouteInterface->addPort(mName, 1 << mId);

    string portGroups = context.getItem(MappingKeyGroups);
    Tokenizer mappingTok(portGroups, mDelimiter);
    vector<string> subStrings = mappingTok.split();
    for (uint32_t i = 0; i < subStrings.size(); i++) {

        mRouteInterface->addPortGroup(subStrings[i], 0, mName);
    }
}

bool AudioPort::receiveFromHW(string & /*error*/)
{
    blackboardWrite(&mIsBlocked, sizeof(mIsBlocked));

    return true;
}

bool AudioPort::sendToHW(string & /*error*/)
{
    // Retrieve blackboard
    blackboardRead(&mIsBlocked, sizeof(mIsBlocked));

    mRouteInterface->setPortBlocked(mName, mIsBlocked);

    return true;
}
