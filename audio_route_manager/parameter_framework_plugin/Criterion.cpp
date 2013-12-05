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
#include "Criterion.hpp"
#include "RouteMappingKeys.hpp"
#include "EnumValuePair.h"
#include "BitParameterType.h"
#include "BitParameter.h"
#include "RouteSubsystem.hpp"

const string Criterion::VALUE_PAIR_CRITERION_TYPE = "ValuePair";
const string Criterion::BIT_PARAM_CRITERION_TYPE = "BitParameter";

Criterion::Criterion(const string &mappingValue,
                     CInstanceConfigurableElement *instanceConfigurableElement,
                     const CMappingContext &context)
    : CSubsystemObject(instanceConfigurableElement),
      _routeSubsystem(static_cast<const RouteSubsystem *>(
                          instanceConfigurableElement->getBelongingSubsystem())),
      _criterionName(context.getItem(MappingKeyName)),
      _criterionType(context.getItem(MappingKeyType))
{
    _routeInterface = _routeSubsystem->getRouteInterface();

    // First checks if criterion Type has been already added, if no, populate value pairs
    if (!_routeInterface->addCriterionType(_criterionType,
                                           context.getItemAsInteger(MappingKeyInclusive))) {

        // If exists, get the children of element to retrieve Criterion type
        const CElement *elementToDiscover = instanceConfigurableElement->getTypeElement();

        uint32_t nbChildren = elementToDiscover->getNbChildren();
        uint32_t index;
        for (index = 0; index < nbChildren; index++) {

            _routeInterface->addCriterionTypeValuePair(
                _criterionType,
                elementToDiscover->getChild(index)->getName(),
                getIndex(elementToDiscover->getChild(index)));
        }
    }
    _routeInterface->addCriterion(_criterionName, _criterionType);
}

uint32_t Criterion::getIndex(const CElement *element) const
{
    if (element->getKind() == VALUE_PAIR_CRITERION_TYPE) {

        const CEnumValuePair *enumPair = static_cast<const CEnumValuePair *>(element);
        return enumPair->getNumerical();

    } else if (element->getKind() == BIT_PARAM_CRITERION_TYPE) {

        const CBitParameterType *bitParameterType = static_cast<const CBitParameterType *>(element);
        return 1 << bitParameterType->getBitPos();
    }

    log_warning("no other kind of element type supported");
    return 0;
}

bool Criterion::receiveFromHW(string &error)
{
    blackboardWrite(&_value, sizeof(_value));

    return true;
}

bool Criterion::sendToHW(string &error)
{
    // Retrieve blackboard
    blackboardRead(&_value, sizeof(_value));

    // Informs the route manager of a change of criterion
    _routeInterface->setParameter(_criterionName, _value);
    return true;
}
