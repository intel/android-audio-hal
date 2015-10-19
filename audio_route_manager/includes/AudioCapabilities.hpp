/*
 * Copyright (C) 2015 Intel Corporation
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
#include <convert.hpp>
#include <vector>

namespace intel_audio
{

struct AudioCapabilities
{
    std::vector<uint32_t> supportedRates; /**< Collection of supported rates. */
    std::vector<audio_format_t> supportedFormats; /**< Collection of supported formats. */
    std::vector<audio_channel_mask_t> supportedChannelMasks; /**< Supported channel masks. */

    /**
     * @return the default rate which is either the first value in the supported rate collection
     *         or 0 if empty. 0 will be interpreted by Android as "dynamic".
     */
    uint32_t getDefaultRate() const
    {
        return supportedRates.empty() ? 0 : supportedRates[0];
    }
    /**
     * Checks if a rate belongs to the capabilities.
     * @param[in] rate to check upon support
     * @return true if the rate is found in the collection of supported rates, false otherwise.
     */
    bool supportRate(uint32_t rate) const
    {
        return std::find(supportedRates.begin(), supportedRates.end(), rate) !=
                supportedRates.end();
    }

    /**
     * @return the default format which is either the first value in the supported format collection
     *         or AUDIO_FORMAT_DEFAULT if empty.
     *         AUDIO_FORMAT_DEFAULT will be interpreted by Android as "dynamic".
     */
    audio_format_t getDefaultFormat() const
    {
        return supportedFormats.empty() ? AUDIO_FORMAT_DEFAULT : supportedFormats[0];
    }

    /**
     * Checks if a format belongs to the capabilities.
     * @param[in] format to check upon support
     * @return true if the format is found in the collection of supported format, false otherwise.
     */
    bool supportFormat(audio_format_t format) const
    {
        return std::find(supportedFormats.begin(), supportedFormats.end(), format) !=
                supportedFormats.end();
    }

    /**
     * @return the default channel mask which is either the first value in the supported channel
     *         mask collection or AUDIO_CHANNEL_NONE if empty.
     *         AUDIO_CHANNEL_NONE will be interpreted by Android as "dynamic".
     */
    audio_channel_mask_t getDefaultChannelMask() const
    {
        return supportedChannelMasks.empty() ? AUDIO_CHANNEL_NONE : supportedChannelMasks[0];
    }

    /**
     * Checks if a channel mask belongs to the capabilities.
     * @param[in] channel mask to check upon support
     * @return true if the channel mask is found in the collection of supported channel mask,
     *         false otherwise.
     */
    bool supportChannelMask(audio_channel_mask_t mask) const
    {
        return std::find(supportedChannelMasks.begin(), supportedChannelMasks.end(), mask) !=
                supportedChannelMasks.end();
    }
};

} // namespace intel_audio
