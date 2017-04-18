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

#include "Criterion.hpp"
#include "RouteMappingKeys.hpp"
#include "EnumValuePair.h"
#include "BitParameterType.h"
#include "BitParameter.h"
#include "RouteSubsystem.hpp"

using intel_audio::IRouteInterface;

const std::string Criterion::mValuePairCriterionType = "ValuePair";
const std::string Criterion::mBitParamCriterionType = "BitParameter";

Criterion::Criterion(const std::string & /*mappingValue*/,
                     CInstanceConfigurableElement *instanceConfigurableElement,
                     const CMappingContext &context,
                     core::log::Logger &logger)
    : CSubsystemObject(instanceConfigurableElement, logger),
      mRouteSubsystem(static_cast<const RouteSubsystem *>(
                          instanceConfigurableElement->getBelongingSubsystem())),
      mCriterionName(context.getItem(MappingKeyName)),
      mCriterionType(context.getItem(MappingKeyType)),
      mValue(mDefaultValue)
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

    warning() << "no other kind of element type supported";
    return 0;
}

bool Criterion::sendToHW(std::string & /*error*/)
{
    // Retrieve blackboard
    blackboardRead(&mValue, sizeof(mValue));

    // Informs the route manager of a change of criterion
    mRouteInterface->setParameter(mCriterionName, mValue);
    return true;
}
