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
#pragma once

#include <hardware/audio.h>
#include <utils/Errors.h>
#include <vector>

namespace intel_audio
{

struct AudioCapability
{
    audio_format_t mSupportedFormat;
    std::vector<uint32_t> mSupportedRates; /**< Collection of supported rates. */
    std::vector<audio_channel_mask_t> mSupportedChannelMasks; /**< Supported channel masks. */

    /**
     * @return the default rate which is either the first value in the supported rate collection
     *         or 0 if empty. 0 will be interpreted by Android as "dynamic".
     */
    uint32_t getDefaultRate() const { return mSupportedRates.empty() ? 0 : mSupportedRates[0]; }

    /**
     * Checks if a rate belongs to the capabilities.
     * @param[in] rate to check upon support
     * @return true if the rate is found in the collection of supported rates, false otherwise.
     */
    bool supportRate(uint32_t rate) const;

    uint32_t getRateNear(uint32_t rate, bool isOut) const;

    bool supportRateNear(uint32_t rate, bool isOut) const { return getRateNear(rate, isOut) != 0; }

    audio_channel_mask_t getChannelMaskNear(audio_channel_mask_t mask, bool isOut) const;

    bool supportChannelMaskNear(audio_channel_mask_t mask, bool isOut) const
    {
        return getChannelMaskNear(mask, isOut) != mask;
    }

    audio_format_t getFormat() const { return mSupportedFormat; }

    audio_format_t getFormatNear(audio_format_t format, bool isOut) const;

    /**
     * Checks if a format belongs to the capabilities.
     * @param[in] format to check upon support
     * @return true if the format is found in the collection of supported format, false otherwise.
     */
    bool supportFormat(audio_format_t format) const { return mSupportedFormat == format; }

    /**
     * @return the default channel mask which is either the first value in the supported channel
     *         mask collection or AUDIO_CHANNEL_NONE if empty.
     *         AUDIO_CHANNEL_NONE will be interpreted by Android as "dynamic".
     */
    audio_channel_mask_t getDefaultChannelMask() const
    {
        return mSupportedChannelMasks.empty() ? AUDIO_CHANNEL_NONE : mSupportedChannelMasks[0];
    }

    /**
     * Checks if a channel mask belongs to the capabilities.
     * @param[in] channel mask to check upon support
     * @return true if the channel mask is found in the collection of supported channel mask,
     *         false otherwise.
     */
    bool supportChannelMask(audio_channel_mask_t mask) const
    {
        return std::find(mSupportedChannelMasks.begin(), mSupportedChannelMasks.end(), mask) !=
               mSupportedChannelMasks.end();
    }

    /**
     * Get the supported rates.
     * @return pipe separated string of rates.
     */
    const std::string getSupportedRates() const;

    /**
     * Get the supported channel masks.
     * @param[in] isOut direction of the stream capabilities requested
     * @return "|" separated string of formats. Empty if invalid or none.
     */
    const std::string getSupportedChannelMasks(bool isOut) const;

    const std::string getSupportedFormat() const;

    void reset();

    bool isChannelMaskDynamic = false;
    bool isRateDynamic = false;
    bool isFormatDynamic = false;

    android::status_t dump(const int fd, int spaces, bool isOut) const;
};

struct AudioCapabilities : public std::vector<AudioCapability>
{
    const std::string getSupportedFormats() const;
    const std::string getSupportedChannelMasks(bool isOut) const;
    const std::string getSupportedRates() const;
    bool add(AudioCapability &element)
    {
        push_back(element);
        return true;
    }
};

} // namespace intel_audio
