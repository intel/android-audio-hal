/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
 * Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#define LOG_TAG "RouteManager/StreamRoute"

#include "AudioStreamRoute.hpp"
#include <AudioDevice.hpp>
#include <AudioUtils.hpp>
#include <Stream.hpp>
#include <StreamLib.hpp>
#include <EffectHelper.hpp>
#include <AudioCommsAssert.hpp>
#include <utils/Log.h>

using android_audio_legacy::AudioUtils;
using android_audio_legacy::SampleSpec;
using android::status_t;
using android::NO_ERROR;
using android::NO_MEMORY;
using android::NO_INIT;
using android::OK;
using std::string;

AudioStreamRoute::AudioStreamRoute(const string &name, uint32_t routeIndex)
    : AudioRoute(name, routeIndex),
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
    ALOGV("%s: config for route %s", __FUNCTION__, getName().c_str());

    ALOGV("%s: requirePreEnable=%d", __FUNCTION__, config.requirePreEnable);
    ALOGV("%s: requirePostDisable=%d", __FUNCTION__, config.requirePostDisable);
    ALOGV("%s: cardName=%s", __FUNCTION__, config.cardName);
    ALOGV("%s: deviceId=%d", __FUNCTION__, config.deviceId);
    ALOGV("%s: rate=%d", __FUNCTION__, config.rate);
    ALOGV("%s: silencePrologInMs=%d", __FUNCTION__, config.silencePrologInMs);
    ALOGV("%s: applicabilityMask=0x%X", __FUNCTION__,  config.applicabilityMask);
    ALOGV("%s: channels=%d", __FUNCTION__, config.channels);
    ALOGV("%s: rate=%d", __FUNCTION__, config.rate);
    ALOGV("%s: format=0x%X", __FUNCTION__, config.format);
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

status_t AudioStreamRoute::route(bool isPreEnable)
{
    if (isPreEnable == isPreEnableRequired()) {

        status_t err = mAudioDevice->open(getCardName(), getPcmDeviceId(),
                                          getRouteConfig(), isOut());
        if (err) {

            // Failed to open PCM device -> bailing out
            return err;
        }
    }

    if (!isPreEnable) {

        if (!mAudioDevice->isOpened()) {

            ALOGE("%s: error opening audio device, cannot route new stream", __FUNCTION__);
            return NO_INIT;
        }

        /**
         * Attach the stream to its route only once routing stage is completed
         * to let the audio-parameter-manager performing the required configuration of the
         * audio path.
         */
        status_t err = attachNewStream();
        if (err) {

            // Failed to open PCM device -> bailing out
            return err;
        }
    }
    return OK;
}

void AudioStreamRoute::unroute(bool isPostDisable)
{
    if (!isPostDisable) {

        if (!mAudioDevice->isOpened()) {

            ALOGE("%s: error opening audio device, cannot unroute current stream", __FUNCTION__);
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

        status_t err = mAudioDevice->close();
        if (err) {

            return;
        }
    }
}

void AudioStreamRoute::configure()
{
    if (mCurrentStream != mNewStream) {

        if (!mAudioDevice->isOpened()) {

            ALOGE("%s: error opening audio device, cannot configure any stream", __FUNCTION__);
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

void AudioStreamRoute::setStream(Stream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Fatal: invalid stream parameter!");

    ALOGV("%s to %s route", __FUNCTION__, getName().c_str());
    AUDIOCOMMS_ASSERT(stream->isOut() == isOut(), "Fatal: unexpected stream direction!");

    AUDIOCOMMS_ASSERT(mNewStream == NULL, "Fatal: invalid stream value!");

    mNewStream = stream;
    mNewStream->setNewStreamRoute(this);
}

bool AudioStreamRoute::isApplicable(const Stream *stream) const
{
    AUDIOCOMMS_ASSERT(stream != NULL, "NULL stream");
    uint32_t mask = stream->getApplicabilityMask();
    ALOGV("%s: is Route %s applicable? ", __FUNCTION__, getName().c_str());
    ALOGV("%s: \t\t\t isOut=%s && uiMask=0x%X & _uiApplicableMask[%s]=0x%X", __FUNCTION__,
          isOut() ? "output" : "input",
          mask,
          isOut() ? "output" : "input",
          mConfig.applicabilityMask);

    return AudioRoute::isApplicable() && !isUsed() && (mask & mConfig.applicabilityMask) &&
           implementsEffects(stream->getEffectRequested());
}

bool AudioStreamRoute::implementsEffects(uint32_t effectsMask) const
{
    return (mEffectSupportedMask & effectsMask) == effectsMask;
}

status_t AudioStreamRoute::attachNewStream()
{
    AUDIOCOMMS_ASSERT(mNewStream != NULL, "Fatal: invalid stream value!");

    status_t err = mNewStream->attachRoute();

    if (err != NO_ERROR) {
        ALOGE("Failing to attach route for new stream : %d", err);
        return err;
    }

    mCurrentStream = mNewStream;

    return NO_ERROR;
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
