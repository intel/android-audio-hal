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
#pragma once

#include <SampleSpec.hpp>
#include <StreamInterface.hpp>
#include <AudioNonCopyable.hpp>
#include <Direction.hpp>
#include <IoStream.hpp>
#include <media/AudioBufferProvider.h>
#include <hardware/audio.h>
#include <string>
#include <utils/RWLock.h>

class HalAudioDump;

namespace intel_audio
{

class Device;
class AudioConversion;


class Stream
    : public virtual StreamInterface,
      public IoStream,
      private audio_comms::utilities::NonCopyable
{
public:
    virtual ~Stream();

    /**
     * Sets the sample specifications of the stream.
     * If fields are not set, or values are unsupported, Audio Flinger expects us to return an error
     * and a config that would be suitable for us.
     *
     * @param[in,out] config: audio configuration of the stream (i.e. sample specifications).
     *
     * @return status: error code to set if parameters given by playback client not supported.
     */
    virtual android::status_t set(audio_config_t &config);

    // From StreamInterface
    virtual uint32_t getSampleRate() const;
    virtual android::status_t setSampleRate(uint32_t rate);
    virtual size_t getBufferSize() const;
    virtual audio_channel_mask_t getChannels() const;
    virtual audio_format_t getFormat() const;
    virtual android::status_t setFormat(audio_format_t format);
    virtual android::status_t standby();
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t dump(int) const { return android::OK; }
    virtual audio_devices_t getDevice() const;
    virtual android::status_t setDevice(audio_devices_t device) = 0;
    /** @note API not implemented in stream base class, input specific implementation only. */
    virtual android::status_t addAudioEffect(effect_handle_t /*effect*/) { return android::OK; }
    /** @note API not implemented in stream base class, input specific implementation only. */
    virtual android::status_t removeAudioEffect(effect_handle_t /*effect*/) { return android::OK; }
    /** @note API not used anymore for routing since Routing Control API 3.0. */
    virtual android::status_t setParameters(const std::string &keyValuePairs);
    virtual std::string getParameters(const std::string &keys) const;

    // From IoStream
    virtual bool isRoutedByPolicy() const;
    virtual uint32_t getFlagMask() const;
    virtual uint32_t getUseCaseMask() const;
    virtual bool isStarted() const;
    virtual bool isOut() const = 0;

    virtual bool isMuted() const = 0;

    audio_io_handle_t getIoHandle() const { return mHandle; }

    /**
     * Set the patch in which this stream (which is considered by the policy as a MIX Port)
     * is involved.
     *
     * @param[in] patch that connects this mix port to/from one or more device Port.
     */
    void setPatchHandle(audio_patch_handle_t patchHandle);

    /**
     * @return the patch in which the stream (considered by the policy as a MIX Port)
     *                    is involved
     */
    audio_patch_handle_t getPatchHandle() const { return mPatchHandle; }

    /**
     * Update the latency according to the flag.
     * Request will be done to the route manager to informs the latency introduced by the route
     * supporting this stream flags.
     *
     */
    void updateLatency();

protected:
    Stream(Device *parent, audio_io_handle_t handle, uint32_t flagMask);

    /**
     * Set the stream state.
     *
     * @param[in] isSet: true if the client requests the stream to enter standby, false to start
     *
     * @return OK if stream started/standbyed successfully, error code otherwise.
     */
    android::status_t setStandby(bool isSet);

    /**
     * Set the use case mask, which is an input source mask for an input stream, and still
     * not used for output streams.
     * This function is non-reetrant.
     *
     * @param[in] useCaseMask
     */
    void setUseCaseMask(uint32_t useCaseMask);

    /**
     * Callback of route attachement called by the stream lib. (and so route manager)
     * Inherited from Stream class
     * Set the new pcm device and sample spec given by the audio stream route.
     *
     * @return OK if streams attached successfully to the route, error code otherwise.
     */
    virtual android::status_t attachRouteL();

    /**
     * Callback of route detachement called by the stream lib. (and so route manager)
     * Inherited from Stream class
     *
     * @return OK if streams detached successfully from the route, error code otherwise.
     */
    virtual android::status_t detachRouteL();

    /**
     * Apply audio conversion.
     * Stream is attached to an audio route. Sample specification of streams and routes might
     * differ. This function will adapt if needed the samples between the 2 sample specifications.
     *
     * @param[in] src the source buffer.
     * @param[out] dst the destination buffer.
     *                  Note that if the memory is allocated by the converted, it is freed upon next
     *                  call of configure or upon deletion of the converter.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames pointer on output frames processed.
     *
     * @return status OK is conversion is successful, error code otherwise.
     */
    android::status_t applyAudioConversion(const void *src, void **dst,
                                           size_t inFrames, size_t *outFrames);

    /**
     * Converts audio samples and output an exact number of output frames.
     * The caller must give an AudioBufferProvider object that may implement getNextBuffer API
     * to feed the conversion chain.
     * The caller must allocate itself the destination buffer and garantee overflow will not happen.
     *
     * @param[out] dst pointer on the caller destination buffer.
     * @param[in] outFrames frames in the destination sample specification requested to be outputed.
     * @param[in:out] bufferProvider object that will provide source buffer.
     *
     * @return status OK, error code otherwise.
     */
    android::status_t getConvertedBuffer(void *dst, const size_t outFrames,
                                         android::AudioBufferProvider *bufferProvider);

    /**
     * Generate silence.
     * According to the direction, the meaning is different. For an output stream, it means
     * trashing audio samples, while for an input stream, it means providing zeroed samples.
     * To emulate the behavior of the HW and to keep time sync, this function will sleep the time
     * the HW would have used to read/write the amount of requested bytes.
     *
     * @param[in,out] bytes amount of byte to set to 0 within the buffer.
     * @param[in,out] buffer: if provided, need to fill with 0 (expected for input)
     *
     * @return size of the sample trashed / filled with 0.
     */
    android::status_t generateSilence(size_t &bytes, void *buffer = NULL);

    /**
     * Get the latency of the stream.
     * Latency returns the worst case, ie the latency introduced by the alsa ring buffer.
     *
     * @return latency in milliseconds.
     */
    uint32_t getLatencyMs() const;

    /**
     * Sets the state of the status.
     *
     * @param[in] isStarted true if start request, false if standby request.
     */
    void setStarted(bool isStarted);

    /**
     * Get audio dump object before conversion for debug purposes
     *
     * @return a HALAudioDump object before conversion
     */
    HalAudioDump *getDumpObjectBeforeConv() const
    {
        return mDumpBeforeConv;
    }

    /**
     * Get audio dump objects after conversion for debug purposes
     *
     * @return a HALAudioDump object after conversion
     */
    HalAudioDump *getDumpObjectAfterConv() const
    {
        return mDumpAfterConv;
    }

    /**
     * Used to sleep on the current thread.
     *
     * This function is used to get a POSIX-compliant way
     * to accurately sleep the current thread.
     *
     * If function is successful, zero is returned
     * and request has been honored, if function fails,
     * EINTR has been raised by the system and -1 is returned.
     *
     * The other two errors considered by standard
     * are not applicable in our context (EINVAL, ENOSYS)
     *
     * @param[in] sleepTimeUs: desired to sleep, in microseconds.
     *
     * @return on success true is returned, false otherwise.
     */
    bool safeSleep(uint32_t sleepTimeUs);

    Device *mParent; /**< Audio HAL singleton handler. */

    /**
     * Lock to protect preprocessing effects accessed from multiple contexts.
     * For output streams, variable protected by the lock is the echo reference, populated by the
     * output stream and accessed by the input stream.
     * For input streams, variable protected by the lock is the list of pre processing effects
     * pushed by Audio Flinger and hooked by the stream in the context of the record thread.
     */
    android::RWLock mPreProcEffectLock;

    static const uint32_t mDefaultSampleRate = 48000; /**< Default HAL sample rate. */
    static const uint32_t mDefaultChannelCount = 2; /**< Default HAL nb of channels. */
    static const audio_format_t mDefaultFormat = AUDIO_FORMAT_PCM_16_BIT; /**< Default HAL format.*/

private:
    void getDefaultConfig(audio_config_t &config) const;

    /**
     * Configures the conversion chain.
     * It configures the conversion chain that may be used to convert samples from the source
     * to destination sample specification. This configuration tries to order the list of converters
     * so that it minimizes the number of samples on which the resampling is done.
     *
     * @param[in] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specifications.
     *
     * @return status OK, error code otherwise.
     */
    android::status_t configureAudioConversion(const SampleSpec &ssSrc, const SampleSpec &ssDst);

    /**
     * Init audio dump if dump properties are activated to create the dump object(s).
     * Triggered when the stream is started.
     */
    void initAudioDump();


    bool mStandby; /**< state of the stream, true if standby, false if started. */

    AudioConversion *mAudioConversion; /**< Audio Conversion utility class. */

    uint32_t mLatencyMs; /**< Latency associated with the current flag of the stream. */

    /**
     * Flags mask is either:
     *  -for output streams: stream flags, from audio_output_flags_t in audio.h file.
     *                       Note that the stream flags are given at output creation and will not
     *                       changed until output is destroyed.
     *  -for input streams: audio_input_flags_t.
     *          Note that 0 will be taken as none.
     * The values must match audio.h file definitions.
     */
    uint32_t mFlagMask;

    /**
     * Use case mask is either:
     *  -for output streams: Not used.
     *  -for input streams: input source translated into a bit.
     *          Note that 0 will be taken as none.
     */
    uint32_t mUseCaseMask;

    /**
     * Audio dump object used if one of the dump property before
     * conversion is true (check init.rc file)
     */
    HalAudioDump *mDumpBeforeConv;

    /**
     * Audio dump object used if one of the dump property after
     * conversion is true (check init.rc file)
     */
    HalAudioDump *mDumpAfterConv;

    /**
     * Array of property names before conversion
     */
    static const std::string dumpBeforeConvProps[Direction::gNbDirections];

    /**
     * Array of property names after conversion
     */
    static const std::string dumpAfterConvProps[Direction::gNbDirections];

    /** maximum sleep time to be allowed by HAL, in microseconds. */
    static const uint32_t mMaxSleepTime = 1000000UL;

    /** Ratio between nanoseconds and microseconds */
    static const uint32_t mNsecPerUsec = 1000;

    audio_io_handle_t mHandle; /**< Unique IO handle identifier assigned by the audio policy. */

    /**
     * Unique Patch Handle involving this stream (which is considered as a MIX Port by Policy).
     */
    audio_patch_handle_t mPatchHandle;
};
} // namespace intel_audio
