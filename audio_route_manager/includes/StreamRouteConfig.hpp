/*
 * Copyright (C) 2013-2017 Intel Corporation
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

#include <AudioCapabilities.hpp>
#include <SampleSpec.hpp>
#include <string>

namespace intel_audio
{

struct StreamRouteConfig
{
    bool isOut;
    /**
     * flags to indicate whether the route must be enabled before or after opening the device.
     */
    bool requirePreEnable;

    /**
     * flags to indicate whether the route must be closed before or after opening the device.
     */
    bool requirePostDisable;

    std::string cardName;   /**< Audio card name used by the stream route. */
    uint32_t deviceId;       /**< Audio Device Id used by the stream route. */

    /**
     * pcm configuration supported by the stream route.
     */
    uint32_t periodSize;
    uint32_t periodCount;
    uint32_t startThreshold;
    uint32_t stopThreshold;
    uint32_t silenceThreshold;
    uint32_t availMin;

    AudioCapabilities mAudioCapabilities;

    bool supportSampleSpec(const SampleSpec &spec) const;

    bool supportSampleSpecNear(const SampleSpec &spec) const;

    bool setCurrentSampleSpec(const SampleSpec &streamSpec);

    std::string dynamicChannelMapsControl; /**< Control to retrieve supported channel maps. */
    std::string dynamicFormatsControl; /**< Control to retrieve supported formats. */
    std::string dynamicRatesControl; /**< Control to retrieve supported rates. */

    uint32_t silencePrologInMs; /**< if needed, silence to be appended before valid samples. */
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

    /**
     * Mask of device supported / managed by the stream route.
     * For devices that can be connected / disconnected at runtime, it is mandatory to fill
     * this field in order to be able to retrieve capabilities on newly connected device
     * (for example: HDMI devices, need to retrieve capabilities by reading EDID informations).
     */
    uint32_t supportedDeviceMask;
    std::string deviceAddress; /**< Supported device address. */

    uint32_t mCurrentRate = 0;
    audio_format_t mCurrentFormat = AUDIO_FORMAT_DEFAULT;
    audio_channel_mask_t mCurrentChannelMask = AUDIO_CHANNEL_NONE;

    uint32_t getRate() const;
    audio_format_t getFormat() const;
    audio_channel_mask_t getChannelMask() const;
    uint32_t getChannelCount() const;

    void resetCapabilities();

    void loadCapabilities();

    /**
     * Load the capabilities in term of channel mask supported, i.e. it initializes the vector of
     * supported channel mask (stereo, 5.1, 7.1, ...)
     * @return OK is channel masks supported has been set correctly, error code otherwise.
     */
    android::status_t loadChannelMaskCapabilities(AudioCapability &capability);
};

} // namespace intel_audio
