/*
 * Copyright (C) 2017-2018 Intel Corporation
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

#include <stdint.h>
#include <vector>
#include <map>
#include <string>
#include <string.h>
#include <system/audio.h>
#include <convert/convert.hpp>
#include <assert.h>

namespace intel_audio
{

/**
 * Introduce a primary flag for input as well to manage route applicability for stream symetrically.
 */
static const audio_input_flags_t AUDIO_INPUT_FLAG_PRIMARY =
    static_cast<audio_input_flags_t>(AUDIO_INPUT_FLAG_SYNC << 1);

struct SampleRateTraits
{
    typedef uint32_t Type;
};
struct DeviceTraits
{
    typedef audio_devices_t Type;
};
struct OutputFlagTraits
{
    typedef audio_output_flags_t Type;
};
struct InputFlagTraits
{
    typedef audio_input_flags_t Type;
};
struct FormatTraits
{
    typedef audio_format_t Type;
};
struct ChannelTraits
{
    typedef audio_channel_mask_t Type;
};
struct OutputChannelTraits : public ChannelTraits {};
struct InputChannelTraits : public ChannelTraits {};
struct ChannelIndexTraits : public ChannelTraits {};
struct GainModeTraits
{
    typedef audio_gain_mode_t Type;
};
struct StreamTraits
{
    typedef audio_stream_type_t Type;
};
struct InputSourceTraits
{
    typedef audio_source_t Type;
};

template <typename T>
struct DefaultTraits
{
    typedef T Type;
};

template <class Traits>
static void collectionFromString(const std::string &str,
                                 std::vector<typename Traits::Type> &collection,
                                 const char *del = "|")
{
    char *literal = strdup(str.c_str());
    for (const char *cstr = strtok(literal, del); cstr != NULL; cstr = strtok(NULL, del)) {
        typename Traits::Type value;
        if (audio_comms::utilities::convertTo<std::string, typename Traits::Type>(cstr,
                                                                                  value)) {
            collection.push_back(value);
        }
    }
    free(literal);
}

template <class Traits>
std::string collectionToString(const std::vector<typename Traits::Type> &collection,
                               const char *del = "|")
{
    std::string literals;
    for (auto &value : collection) {
        std::string literal;
        if (audio_comms::utilities::convertTo(value, literal)) {
            literals += literal;
        }
        if (&value != &collection.back()) {
            literals += del;
        }
    }
    return literals;
}

template <class Traits>
class TypeConverter
{
public:
    typedef typename Traits::Type T;
    typedef const std::map<const std::string, T> Table;
    typedef std::vector<T> Collection;

    static bool toEnum(const std::string &literal, T &enumVal);
    static bool toString(const T enumVal, std::string &literal);
    static std::string toString(const T enumVal);

    static void collectionFromString(const std::string &str, Collection &collection,
                                     const char *del);
    static std::string collectionToString(const Collection &collection, const char *del = "|");
    static uint32_t maskFromString(const std::string &str, const char *del);
    static std::string maskToString(uint32_t mask, const char *del = "|");

    static Table mTypeConversion;
};

typedef TypeConverter<DeviceTraits> DeviceConverter;
typedef TypeConverter<OutputFlagTraits> OutputFlagConverter;
typedef TypeConverter<InputFlagTraits> InputFlagConverter;
typedef TypeConverter<FormatTraits> FormatConverter;
typedef TypeConverter<OutputChannelTraits> OutputChannelConverter;
typedef TypeConverter<InputChannelTraits> InputChannelConverter;
typedef TypeConverter<ChannelIndexTraits> ChannelIndexConverter;
typedef TypeConverter<GainModeTraits> GainModeConverter;
typedef TypeConverter<StreamTraits> StreamTypeConverter;
typedef TypeConverter<InputSourceTraits> InputSourceConverter;
typedef TypeConverter<DefaultTraits<audio_port_role_t> > PortRoleConverter;
typedef TypeConverter<DefaultTraits<audio_port_type_t> > PortTypeConverter;

static std::vector<SampleRateTraits::Type> samplingRatesFromString(const std::string &samplingRates,
                                                                   const char *del = "|")
{
    std::vector<SampleRateTraits::Type> samplingRateCollection;
    collectionFromString<SampleRateTraits>(samplingRates, samplingRateCollection, del);
    return samplingRateCollection;
}


static std::string samplingRatesToString(const std::vector<SampleRateTraits::Type> &samplingRates,
                                         const char *del = "|")
{
    return collectionToString<SampleRateTraits>(samplingRates, del);
}

static std::vector<FormatTraits::Type> formatsFromString(const std::string &formats,
                                                         const char *del = "|")
{
    std::vector<FormatTraits::Type> formatCollection;
    FormatConverter::collectionFromString(formats, formatCollection, del);
    return formatCollection;
}

static audio_format_t formatFromString(const std::string &literalFormat)
{
    audio_format_t format = AUDIO_FORMAT_INVALID;
    if (literalFormat.empty()) {
        return format;
    }
    FormatConverter::toEnum(literalFormat, format);
    return format;
}

static audio_channel_mask_t channelMaskFromString(const std::string &literalChannels)
{
    audio_channel_mask_t channels = AUDIO_CHANNEL_INVALID;
    if (OutputChannelConverter::toEnum(literalChannels, channels)) {
        return channels;
    } else if (InputChannelConverter::toEnum(literalChannels, channels)) {
        return channels;
    }
    return AUDIO_CHANNEL_INVALID;
}

static bool channelMaskFromString(const std::string &literalChannels, uint32_t &channels)
{
    if (OutputChannelConverter::toEnum(literalChannels, channels)) {
        return true;
    } else if (InputChannelConverter::toEnum(literalChannels, channels)) {
        return true;
    }
    return false;
}

static std::string channelMaskToString(uint32_t channels, bool isOut)
{
    std::string channelMaskLiteral;
    if (isOut) {
        OutputChannelConverter::toString(channels, channelMaskLiteral);
    } else {
        InputChannelConverter::toString(channels, channelMaskLiteral);
    }
    return channelMaskLiteral;
}

static bool channelsFromString(const std::string &literalChannels, uint32_t &channels)
{
    audio_channel_mask_t channelMask = 0;
    if (OutputChannelConverter::toEnum(literalChannels, channelMask)) {
        channels = audio_channel_count_from_out_mask(channelMask);
        return true;
    } else if (InputChannelConverter::toEnum(literalChannels, channelMask)) {
        channels = audio_channel_count_from_in_mask(channelMask);
        return true;
    }
    return false;
}

static std::vector<ChannelTraits::Type> channelMasksFromString(const std::string &channels,
                                                               const char *del = "|")
{
    std::vector<ChannelTraits::Type> channelMaskCollection;
    OutputChannelConverter::collectionFromString(channels, channelMaskCollection, del);
    InputChannelConverter::collectionFromString(channels, channelMaskCollection, del);
    ChannelIndexConverter::collectionFromString(channels, channelMaskCollection, del);
    return channelMaskCollection;
}

static std::vector<InputChannelTraits::Type> inputChannelMasksFromString(
    const std::string &inChannels,
    const char *del = "|")
{
    std::vector<InputChannelTraits::Type> inputChannelMaskCollection;
    InputChannelConverter::collectionFromString(inChannels, inputChannelMaskCollection, del);
    ChannelIndexConverter::collectionFromString(inChannels, inputChannelMaskCollection, del);
    return inputChannelMaskCollection;
}

static std::vector<OutputChannelTraits::Type> outputChannelMasksFromString(
    const std::string &outChannels,
    const char *del = "|")
{
    std::vector<OutputChannelTraits::Type> outputChannelMaskCollection;
    OutputChannelConverter::collectionFromString(outChannels, outputChannelMaskCollection, del);
    ChannelIndexConverter::collectionFromString(outChannels, outputChannelMaskCollection, del);
    return outputChannelMaskCollection;
}

}  // namespace intel_audio
