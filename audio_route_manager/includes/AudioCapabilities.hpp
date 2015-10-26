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

/**
 * @note: this code is a pure copy paste of policy parser that is expected to be moved to a separate
 * file.
 * As far as the policy uses the same code to read capabilities from stream and to parse the conf
 * file, we need to return the capabilities as a "|" separated string aligned with audio.h
 * definition.
 */
struct StringToEnum
{
    const char *name;
    uint32_t value;
};

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef STRING_TO_ENUM
#define STRING_TO_ENUM(string) { #string, string }
#endif

static const StringToEnum sFormatNameToEnumTable[] = {
    STRING_TO_ENUM(AUDIO_FORMAT_PCM_16_BIT),
    STRING_TO_ENUM(AUDIO_FORMAT_PCM_8_BIT),
    STRING_TO_ENUM(AUDIO_FORMAT_PCM_32_BIT),
    STRING_TO_ENUM(AUDIO_FORMAT_PCM_8_24_BIT),
    STRING_TO_ENUM(AUDIO_FORMAT_PCM_FLOAT),
    STRING_TO_ENUM(AUDIO_FORMAT_PCM_24_BIT_PACKED),
    STRING_TO_ENUM(AUDIO_FORMAT_MP3),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_MAIN),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_LC),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_SSR),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_LTP),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_HE_V1),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_SCALABLE),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_ERLC),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_LD),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_HE_V2),
    STRING_TO_ENUM(AUDIO_FORMAT_AAC_ELD),
    STRING_TO_ENUM(AUDIO_FORMAT_VORBIS),
    STRING_TO_ENUM(AUDIO_FORMAT_HE_AAC_V1),
    STRING_TO_ENUM(AUDIO_FORMAT_HE_AAC_V2),
    STRING_TO_ENUM(AUDIO_FORMAT_OPUS),
    STRING_TO_ENUM(AUDIO_FORMAT_AC3),
    STRING_TO_ENUM(AUDIO_FORMAT_E_AC3),
    STRING_TO_ENUM(AUDIO_FORMAT_DTS),
    STRING_TO_ENUM(AUDIO_FORMAT_DTS_HD),
};

static const StringToEnum sOutChannelsNameToEnumTable[] = {
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_MONO),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_STEREO),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_QUAD),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_5POINT1),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_7POINT1),
};

static const StringToEnum sInChannelsNameToEnumTable[] = {
    STRING_TO_ENUM(AUDIO_CHANNEL_IN_MONO),
    STRING_TO_ENUM(AUDIO_CHANNEL_IN_STEREO),
    STRING_TO_ENUM(AUDIO_CHANNEL_IN_FRONT_BACK),
};

static const char *enumToString(const struct StringToEnum *table, size_t size, uint32_t value)
{
    for (size_t tableIndex = 0; tableIndex < size; tableIndex++) {
        if (table[tableIndex].value == value) {
            return table[tableIndex].name;
        }
    }
    return "";
}

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

    /**
     * Get the supported rates.
     * @return pipe separated string of rates.
     */
    const std::string getSupportedRates() const
    {
        std::string formattedRates;
        for (auto &rate : supportedRates) {
            std::string literalRate;
            if (audio_comms::utilities::convertTo(rate, literalRate)) {
                formattedRates += literalRate;
            }
            if (&rate != &supportedRates.back()) {
                formattedRates += "|";
            }
        }
        return formattedRates;
    }
    /**
     * Get the supported channel masks.
     * @param[in] isOut direction of the stream capabilities requested
     * @return "|" separated string of formats. Empty if invalid or none.
     */
    const std::string getSupportedChannelMasks(bool isOut) const
    {
        std::string formattedMasks;
        for (auto &channelMask : supportedChannelMasks) {
            if (isOut) {
                formattedMasks += enumToString(sOutChannelsNameToEnumTable,
                                               ARRAY_SIZE(sOutChannelsNameToEnumTable),
                                               channelMask);
            } else {
                formattedMasks += enumToString(sInChannelsNameToEnumTable,
                                               ARRAY_SIZE(sInChannelsNameToEnumTable),
                                               channelMask);
            }
            if (&channelMask != &supportedChannelMasks.back()) {
                formattedMasks += "|";
            }
        }
        return formattedMasks;
    }
    /**
     * Get the supported formats.
     * @return pipe separated string of formats. Empty if invalid or none.
     */
    const std::string getSupportedFormats() const
    {
        std::string formattedFormats;
        for (auto &format : supportedFormats) {
            formattedFormats += enumToString(sFormatNameToEnumTable,
                                             ARRAY_SIZE(sFormatNameToEnumTable),
                                             format);
            if (&format != &supportedFormats.back()) {
                formattedFormats += "|";
            }
        }
        return formattedFormats;
    }
};

} // namespace intel_audio
