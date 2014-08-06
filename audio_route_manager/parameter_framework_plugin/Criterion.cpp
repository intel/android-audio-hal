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

#include "Criterion.hpp"
#include "RouteMappingKeys.hpp"
#include "EnumValuePair.h"
#include "BitParameterType.h"
#include "BitParameter.h"
#include "RouteSubsystem.hpp"

using intel_audio::IRouteInterface;

const string Criterion::mValuePairCriterionType = "ValuePair";
const string Criterion::mBitParamCriterionType = "BitParameter";

Criterion::Criterion(const string & /*mappingValue*/,
                     CInstanceConfigurableElement *instanceConfigurableElement,
                     const CMappingContext &context)
    : CSubsystemObject(instanceConfigurableElement),
      mRouteSubsystem(static_cast<const RouteSubsystem *>(
                          instanceConfigurableElement->getBelongingSubsystem())),
      mCriterionName(context.getItem(MappingKeyName)),
      mCriterionType(context.getItem(MappingKeyType))
{
    mRouteInterface = mRouteSubsystem->getRouteInterface();

    // First checks if criterion Type has been already added, if no, populate value pairs
    if (!mRouteInterface->addAudioCriterionType(mCriterionType,
                                                context.getItemAsInteger(MappingKeyInclusive))) {

        // If exists, get the children of element to retrieve Criterion type
        const CElement *elementToDiscover = instanceConfigurableElement->getTypeElement();

        uint32_t nbChildren = elementToDiscover->getNbChildren();
        uint32_t index;
        for (index = 0; index < nbChildren; index++) {

            mRouteInterface->addAudioCriterionTypeValuePair(
                mCriterionType,
                elementToDiscover->getChild(index)->getName(),
                getIndex(elementToDiscover->getChild(index)));
        }
    }
    mRouteInterface->addAudioCriterion(mCriterionName, mCriterionType);
}

uint32_t Criterion::getIndex(const CElement *element) const
{
    if (element->getKind() == mValuePairCriterionType) {

        const CEnumValuePair *enumPair = static_cast<const CEnumValuePair *>(element);
        return enumPair->getNumerical();

    } else if (element->getKind() == mBitParamCriterionType) {

        const CBitParameterType *bitParameterType = static_cast<const CBitParameterType *>(element);
        return 1 << bitParameterType->getBitPos();
    }

    log_warning("no other kind of element type supported");
    return 0;
}

bool Criterion::receiveFromHW(string & /*error*/)
{
    blackboardWrite(&mValue, sizeof(mValue));

    return true;
}

bool Criterion::sendToHW(string & /*error*/)
{
    // Retrieve blackboard
    blackboardRead(&mValue, sizeof(mValue));

    // Informs the route manager of a change of criterion
    mRouteInterface->setParameter(mCriterionName, mValue);
    return true;
}
