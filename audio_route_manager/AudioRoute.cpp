/*
 * Copyright (C) 2013-2015 Intel Corporation
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
#define LOG_TAG "RouteManager/Route"

#include "AudioPort.hpp"
#include "AudioRoute.hpp"
#include <cutils/bitops.h>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using std::string;
using audio_comms::utilities::Log;

namespace intel_audio
{

AudioRoute::AudioRoute(const string &name)
    : RoutingElement(name),
      mIsUsed(false),
      mPreviouslyUsed(false),
      mIsApplicable(false),
      mRoutingStageRequested(0)
{
    mPort[EPortSource] = NULL;
    mPort[EPortDest] = NULL;
}

AudioRoute::~AudioRoute()
{
}


void AudioRoute::addPort(AudioPort &port)
{
    Log::Verbose() << __FUNCTION__ << ": " << port.getName() << " to route " << getName();

    port.addRouteToPortUsers(*this);
    if (!mPort[EPortSource]) {

        mPort[EPortSource] = &port;
    } else {
        mPort[EPortDest] = &port;
    }
}

void AudioRoute::resetAvailability()
{
    mBlocked = false;
    mPreviouslyUsed = mIsUsed;
    mIsUsed = false;
}

bool AudioRoute::isApplicable() const
{
    Log::Verbose() << __FUNCTION__ << ": " << getName()
                   << "!isBlocked()=" << !isBlocked() << " && _isApplicable=" << mIsApplicable;
    return !isBlocked() && mIsApplicable;
}

void AudioRoute::setUsed(bool isUsed)
{
    if (!isUsed) {

        return;
    }
    if (isApplicable()) {
        Log::Verbose() << __FUNCTION__ << ": route " << getName() << " is now in use in "
                       << (mIsOut ? "PLAYBACK" : "CAPTURE");
        mIsUsed = true;

        // Propagate the in use attribute to the ports
        // used by this route
        for (int i = 0; i < ENbPorts; i++) {

            if (mPort[i]) {

                mPort[i]->setUsed(*this);
            }
        }
    }
}

void AudioRoute::setBlocked()
{
    if (!mBlocked) {
        Log::Verbose() << __FUNCTION__ << ": route " << getName() << " is now blocked";
        mBlocked = true;
    }
}

} // namespace intel_audio
