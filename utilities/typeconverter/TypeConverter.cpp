/*
 * Copyright (C) 2017 Intel Corporation
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

#include "TypeConverter.hpp"
#include <policy.h>

namespace intel_audio
{

template <>
const DeviceConverter::Table DeviceConverter::mTypeConversion = {
    { "AUDIO_DEVICE_OUT_EARPIECE", AUDIO_DEVICE_OUT_EARPIECE },
    { "AUDIO_DEVICE_OUT_SPEAKER", AUDIO_DEVICE_OUT_SPEAKER },
    { "AUDIO_DEVICE_OUT_SPEAKER_SAFE", AUDIO_DEVICE_OUT_SPEAKER_SAFE },
    { "AUDIO_DEVICE_OUT_WIRED_HEADSET", AUDIO_DEVICE_OUT_WIRED_HEADSET },
    { "AUDIO_DEVICE_OUT_WIRED_HEADPHONE", AUDIO_DEVICE_OUT_WIRED_HEADPHONE },
    { "AUDIO_DEVICE_OUT_BLUETOOTH_SCO", AUDIO_DEVICE_OUT_BLUETOOTH_SCO },
    { "AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET", AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET },
    { "AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT", AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT },
    { "AUDIO_DEVICE_OUT_ALL_SCO", AUDIO_DEVICE_OUT_ALL_SCO },
    { "AUDIO_DEVICE_OUT_BLUETOOTH_A2DP", AUDIO_DEVICE_OUT_BLUETOOTH_A2DP },
    { "AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES", AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES },
    { "AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER", AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER },
    { "AUDIO_DEVICE_OUT_ALL_A2DP", AUDIO_DEVICE_OUT_ALL_A2DP },
    { "AUDIO_DEVICE_OUT_HDMI", AUDIO_DEVICE_OUT_HDMI },
    { "AUDIO_DEVICE_OUT_AUX_DIGITAL", AUDIO_DEVICE_OUT_AUX_DIGITAL },
    { "AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET", AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET },
    { "AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET", AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET },
    { "AUDIO_DEVICE_OUT_USB_ACCESSORY", AUDIO_DEVICE_OUT_USB_ACCESSORY },
    { "AUDIO_DEVICE_OUT_USB_DEVICE", AUDIO_DEVICE_OUT_USB_DEVICE },
    { "AUDIO_DEVICE_OUT_ALL_USB", AUDIO_DEVICE_OUT_ALL_USB },
    { "AUDIO_DEVICE_OUT_REMOTE_SUBMIX", AUDIO_DEVICE_OUT_REMOTE_SUBMIX },
    { "AUDIO_DEVICE_OUT_TELEPHONY_TX", AUDIO_DEVICE_OUT_TELEPHONY_TX },
    { "AUDIO_DEVICE_OUT_LINE", AUDIO_DEVICE_OUT_LINE },
    { "AUDIO_DEVICE_OUT_HDMI_ARC", AUDIO_DEVICE_OUT_HDMI_ARC },
    { "AUDIO_DEVICE_OUT_SPDIF", AUDIO_DEVICE_OUT_SPDIF },
    { "AUDIO_DEVICE_OUT_FM", AUDIO_DEVICE_OUT_FM },
    { "AUDIO_DEVICE_OUT_AUX_LINE", AUDIO_DEVICE_OUT_AUX_LINE },
    { "AUDIO_DEVICE_OUT_IP", AUDIO_DEVICE_OUT_IP },
    { "AUDIO_DEVICE_OUT_BUS", AUDIO_DEVICE_OUT_BUS },
    { "AUDIO_DEVICE_OUT_STUB", AUDIO_DEVICE_OUT_STUB },
    { "AUDIO_DEVICE_IN_AMBIENT", AUDIO_DEVICE_IN_AMBIENT },
    { "AUDIO_DEVICE_IN_BUILTIN_MIC", AUDIO_DEVICE_IN_BUILTIN_MIC },
    { "AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET", AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET },
    { "AUDIO_DEVICE_IN_ALL_SCO", AUDIO_DEVICE_IN_ALL_SCO },
    { "AUDIO_DEVICE_IN_WIRED_HEADSET", AUDIO_DEVICE_IN_WIRED_HEADSET },
    { "AUDIO_DEVICE_IN_AUX_DIGITAL", AUDIO_DEVICE_IN_AUX_DIGITAL },
    { "AUDIO_DEVICE_IN_HDMI", AUDIO_DEVICE_IN_HDMI },
    { "AUDIO_DEVICE_IN_TELEPHONY_RX", AUDIO_DEVICE_IN_TELEPHONY_RX },
    { "AUDIO_DEVICE_IN_VOICE_CALL", AUDIO_DEVICE_IN_VOICE_CALL },
    { "AUDIO_DEVICE_IN_BACK_MIC", AUDIO_DEVICE_IN_BACK_MIC },
    { "AUDIO_DEVICE_IN_REMOTE_SUBMIX", AUDIO_DEVICE_IN_REMOTE_SUBMIX },
    { "AUDIO_DEVICE_IN_ANLG_DOCK_HEADSET", AUDIO_DEVICE_IN_ANLG_DOCK_HEADSET },
    { "AUDIO_DEVICE_IN_DGTL_DOCK_HEADSET", AUDIO_DEVICE_IN_DGTL_DOCK_HEADSET },
    { "AUDIO_DEVICE_IN_USB_ACCESSORY", AUDIO_DEVICE_IN_USB_ACCESSORY },
    { "AUDIO_DEVICE_IN_USB_DEVICE", AUDIO_DEVICE_IN_USB_DEVICE },
    { "AUDIO_DEVICE_IN_FM_TUNER", AUDIO_DEVICE_IN_FM_TUNER },
    { "AUDIO_DEVICE_IN_TV_TUNER", AUDIO_DEVICE_IN_TV_TUNER },
    { "AUDIO_DEVICE_IN_LINE", AUDIO_DEVICE_IN_LINE },
    { "AUDIO_DEVICE_IN_SPDIF", AUDIO_DEVICE_IN_SPDIF },
    { "AUDIO_DEVICE_IN_BLUETOOTH_A2DP", AUDIO_DEVICE_IN_BLUETOOTH_A2DP },
    { "AUDIO_DEVICE_IN_LOOPBACK", AUDIO_DEVICE_IN_LOOPBACK },
    { "AUDIO_DEVICE_IN_IP", AUDIO_DEVICE_IN_IP },
    { "AUDIO_DEVICE_IN_BUS", AUDIO_DEVICE_IN_BUS },
    { "AUDIO_DEVICE_IN_STUB", AUDIO_DEVICE_IN_STUB },
};


template <>
const OutputFlagConverter::Table OutputFlagConverter::mTypeConversion = {
    { "AUDIO_OUTPUT_FLAG_DIRECT", AUDIO_OUTPUT_FLAG_DIRECT },
    { "AUDIO_OUTPUT_FLAG_PRIMARY", AUDIO_OUTPUT_FLAG_PRIMARY },
    { "AUDIO_OUTPUT_FLAG_FAST", AUDIO_OUTPUT_FLAG_FAST },
    { "AUDIO_OUTPUT_FLAG_DEEP_BUFFER", AUDIO_OUTPUT_FLAG_DEEP_BUFFER },
    { "AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD", AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD },
    { "AUDIO_OUTPUT_FLAG_NON_BLOCKING", AUDIO_OUTPUT_FLAG_NON_BLOCKING },
    { "AUDIO_OUTPUT_FLAG_HW_AV_SYNC", AUDIO_OUTPUT_FLAG_HW_AV_SYNC },
    { "AUDIO_OUTPUT_FLAG_TTS", AUDIO_OUTPUT_FLAG_TTS },
    { "AUDIO_OUTPUT_FLAG_RAW", AUDIO_OUTPUT_FLAG_RAW },
    { "AUDIO_OUTPUT_FLAG_SYNC", AUDIO_OUTPUT_FLAG_SYNC },
    { "AUDIO_OUTPUT_FLAG_IEC958_NONAUDIO", AUDIO_OUTPUT_FLAG_IEC958_NONAUDIO },
};

template <>
const InputFlagConverter::Table InputFlagConverter::mTypeConversion = {
    { "AUDIO_INPUT_FLAG_FAST", AUDIO_INPUT_FLAG_FAST },
    { "AUDIO_INPUT_FLAG_HW_HOTWORD", AUDIO_INPUT_FLAG_HW_HOTWORD },
    { "AUDIO_INPUT_FLAG_RAW", AUDIO_INPUT_FLAG_RAW },
    { "AUDIO_INPUT_FLAG_SYNC", AUDIO_INPUT_FLAG_SYNC },
    { "AUDIO_INPUT_FLAG_PRIMARY", AUDIO_INPUT_FLAG_PRIMARY },
};

template <>
const FormatConverter::Table FormatConverter::mTypeConversion = {
    { "AUDIO_FORMAT_PCM_16_BIT", AUDIO_FORMAT_PCM_16_BIT },
    { "AUDIO_FORMAT_PCM_8_BIT", AUDIO_FORMAT_PCM_8_BIT },
    { "AUDIO_FORMAT_PCM_32_BIT", AUDIO_FORMAT_PCM_32_BIT },
    { "AUDIO_FORMAT_PCM_8_24_BIT", AUDIO_FORMAT_PCM_8_24_BIT },
    { "AUDIO_FORMAT_PCM_FLOAT", AUDIO_FORMAT_PCM_FLOAT },
    { "AUDIO_FORMAT_PCM_24_BIT_PACKED", AUDIO_FORMAT_PCM_24_BIT_PACKED },
    { "AUDIO_FORMAT_MP3", AUDIO_FORMAT_MP3 },
    { "AUDIO_FORMAT_AAC", AUDIO_FORMAT_AAC },
    { "AUDIO_FORMAT_AAC_MAIN", AUDIO_FORMAT_AAC_MAIN },
    { "AUDIO_FORMAT_AAC_LC", AUDIO_FORMAT_AAC_LC },
    { "AUDIO_FORMAT_AAC_SSR", AUDIO_FORMAT_AAC_SSR },
    { "AUDIO_FORMAT_AAC_LTP", AUDIO_FORMAT_AAC_LTP },
    { "AUDIO_FORMAT_AAC_HE_V1", AUDIO_FORMAT_AAC_HE_V1 },
    { "AUDIO_FORMAT_AAC_SCALABLE", AUDIO_FORMAT_AAC_SCALABLE },
    { "AUDIO_FORMAT_AAC_ERLC", AUDIO_FORMAT_AAC_ERLC },
    { "AUDIO_FORMAT_AAC_LD", AUDIO_FORMAT_AAC_LD },
    { "AUDIO_FORMAT_AAC_HE_V2", AUDIO_FORMAT_AAC_HE_V2 },
    { "AUDIO_FORMAT_AAC_ELD", AUDIO_FORMAT_AAC_ELD },
    { "AUDIO_FORMAT_VORBIS", AUDIO_FORMAT_VORBIS },
    { "AUDIO_FORMAT_HE_AAC_V1", AUDIO_FORMAT_HE_AAC_V1 },
    { "AUDIO_FORMAT_HE_AAC_V2", AUDIO_FORMAT_HE_AAC_V2 },
    { "AUDIO_FORMAT_OPUS", AUDIO_FORMAT_OPUS },
    { "AUDIO_FORMAT_AC3", AUDIO_FORMAT_AC3 },
    { "AUDIO_FORMAT_E_AC3", AUDIO_FORMAT_E_AC3 },
    { "AUDIO_FORMAT_DTS", AUDIO_FORMAT_DTS },
    { "AUDIO_FORMAT_DTS_HD", AUDIO_FORMAT_DTS_HD },
};

template <>
const OutputChannelConverter::Table OutputChannelConverter::mTypeConversion = {
    { "AUDIO_CHANNEL_OUT_MONO", AUDIO_CHANNEL_OUT_MONO },
    { "AUDIO_CHANNEL_OUT_STEREO", AUDIO_CHANNEL_OUT_STEREO },
    { "AUDIO_CHANNEL_OUT_QUAD", AUDIO_CHANNEL_OUT_QUAD },
    { "AUDIO_CHANNEL_OUT_QUAD_SIDE", AUDIO_CHANNEL_OUT_QUAD_SIDE },
    { "AUDIO_CHANNEL_OUT_5POINT1", AUDIO_CHANNEL_OUT_5POINT1 },
    { "AUDIO_CHANNEL_OUT_5POINT1_SIDE", AUDIO_CHANNEL_OUT_5POINT1_SIDE },
    { "AUDIO_CHANNEL_OUT_7POINT1", AUDIO_CHANNEL_OUT_7POINT1 },
};

template <>
const InputChannelConverter::Table InputChannelConverter::mTypeConversion = {
    { "AUDIO_CHANNEL_IN_MONO", AUDIO_CHANNEL_IN_MONO },
    { "AUDIO_CHANNEL_IN_STEREO", AUDIO_CHANNEL_IN_STEREO },
    { "AUDIO_CHANNEL_IN_FRONT_BACK", AUDIO_CHANNEL_IN_FRONT_BACK },
};

template <>
const ChannelIndexConverter::Table ChannelIndexConverter::mTypeConversion = {
    { "AUDIO_CHANNEL_INDEX_MASK_1", static_cast<audio_channel_mask_t>(AUDIO_CHANNEL_INDEX_MASK_1) },
    { "AUDIO_CHANNEL_INDEX_MASK_2", static_cast<audio_channel_mask_t>(AUDIO_CHANNEL_INDEX_MASK_2) },
    { "AUDIO_CHANNEL_INDEX_MASK_3", static_cast<audio_channel_mask_t>(AUDIO_CHANNEL_INDEX_MASK_3) },
    { "AUDIO_CHANNEL_INDEX_MASK_4", static_cast<audio_channel_mask_t>(AUDIO_CHANNEL_INDEX_MASK_4) },
    { "AUDIO_CHANNEL_INDEX_MASK_5", static_cast<audio_channel_mask_t>(AUDIO_CHANNEL_INDEX_MASK_5) },
    { "AUDIO_CHANNEL_INDEX_MASK_6", static_cast<audio_channel_mask_t>(AUDIO_CHANNEL_INDEX_MASK_6) },
    { "AUDIO_CHANNEL_INDEX_MASK_7", static_cast<audio_channel_mask_t>(AUDIO_CHANNEL_INDEX_MASK_7) },
    { "AUDIO_CHANNEL_INDEX_MASK_8", static_cast<audio_channel_mask_t>(AUDIO_CHANNEL_INDEX_MASK_8) },
};

template <>
const GainModeConverter::Table GainModeConverter::mTypeConversion = {
    { "AUDIO_GAIN_MODE_JOINT", AUDIO_GAIN_MODE_JOINT },
    { "AUDIO_GAIN_MODE_CHANNELS", AUDIO_GAIN_MODE_CHANNELS },
    { "AUDIO_GAIN_MODE_RAMP", AUDIO_GAIN_MODE_RAMP },
};

template <>
const StreamTypeConverter::Table StreamTypeConverter::mTypeConversion = {
    { "AUDIO_STREAM_VOICE_CALL", AUDIO_STREAM_VOICE_CALL },
    { "AUDIO_STREAM_SYSTEM", AUDIO_STREAM_SYSTEM },
    { "AUDIO_STREAM_RING", AUDIO_STREAM_RING },
    { "AUDIO_STREAM_MUSIC", AUDIO_STREAM_MUSIC },
    { "AUDIO_STREAM_ALARM", AUDIO_STREAM_ALARM },
    { "AUDIO_STREAM_NOTIFICATION", AUDIO_STREAM_NOTIFICATION },
    { "AUDIO_STREAM_BLUETOOTH_SCO", AUDIO_STREAM_BLUETOOTH_SCO },
    { "AUDIO_STREAM_ENFORCED_AUDIBLE", AUDIO_STREAM_ENFORCED_AUDIBLE },
    { "AUDIO_STREAM_DTMF", AUDIO_STREAM_DTMF },
    { "AUDIO_STREAM_TTS", AUDIO_STREAM_TTS },
    { "AUDIO_STREAM_ACCESSIBILITY", AUDIO_STREAM_ACCESSIBILITY },
    { "AUDIO_STREAM_REROUTING", AUDIO_STREAM_REROUTING },
    { "AUDIO_STREAM_PATCH", AUDIO_STREAM_PATCH },
};

template <>
const InputSourceConverter::Table InputSourceConverter::mTypeConversion = {
    { "AUDIO_SOURCE_MIC", AUDIO_SOURCE_MIC },
    { "AUDIO_SOURCE_VOICE_UPLINK", AUDIO_SOURCE_VOICE_UPLINK },
    { "AUDIO_SOURCE_VOICE_DOWNLINK", AUDIO_SOURCE_VOICE_DOWNLINK },
    { "AUDIO_SOURCE_VOICE_CALL", AUDIO_SOURCE_VOICE_CALL },
    { "AUDIO_SOURCE_CAMCORDER", AUDIO_SOURCE_CAMCORDER },
    { "AUDIO_SOURCE_VOICE_RECOGNITION", AUDIO_SOURCE_VOICE_RECOGNITION },
    { "AUDIO_SOURCE_VOICE_COMMUNICATION", AUDIO_SOURCE_VOICE_COMMUNICATION },
    { "AUDIO_SOURCE_REMOTE_SUBMIX", AUDIO_SOURCE_REMOTE_SUBMIX },
    { "AUDIO_SOURCE_UNPROCESSED", AUDIO_SOURCE_UNPROCESSED },
    { "AUDIO_SOURCE_FM_TUNER", static_cast<audio_source_t>(AUDIO_SOURCE_CNT) },
    { "AUDIO_SOURCE_HOTWORD", static_cast<audio_source_t>(AUDIO_SOURCE_CNT + 1) },
};

template <>
const PortRoleConverter::Table PortRoleConverter::mTypeConversion = {
    { "AUDIO_PORT_ROLE_NONE", AUDIO_PORT_ROLE_NONE },
    { "AUDIO_PORT_ROLE_SOURCE", AUDIO_PORT_ROLE_SOURCE },
    { "AUDIO_PORT_ROLE_SINK", AUDIO_PORT_ROLE_SINK },
};

template <>
const PortTypeConverter::Table PortTypeConverter::mTypeConversion = {
    { "AUDIO_PORT_TYPE_NONE", AUDIO_PORT_TYPE_NONE },
    { "AUDIO_PORT_TYPE_DEVICE", AUDIO_PORT_TYPE_DEVICE },
    { "AUDIO_PORT_TYPE_MIX", AUDIO_PORT_TYPE_MIX },
    { "AUDIO_PORT_TYPE_SESSION", AUDIO_PORT_TYPE_SESSION },
};

template <class Traits>
bool TypeConverter<Traits>::toEnum(const std::string &literal, T &enumVal)
{
    try {
        enumVal = mTypeConversion.at(literal);
        return true;
    }
    catch (std::out_of_range &) {
        return false;
    }
    return false;
}

template <class Traits>
bool TypeConverter<Traits>::toString(const T enumVal, std::string &literal)
{
    for (const auto &candidate : mTypeConversion) {
        if (candidate.second == enumVal) {
            literal = candidate.first;
            return true;
        }
    }
    return false;
}

template <class Traits>
std::string TypeConverter<Traits>::toString(const T enumVal)
{
    std::string literal;
    toString(enumVal, literal);
    return literal;
}

template <class Traits>
void TypeConverter<Traits>::collectionFromString(const std::string &str,
                                                 Collection &collection,
                                                 const char *del)
{
    char *literal = strdup(str.c_str());

    for (const char *cstr = strtok(literal, del); cstr != NULL; cstr = strtok(NULL, del)) {
        typename Traits::Type value;
        if (toEnum(cstr, value)) {
            collection.push_back(value);
        }
    }
    free(literal);
}

template <class Traits>
std::string TypeConverter<Traits>::collectionToString(const Collection &collection, const char *del)
{
    std::string collectionLiteral;

    for (const auto &element : collection) {
        collectionLiteral += toString(element);
        if (&element != &collection.back()) {
            collectionLiteral += del;
        }
    }
    return collectionLiteral;
}

template <class Traits>
uint32_t TypeConverter<Traits>::maskFromString(const std::string &str, const char *del)
{
    uint32_t mask = 0;
    char *literal = strdup(str.c_str());
    for (const char *cstr = strtok(literal, del); cstr != NULL; cstr = strtok(NULL, del)) {
        typename Traits::Type type;
        if (not toEnum(cstr, type)) {
            free(literal);
            return 0;
        }
        mask |= static_cast<uint32_t>(type);
    }
    free(literal);
    return mask;
}

template <class Traits>
std::string TypeConverter<Traits>::maskToString(uint32_t mask, const char *del)
{
    std::string formattedMasks;
    for (const auto &candidate : mTypeConversion) {
        if ((mask & candidate.second) == candidate.second) {
            if (not formattedMasks.empty()) {
                formattedMasks += del;
            }
            formattedMasks +=  candidate.first;
        }
    }
    return formattedMasks;
}

template <>
std::string TypeConverter<DeviceTraits>::maskToString(uint32_t mask, const char *del)
{
    std::string literalDevices;
    for (const auto &candidate : mTypeConversion) {
        if ((audio_is_output_devices(candidate.second) == audio_is_output_devices(mask)) &&
            ((mask & candidate.second) == candidate.second)) {
            if (not literalDevices.empty()) {
                literalDevices += del;
            }
            literalDevices +=  candidate.first;
        }
    }
    return literalDevices;
}

template class TypeConverter<DeviceTraits>;
template class TypeConverter<OutputFlagTraits>;
template class TypeConverter<InputFlagTraits>;
template class TypeConverter<FormatTraits>;
template class TypeConverter<OutputChannelTraits>;
template class TypeConverter<InputChannelTraits>;
template class TypeConverter<ChannelIndexTraits>;
template class TypeConverter<GainModeTraits>;
template class TypeConverter<StreamTraits>;
template class TypeConverter<InputSourceTraits>;
template class TypeConverter<DefaultTraits<audio_port_role_t> >;
template class TypeConverter < DefaultTraits < audio_port_type_t >>;

}  // namespace intel_audio
