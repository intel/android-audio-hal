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

#define LOG_TAG "AudioStreamOut"

#include "StreamOut.hpp"
#include <AudioCommsAssert.hpp>
#include <HalAudioDump.hpp>
#include <utilities/Log.hpp>

using namespace std;
using android::status_t;
using audio_comms::utilities::Log;

namespace intel_audio
{

const uint32_t StreamOut::mMaxAgainRetry = 2;
const uint32_t StreamOut::mWaitBeforeRetryUs = 10000; // 10ms
const uint32_t StreamOut::mUsecPerMsec = 1000;

StreamOut::StreamOut(Device *parent, audio_io_handle_t handle, uint32_t flagMask, audio_devices_t devices)
    : Stream(parent, handle, flagMask),
      mFrameCount(0),
      mEchoReference(NULL),
      mIsMuted(false)
{
    setDevice(devices);
}

StreamOut::~StreamOut()
{
}

status_t StreamOut::set(audio_config_t &config)
{
    if (config.channel_mask == AUDIO_CHANNEL_NONE) {
        config.channel_mask = isDirect() ? AUDIO_CHANNEL_OUT_5POINT1 : AUDIO_CHANNEL_OUT_STEREO;
    }
    return Stream::set(config);
}

android::status_t StreamOut::setVolume(float left, float right)
{
    bool muteRequested = (left == 0 && right == 0);
    if (isMuted() != muteRequested) {
        muteRequested ? mute() : unMute();
        return mParent->updateStreamsParametersSync(getRole());
    }
    return android::OK;
}

android::status_t StreamOut::pause()
{
    if (!isDirect()) {
        return android::INVALID_OPERATION;
    }
    return setStandby(true);
}

android::status_t StreamOut::resume()
{
    if (!isDirect()) {
        return android::INVALID_OPERATION;
    }
    return setStandby(false);
}

status_t StreamOut::write(const void *buffer, size_t &bytes)
{
    if (buffer == NULL) {
        Log::Error() << __FUNCTION__ << ": NULL client buffer";
        return android::BAD_VALUE;
    }
    setStandby(false);

    mStreamLock.readLock();
    status_t status;
    const ssize_t srcFrames = streamSampleSpec().convertBytesToFrames(bytes);

    // Check if the audio route is available for this stream or if the stream is muted
    if (!isRoutedL() || isMuted()) {
        Log::Warning() << __FUNCTION__ << ": Trashing " << bytes << " bytes for stream " << this
                       << (isMuted() ? ": Stream muted" : ": No route available");
        status = generateSilence(bytes);
        mFrameCount += srcFrames;
        mStreamLock.unlock();
        return status;
    }

    size_t dstFrames = 0;
    char *dstBuf = NULL;
    uint32_t retryCount = 0;

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

    if (status != android::OK) {
        mStreamLock.unlock();
        return status;
    }
    Log::Verbose() << __FUNCTION__ << ": srcFrames=" << srcFrames << ", bytes=" << bytes
                   << " dstFrames=" << dstFrames;

    do {
        std::string error;

        status = pcmWriteFrames(dstBuf, dstFrames, error);

        if (status < 0) {
            Log::Error() << __FUNCTION__ << ": write error: " << error
                         << " - requested " << srcFrames
                         << " (bytes=" << streamSampleSpec().convertFramesToBytes(srcFrames)
                         << ") frames";

            if (error.find(strerror(EIO)) != std::string::npos) {
                // Dump hw registers debug file info in console
                mParent->printPlatformFwErrorInfo();

            } else if (error.find(strerror(EBADFD)) != std::string::npos) {
                mStreamLock.unlock();
                Log::Error() << __FUNCTION__ << ": execute device recovery";
                setStandby(true);
                return android::DEAD_OBJECT;
            }
            AUDIOCOMMS_ASSERT(error.find(strerror(EBADF)) == std::string::npos,
                              "Audio Device handle closed not by Audio HAL."
                              " A corruption might have happenned, investigation required");

            if (++retryCount > mMaxReadWriteRetried) {
                mStreamLock.unlock();
                Log::Error() << __FUNCTION__ << ": Hardware not responding";
                return android::DEAD_OBJECT;
            }

            // Get the number of microseconds to sleep, inferred from the number of
            // frames to write.
            size_t sleepUsecs = routeSampleSpec().convertFramesToUsec(dstFrames);

            // Go sleeping before trying I/O operation again.
            if (safeSleep(sleepUsecs)) {
                // If some error arises when trying to sleep, try I/O operation anyway.
                // Error counter will provoke the restart of mediaserver.
                Log::Error() << __FUNCTION__ << ":  Error while calling nanosleep interface";
            }
        }
    } while (status < 0);

    Log::Verbose() << __FUNCTION__ << ": returns " << streamSampleSpec().convertFramesToBytes(
        AudioUtils::convertSrcToDstInFrames(status, routeSampleSpec(), streamSampleSpec()));

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
    if (mFrameCount > (std::numeric_limits<uint64_t>::max() - srcFrames)) {
        Log::Error() << __FUNCTION__ << ": overflow detected, resetting framecount";
        mFrameCount = 0;
    }
    mFrameCount += srcFrames;
    mStreamLock.unlock();
    return status;
}

uint32_t StreamOut::getLatency()
{
    return getLatencyMs();
}

status_t StreamOut::attachRouteL()
{
    status_t status = Stream::attachRouteL();
    if (status != android::OK) {

        return status;
    }
    // Need to generate silence?
    uint32_t silenceMs = getOutputSilencePrologMs();
    if (silenceMs) {

        // Allocate a 1Ms buffer in stack
        uint32_t bufferSizeInFrames = routeSampleSpec().convertUsecToframes(1 * mUsecPerMsec);
        void *silenceBuffer = alloca(routeSampleSpec().convertFramesToBytes(bufferSizeInFrames));
        memset(silenceBuffer, 0, routeSampleSpec().convertFramesToBytes(bufferSizeInFrames));

        uint32_t msCount;
        for (msCount = 0; msCount < silenceMs; msCount++) {
            std::string writeError;
            status = pcmWriteFrames(silenceBuffer, bufferSizeInFrames, writeError);
            if (status < 0) {
                Log::Error() << "Write error when writing silence : " << writeError;
            }
        }
    }

    return android::OK;
}

status_t StreamOut::detachRouteL()
{
    removeEchoReference(mEchoReference);
    return Stream::detachRouteL();
}

status_t StreamOut::getRenderPosition(uint32_t &dspFrames) const
{
    dspFrames = mFrameCount;
    return android::OK;
}

status_t StreamOut::getPresentationPosition(uint64_t &frames, struct timespec &timestamp) const
{
    /** Take the stream lock in read mode to avoid the route manager unrouting this stream,
     * and closing the audio device while dealing with it.
     */
    AutoR lock(mStreamLock);
    // Check if the audio route is available for this stream (i.e. an audio device is assign to it).
    if (!isRoutedL()) {
        return android::INVALID_OPERATION;
    }
    size_t avail;
    status_t error = getFramesAvailable(avail, timestamp);
    if (error != android::OK) {
        return error;
    }
    size_t kernelBufferSize = getBufferSizeInFrames();
    // FIXME This calculation is incorrect if there is buffering after app processor
    int64_t signedFrames = mFrameCount - kernelBufferSize + avail;
    if (signedFrames < 0) {
        Log::Error() << __FUNCTION__ << ": signedFrames=" << signedFrames
                     << " unusual negative value, please check avail implementation within driver."
                     << ": mFrameCount=" << mFrameCount
                     << ": kernelBufferSize=" << kernelBufferSize
                     << ": avail=" << avail;
        return android::BAD_VALUE;
    }
    frames = signedFrames;
    return android::OK;
}

status_t StreamOut::flush()
{
    AutoR lock(mStreamLock);
    if (!isRoutedL()) {

        return android::OK;
    }

    return pcmStop();
}

void StreamOut::addEchoReference(struct echo_reference_itfe *reference)
{
    AutoW lock(mPreProcEffectLock);
    Log::Debug() << __FUNCTION__ << ": (reference = " << reference
                 << "): note mEchoReference = " << mEchoReference;
    // Called from a WLocked context
    mEchoReference = reference;
}

void StreamOut::removeEchoReference(struct echo_reference_itfe *reference)
{
    AutoW lock(mPreProcEffectLock);
    if (reference == NULL || mEchoReference == NULL) {

        return;
    }
    Log::Debug() << __FUNCTION__ << ": (reference = " << reference
                 << "): note mEchoReference = " << mEchoReference;
    if (mEchoReference == reference) {

        mEchoReference->write(mEchoReference, NULL);
        mEchoReference = NULL;
    } else {
        Log::Error() << __FUNCTION__ << ": reference requested was not attached to this stream...";
    }
}

int StreamOut::getPlaybackDelay(ssize_t frames, struct echo_reference_buffer *buffer)
{
    size_t kernelFrames;
    int status;
    status = getFramesAvailable(kernelFrames, buffer->time_stamp);
    if (status != android::OK) {

        buffer->time_stamp.tv_sec = 0;
        buffer->time_stamp.tv_nsec = 0;
        buffer->delay_ns = 0;
        Log::Error() << "get_playback_delay(): pcm_get_htimestamp error,"
                     << "setting playbackTimestamp to 0";
        return status;
    }
    kernelFrames = getBufferSizeInFrames() - kernelFrames;

    /* adjust render time stamp with delay added by current driver buffer.
     * Add the duration of current frame as we want the render time of the last
     * sample being written.
     */
    buffer->delay_ns = streamSampleSpec().convertFramesToUsec(kernelFrames + frames);

    Log::Verbose() << __FUNCTION__
                   << ": kernel_frames=" << kernelFrames
                   << " buffer->time_stamp.tv_sec=" << buffer->time_stamp.tv_sec << ","
                   << "buffer->time_stamp.tv_nsec =" << buffer->time_stamp.tv_nsec
                   << " buffer->delay_ns=" << buffer->delay_ns;

    return 0;
}

void StreamOut::pushEchoReference(const void *buffer, ssize_t frames)
{
    AutoR lock(mPreProcEffectLock);
    if (mEchoReference != NULL) {
        struct echo_reference_buffer b;
        b.raw = (void *)buffer;
        b.frame_count = frames;
        getPlaybackDelay(b.frame_count, &b);
        mEchoReference->write(mEchoReference, &b);
    }
}

status_t StreamOut::setDevice(audio_devices_t device)
{
    if (!audio_is_output_devices(device)) {
        Log::Error() << __FUNCTION__ << ": invalid output device " << device;
        return android::BAD_VALUE;
    }
    return setDevices(device);
}

} // namespace intel_audio
