/*
 * INTEL CONFIDENTIAL
 * Copyright © 2013 Intel
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
 * disclosed in any way without Intel’s prior express written permission.
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
      _currentStream(NULL),
      _newStream(NULL),
      _effectSupported(0),
      _audioDevice(StreamLib::createAudioDevice())
{
}

AudioStreamRoute::~AudioStreamRoute()
{
    delete _audioDevice;
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
    _config = config;

    _sampleSpec = SampleSpec(_config.channels, _config.format,
                             _config.rate,  _config.channelsPolicy);
}

bool AudioStreamRoute::needReflow() const
{
    return _previouslyUsed && _isUsed &&
           (_routingStageRequested.test(Flow) || _routingStageRequested.test(Path) ||
            _currentStream != _newStream);
}

status_t AudioStreamRoute::route(bool isPreEnable)
{
    if (isPreEnable == isPreEnableRequired()) {

        status_t err = _audioDevice->open(getCardName(), getPcmDeviceId(),
                                          getRouteConfig(), isOut());
        if (err) {

            // Failed to open PCM device -> bailing out
            return err;
        }
    }

    if (!isPreEnable) {

        if (!_audioDevice->isOpened()) {

            ALOGE("%s: audio device not found, cannot route the stream", __FUNCTION__);
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

        /**
         * Detach the stream from its route at the beginning of unrouting stage
         * Action of audio-parameter-manager on the audio path may lead to blocking issue, so
         * need to garantee that the stream will not access to the device before unrouting.
         */
        detachCurrentStream();
    }

    if (isPostDisable == isPostDisableRequired()) {

        status_t err = _audioDevice->close();
        if (err) {

            return;
        }
    }
}

void AudioStreamRoute::configure()
{
    if (_currentStream != _newStream) {

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
    if (_newStream) {

        _newStream->resetNewStreamRoute();
        _newStream = NULL;
    }
    AudioRoute::resetAvailability();
}

void AudioStreamRoute::setStream(Stream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Fatal: invalid stream parameter!");

    ALOGV("%s to %s route", __FUNCTION__, getName().c_str());
    AUDIOCOMMS_ASSERT(stream->isOut() == isOut(), "Fatal: unexpected stream direction!");

    AUDIOCOMMS_ASSERT(_newStream == NULL, "Fatal: invalid stream value!");

    _newStream = stream;
    _newStream->setNewStreamRoute(this);
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
          _config.applicabilityMask);

    return AudioRoute::isApplicable() && !isUsed() && (mask & _config.applicabilityMask) &&
           implementsEffects(stream->getEffectRequested());
}

bool AudioStreamRoute::implementsEffects(uint32_t effectsMask) const
{
    return (_effectSupportedMask & effectsMask) == effectsMask;
}

status_t AudioStreamRoute::attachNewStream()
{
    AUDIOCOMMS_ASSERT(_newStream != NULL, "Fatal: invalid stream value!");

    status_t err = _newStream->attachRoute();

    if (err != NO_ERROR) {

        // Failed to open output stream -> bailing out
        return err;
    }
    _currentStream = _newStream;

    return NO_ERROR;
}

void AudioStreamRoute::detachCurrentStream()
{
    AUDIOCOMMS_ASSERT(_currentStream != NULL, "Fatal: invalid stream value!");

    _currentStream->detachRoute();
    _currentStream = NULL;
}

void AudioStreamRoute::addEffectSupported(const std::string &effect)
{
    _effectSupportedMask |= EffectHelper::convertEffectNameToProcId(effect);
}

uint32_t AudioStreamRoute::getLatencyInUs() const
{
    return getSampleSpec().convertFramesToUsec(_config.periodSize * _config.periodCount);
}

uint32_t AudioStreamRoute::getPeriodInUs() const
{
    return getSampleSpec().convertFramesToUsec(_config.periodSize);
}
