/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
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
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#include "Criterion.hpp"
#include "CriterionType.hpp"
#include "ParameterMgrPlatformConnector.h"
#include <AudioCommsAssert.hpp>

using std::string;

Criterion::Criterion(const string &name,
                     CriterionType *criterionType,
                     CParameterMgrPlatformConnector *parameterMgrConnector,
                     int32_t defaultValue)
    : mCriterionType(criterionType),
      mParameterMgrConnector(parameterMgrConnector),
      mName(name),
      mValue(defaultValue)
{
    mSelectionCriterionInterface =
        mParameterMgrConnector->createSelectionCriterion(mName, criterionType->getTypeInterface());

    setCriterionState();
}

Criterion::~Criterion()
{
}

string Criterion::getFormattedValue()
{
    return mCriterionType->getTypeInterface()->getFormattedState(mValue);
}

bool Criterion::setValue(int32_t value)
{
    if (mValue != value) {

        mValue = value;
        return true;
    }
    return false;
}

void Criterion::setCriterionState()
{
    AUDIOCOMMS_ASSERT(mSelectionCriterionInterface != NULL, "NULL criterion interface");
    mSelectionCriterionInterface->setCriterionState(mValue);
}

bool Criterion::setCriterionState(int32_t value)
{
    if (setValue(value)) {

        AUDIOCOMMS_ASSERT(mSelectionCriterionInterface != NULL, "NULL criterion interface");
        mSelectionCriterionInterface->setCriterionState(mValue);
        return true;
    }
    return false;
}
