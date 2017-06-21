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

#define LOG_TAG "AudioStream"

#include "Device.hpp"
#include "Stream.hpp"
#include <Parameters.hpp>
#include <KeyValuePairs.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <property/Property.hpp>
#include <AudioConversion.hpp>
#include <HalAudioDump.hpp>
#include <string>

using android::status_t;
using audio_comms::utilities::Log;
using audio_comms::utilities::Property;
using namespace std;

namespace intel_audio
{

/**
 * Audio dump properties management (set with setprop)
 */
const std::string Stream::dumpBeforeConvProps[Direction::gNbDirections] = {
    "media.dump_input.befconv", "media.dump_output.befconv"
};

const std::string Stream::dumpAfterConvProps[Direction::gNbDirections] = {
    "media.dump_input.aftconv", "media.dump_output.aftconv"
};

Stream::Stream(Device *parent, audio_io_handle_t handle, uint32_t flagMask)
    : mParent(parent),
      mStandby(true),
      mAudioConversion(new AudioConversion),
      mLatencyMs(0),
      mFlagMask(flagMask),
      mUseCaseMask(0),
      mDumpBeforeConv(NULL),
      mDumpAfterConv(NULL),
      mHandle(handle),
      mPatchHandle(AUDIO_PATCH_HANDLE_NONE)
{
}

Stream::~Stream()
{
    delete mAudioConversion;
    delete mDumpAfterConv;
    delete mDumpBeforeConv;
}

void Stream::getDefaultConfig(audio_config_t &config) const
{
    config.channel_mask = isOut() ? AUDIO_CHANNEL_OUT_STEREO : AUDIO_CHANNEL_IN_STEREO;
    config.format = mDefaultFormat;
    config.sample_rate = mDefaultSampleRate;
}

status_t Stream::set(audio_config_t &config)
{
    if (config.sample_rate == 0) {
        config.sample_rate = mDefaultSampleRate;
    }
    if (config.format == AUDIO_FORMAT_DEFAULT) {
        config.format = mDefaultFormat;
    }
    setConfig(config, isOut());
    if (mParent->getStreamInterface().supportStreamConfig(*this)) {
        updateLatency();
        return android::OK;
    }
    getDefaultConfig(config);
    return android::BAD_VALUE;
}

std::string Stream::getParameters(const std::string &keys) const
{
    KeyValuePairs pairs(keys);
    KeyValuePairs returnedPairs(keys);
    const AudioCapabilities &capabilities = mParent->getStreamInterface().getCapabilities(*this);

    string key(AUDIO_PARAMETER_STREAM_SUP_CHANNELS);
    if (pairs.hasKey(key)) {
        returnedPairs.add(key, capabilities.getSupportedChannelMasks(isOut()));
    }
    key = AUDIO_PARAMETER_STREAM_SUP_FORMATS;
    if (pairs.hasKey(key)) {
        returnedPairs.add(key, capabilities.getSupportedFormats());
    }
    key =  AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES;
    if (pairs.hasKey(key)) {
        returnedPairs.add(key, capabilities.getSupportedRates());
    }
    return returnedPairs.toString();
}

uint32_t Stream::getSampleRate() const
{
    return IoStream::getSampleRate();
}

status_t Stream::setSampleRate(uint32_t rate)
{
    IoStream::setSampleRate(rate);
    return android::OK;
}

audio_channel_mask_t Stream::getChannels() const
{
    return IoStream::getChannels();
}

audio_format_t Stream::getFormat() const
{
    return IoStream::getFormat();
}

status_t Stream::setFormat(audio_format_t format)
{
    IoStream::setFormat(format);
    return android::OK;
}

status_t Stream::standby()
{
    return setStandby(true);
}

audio_devices_t Stream::getDevice() const
{
    return getDevices();
}

bool Stream::isRoutedByPolicy() const
{
    return mPatchHandle != AUDIO_PATCH_HANDLE_NONE;
}

uint32_t Stream::getFlagMask() const
{
    AutoR lock(mStreamLock);
    return mFlagMask;
}

uint32_t Stream::getUseCaseMask() const
{
    AutoR lock(mStreamLock);
    return mUseCaseMask;
}

status_t Stream::setParameters(const string &keyValuePairs)
{
    Log::Warning() << __FUNCTION__ << ": " << keyValuePairs
                   << ": Not implemented, Using routing API 3.0";
    return keyValuePairs.empty() ? android::OK : android::BAD_VALUE;
}

size_t Stream::getBufferSize() const
{
    AutoR lock(mStreamLock);
    size_t size = mSampleSpec.convertUsecToframes(
        mParent->getStreamInterface().getPeriodInUs(*this));

    size = AudioUtils::alignOn16(size);

    size_t bytes = mSampleSpec.convertFramesToBytes(size);
    if (bytes == 0) {
        Log::Error() << __FUNCTION__ << ": null buffer size for "
                     << (isOut() ? "output" : "input") << " stream.";
        return 0;
    }
    Log::Debug() << __FUNCTION__ << ": " << bytes << " (in bytes) for "
                 << (isOut() ? "output" : "input") << " stream.";
    return bytes;
}


status_t Stream::generateSilence(size_t &bytes, void *buffer)
{
    if (buffer != NULL) {
        // Send zeroed buffer
        memset(buffer, 0, bytes);
    }
    usleep(streamSampleSpec().convertFramesToUsec(streamSampleSpec().convertBytesToFrames(bytes)));
    return android::OK;
}

uint32_t Stream::getLatencyMs() const
{
    return mLatencyMs;
}

void Stream::setUseCaseMask(uint32_t useCaseMask)
{
    if (getUseCaseMask() == useCaseMask) {

        return;
    }
    mStreamLock.writeLock();
    mUseCaseMask = useCaseMask;
    mStreamLock.unlock();
    updateLatency();
}

void Stream::updateLatency()
{
    AutoR lock(mStreamLock);
    mLatencyMs =
        AudioUtils::convertUsecToMsec(mParent->getStreamInterface().getLatencyInUs(*this));
}

status_t Stream::setStandby(bool isSet)
{
    if (isStarted() == !isSet) {

        return android::OK;
    }
    setStarted(!isSet);

    Log::Debug() << __FUNCTION__ << ": " << (isSet ? "stopping " : "starting ")
                 << (isOut() ? "output" : "input") << " stream";
    // Start / Stop streams operation are expected to be synchronous, since we want to avoid loosing
    // audio data before the stream is routed to its route, i.e. audio device.
    return mParent->updateStreamsParametersSync(getRole());
}

status_t Stream::attachRouteL()
{
    Log::Verbose() << __FUNCTION__ << ": " << (isOut() ? "output" : "input") << " stream";
    IoStream::attachRouteL();

    SampleSpec ssSrc;
    SampleSpec ssDst;

    ssSrc = isOut() ? streamSampleSpec() : routeSampleSpec();
    ssDst = isOut() ? routeSampleSpec() : streamSampleSpec();

    status_t err = configureAudioConversion(ssSrc, ssDst);
    if (err != android::OK) {
        Log::Error() << __FUNCTION__
                     << ": could not initialize audio conversion chain (err=" << err << ")";
        return err;
    }

    return android::OK;
}

status_t Stream::detachRouteL()
{
    Log::Verbose() << __FUNCTION__ << ": " << (isOut() ? "output" : "input") << " stream";
    IoStream::detachRouteL();

    return android::OK;
}

status_t Stream::configureAudioConversion(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    return mAudioConversion->configure(ssSrc, ssDst);
}

status_t Stream::getConvertedBuffer(void *dst, const size_t outFrames,
                                    android::AudioBufferProvider *bufferProvider)
{
    return mAudioConversion->getConvertedBuffer(dst, outFrames, bufferProvider);
}

status_t Stream::applyAudioConversion(const void *src, void **dst, size_t inFrames,
                                      size_t *outFrames)
{
    return mAudioConversion->convert(src, dst, inFrames, outFrames);
}

bool Stream::isStarted() const
{
    AutoR lock(mStreamLock);
    return !mStandby;
}

void Stream::setStarted(bool isStarted)
{
    AutoW lock(mStreamLock);
    mStandby = !isStarted;

    if (isStarted) {

        initAudioDump();
    }
}

void Stream::initAudioDump()
{
    /**
     * Read the dump properties when a new output/input stream is started.
     * False in second argument is the default value. If the property is true
     * then the dump object is created if it doesn't exist. Otherwise if it
     * is set to false, the dump object will be deleted to stop the dump.
     */
    if (Property<bool>(dumpBeforeConvProps[isOut()], false).getValue()) {
        if (!mDumpBeforeConv) {
            Log::Info() << __FUNCTION__ << ": create dump object for audio before conversion";
            mDumpBeforeConv = new HalAudioDump();
        }
    } else if (mDumpBeforeConv) {
        delete mDumpBeforeConv;
        mDumpBeforeConv = NULL;
    }
    if (Property<bool>(dumpAfterConvProps[isOut()], false).getValue()) {
        if (!mDumpAfterConv) {
            Log::Info() << __FUNCTION__ << ": create dump object for audio after conversion";
            mDumpAfterConv = new HalAudioDump();
        }
    } else if (mDumpAfterConv) {
        delete mDumpAfterConv;
        mDumpAfterConv = NULL;
    }
}

bool Stream::safeSleep(uint32_t sleepTimeUs)
{
    struct timespec tim;

    if (sleepTimeUs > mMaxSleepTime) {
        sleepTimeUs = mMaxSleepTime;
    }

    tim.tv_sec = 0;
    tim.tv_nsec = sleepTimeUs * mNsecPerUsec;

    return nanosleep(&tim, NULL) > 0;
}

void Stream::setPatchHandle(audio_patch_handle_t patchHandle)
{
    mPatchHandle = patchHandle;
}

} // namespace intel_audio
