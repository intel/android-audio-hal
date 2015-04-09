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
#pragma once

#include <hardware/audio.h>
#include <SampleSpec.hpp>

namespace intel_audio
{

struct StreamRouteConfig
{
    /**
     * flags to indicate whether the route must be enabled before or after opening the device.
     */
    bool requirePreEnable;

    /**
     * flags to indicate whether the route must be closed before or after opening the device.
     */
    bool requirePostDisable;

    const char *cardName;   /**< Alsa card used by the stream route. */
    uint8_t deviceId;       /**< Alsa card used by the stream route. */

    /**
     * pcm configuration supported by the stream route.
     */
    uint32_t channels;
    uint32_t rate;
    uint32_t periodSize;
    uint32_t periodCount;
    audio_format_t format;
    uint32_t startThreshold;
    uint32_t stopThreshold;
    uint32_t silenceThreshold;
    uint32_t availMin;

    uint8_t silencePrologInMs; /**< if needed, silence to be appended before valid samples. */
    uint32_t flagMask; /**< flags supported by this route. To be checked with stream flags. */
    uint32_t useCaseMask; /**< use cases supported by this route. To be checked with stream. */

    /**
     * Channel policy vector followed by this route.
     * Each channel must specify its channel policy among these values:
     *   - copy policy: channel has no specific weight
     *   - ignore policy: channel has a null weight, and must/will be ignored by the HW
     *   - average policy: channel has a highest weight among the other, it must contains all valid
     */
    std::vector<SampleSpec::ChannelsPolicy> channelsPolicy;
};

} // namespace intel_audio
