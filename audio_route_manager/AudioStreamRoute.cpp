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
#define LOG_TAG "RouteManager/StreamRoute"

#include "AudioStreamRoute.hpp"
#include <AudioDevice.hpp>
#include <AudioUtils.hpp>
#include <IoStream.hpp>
#include <StreamLib.hpp>
#include <EffectHelper.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using std::string;
using audio_comms::utilities::Log;

namespace intel_audio
{

AudioStreamRoute::AudioStreamRoute(const string &name)
    : AudioRoute(name),
      mCurrentStream(NULL),
      mNewStream(NULL),
      mEffectSupported(0),
      mAudioDevice(StreamLib::createAudioDevice())
{
}

AudioStreamRoute::~AudioStreamRoute()
{
    delete mAudioDevice;
}

void AudioStreamRoute::updateStreamRouteConfig(const StreamRouteConfig &config)
{
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
                   << "\n\t  format=" << static_cast<int32_t>(config.format);
    mConfig = config;

    mSampleSpec = SampleSpec(mConfig.channels, mConfig.format,
                             mConfig.rate,  mConfig.channelsPolicy);
}

bool AudioStreamRoute::needReflow() const
{
    return mPreviouslyUsed && mIsUsed &&
           (mRoutingStageRequested.test(Flow) || mRoutingStageRequested.test(Path) ||
            mCurrentStream != mNewStream);
}

android::status_t AudioStreamRoute::route(bool isPreEnable)
{
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

void AudioStreamRoute::configure()
{
    if (mCurrentStream != mNewStream) {

        if (!mAudioDevice->isOpened()) {
            Log::Error() << __FUNCTION__
                         << ": error opening audio device, cannot configure any stream";
            return;
        }

        /**
         * Route is still in use, but the stream attached to this route has changed...
         * Unroute previous stream.
         */
        detachCurrentStream();

        // route new stream
        attachNewStream();
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

void AudioStreamRoute::setStream(IoStream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Fatal: invalid stream parameter!");
    Log::Verbose() << __FUNCTION__ << ": to " << getName() << " route";
    AUDIOCOMMS_ASSERT(stream->isOut() == isOut(), "Fatal: unexpected stream direction!");

    AUDIOCOMMS_ASSERT(mNewStream == NULL, "Fatal: invalid stream value!");

    mNewStream = stream;
    mNewStream->setNewStreamRoute(this);
}

bool AudioStreamRoute::isApplicable(const IoStream *stream) const
{
    return AudioRoute::isApplicable() && !isUsed() && isMatchingWithStream(stream);
}

bool AudioStreamRoute::isMatchingWithStream(const IoStream *stream) const
{
    AUDIOCOMMS_ASSERT(stream != NULL, "NULL stream");
    uint32_t streamFlagMask = stream->getFlagMask();
    uint32_t streamUseCaseMask = stream->getUseCaseMask();

    if (stream->isOut()) {
        // If no flags is provided for output, take primary by default
        streamFlagMask = !streamFlagMask ?
                         static_cast<uint32_t>(AUDIO_OUTPUT_FLAG_PRIMARY) : streamFlagMask;
    }

    Log::Verbose() << __FUNCTION__ << ": is Route " << getName() << " applicable? "
                   << "\n\t\t\t route direction=" << (isOut() ? "output" : "input")
                   << " stream direction=" << (stream->isOut() ? "output" : "input") << std::hex
                   << " && stream flags mask=0x" << streamFlagMask
                   << " & route applicable flags mask=0x" << getFlagsMask()
                   << " && stream use case mask=0x" << streamUseCaseMask
                   << " & route applicable use case mask=0x" << getUseCaseMask();

    return (stream->isOut() == isOut()) &&
           areFlagsMatching(streamFlagMask) &&
           areUseCasesMatching(streamUseCaseMask) &&
           implementsEffects(stream->getEffectRequested());
}

inline bool AudioStreamRoute::areFlagsMatching(uint32_t streamFlagMask) const
{
    /**
     * Note that if the streamFlagsMask not 0 (one or more flags are requested), the route selected
     * must expose all flags requested by the stream.
     */
    if (streamFlagMask != 0) {
        return (streamFlagMask & getFlagsMask()) == streamFlagMask;
    }
    /**
     * Note that if the streamFlagsMask is 0 (no particular flags requested), the route selected
     * must not expose also any particular flags to avoid using a dedicated pipe for example.
     */
    return getFlagsMask() == 0;
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
    AUDIOCOMMS_ASSERT(mNewStream != NULL, "Fatal: invalid stream value!");

    android::status_t err = mNewStream->attachRoute();

    if (err != android::OK) {
        Log::Error() << "Failing to attach route for new stream : " << err;
        return err;
    }

    mCurrentStream = mNewStream;

    return android::OK;
}

void AudioStreamRoute::detachCurrentStream()
{
    AUDIOCOMMS_ASSERT(mCurrentStream != NULL, "Fatal: invalid stream value!");

    mCurrentStream->detachRoute();
    mCurrentStream = NULL;
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

} // namespace intel_audio
