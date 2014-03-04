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
#include <utils/Log.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioStreamOutImplAlsa"

#include "AudioStreamOutImpl.hpp"

#include <AudioCommsAssert.hpp>
#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <utils/String8.h>


namespace android_audio_legacy
{

const uint32_t AudioStreamOutImpl::_maxAgainRetry = 2;
const uint32_t AudioStreamOutImpl::_waitBeforeRetryUs = 10000; // 10ms
const uint32_t AudioStreamOutImpl::_usecPerMsec = 1000;

AudioStreamOutImpl::AudioStreamOutImpl(AudioIntelHAL *parent)
    : AudioStream(parent),
      _frameCount(0),
      _echoReference(NULL)
{
}

AudioStreamOutImpl::~AudioStreamOutImpl()
{
}

uint32_t AudioStreamOutImpl::channels() const
{
    return AudioStream::channels();
}

status_t AudioStreamOutImpl::setVolume(float left, float right)
{
    return NO_ERROR;
}

ssize_t AudioStreamOutImpl::write(const void *buffer, size_t bytes)
{
    AUDIOCOMMS_ASSERT(buffer != NULL, "NULL client buffer");
    setStandby(false);

    AutoR lock(_streamLock);
    // Check if the audio route is available for this stream
    if (!isRoutedL()) {

        ALOGW("%s(buffer=%p, bytes=%d) No route available. Trashing samples for stream %p.",
              __FUNCTION__, buffer, bytes, this);
        return generateSilence(bytes);
    }

    ssize_t srcFrames = streamSampleSpec().convertBytesToFrames(bytes);
    size_t dstFrames = 0;
    char *dstBuf = NULL;
    status_t status;

    pushEchoReference(buffer, srcFrames);

    // Dump audio output before any conversion.
    // FOR DEBUG PURPOSE ONLY
    if (getDumpObjectBeforeConv() != NULL) {
        getDumpObjectBeforeConv()->dumpAudioSamples(buffer,
                                                    bytes,
                                                    isOut(),
                                                    streamSampleSpec().getSampleRate(),
                                                    streamSampleSpec().getChannelCount(),
                                                    "before_conversion");
    }

    status = applyAudioConversion(buffer, (void **)&dstBuf, srcFrames, &dstFrames);

    if (status != NO_ERROR) {

        return status;
    }
    ALOGV("%s: srcFrames=%lu, bytes=%d dstFrames=%d", __FUNCTION__, srcFrames, bytes, dstFrames);

    ssize_t ret = pcmWriteFrames(dstBuf, dstFrames);

    if (ret < 0) {

        if (ret != -EPIPE) {

            // Returns asap to catch up the returned error else, trash the audio data
            // and sleep the time the driver may use to consume it.
            ALOGD("%s(buffer=%p, bytes=%d) Trashing samples for stream %p.",
                  __FUNCTION__, buffer, bytes, this);
            generateSilence(bytes);
        }
        return ret;
    }
    ALOGV("%s: returns %u", __FUNCTION__, streamSampleSpec().convertFramesToBytes(
              AudioUtils::convertSrcToDstInFrames(ret, routeSampleSpec(), streamSampleSpec())));

    // Dump audio output after eventual conversions
    // FOR DEBUG PURPOSE ONLY
    if (getDumpObjectAfterConv() != NULL) {
        getDumpObjectAfterConv()->dumpAudioSamples((const void *)dstBuf,
                                                   routeSampleSpec().convertFramesToBytes(
                                                       dstFrames),
                                                   isOut(),
                                                   routeSampleSpec().getSampleRate(),
                                                   routeSampleSpec().getChannelCount(),
                                                   "after_conversion");
    }

    return streamSampleSpec().convertFramesToBytes(AudioUtils::convertSrcToDstInFrames(ret,
                                                                                       routeSampleSpec(),
                                                                                       streamSampleSpec()));
}

status_t AudioStreamOutImpl::dump(int, const Vector<String16> &)
{
    return NO_ERROR;
}

status_t AudioStreamOutImpl::attachRouteL()
{
    status_t status = AudioStream::attachRouteL();
    if (status != NO_ERROR) {

        return status;
    }
    // Need to generate silence?
    uint32_t silenceMs = getOutputSilencePrologMs();
    if (silenceMs) {

        // Allocate a 1Ms buffer in stack
        uint32_t bufferSizeInFrames = routeSampleSpec().convertUsecToframes(1 * _usecPerMsec);
        void *silenceBuffer = alloca(routeSampleSpec().convertFramesToBytes(bufferSizeInFrames));
        memset(silenceBuffer, 0, routeSampleSpec().convertFramesToBytes(bufferSizeInFrames));

        uint32_t msCount;
        for (msCount = 0; msCount < silenceMs; msCount++) {

            pcmWriteFrames(silenceBuffer, bufferSizeInFrames);
        }
    }

    return NO_ERROR;
}

status_t AudioStreamOutImpl::detachRouteL()
{
    removeEchoReference(_echoReference);
    return AudioStream::detachRouteL();
}

status_t AudioStreamOutImpl::standby()
{
    _frameCount = 0;
    return setStandby(true);
}

uint32_t AudioStreamOutImpl::latency() const
{
    return AudioStream::latencyMs();
}

size_t AudioStreamOutImpl::bufferSize() const
{
    return getBufferSize();
}

status_t AudioStreamOutImpl::getRenderPosition(uint32_t *dspFrames)
{
    *dspFrames = _frameCount;
    return NO_ERROR;
}

status_t AudioStreamOutImpl::flush()
{
    AutoR lock(_streamLock);
    if (!isRoutedL()) {

        return NO_ERROR;
    }

    return pcmStop();
}

status_t  AudioStreamOutImpl::setParameters(const String8 &keyValuePairs)
{
    // Give a chance to parent to handle the change
    return _parent->setStreamParameters(this, keyValuePairs);
}

void AudioStreamOutImpl::addEchoReference(struct echo_reference_itfe *reference)
{
    AUDIOCOMMS_ASSERT(reference != NULL, "Null echo reference pointer");
    AutoW lock(_preProcEffectLock);
    ALOGD("%s(reference = %p): note mEchoReference = %p", __FUNCTION__, reference, _echoReference);

    // Called from a WLocked context
    _echoReference = reference;
}

void AudioStreamOutImpl::removeEchoReference(struct echo_reference_itfe *reference)
{
    AutoW lock(_preProcEffectLock);
    if (reference == NULL || _echoReference == NULL) {

        return;
    }

    ALOGD("%s(reference = %p): note mEchoReference = %p", __FUNCTION__, reference, _echoReference);
    if (_echoReference == reference) {

        _echoReference->write(_echoReference, NULL);
        _echoReference = NULL;
    } else {

        ALOGE("%s: reference requested was not attached to this stream...", __FUNCTION__);
    }
}

int AudioStreamOutImpl::getPlaybackDelay(ssize_t frames, struct echo_reference_buffer *buffer)
{
    size_t kernelFrames;
    int status;
    status = getFramesAvailable(kernelFrames, buffer->time_stamp);
    if (status != OK) {

        buffer->time_stamp.tv_sec = 0;
        buffer->time_stamp.tv_nsec = 0;
        buffer->delay_ns = 0;
        ALOGE("get_playback_delay(): pcm_get_htimestamp error,"
              "setting playbackTimestamp to 0");
        return status;
    }
    kernelFrames = getBufferSizeInFrames() - kernelFrames;

    /* adjust render time stamp with delay added by current driver buffer.
     * Add the duration of current frame as we want the render time of the last
     * sample being written.
     */
    buffer->delay_ns = streamSampleSpec().convertFramesToUsec(kernelFrames + frames);

    ALOGV("%s: kernel_frames=%d buffer->time_stamp.tv_sec=%lu,"
          "buffer->time_stamp.tv_nsec =%lu buffer->delay_ns=%d",
          __FUNCTION__,
          kernelFrames,
          buffer->time_stamp.tv_sec,
          buffer->time_stamp.tv_nsec,
          buffer->delay_ns);

    return 0;
}

void AudioStreamOutImpl::pushEchoReference(const void *buffer, ssize_t frames)
{
    AutoR lock(_preProcEffectLock);
    if (_echoReference != NULL) {
        struct echo_reference_buffer b;
        b.raw = (void *)buffer;
        b.frame_count = frames;
        getPlaybackDelay(b.frame_count, &b);
        _echoReference->write(_echoReference, &b);
    }
}
}       // namespace android
