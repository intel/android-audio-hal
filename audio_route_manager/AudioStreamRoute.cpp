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
#define LOG_TAG "RouteManager/StreamRoute"
// #define LOG_NDEBUG 0

#include "AudioStreamRoute.hpp"
#include <AudioDevice.hpp>
#include <typeconverter/TypeConverter.hpp>
#include <AudioUtils.hpp>
#include <Direction.hpp>
#include <EffectHelper.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <policy.h>

using namespace std;
using audio_comms::utilities::Log;

namespace intel_audio
{

static uint32_t count[Direction::gNbDirections] = {
    0, 0
};

AudioStreamRoute::AudioStreamRoute(const string &name, bool isOut)
    : IStreamRoute(name, isOut),
      mCurrentStream(NULL),
      mNewStream(NULL),
      mEffectSupported(0),
      mMask(1 << count[isOut]++),
      mIsUsed(false),
      mPreviouslyUsed(false),
      mAudioDevice(nullptr)
{
}

AudioStreamRoute::~AudioStreamRoute()
{
    delete mAudioDevice;
}

void AudioStreamRoute::loadCapabilities()
{
    Log::Debug() << __FUNCTION__ << ": for route " << getName();
    mConfig.loadCapabilities();
}

void AudioStreamRoute::resetCapabilities()
{
    mConfig.resetCapabilities();
}

bool AudioStreamRoute::needReflow()
{
    if (stillUsed() && (mCurrentStream == mNewStream) && mCurrentStream->needReconfigure()) {
        // it is now safe to reset the stream NeedReconfigure flag, route has been marked as
        // need to be reconfigured to be muted and unmuted while the change is taken into account.
        mCurrentStream->resetNeedReconfigure();
        return true;
    }
    return false;
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
    mPreviouslyUsed = mIsUsed;
    mIsUsed = false;
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

    mConfig.setCurrentSampleSpec(mNewStream->streamSampleSpec());
    mNewStream->setNewStreamRoute(this);
    return true;
}

bool AudioStreamRoute::isMatchingWithStream(const IoStream &stream) const
{
    bool verdict = ((stream.isOut() == isOut()) &&
                    areFlagsMatching(stream.getFlagMask()) &&
                    areUseCasesMatching(stream.getUseCaseMask()) &&
                    implementsEffects(stream.getEffectRequested()) &&
                    supportDeviceAddress(stream.getDeviceAddress(), stream.getDevices()) &&
                    supportStreamConfig(stream) &&
                    supportDevices(stream.getDevices()));


    Log::Verbose() << __FUNCTION__ << ": is Route " << getName() << " applicable? "
                   << "\n\t\t\t route direction=" << (isOut() ? "output" : "input")
                   << " stream direction=" << (stream.isOut() ? "output" : "input") << std::hex
                   << " && stream flags mask=0x" << stream.getFlagMask()
                   << " & route applicable flags mask=0x" << getFlagsMask()
                   << " && stream use case mask=0x" << stream.getUseCaseMask()
                   << " & route applicable use case mask=0x" << getUseCaseMask()
                   << " && stream device mask=0x" << stream.getDevices()
                   << " & route applicable device mask=0x" << getSupportedDeviceMask()
                   << " supportStreamConfig(stream)=" << supportStreamConfig(stream)
                   << "\n VERDICT=" << verdict;
    return verdict;
}

bool AudioStreamRoute::supportDeviceAddress(const std::string &streamDeviceAddress,
                                            audio_devices_t device) const
{
    Log::Verbose() << __FUNCTION__ << ": route device address " << mConfig.deviceAddress
                   << ", stream device address " << streamDeviceAddress
                   << ", verdict " <<
        ((!device_distinguishes_on_address(device) && mConfig.deviceAddress.empty())
         || (streamDeviceAddress == mConfig.deviceAddress));

    // If both stream and route do not specify a supported device address, consider as matching
    return (!device_distinguishes_on_address(device) && mConfig.deviceAddress.empty())
           || (streamDeviceAddress == mConfig.deviceAddress);
}

bool AudioStreamRoute::supportDevices(audio_devices_t streamDeviceMask) const
{
    Log::Verbose() << __FUNCTION__ << ": route devices  " << getSupportedDeviceMask()
                   << ", stream device mask" << streamDeviceMask;

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

void AudioStreamRoute::setEffectSupported(const vector<string> &effects)
{
    for (auto effect : effects) {
        mEffectSupportedMask |= EffectHelper::convertEffectNameToProcId(effect);
    }
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
