/*
 * Copyright (C) 2015 Intel Corporation
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

#define LOG_TAG "CompressedStreamOut"

#include "CompressedStreamOut.hpp"
#include "AudioUtils.hpp"
#include <property/Property.hpp>
#include <convert/convert.hpp>
#include <utilities/Log.hpp>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <math.h>
#include <stdio.h>
#include <sys/types.h>
#include <linux/types.h>
#include <fcntl.h>
#include <time.h>
#include <utils/threads.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <cutils/sched_policy.h>
#include <string>

/* -1440 is the value expected by vol lib for a gain of -144dB */
#define SST_VOLUME_MUTE 0xFA60 /* 2s complement of 1440 */

using android::status_t;
using namespace std;
using audio_comms::utilities::convertTo;
using audio_comms::utilities::Log;
using audio_comms::utilities::Property;
using audio_comms::utilities::Mutex;
using audio_comms::utilities::ConditionVariable;

namespace intel_audio
{

static const int gDefaultRampInMs = 5; // valid mixer range is from 5 to 5000
static const size_t gOffloadMinAllowedBufferSizeInBytes = (2 * 1024);
static const size_t gOffloadMaxAllowedBufferSizeInBytes = (128 * 1024);
static const uint32_t gOffloadTransferIntervalInSec = 8;
static const uint32_t gCodecOffloadDefaultBitrateInBps = 128000;

CompressedStreamOut::CompressedStreamOut(Device *parent, audio_io_handle_t handle,
                                         uint32_t flagMask)
    : StreamOut(parent, handle, flagMask),
      mCompress(NULL),
      mVolume(SST_VOLUME_MUTE),
      mIsVolumeChangeRequestPending(false),
      mIsOffloadThreadBlocked(false),
      mOffloadCookie(NULL),
      mNewMetadataPendingToSend(true),
      mSoundCardNo(-1),
      mRecoveryOnGoing(false),
      mIsInFlushedState(false)
{
    Log::Verbose() << __FUNCTION__ << ": flag = 0x" << std::hex << flagMask;
    if (flagMask & AUDIO_OUTPUT_FLAG_NON_BLOCKING) {
        Log::Verbose() << __FUNCTION__ << ": setting non-blocking to true";
        mIsNonBlocking = true;
    }
    StreamOut::mute();

    // Read the property to get the mixer control names
    mMixVolumeCtl = Property<string>("offload.mixer.volume.ctl.name",
                                     "media0_in volume 0 volume").getValue();
    mMixMuteCtl = Property<string>("offload.mixer.mute.ctl.name",
                                   "media0_in volume 0 mute").getValue();
    mMixVolumeRampCtl = Property<string>("offload.mixer.rp.ctl.name",
                                         "media0_in volume 0 rampduration").getValue();

    Log::Info() << __FUNCTION__ << ": The mixer control name for volume = "
                << mMixVolumeCtl << ", mute = " << mMixMuteCtl << ", Ramp = " << mMixVolumeRampCtl;

    string cardName(Property<string>("audio.device.name", "0").getValue());
    mSoundCardNo = AudioUtils::getCardIndexByName(cardName.c_str());

    Log::Verbose() << __FUNCTION__ << ": creating callback";
    createOffloadCallbackThread();
}

bool CompressedStreamOut::isFormatSupported(audio_format_t format) const
{
    return (format == AUDIO_FORMAT_MP3) || ((format & AUDIO_FORMAT_AAC) == AUDIO_FORMAT_AAC);
}

status_t CompressedStreamOut::set(audio_config_t &config)
{
    if (!(getFlagMask() & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) ||
        config.offload_info.version != AUDIO_INFO_INITIALIZER.version ||
        config.offload_info.size != AUDIO_INFO_INITIALIZER.size) {
        Log::Error() << __FUNCTION__ << ": unsupported offload information";
        return android::BAD_VALUE;
    }
    if (mSoundCardNo < 0) {
        Log::Error() << __FUNCTION__ << ": Invalid sound card " << mSoundCardNo;
        return android::BAD_VALUE;
    }
    if (!isFormatSupported(config.format)) {
        return android::BAD_VALUE;
    }
    setFormat(config.format);
    setSampleRate(config.sample_rate);
    setChannels(config.channel_mask);

    mCodec.avgBitRate = config.offload_info.bit_rate;
    mCodec.sampleRate = config.sample_rate;
    mCodec.numChannels = config.channel_mask;

    setBufferSize();
    return android::OK;
}

CompressedStreamOut::~CompressedStreamOut()
{
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] in";
    standby();
    destroyOffloadCallbackThread();
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] out";
}

status_t CompressedStreamOut::pause()
{
    Mutex::Locker locker(mCodecLock);

    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] in";
    if (mCompress == NULL || mState != SstState::PLAYING) {
        Log::Verbose() << __FUNCTION__ << ": [" << mState << "] ignored";
        return android::OK;
    }
    if (compress_pause(mCompress) < 0) {
        Log::Error() << __FUNCTION__ << ": failed, Err=" << compress_get_error(mCompress);
        return android::INVALID_OPERATION;
    }
    mState = SstState::PAUSED;
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] out";
    return android::OK;
}

status_t CompressedStreamOut::resume()
{
    Mutex::Locker locker(mCodecLock);

    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] in";
    if (mCompress == NULL || mState != SstState::PAUSED) {
        Log::Verbose() << __FUNCTION__ << ": [" << mState << "] ignored";
        return android::OK;
    }
    if (compress_resume(mCompress) < 0) {
        Log::Error() << __FUNCTION__ << ": failed, Err=" << compress_get_error(mCompress);
        return android::INVALID_OPERATION;
    }
    mState = SstState::PLAYING;
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] out";
    return android::OK;
}

status_t CompressedStreamOut::closeDeviceUnsafe()
{
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "]";
    if (mCompress != NULL) {
        if (mState == SstState::DRAINING) {
            Log::Verbose() << __FUNCTION__ << ": called after partial drain, Call the drain";
            compress_drain(mCompress);
            Log::Verbose() << __FUNCTION__ << ": coming out of drain";
        }
        Log::Verbose() << __FUNCTION__ << ": compress_close";
        compress_close(mCompress);
        mCompress = NULL;
    }
    mState = SstState::CLOSED;
    return android::OK;
}

status_t CompressedStreamOut::setMuteUnsafe(bool muted, struct mixer *mixer)
{
    if (muted == isMuted()) {
        return android::OK;
    }
    muted ? StreamOut::mute() : StreamOut::unMute();
    struct mixer_ctl *mute_ctl = mixer_get_ctl_by_name(mixer, mMixMuteCtl.c_str());
    if (!mute_ctl) {
        Log::Error() << __FUNCTION__ << ": Error opening mixerMutecontrol" << mMixMuteCtl;
        return android::BAD_VALUE;
    }
    mixer_ctl_set_value(mute_ctl, 0, muted);
    Log::Verbose() << __FUNCTION__ << ": muting=" << isMuted();
    return android::OK;
}

status_t CompressedStreamOut::openDeviceUnsafe()
{
    struct compr_config config;
    struct snd_codec codec;
    int device = AudioUtils::getCompressDeviceIndex();
    if (device < 0) {
        Log::Error() << __FUNCTION__ << ": Error getting device number ";
        return android::BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": device " << device;

    // update the configuration structure for given type of stream
    codec.id = (getFormat() == AUDIO_FORMAT_MP3) ? SND_AUDIOCODEC_MP3 : SND_AUDIOCODEC_AAC;
    /* the channel maks is the one that come to hal. Converting the mask to channel number */
    int channel_count = popcount(getChannels());
    codec.ch_out = channel_count;
    codec.ch_in = channel_count;
    codec.sample_rate =  getSampleRate();
    codec.bit_rate = mCodec.avgBitRate ? mCodec.avgBitRate : gCodecOffloadDefaultBitrateInBps;
    codec.rate_control = 0;
    codec.profile = (getFormat() == AUDIO_FORMAT_MP3) ? 0 : SND_AUDIOPROFILE_AAC;
    codec.level = 0;
    codec.ch_mode = 0;
    codec.format = (getFormat() == AUDIO_FORMAT_MP3) ? 0 : SND_AUDIOSTREAMFORMAT_RAW;

    Log::Info() << __FUNCTION__ << ": params: codec.id =" << codec.id << ",codec.ch_in="
                << codec.ch_in << ",codec.ch_out=" << codec.ch_out << ", codec.sample_rate="
                << codec.sample_rate << ", codec.bit_rate=" << codec.bit_rate
                << ",codec.rate_control=" << codec.rate_control << ", codec.profile="
                << codec.profile << ",codec.level=" << codec.level << ",codec.ch_mode="
                << codec.ch_mode << ",codec.format=" << codec.format;
    config.fragment_size = mBufferSize;
    config.fragments = 2;
    config.codec = &codec;

    mCompress = compress_open(mSoundCardNo, device, COMPRESS_IN, &config);
    if (mCompress && !is_compress_ready(mCompress)) {
        Log::Error() << __FUNCTION__ << ": Failed opening card " << mSoundCardNo << " device "
                     << device << " (error=" << compress_get_error(mCompress) << ")";
        closeDeviceUnsafe();
        return android::BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": Compress device opened sucessfully";
    Log::Verbose() << __FUNCTION__ << ": setting compress non block";
    compress_nonblock(mCompress, mIsNonBlocking);

    struct mixer *mixer;
    mixer = mixer_open(mSoundCardNo);
    if (!mixer) {
        Log::Error() << __FUNCTION__ << ": Failed to open mixer for card " << mSoundCardNo;
        return android::INVALID_OPERATION;
    }
    unmute(mixer);
    mixer_close(mixer);

    mState = SstState::IDLE;
    return android::OK;
}

void CompressedStreamOut::stopCompressedOutputUnsafe()
{
    mNewMetadataPendingToSend = true;
    if (mCompress != NULL) {
        compress_stop(mCompress);
        while (mIsOffloadThreadBlocked) {
            mCond.wait(mCodecLock);
        }
        mState = SstState::IDLE;
    }
}

status_t CompressedStreamOut::standby()
{
    Mutex::Locker autolock(mCodecLock);

    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] in";
    if (isStarted()) {
        stopCompressedOutputUnsafe();
        mGaplessMdata.encoder_delay = 0;
        mGaplessMdata.encoder_padding = 0;
        closeDeviceUnsafe();
        StreamOut::setStandby(true);
    }
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] out";
    return android::OK;
}

status_t CompressedStreamOut::setParameters(const std::string &kvpairs)
{
    Log::Verbose() << __FUNCTION__ << ": kvpairs = " << kvpairs;
    int delay = -1;
    int padding = -1;

    Mutex::Locker locker(mCodecLock);

    KeyValuePairs pairs(kvpairs);
    string key(AUDIO_OFFLOAD_CODEC_BIT_PER_SAMPLE);
    status_t status = pairs.get<int>(key, mCodec.bitsPerSample);
    if (status == android::OK) {
        pairs.remove(key);
    }
    // Avg bitrate in bps - for AAC/MP3
    key = AUDIO_OFFLOAD_CODEC_AVG_BIT_RATE;
    status = pairs.get<int>(key, mCodec.avgBitRate);
    if (status == android::OK) {
        pairs.remove(key);
        Log::Verbose() << __FUNCTION__ << ": average bit rate set to " << mCodec.avgBitRate;
    }
    // Number of channels present (for AAC)
    key = AUDIO_OFFLOAD_CODEC_NUM_CHANNEL;
    status = pairs.get<int>(key, mCodec.numChannels);
    if (status == android::OK) {
        pairs.remove(key);
    }
    // Sample rate - for AAC direct from parser
    key = AUDIO_OFFLOAD_CODEC_SAMPLE_RATE;
    status = pairs.get<int>(key, mCodec.sampleRate);
    if (status == android::OK) {
        pairs.remove(key);
    }
    // Delay and Padding samples shall be sent at the same time - for MP3
    key = AUDIO_OFFLOAD_CODEC_DELAY_SAMPLES;
    status = pairs.get<int>(key, delay);
    if (status == android::OK) {
        pairs.remove(key);

        key = AUDIO_OFFLOAD_CODEC_PADDING_SAMPLES;
        status = pairs.get<int>(key, padding);
        if (status == android::OK) {
            pairs.remove(key);
            mGaplessMdata.encoder_delay = delay;
            mGaplessMdata.encoder_padding = padding;
            Log::Verbose() << __FUNCTION__ << ": Delay=" << delay << ", Padding=" << padding;
            mNewMetadataPendingToSend = true;
        }
    }
    return android::OK;
}

int CompressedStreamOut::convertAmplToBel(float amplification)
{
    return static_cast<int>((20 * log10(amplification)) * 10);
}

android::status_t CompressedStreamOut::setVolume(float left, float right)
{
    Log::Verbose() << __FUNCTION__ << ": right vol= " << right << ", left vol = " << left;
    if (left < 0.0f || left > 1.0f) {
        Log::Error() << __FUNCTION__ << ": Invalid data as vol=" << left;
        return android::BAD_VALUE;
    }

    Mutex::Locker locker(mCodecLock);

    return setVolumeUnsafe(left, right);
}

android::status_t CompressedStreamOut::setVolumeUnsafe(float left, float right)
{
    mVolume = left; // needed if we set volume in  out_write
    // If error happens during setting the volume, try to set while in out_write
    mIsVolumeChangeRequestPending = true;

    struct mixer *mixer;
    struct mixer_ctl *vol_ctl;
    int volume[2];
    int prevVolume[2];
    struct mixer_ctl *volRamp_ctl;

    mixer = mixer_open(mSoundCardNo);
    if (!mixer) {
        Log::Error() << __FUNCTION__ << ": Failed to open mixer for card " << mSoundCardNo;
        return android::INVALID_OPERATION;
    }
    status_t ret = (left == 0 && right == 0) ? mute(mixer) : unmute(mixer);
    if (ret != android::OK) {
        mixer_close(mixer);
        return ret;
    }
    if (isMuted()) {
        mixer_close(mixer);
        mIsVolumeChangeRequestPending = false;
        return android::OK;
    }

    // gain library expects user input of integer gain in 0.1dB
    // Eg., 60 in decimal represents 6dB
    volume[0] = convertAmplToBel(left);
    volume[1] = convertAmplToBel(right);
    vol_ctl = mixer_get_ctl_by_name(mixer, mMixVolumeCtl.c_str());
    if (!vol_ctl) {
        Log::Error() << __FUNCTION__ << ": Error opening mixerVolumecontrol " << mMixVolumeCtl;
        mixer_close(mixer);
        return android::INVALID_OPERATION;
    }
    Log::Verbose() << __FUNCTION__ << ": volume computed: %x db" << volume[0];
    mixer_ctl_get_array(vol_ctl, prevVolume, 2);
    if (prevVolume[0] == volume[0] && prevVolume[1] == volume[1]) {
        Log::Verbose() << __FUNCTION__ << ": No update since volume requested";
        mixer_close(mixer);
        mIsVolumeChangeRequestPending = false;
        return android::OK;
    }
    volRamp_ctl = mixer_get_ctl_by_name(mixer, mMixVolumeRampCtl.c_str());
    if (!volRamp_ctl) {
        Log::Error() << __FUNCTION__ << ": Error opening mixerVolRamp ctl " << mMixVolumeRampCtl;
        mixer_close(mixer);
        return android::INVALID_OPERATION;
    }
    unsigned int num_ctl_values =  mixer_ctl_get_num_values(volRamp_ctl);
    Log::Verbose() << __FUNCTION__ << ": num_ctl_ramp_values = " << num_ctl_values;
    for (unsigned int i = 0; i < num_ctl_values; i++) {
        int retval = mixer_ctl_set_value(volRamp_ctl, i, gDefaultRampInMs);
        if (retval < 0) {
            Log::Info() << __FUNCTION__ << ": Error setting volumeRamp =" << std::hex
                        << gDefaultRampInMs << ", error =" << retval;
        }
    }
    int retval1 = mixer_ctl_set_array(vol_ctl, volume, 2);
    if (retval1 < 0) {
        Log::Error() << __FUNCTION__ << ": Err setting volume dB value %x" << volume[0];
        mixer_close(mixer);
        return android::INVALID_OPERATION;
    }
    Log::Verbose() << __FUNCTION__ << ": Successful in set volume";
    mixer_close(mixer);
    mIsVolumeChangeRequestPending = false;
    return android::OK;
}

android::status_t CompressedStreamOut::sendOffloadCmdUnsafe(offload_cmd::Command command)
{
    struct offload_cmd *cmd = new struct offload_cmd (command);
    if (!cmd) {
        Log::Verbose() << __FUNCTION__ << ": NO_MEMORY";
        return android::NO_MEMORY;
    }
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] cmd=" << command;

    list_add_tail(&mOffloadCmdList, &cmd->node);
    mOffloadCond.signal();
    return android::OK;
}

android::status_t CompressedStreamOut::write(const void *buffer, size_t &bytes)
{
    Mutex::Locker locker(mCodecLock);

    Log::Verbose() << __FUNCTION__ << ": [" << mState << "]";
    if (!isStarted()) {
        if (openDeviceUnsafe()) {
            Log::Error() << __FUNCTION__ << ": [" << mState << "] Device open error";
            closeDeviceUnsafe();
            return -EINVAL;
        }
        StreamOut::setStandby(false);
    }

    if (mNewMetadataPendingToSend) {
        if ((compress_set_gapless_metadata(mCompress, &mGaplessMdata)) < 0) {
            Log::Error() << __FUNCTION__ << ": setting meta data failed, err="
                         << compress_get_error(mCompress);
            return -EINVAL;
        }
        mNewMetadataPendingToSend = false;
    }
    if (mIsVolumeChangeRequestPending) {
        setVolumeUnsafe(mVolume, mVolume);
    }
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] Calling compress write with "
                   << bytes << " bytes";
    int ret = compress_write(mCompress, buffer, bytes);
    if ((ret >= 0) && (ret < static_cast<int>(bytes))) {
        Log::Verbose() << __FUNCTION__ << ": [" << mState << "] sending wait for buffer cmd";
        sendOffloadCmdUnsafe(offload_cmd::WAIT_FOR_BUFFER);
    }
    if (ret < 0) {
        Log::Error() << __FUNCTION__ << ": compress write error: " << compress_get_error(mCompress);
        return ret;
    }
    bytes = ret;
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] written " << ret << " bytes now";
    if (mState != SstState::PLAYING) {
        ret = compress_start(mCompress);
        if (ret < 0) {
            Log::Info() << __FUNCTION__ << ": [" << mState << "] compress_start error: "
                        << compress_get_error(mCompress);
            return ret;
        }
        mState = SstState::PLAYING;
        Log::Verbose() << __FUNCTION__ << ": [" << mState << "] compress_start success";
    }
    return android::OK;
}

android::status_t CompressedStreamOut::getRenderPosition(uint32_t &dspFrames) const
{
    unsigned int avail;
    struct timespec tstamp;
    uint32_t calTimeMs;

    Mutex::Locker locker(mCodecLock);

    dspFrames = 0;
    if (!isStarted()) {
        Log::Verbose() << __FUNCTION__ << ": [" << mState << "] stream not started";
        return -EINVAL;
    }
    if (mState == SstState::DRAINING) {
        // In repeated mode, the track will get recycled and this API is called during draining.
        // So, it keeps updating the progress bar
        dspFrames = 0;
        return android::OK;
    }

    if (compress_get_hpointer(mCompress, &avail, &tstamp) < 0) {
        Log::Warning() << __FUNCTION__ << ": Failed Err=" << compress_get_error(mCompress);
        return -EINVAL;
    }
    calTimeMs = (tstamp.tv_sec * 1000) + (tstamp.tv_nsec / 1000000);
    dspFrames += calTimeMs;
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] time (ms) returned = " << dspFrames;
    return android::OK;
}


status_t CompressedStreamOut::setCallback(stream_callback_t callback, void *cookie)
{
    Mutex::Locker locker(mCodecLock);

    Log::Verbose() << __FUNCTION__;
    mOffloadCallback = callback;
    mOffloadCookie = cookie;
    return 0;
}

status_t CompressedStreamOut::drain(audio_drain_type_t type)
{
    Mutex::Locker locker(mCodecLock);

    Log::Verbose() << __FUNCTION__;
    int status = -ENOSYS;
    if (type == AUDIO_DRAIN_EARLY_NOTIFY) {
        Log::Verbose() << __FUNCTION__ << ": send command PARTIAL_DRAIN";
        status = sendOffloadCmdUnsafe(offload_cmd::PARTIAL_DRAIN);
        Log::Verbose() << __FUNCTION__ << ": recovery " << mRecoveryOnGoing;
        if (mRecoveryOnGoing) {
            Log::Verbose() << __FUNCTION__ << ": stop compress output due to recovery";
            stopCompressedOutputUnsafe();
            mRecoveryOnGoing = false;
        }
    } else {
        Log::Verbose() << __FUNCTION__ << ": send command DRAIN";
        status = sendOffloadCmdUnsafe(offload_cmd::DRAIN);
    }
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] return status " << status;
    return status;
}

status_t CompressedStreamOut::flush()
{
    Mutex::Locker locker(mCodecLock);

    if (!isStarted()) {
        Log::Verbose() << __FUNCTION__ << ": [" << mState << "] compress not active, ignored";
        return android::OK;
    }
    if (mState == SstState::PAUSED) {
        /* @TODO: remove this code when proper value other than -1 is
         * returned by compress_wait() to trigger recovery */
        mIsInFlushedState = true;
    }
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] calling Compress Stop";
    stopCompressedOutputUnsafe();
    mIsInFlushedState = false;
    return android::OK;
}

void CompressedStreamOut::recover()
{
    Mutex::Locker locker(mCodecLock);

    compress_stop(mCompress);
    closeDeviceUnsafe();
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] device closed";
    openDeviceUnsafe();
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] device opened";
    mRecoveryOnGoing = true;
    Log::Verbose() << __FUNCTION__ << ": [" << mState << "] write old buffer";
}

bool CompressedStreamOut::handleCommand(offload_cmd::Command cmd, stream_callback_event_t &event)
{
    int retval;
    switch (cmd) {
    case offload_cmd::WAIT_FOR_BUFFER:
        retval = compress_wait(mCompress, -1);
        Log::Verbose() << __FUNCTION__ << ": compress_wait returns " << retval;

        /* TODO: remove the below check for value of flushedState and
         * modify the check for retval according to proper value
         * (other than -1) i.e. received to trigger recovery */
        if (retval < 0 && !mIsInFlushedState) {
            Log::Verbose() << __FUNCTION__ << ": compress_wait returns error, do recovery";
            recover();
        }
        Log::Verbose() << __FUNCTION__ << ": WAIT_FOR_BUFFER out of Compress_wait";
        event = STREAM_CBK_EVENT_WRITE_READY;
        return true;

    case offload_cmd::PARTIAL_DRAIN:
        Log::Verbose() << __FUNCTION__ << ": PARTIAL_DRAIN: Calling next_track";
        compress_next_track(mCompress);
        Log::Verbose() << __FUNCTION__ << ": PARTIAL_DRAIN: Calling partial drain";
        retval = compress_partial_drain(mCompress);
        Log::Verbose() << __FUNCTION__ << ": PARTIAL_DRAIN: returns " << retval;
        event = STREAM_CBK_EVENT_DRAIN_READY;
        {
            Mutex::Locker locker(mCodecLock);
            mState = SstState::DRAINING;
            // Resend the metadata for next iteration.
            mNewMetadataPendingToSend = true;
        }
        return true;

    case offload_cmd::DRAIN:
        Log::Verbose() << __FUNCTION__ << ": DRAIN: calling compress_drain";
        compress_drain(mCompress);
        event = STREAM_CBK_EVENT_DRAIN_READY;
        {
            Mutex::Locker locker(mCodecLock);
            mState = SstState::IDLE;
        }
        return true;
    }
    Log::Error() << __FUNCTION__ << ": unknown command received: " << cmd;
    return false;
}

void *CompressedStreamOut::offloadThreadLoop(void *context)
{
    CompressedStreamOut *out = static_cast<CompressedStreamOut *>(context);
    struct listnode *item;

    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_AUDIO);
    set_sched_policy(0, SP_FOREGROUND);
    prctl(PR_SET_NAME, (unsigned long)"Offload Callback", 0, 0, 0);

    Log::Verbose() << __FUNCTION__;

    Mutex::Locker locker(out->mCodecLock);

    for (;;) {
        offload_cmd *cmd = NULL;

        if (list_empty(&out->mOffloadCmdList)) {
            Log::Verbose() << __FUNCTION__ << ": [" << out->mState << "] Cmd list empty, SLEEPING";
            out->mOffloadCond.wait(out->mCodecLock);
            Log::Verbose() << __FUNCTION__ << ": [" << out->mState << "] RUNNING";
            continue;
        }

        item = list_head(&out->mOffloadCmdList);
        cmd = node_to_item(item, offload_cmd, node);
        list_remove(item);

        Log::Verbose() << __FUNCTION__ << ": [" << out->mState << "] CMD " << cmd->get();

        if (cmd->get() == offload_cmd::EXIT) {
            delete cmd;
            Log::Verbose() << __FUNCTION__ << ": [" << out->mState << "] EXITING";
            break;
        }

        if (out->mCompress == NULL) {
            Log::Error() << __FUNCTION__ << ": [" << out->mState << "] Compress handle is NULL";
            out->mCond.signal();
            continue;
        }
        out->mIsOffloadThreadBlocked = true;

        out->mCodecLock.unlock();

        stream_callback_event_t event;
        bool sendCallback = out->handleCommand(cmd->get(), event);

        out->mCodecLock.lock();

        out->mIsOffloadThreadBlocked = false;
        out->mCond.signal();
        if (sendCallback) {
            Log::Verbose() << __FUNCTION__ << ": sending callback event" << static_cast<int>(event);
            out->mOffloadCallback(event, NULL, out->mOffloadCookie);
        }
        delete cmd;
    }

    out->mCond.signal();
    while (!list_empty(&out->mOffloadCmdList)) {
        item = list_head(&out->mOffloadCmdList);
        list_remove(item);
        free(node_to_item(item, offload_cmd, node));
    }
    return NULL;
}

status_t CompressedStreamOut::createOffloadCallbackThread()
{
    list_init(&mOffloadCmdList);
    pthread_create(&mOffloadThread, (const pthread_attr_t *)NULL, offloadThreadLoop, this);
    return android::OK;
}

status_t CompressedStreamOut::destroyOffloadCallbackThread()
{
    mCodecLock.lock();

    Log::Verbose() << __FUNCTION__;
    stopCompressedOutputUnsafe();
    sendOffloadCmdUnsafe(offload_cmd::EXIT);

    mCodecLock.unlock();

    pthread_join(mOffloadThread, (void **)NULL);
    return android::OK;
}

void CompressedStreamOut::setBufferSize()
{
    if (mCodec.avgBitRate >= 12000) {
        mBufferSize = (gOffloadTransferIntervalInSec * mCodec.avgBitRate) / 8; /* in bytes */
    } else {
        // Though we could not take the decision based on exact bit-rate,
        // select optimal bufferSize based on samplingRate & Channel of the stream
        if (mCodec.sampleRate <= 8000) {
            mBufferSize = gOffloadMinAllowedBufferSizeInBytes; // Voice data in Mono/Stereo
        } else if (mCodec.numChannels == AUDIO_CHANNEL_OUT_MONO) {
            mBufferSize = 4 * 1024; // Mono music
        } else if (mCodec.sampleRate <= 32000) {
            mBufferSize = 16 * 1024; // Stereo low quality music
        } else if (mCodec.sampleRate <= 48000) {
            mBufferSize = 32 * 1024; // Stereo high quality music
        } else {
            mBufferSize = 64 * 1024; // HiFi stereo music
        }
    }
    if (mBufferSize > gOffloadMaxAllowedBufferSizeInBytes) {
        mBufferSize = gOffloadMaxAllowedBufferSizeInBytes;
    }
    // Make the bufferSize to be of 2^n bytes
    for (size_t i = 1; (mBufferSize & ~i) != 0; i <<= 1) {
        mBufferSize &= ~i;
    }
    Log::Verbose() << __FUNCTION__ << ": bufSize=" << mBufferSize;
}

} // namespace intel_audio
