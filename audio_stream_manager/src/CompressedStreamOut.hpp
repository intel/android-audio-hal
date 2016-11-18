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
#pragma once

#include "StreamOut.hpp"

#include <sound/compress_params.h>
#include <tinycompress/tinycompress.h>

#include <Mutex.hpp>
#include <ConditionVariable.hpp>
#include <cutils/list.h>

static const uint32_t CODEC_OFFLOAD_LATENCY = 10;      /* Default latency in mSec  */

struct compress;
struct compr_gapless_mdata;

namespace intel_audio
{

class CompressedStreamOut : public StreamOut
{
private:
    struct SstState
    {
        enum Enum
        {
            CLOSED,     /* Device not opened yet  */
            IDLE,       /* Device opened, mixer setting done, ready for write operations   */
            PLAYING,    /* STREAM_START done and write calls going  */
            PAUSED,     /* STREAM_PAUSE to call and stream is pausing */
            DRAINING    /* STREAM_DRAINING to call and is OK for writes */
        };
        typedef int Type;
        SstState(Enum sstState = CLOSED) : mState(sstState) {}

        operator Enum() const
        {
            return static_cast<Enum>(mState);
        }

        private:
            Type mState;
    };

    struct offload_cmd
    {
        enum Enum
        {
            EXIT,               /* exit compress offload thread loop*/
            DRAIN,              /* send a full drain request to DSP */
            PARTIAL_DRAIN,      /* send a partial drain request to DSP */
            WAIT_FOR_BUFFER     /* wait for buffer released by DSP */
        };
        typedef int Command;

        offload_cmd(Command offloadCommand) : mCmd(offloadCommand) {}

        Command get() const { return mCmd; }
        struct listnode node;
        // @todo: private member but beware since used in list.h with widespread usage
        // of offsetof
        Command mCmd;
    };

public:
    CompressedStreamOut(Device *parent, audio_io_handle_t handle, uint32_t flagMask,
                        audio_devices_t devices, const std::string &address);

    virtual ~CompressedStreamOut();

    virtual android::status_t set(audio_config_t &config);

    virtual android::status_t pause();

    virtual android::status_t resume();

    virtual size_t getBufferSize() const { return mBufferSize; }

    virtual android::status_t standby();

    virtual android::status_t dump(int) const { return android::OK; }

    virtual android::status_t setParameters(const std::string &keyValuePairs);

    virtual uint32_t getLatency() { return CODEC_OFFLOAD_LATENCY; }

    virtual android::status_t setVolume(float left, float right);

    virtual android::status_t write(const void *buffer, size_t &bytes);

    virtual android::status_t getRenderPosition(uint32_t &dspFrames) const;

    virtual android::status_t getPresentationPosition(uint64_t &frames, struct timespec &timestamp) const;

    virtual android::status_t setCallback(stream_callback_t, void *);

    virtual android::status_t drain(audio_drain_type_t);

    virtual android::status_t flush();

private:
    /**
     * Converts an amplification given as a float to a volume in bel
     * @param[in] amplification volume in float given by the policy from 0 to 1
     * @return volume in bel
     */
    static inline int convertAmplToBel(float amplification);

    /**
     * Handle a codec offload command
     *
     * @param[in] cmd to be parsed
     * @param[out] event to be sent if command requires a callback notification.
     *
     * @return true if callback shall be sent with event and event is set, false otherwise
     */
    bool handleCommand(offload_cmd::Command cmd, stream_callback_event_t &event);

    /**
     * Set the volume using mixer control retrieved by android property,
     * It must be call with lock held.
     *
     * @todo: shall be hidden behind rogue parameters of Audio PFW instance.
     *
     * @param[in] left value to set
     * @param[in] right value to set
     *
     * @return 0 is set correctly, error code otherwise
     */
    android::status_t setVolumeUnsafe(float left, float right);

    /**
     * Mute/unmute in HW. As the stream is compressed, Audio Flinger is unable to mute the stream
     * directly. So the must is performed by the codec itself. Must be call with lock held.
     *
     * @param muted true if mute request, false if unmute request
     * @param mixer to be used for mute control
     * @return OK if operation is successfull, false otherwise.
     */
    android::status_t setMuteUnsafe(bool muted, struct mixer *mixer);

    android::status_t mute(struct mixer *mixer) { return setMuteUnsafe(true, mixer); }

    android::status_t unmute(struct mixer *mixer) { return setMuteUnsafe(false, mixer); }

    /**
     * Check if a given format is supported for HW decoding by the codec.
     *
     * @param[in] format to be checked
     *
     * @return true if the format is supported by the codec, false otherwise.
     */
    bool isFormatSupported(audio_format_t format) const;

    /**
     * Goal is to compute an optimal bufferSize that shall be used by
     * Multimedia framework in transferring the encoded stream to LPE firmware
     * in duration of OFFLOAD_TRANSFER_INTERVAL defined.
     */
    void setBufferSize();

    /**
     * Send a command to offload thread.
     *
     * @param[in] command to send to offload thread
     *
     * @return OK is sent is successfull, error code otherwise.
     */
    android::status_t sendOffloadCmdUnsafe(offload_cmd::Command command);

    /**
     * Stop the compress output stream, wait that all buffer has been consummed (drain or partial
     * drain).
     */
    void stopCompressedOutputUnsafe();

    /**
     * Close the compress device. It switches to Closed state. Must be call with lock held.
     *
     * @return OK if operation is successfull, error code otherwise.
     */
    android::status_t closeDeviceUnsafe();

    /**
     * Open the compress device. It switches to Idle state. Must be call with lock held.
     *
     * @return OK if operation is successfull, error code otherwise.
     */
    android::status_t openDeviceUnsafe();

    /**
     * @brief destroyOffloadCallbackThread
     * @return OK if operation is successfull, error code otherwise.
     */
    android::status_t destroyOffloadCallbackThread();

    /**
     * @brief createOffloadCallbackThread
     * @return OK if operation is successfull, error code otherwise.
     */
    android::status_t createOffloadCallbackThread();

    /**
     * Offload Thread Main loop: it waits commands from the list, and notify if required the caller.
     * @param[in] context
     * @return start_routing exit status
     */
    static void *offloadThreadLoop(void *context);

    /**
     * @brief recover: when an unrecoverable error is detected, Compress Stream may recover itself.
     */
    void recover();

    audio_comms::utilities::ConditionVariable mCond;
    SstState mState;
    compress *mCompress;
    float mVolume;
    bool mIsVolumeChangeRequestPending;
    size_t mBufferSize;
    mutable audio_comms::utilities::Mutex mCodecLock;
    bool mIsNonBlocking;
    audio_comms::utilities::ConditionVariable mOffloadCond;
    pthread_t mOffloadThread;
    struct listnode mOffloadCmdList;
    bool mIsOffloadThreadBlocked;

    stream_callback_t mOffloadCallback;
    void *mOffloadCookie;
    struct compr_gapless_mdata mGaplessMdata;
    bool mNewMetadataPendingToSend;
    int mSoundCardNo;
    bool mRecoveryOnGoing;

    /* TODO: remove this variable when proper value other than -1 is returned
     * by compress_wait() to trigger recovery */
    bool mIsInFlushedState;

    std::string mMixVolumeCtl;
    std::string mMixMuteCtl;
    std::string mMixVolumeRampCtl;

    /**
     * The data structure used for passing the codec specific information to the
     * HAL offload for configuring the playback
     */
    struct CodecInformation
    {
        audio_format_t format;
        int32_t numChannels;
        int32_t sampleRate;
        int32_t bitsPerSample;
        int32_t avgBitRate;
    } mCodec;
};

} // namespace intel_audio
