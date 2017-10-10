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
#define LOG_TAG "RouteManager/StreamRouteConfig"
// #define LOG_NDEBUG 0

#include "StreamRouteConfig.hpp"
#include <AudioConversion.hpp>
#include <tinyalsa/asoundlib.h>
#include <AudioUtils.hpp>
#include <convert.hpp>
#include <utilities/Log.hpp>
#include <string>

using namespace std;
using audio_comms::utilities::Log;
using audio_comms::utilities::convertTo;

namespace intel_audio
{

bool StreamRouteConfig::supportSampleSpec(const SampleSpec &spec) const
{
    for (const auto &capabilities : mAudioCapabilities) {
        if (capabilities.supportFormat(spec.getFormat())) {
            if (capabilities.supportRate(spec.getSampleRate()) &&
                capabilities.supportChannelMask(spec.getChannelMask())) {
                return true;
            }
        }
    }
    return false;
}

bool StreamRouteConfig::supportSampleSpecNear(const SampleSpec &spec) const
{
    // Try with conversion...
    for (const auto &capabilities : mAudioCapabilities) {
        audio_format_t streamFormat = spec.getFormat();
        audio_format_t routeFormat = capabilities.getFormat();
        if (capabilities.supportFormat(streamFormat) ||
            (isOut ? AudioConversion::supportReformat(streamFormat, routeFormat) :
             AudioConversion::supportReformat(routeFormat, streamFormat))) {
            if (capabilities.supportRateNear(spec.getSampleRate(), isOut) &&
                capabilities.supportChannelMaskNear(spec.getChannelMask(), isOut)) {
                return true;
            }
        }
    }
    return false;
}

bool StreamRouteConfig::setCurrentSampleSpec(const SampleSpec &streamSpec)
{
    // Beeing optimistic, try without conversion first
    if (supportSampleSpec(streamSpec)) {
        mCurrentRate = streamSpec.getSampleRate();
        mCurrentFormat = streamSpec.getFormat();
        mCurrentChannelMask = streamSpec.getChannelMask();
        return true;
    }
    // Try with conversion...
    for (const auto &capabilities : mAudioCapabilities) {
        audio_format_t formatNear = capabilities.getFormatNear(streamSpec.getFormat(), isOut);
        uint32_t rateNear = capabilities.getRateNear(streamSpec.getSampleRate(), isOut);
        audio_channel_mask_t maskNear = capabilities.getChannelMaskNear(
            streamSpec.getChannelMask(), isOut);

        if ((formatNear != AUDIO_FORMAT_DEFAULT) && (rateNear != 0) &&
            (maskNear != AUDIO_CHANNEL_NONE)) {
            mCurrentRate = rateNear;
            mCurrentFormat = formatNear;
            mCurrentChannelMask = maskNear;
            return true;
        }
    }
    return false;
}

uint32_t StreamRouteConfig::getRate() const
{
    return (mCurrentRate !=
            0) ? mCurrentRate : mAudioCapabilities.empty() ? 0 : mAudioCapabilities[0].
           getDefaultRate();
}

audio_format_t StreamRouteConfig::getFormat() const
{
    return (mCurrentFormat != AUDIO_FORMAT_DEFAULT) ?
           mCurrentFormat : mAudioCapabilities.empty() ? AUDIO_FORMAT_DEFAULT : mAudioCapabilities[0
           ].getFormat();
}

audio_channel_mask_t StreamRouteConfig::getChannelMask() const
{
    return (mCurrentChannelMask != AUDIO_CHANNEL_NONE) ?
           mCurrentChannelMask : mAudioCapabilities.empty() ? AUDIO_CHANNEL_NONE :
           mAudioCapabilities[0].getDefaultChannelMask();
}

uint32_t StreamRouteConfig::getChannelCount() const
{
    return isOut ? audio_channel_count_from_out_mask(getChannelMask()) :
           audio_channel_count_from_in_mask(getChannelMask());
}

void StreamRouteConfig::resetCapabilities()
{
    for (auto &capabilities : mAudioCapabilities) {
        capabilities.reset();
    }
}

void StreamRouteConfig::loadCapabilities()
{
    resetCapabilities();

    for (auto &capability : mAudioCapabilities) {
        if (capability.isChannelMaskDynamic) {
            loadChannelMaskCapabilities(capability);
        }
        if (capability.isRateDynamic) {
            Log::Debug() << __FUNCTION__ << ": Control for rate: " << dynamicRatesControl;
        }
        if (capability.isFormatDynamic) {
            Log::Debug() << __FUNCTION__ << ": Control for format: " << dynamicFormatsControl;
        }
    }
}

/**
 * Load the capabilities in term of channel mask supported, i.e. it initializes the vector of
 * supported channel mask (stereo, 5.1, 7.1, ...)
 * @return OK is channel masks supported has been set correctly, error code otherwise.
 */
android::status_t StreamRouteConfig::loadChannelMaskCapabilities(AudioCapability &capability)
{
    // Discover supported channel maps from control parameter
    Log::Debug() << __FUNCTION__ << ": Control for channels: " << dynamicChannelMapsControl;

    struct mixer *mixer;
    struct mixer_ctl *ctl;
    unsigned int channelMaskSize;
    int channelCount = 0;

    int cardIndex = AudioUtils::getCardIndexByName(cardName.c_str());
    if (cardIndex < 0) {
        Log::Error() << __FUNCTION__ << ": Failed to get Card Name index " << cardIndex;
        return android::BAD_VALUE;
    }
    mixer = mixer_open(cardIndex);
    if (!mixer) {
        Log::Error() << __FUNCTION__ << ": Failed to open mixer for card " << cardName.c_str();
        return android::BAD_VALUE;
    }
    uint32_t controlNumber;
    if (convertTo<std::string, uint32_t>(dynamicChannelMapsControl, controlNumber)) {
        ctl = mixer_get_ctl(mixer, controlNumber);
    } else {
        ctl = mixer_get_ctl_by_name(mixer, dynamicChannelMapsControl.c_str());
    }
    if (mixer_ctl_get_type(ctl) != MIXER_CTL_TYPE_INT) {
        audio_comms::utilities::Log::Error() << __FUNCTION__ << ": invalid mixer type";
        mixer_close(mixer);
        return android::BAD_VALUE;
    }
    channelMaskSize = mixer_ctl_get_num_values(ctl);

    // Parse the channel allocation array to check if present or not.
    for (uint32_t channelPosition = 0; channelPosition < channelMaskSize; channelPosition++) {
        if (mixer_ctl_get_value(ctl, channelPosition) > 0) {
            ++channelCount;
            if (isOut && channelCount != 2 && channelCount != 6 && channelCount != 8) {
                // Until now, limit the support to stereo, 5.1 & 7.1
                continue;
            }
            audio_channel_mask_t mask = isOut ? audio_channel_out_mask_from_count(channelCount) :
                                        audio_channel_in_mask_from_count(channelCount);
            if (mask != AUDIO_CHANNEL_INVALID) {
                capability.mSupportedChannelMasks.push_back(mask);
                Log::Debug() << __FUNCTION__ << ": Supported channel mask 0x" << std::hex << mask;
            }
        }
    }
    if (capability.mSupportedChannelMasks.empty()) {
        // Fallback on stereo channel map in case of no information retrieved from device
        Log::Error() << __FUNCTION__ << ": No Channel info retrieved, falling back to stereo";
        audio_channel_mask_t mask = isOut ? AUDIO_CHANNEL_OUT_STEREO : AUDIO_CHANNEL_IN_STEREO;
        capability.mSupportedChannelMasks.push_back(mask);
    }
    mixer_close(mixer);
    return android::OK;
}

} // namespace intel_audio
