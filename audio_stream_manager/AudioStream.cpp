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
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioStream"

#include "AudioIntelHAL.hpp"
#include "AudioStream.hpp"

#include <AudioCommsAssert.hpp>
#include "Property.h"
#include <AudioConversion.hpp>
#include <cutils/bitops.h>
#include <cutils/properties.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <media/AudioRecord.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utils/Log.h>
#include <utils/String8.h>

using namespace android;
using audio_comms::utilities::Direction;

namespace android_audio_legacy
{

/**
 * Audio dump properties management (set with setprop)
 */
const std::string AudioStream::dumpBeforeConvProps[Direction::_nbDirections] = {
    "media.dump_input.befconv", "media.dump_output.befconv"
};

const std::string AudioStream::dumpAfterConvProps[Direction::_nbDirections] = {
    "media.dump_input.aftconv", "media.dump_output.aftconv"
};

AudioStream::AudioStream(AudioIntelHAL *parent)
    : _parent(parent),
      _standby(true),
      _devices(0),
      _audioConversion(new AudioConversion),
      _latencyMs(0),
      _applicabilityMask(0),
      _dumpBeforeConv(NULL),
      _dumpAfterConv(NULL)
{
}

AudioStream::~AudioStream()
{
    setStandby(true);

    delete _audioConversion;
    delete _dumpAfterConv;
    delete _dumpBeforeConv;
}

status_t AudioStream::set(int *format, uint32_t *channels, uint32_t *rate)
{
    bool bad_channels = false;
    bool bad_rate = false;
    bool bad_format = false;

    ALOGV("%s() -- IN", __FUNCTION__);

    if (channels != NULL) {

        if (*channels != 0) {

            ALOGD("%s(requested channels: 0x%x (popcount returns %d))",
                  __FUNCTION__, *channels, popcount(*channels));
            // Always accept the channels requested by the client
            // as far as the channel count is supported
            _sampleSpec.setChannelMask(*channels);

            if (popcount(*channels) > 2) {

                ALOGD("%s: channels=(0x%x, %d) not supported", __FUNCTION__, *channels,
                      popcount(*channels));
                bad_channels = true;
            }
        }
        if ((bad_channels) || (*channels == 0)) {

            // No channels information was provided by the client
            // or not supported channels
            // Use default: stereo
            if (isOut()) {

                *channels = AudioSystem::CHANNEL_OUT_FRONT_LEFT |
                            AudioSystem::CHANNEL_OUT_FRONT_RIGHT;
            } else {

                *channels = AudioSystem::CHANNEL_IN_LEFT | AudioSystem::CHANNEL_IN_RIGHT;
            }
            _sampleSpec.setChannelMask(*channels);
        }
        ALOGD("%s: set channels to 0x%x", __FUNCTION__, *channels);

        // Resampler is always working @ the channel count of the HAL
        _sampleSpec.setChannelCount(popcount(_sampleSpec.getChannelMask()));
    }

    if (rate != NULL) {

        if (*rate != 0) {

            ALOGD("%s(requested rate: %d))", __FUNCTION__, *rate);
            // Always accept the rate provided by the client
            _sampleSpec.setSampleRate(*rate);
        }
        if ((bad_rate) || (*rate == 0)) {

            // No rate information was provided by the client
            // or set rate error
            // Use default HAL rate
            *rate = AudioStream::_defaultSampleRate;
            _sampleSpec.setSampleRate(*rate);
        }
        ALOGD("%s: set rate to %d", __FUNCTION__, *rate);
    }

    if (format != NULL) {

        if (*format != 0) {

            ALOGD("%s(requested format: %d))", __FUNCTION__, *format);
            // Always accept the rate provided by the client
            // as far as this rate is supported
            if ((*format != AUDIO_FORMAT_PCM_16_BIT) && (*format != AUDIO_FORMAT_PCM_8_24_BIT)) {

                ALOGD("%s: format=(0x%x) not supported", __FUNCTION__, *format);
                bad_format = true;
            }

            _sampleSpec.setFormat(*format);
        }
        if ((bad_format) || (*format == 0)) {

            // No format provided or set format error
            // Use default HAL format
            *format = AudioStream::_defaultFormat;
            _sampleSpec.setFormat(*format);
        }
        ALOGD("%s : set format to %d (%d)", __FUNCTION__, *format, this->format());
    }


    if (bad_channels || bad_rate || bad_format) {

        return BAD_VALUE;
    }

    ALOGD("%s() -- OUT", __FUNCTION__);
    return NO_ERROR;
}

String8 AudioStream::getParameters(const String8 &keys)
{
    AudioParameter param = AudioParameter(keys);
    String8 value;
    String8 key = String8(AudioParameter::keyRouting);

    if (param.get(key, value) == NO_ERROR) {

        param.addInt(key, static_cast<int>(getDevices()));
    }

    LOGV("getParameters() %s", param.toString().string());
    return param.toString();
}

size_t AudioStream::getBufferSize() const
{
    size_t size = _sampleSpec.convertUsecToframes(
        _parent->getStreamInterface()->getPeriodInUs(isOut(), getApplicabilityMask()));

    size = AudioUtils::alignOn16(size);

    size_t bytes = _sampleSpec.convertFramesToBytes(size);
    ALOGD("%s: %d (in bytes) for %s stream", __FUNCTION__, bytes, isOut() ? "output" : "input");

    return bytes;
}


size_t AudioStream::generateSilence(size_t bytes, void *buffer)
{
    if (buffer != NULL) {

        // Send zeroed buffer
        memset(buffer, 0, bytes);
    }
    usleep(streamSampleSpec().convertFramesToUsec(streamSampleSpec().convertBytesToFrames(bytes)));
    return bytes;
}

uint32_t AudioStream::latencyMs() const
{
    return _latencyMs;
}

void AudioStream::setApplicabilityMask(uint32_t applicabilityMask)
{
    AutoW lock(_streamLock);
    if (_applicabilityMask == applicabilityMask) {

        return;
    }
    _applicabilityMask = applicabilityMask;
    updateLatency();
}

void AudioStream::updateLatency()
{
    _latencyMs = AudioUtils::convertUsecToMsec(
        _parent->getStreamInterface()->getLatencyInUs(isOut(), getApplicabilityMask()));
}

status_t AudioStream::setStandby(bool isSet)
{
    if (isStarted() == !isSet) {

        return OK;
    }
    setStarted(!isSet);

    return isSet ? _parent->stopStream(this) : _parent->startStream(this);
}

status_t AudioStream::attachRouteL()
{
    ALOGV("%s %s stream", __FUNCTION__, isOut() ? "output" : "input");

    TinyAlsaStream::attachRouteL();

    SampleSpec ssSrc;
    SampleSpec ssDst;

    ssSrc = isOut() ? streamSampleSpec() : routeSampleSpec();
    ssDst = isOut() ? routeSampleSpec() : streamSampleSpec();

    status_t err = configureAudioConversion(ssSrc, ssDst);
    if (err != NO_ERROR) {

        ALOGE("%s: could not initialize audio conversion chain (err=%d)", __FUNCTION__, err);
        return err;
    }

    return NO_ERROR;
}

status_t AudioStream::detachRouteL()
{
    ALOGV("%s %s stream", __FUNCTION__, isOut() ? "output" : "input");

    TinyAlsaStream::detachRouteL();

    return NO_ERROR;
}

status_t AudioStream::configureAudioConversion(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    return _audioConversion->configure(ssSrc, ssDst);
}

status_t AudioStream::getConvertedBuffer(void *dst, const uint32_t outFrames,
                                         AudioBufferProvider *bufferProvider)
{
    return _audioConversion->getConvertedBuffer(dst, outFrames, bufferProvider);
}

status_t AudioStream::applyAudioConversion(const void *src, void **dst, uint32_t inFrames,
                                           uint32_t *outFrames)
{
    return _audioConversion->convert(src, dst, inFrames, outFrames);
}


void AudioStream::setDevices(uint32_t devices)
{
    AutoW lock(_streamLock);
    _devices = devices;
}

bool AudioStream::isStarted() const
{
    AutoR lock(_streamLock);
    return !_standby;
}

void AudioStream::setStarted(bool isStarted)
{
    AutoW lock(_streamLock);
    _standby = !isStarted;

    if (isStarted) {

        initAudioDump();
    }
}

void AudioStream::initAudioDump()
{
    /**
     * Read the dump properties when a new output/input stream is started.
     * False in second argument is the default value. If the property is true
     * then the dump object is created if it doesn't exist. Otherwise if it
     * is set to false, the dump object will be deleted to stop the dump.
     */
    if (TProperty<bool>(dumpBeforeConvProps[isOut()], false)) {
        if (!_dumpBeforeConv) {
            LOGI("Debug: create dump object for audio before conversion");
            _dumpBeforeConv = new HALAudioDump();
        }
    } else if (_dumpBeforeConv) {
        delete _dumpBeforeConv;
        _dumpBeforeConv = NULL;
    }
    if (TProperty<bool>(dumpAfterConvProps[isOut()], false)) {
        if (!_dumpAfterConv) {
            LOGI("Debug: create dump object for audio after conversion");
            _dumpAfterConv = new HALAudioDump();
        }
    } else if (_dumpAfterConv) {
        delete _dumpAfterConv;
        _dumpAfterConv = NULL;
    }
}
}       // namespace android
