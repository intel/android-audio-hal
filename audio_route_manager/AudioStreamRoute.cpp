/*
 * Copyright (C) 2013-2016 Intel Corporation
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
#define LOG_TAG "RouteManager/StreamRoute"

#include "AudioStreamRoute.hpp"
#include <AudioDevice.hpp>
#include <AudioUtils.hpp>
#include <StreamLib.hpp>
#include <EffectHelper.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <tinyalsa/asoundlib.h>
#include <policy.h>

using std::string;
using audio_comms::utilities::Log;

namespace intel_audio
{

AudioStreamRoute::AudioStreamRoute(const string &name, bool isOut)
    : AudioRoute(name, isOut),
      mCurrentStream(NULL),
      mNewStream(NULL),
      mEffectSupported(0),
      mAudioDevice(nullptr)
{
}

AudioStreamRoute::~AudioStreamRoute()
{
    delete mAudioDevice;
}

android::status_t AudioStreamRoute::loadChannelMaskCapabilities()
{
    // Discover supported channel maps from control parameter
    Log::Debug() << __FUNCTION__ << ": Control for channels: " << mConfig.dynamicFormatsControl;

    struct mixer *mixer;
    struct mixer_ctl *ctl;
    unsigned int channelMaskSize;
    int channelCount = 0;

    int cardIndex = AudioUtils::getCardIndexByName(getCardName());
    if (cardIndex < 0) {
        Log::Error() << __FUNCTION__ << ": Failed to get Card Name index " << cardIndex;
        return android::BAD_VALUE;
    }
    mixer = mixer_open(cardIndex);
    if (!mixer) {
        Log::Error() << __FUNCTION__ << ": Failed to open mixer for card " << getCardName();
        return android::BAD_VALUE;
    }
    ctl = mixer_get_ctl_by_name(mixer, mConfig.dynamicChannelMapsControl.c_str());

    if (mixer_ctl_get_type(ctl) != MIXER_CTL_TYPE_INT) {
        Log::Error() << __FUNCTION__ << ": invalid mixer type";
        mixer_close(mixer);
        return android::BAD_VALUE;
    }
    channelMaskSize = mixer_ctl_get_num_values(ctl);

    // Parse the channel allocation array to check if present or not.
    for (uint32_t channelPosition = 0; channelPosition < channelMaskSize; channelPosition++) {
        if (mixer_ctl_get_value(ctl, channelPosition) > 0) {
            ++channelCount;
            if (isOut() && channelCount != 2 && channelCount != 6 && channelCount != 8) {
                // Until now, limit the support to stereo, 5.1 & 7.1
                continue;
            }
            audio_channel_mask_t mask = isOut() ? audio_channel_out_mask_from_count(channelCount) :
                                        audio_channel_in_mask_from_count(channelCount);
            if (mask != AUDIO_CHANNEL_INVALID) {
                mCapabilities.supportedChannelMasks.push_back(mask);
                Log::Debug() << __FUNCTION__ << ": Supported channel mask 0x" << std::hex << mask;
            }
        }
    }
    if (mCapabilities.supportedChannelMasks.empty()) {
        // Fallback on stereo channel map in case of no information retrieved from device
        Log::Error() << __FUNCTION__ << ": No Channel info retrieved, falling back to stereo";
        audio_channel_mask_t mask =
                isOut() ? AUDIO_CHANNEL_OUT_STEREO :
                          AUDIO_CHANNEL_IN_STEREO;
        mCapabilities.supportedChannelMasks.push_back(mask);
    }
    mixer_close(mixer);
    return android::OK;
}

void AudioStreamRoute::loadCapabilities()
{
    Log::Debug() << __FUNCTION__ << ": for route " << getName();

    resetCapabilities();

    if (!mConfig.dynamicChannelMapsControl.empty()) {
        loadChannelMaskCapabilities();
    }
    if (!mConfig.dynamicRatesControl.empty()) {
        Log::Debug() << __FUNCTION__ << ": Control for rate: " << mConfig.dynamicRatesControl;
    }
    if (!mConfig.dynamicFormatsControl.empty()) {
        Log::Debug() << __FUNCTION__ << ": Control for format: " << mConfig.dynamicFormatsControl;
    }
}

void AudioStreamRoute::resetCapabilities()
{
    if (StreamRouteConfig::isDynamic(mConfig.channels)) {
        mCapabilities.supportedChannelMasks.clear();
    }
    if (StreamRouteConfig::isDynamic(mConfig.format)) {
        mCapabilities.supportedFormats.clear();
    }
    if (StreamRouteConfig::isDynamic(mConfig.rate)) {
        mCapabilities.supportedRates.clear();
    }
}

const StreamRouteConfig AudioStreamRoute::getRouteConfig() const
{
    StreamRouteConfig config = mConfig;
    config.rate = mCurrentRate;
    config.format = mCurrentFormat;
    config.channels = isOut() ? audio_channel_count_from_out_mask(mCurrentChannelMask) :
                      audio_channel_count_from_in_mask(mCurrentChannelMask);
    return config;
}

void AudioStreamRoute::updateStreamRouteConfig(const StreamRouteConfig &config)
{
    // Either the name of the Audio Card could be read from proc fs -> use tiny alsa library
    // or the name could not be identified and must be refered by alsa device in asound.conf file
    bool isAlsaPluginDevice = (AudioUtils::getCardIndexByName(config.cardName) < 0);
    if (mAudioDevice == nullptr) {
        mAudioDevice = StreamLib::createAudioDevice(isAlsaPluginDevice);
    }

    Log::Verbose() << __FUNCTION__
                   << ": config for route " << getName() << ":"
                   << "\n\t requirePreEnable=" << config.requirePreEnable
                   << "\n\t  requirePostDisable=" << config.requirePostDisable
                   << "\n\t  cardName=" << config.cardName
                   << "\n\t  deviceId=" << config.deviceId
                   << "\n\t  rate=" << config.rate
                   << "\n\t  silencePrologInMs=" << config.silencePrologInMs
                   << "\n\t  flagMask=" << config.flagMask
                   << "\n\t  channels=" << config.channels
                   << "\n\t  rate=" << config.rate
                   << "\n\t  format=" << static_cast<int32_t>(config.format)
                   << "\n\t  device=" << config.supportedDeviceMask
                   << "\n\t  channel control=" << config.dynamicChannelMapsControl
                   << "\n\t  format control=" << config.dynamicFormatsControl
                   << "\n\t  rate control=" << config.dynamicRatesControl;
    mConfig = config;
    if (!StreamRouteConfig::isDynamic(config.rate)) {
        mCapabilities.supportedRates.push_back(config.rate);
        mCurrentRate = config.rate;
    }
    if (!StreamRouteConfig::isDynamic(mConfig.format)) {
        mCapabilities.supportedFormats.push_back(config.format);
        mCurrentFormat = config.format;
    }
    if (!StreamRouteConfig::isDynamic(mConfig.channels)) {
        audio_channel_mask_t mask = isOut() ? audio_channel_out_mask_from_count(mConfig.channels) :
                                    audio_channel_in_mask_from_count(mConfig.channels);
        mCapabilities.supportedChannelMasks.push_back(mask);
        mCurrentChannelMask = mask;
    }
}

android::status_t AudioStreamRoute::route(bool isPreEnable)
{
    AUDIOCOMMS_ASSERT(mAudioDevice != nullptr, "No valid device attached");
    if (isPreEnable == isPreEnableRequired()) {

        android::status_t err = mAudioDevice->open(getCardName(), getPcmDeviceId(),
                                                   getRouteConfig(), isOut());
        if (err) {

            // Failed to open PCM device -> bailing out
            return err;
        }
    }

    if (!isPreEnable) {

        if (!mAudioDevice->isOpened()) {
            Log::Error() << __FUNCTION__ << ": error opening audio device, cannot route new stream";
            return android::NO_INIT;
        }

        /**
         * Attach the stream to its route only once routing stage is completed
         * to let the audio-parameter-manager performing the required configuration of the
         * audio path.
         */
        android::status_t err = attachNewStream();
        if (err) {

            // Failed to open PCM device -> bailing out
            return err;
        }
    }
    return android::OK;
}

void AudioStreamRoute::unroute(bool isPostDisable)
{
    AUDIOCOMMS_ASSERT(mAudioDevice != nullptr, "No valid device attached");
    if (!isPostDisable) {

        if (!mAudioDevice->isOpened()) {
            Log::Error() << __FUNCTION__
                         << ": error opening audio device, cannot unroute current stream";
            return;
        }

        /**
         * Detach the stream from its route at the beginning of unrouting stage
         * Action of audio-parameter-manager on the audio path may lead to blocking issue, so
         * need to garantee that the stream will not access to the device before unrouting.
         */
        detachCurrentStream();
    }

    if (isPostDisable == isPostDisableRequired()) {

        android::status_t err = mAudioDevice->close();
        if (err) {

            return;
        }
    }
}

void AudioStreamRoute::resetAvailability()
{
    if (mNewStream) {
        mNewStream->resetNewStreamRoute();
        mNewStream = NULL;
    }
    AudioRoute::resetAvailability();
}

bool AudioStreamRoute::setStream(IoStream &stream)
{
    if (stream.isOut() != isOut()) {
        Log::Error() << __FUNCTION__ << ": to route " << getName() << " which has not the same dir";
        return false;
    }
    if (mNewStream != NULL) {
        Log::Error() << __FUNCTION__ << ": route " << getName() << " is busy";
        return false;
    }
    Log::Verbose() << __FUNCTION__ << ": to " << getName() << " route";
    mNewStream = &stream;

    // For each element of the audio configuration:
    // Either the conf of the stream is supported by the route (so use stream conf),
    // or a resampler can be used from the route default conf (so use route default conf)
    mCurrentRate = mCapabilities.supportRate(stream.getSampleRate()) ?
                   stream.getSampleRate() : mCapabilities.getDefaultRate();
    mCurrentFormat = mCapabilities.supportFormat(stream.getFormat()) ?
                     stream.getFormat() : mCapabilities.getDefaultFormat();
    mCurrentChannelMask = mCapabilities.supportChannelMask(stream.getChannels()) ?
                          stream.getChannels() : mCapabilities.getDefaultChannelMask();

    mNewStream->setNewStreamRoute(this);
    return true;
}

bool AudioStreamRoute::isMatchingWithStream(const IoStream &stream) const
{
    Log::Verbose() << __FUNCTION__ << ": is Route " << getName() << " applicable? "
                   << "\n\t\t\t route direction=" << (isOut() ? "output" : "input")
                   << " stream direction=" << (stream.isOut() ? "output" : "input") << std::hex
                   << " && stream flags mask=0x" << stream.getFlagMask()
                   << " & route applicable flags mask=0x" << getFlagsMask()
                   << " && stream use case mask=0x" << stream.getUseCaseMask()
                   << " & route applicable use case mask=0x" << getUseCaseMask();

    return (stream.isOut() == isOut()) &&
           areFlagsMatching(stream.getFlagMask()) &&
           areUseCasesMatching(stream.getUseCaseMask()) &&
           implementsEffects(stream.getEffectRequested()) &&
           supportDeviceAddress(stream.getDeviceAddress(), stream.getDevices()) &&
           supportStreamConfig(stream);
}

bool AudioStreamRoute::supportDeviceAddress(const std::string& streamDeviceAddress, audio_devices_t device) const
{
    Log::Verbose() << __FUNCTION__ << ": gustave route device address " << mConfig.deviceAddress
                 << ", stream device address " << streamDeviceAddress;
    // If both stream and route do not specify a supported device address, consider as matching
    return (!device_distinguishes_on_address(device) || (streamDeviceAddress == mConfig.deviceAddress));
}

bool AudioStreamRoute::supportDevices(audio_devices_t streamDeviceMask) const
{
    return streamDeviceMask != AUDIO_DEVICE_NONE &&
            (getSupportedDeviceMask() & streamDeviceMask) == streamDeviceMask;
}

inline bool AudioStreamRoute::areFlagsMatching(uint32_t streamFlagMask) const
{
    return (streamFlagMask & getFlagsMask()) == streamFlagMask;
}

inline bool AudioStreamRoute::areUseCasesMatching(uint32_t streamUseCaseMask) const
{
    return (streamUseCaseMask & getUseCaseMask()) == streamUseCaseMask;
}

bool AudioStreamRoute::implementsEffects(uint32_t effectMask) const
{
    return (mEffectSupportedMask & effectMask) == effectMask;
}

android::status_t AudioStreamRoute::attachNewStream()
{
    if (mNewStream == NULL) {
        Log::Error() << __FUNCTION__ << ": trying to attach route " << getName()
                     << " to invalid stream";
        return android::DEAD_OBJECT;
    }

    android::status_t err = mNewStream->attachRoute();

    if (err != android::OK) {
        Log::Error() << "Failing to attach route for new stream : " << err;
        return err;
    }

    mCurrentStream = mNewStream;

    return android::OK;
}

android::status_t AudioStreamRoute::detachCurrentStream()
{
    if (mCurrentStream == NULL) {
        Log::Error() << __FUNCTION__ << ": trying to detach route " << getName()
                     << " from invalid stream";
        return android::DEAD_OBJECT;
    }
    mCurrentStream->detachRoute();
    mCurrentStream = NULL;
    return android::OK;
}

void AudioStreamRoute::addEffectSupported(const std::string &effect)
{
    mEffectSupportedMask |= EffectHelper::convertEffectNameToProcId(effect);
}

uint32_t AudioStreamRoute::getLatencyInUs() const
{
    return getSampleSpec().convertFramesToUsec(mConfig.periodSize * mConfig.periodCount);
}

uint32_t AudioStreamRoute::getPeriodInUs() const
{
    return getSampleSpec().convertFramesToUsec(mConfig.periodSize);
}

inline bool AudioStreamRoute::supportRate(uint32_t rate) const
{
    Log::Verbose() << __FUNCTION__ << ": "
                   << (mCapabilities.supportRate(rate) ? "supported by route" :
                      (resamplerSupported(rate) ? "conversion supported" : "NOT SUPPORTED"));
    return mCapabilities.supportRate(rate) || resamplerSupported(rate);
}

inline bool AudioStreamRoute::supportFormat(audio_format_t format) const
{
    Log::Verbose() << __FUNCTION__ << ": "
                   << (mCapabilities.supportFormat(format) ? "supported by route" :
                      (reformatterSupported(format) ? "conversion supported" : "NOT SUPPORTED"));
    return mCapabilities.supportFormat(format) || reformatterSupported(format);
}

inline bool AudioStreamRoute::supportChannelMask(audio_channel_mask_t channelMask) const
{
    Log::Verbose() << __FUNCTION__ << ": "
                   << (mCapabilities.supportChannelMask(channelMask) ? "supported by route" :
                      (remapperSupported(channelMask) ? "conversion supported" : "NOT SUPPORTED"));
    return mCapabilities.supportChannelMask(channelMask) || remapperSupported(channelMask);
}

} // namespace intel_audio
