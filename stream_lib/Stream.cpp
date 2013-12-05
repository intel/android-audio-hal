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
#include "Stream.hpp"
#include "AudioDevice.hpp"
#include <IStreamRoute.hpp>
#include <AudioCommsAssert.hpp>
#include <SampleSpec.hpp>
#include <utils/RWLock.h>
#include <cutils/log.h>

using android_audio_legacy::SampleSpec;
using std::string;

bool Stream::isRouted() const
{
    AutoR lock(_streamLock);
    return isRoutedL();
}

bool Stream::isRoutedL() const
{
    return _currentStreamRoute != NULL;
}

bool Stream::isNewRouteAvailable() const
{
    AutoR lock(_streamLock);
    return _newStreamRoute != NULL;
}

android::status_t Stream::attachRoute()
{
    AutoW lock(_streamLock);
    return attachRouteL();
}


android::status_t Stream::detachRoute()
{
    AutoW lock(_streamLock);
    return detachRouteL();
}

android::status_t Stream::attachRouteL()
{
    AUDIOCOMMS_ASSERT(_newStreamRoute != NULL, "NULL route pointer");
    setCurrentStreamRouteL(_newStreamRoute);
    AUDIOCOMMS_ASSERT(_currentStreamRoute != NULL, "NULL route pointer");
    setRouteSampleSpecL(_currentStreamRoute->getSampleSpec());
    return android::OK;
}


android::status_t Stream::detachRouteL()
{
    _currentStreamRoute = NULL;
    return android::OK;
}

void Stream::addRequestedEffect(uint32_t effectId)
{
    _effectsRequestedMask |= effectId;
}

void Stream::removeRequestedEffect(uint32_t effectId)
{
    _effectsRequestedMask &= ~effectId;
}

uint32_t Stream::getOutputSilencePrologMs() const
{
    AUDIOCOMMS_ASSERT(_currentStreamRoute != NULL, "NULL route pointer");
    return _currentStreamRoute->getOutputSilencePrologMs();
}

void Stream::resetNewStreamRoute()
{
    _newStreamRoute = NULL;
}

void Stream::setNewStreamRoute(IStreamRoute *newStreamRoute)
{
    _newStreamRoute = newStreamRoute;
}

void Stream::setCurrentStreamRouteL(IStreamRoute *currentStreamRoute)
{
    _currentStreamRoute = currentStreamRoute;
}

void Stream::setRouteSampleSpecL(SampleSpec sampleSpec)
{
    _routeSampleSpec = sampleSpec;
}
