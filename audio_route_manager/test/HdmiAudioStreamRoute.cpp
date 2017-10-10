/*
 * Copyright (C) 2015-2017 Intel Corporation
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
#define LOG_TAG "RouteManager/HdmiStreamRoute"

#include "test/HdmiAudioStreamRoute.hpp"
#include <utilities/Log.hpp>

using android::status_t;
using std::string;
using audio_comms::utilities::Log;

namespace intel_audio
{

RegisterStreamRoute<HdmiAudioStreamRoute> HdmiAudioStreamRoute::reg("Hdmi");

void HdmiAudioStreamRoute::loadCapabilities()
{
    Log::Debug() << __FUNCTION__ << ": for route " << getName();

    resetCapabilities();

    if (getName() == "Hdmi") {
        Log::Debug() << __FUNCTION__ << ": TEST for route " << getName();
        for (auto &capability : mConfig.mAudioCapabilities) {
            capability.mSupportedChannelMasks.push_back(AUDIO_CHANNEL_OUT_STEREO);
            capability.mSupportedChannelMasks.push_back(AUDIO_CHANNEL_OUT_5POINT1);
            capability.mSupportedChannelMasks.push_back(AUDIO_CHANNEL_OUT_7POINT1);

            capability.mSupportedFormat = AUDIO_FORMAT_PCM_8_24_BIT;

            capability.mSupportedRates.push_back(48000);
            capability.mSupportedRates.push_back(192000);
        }
    }
}

} // namespace intel_audio
