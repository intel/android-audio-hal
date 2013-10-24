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
#pragma once


#include "SampleSpec.h"
#include <NonCopyable.hpp>
#include <Direction.hpp>
#include <TinyAlsaStream.h>
#include <media/AudioBufferProvider.h>
#include <utils/String8.h>
/**
 * For debug purposes only, property-driven (dynamic)
 */
#include <HALAudioDump.h>

namespace android_audio_legacy
{

class AudioIntelHAL;
class AudioConversion;


class AudioStream : public TinyAlsaStream,
                    private audio_comms::utilities::NonCopyable
{
public:
    virtual ~AudioStream();

    /**
     * Sets the sample specifications of the stream.
     *
     * @param[in|out] format: format of the samples of the playback client.
     *                        If not set, stream returns format it supports.
     * @param[in|out] channels: mask of channels of the samples of the playback client.
     *                          If not set, stream returns channels it supports.
     * @param[in|out] sampleRate: sample rate of the samples of the playback client.
     *                            If not set, stream returns sample rate it supports.
     *
     * @return status: error code to set if parameters given by playback client not supported.
     */
    android::status_t set(int *format, uint32_t *channels, uint32_t *rate);

    /**
     * Get the parameters of the stream.
     *
     * @param[out] keys: one or more value pair "name:value", coma separated
     *
     * @return OK if set is successfull, error code otherwise.
     */
    android::String8 getParameters(const android::String8 &keys);

    /**
     * Get the sample rate of the stream.
     *
     * @return sample rate of the stream.
     */
    inline uint32_t sampleRate() const
    {
        return _sampleSpec.getSampleRate();
    }

    /**
     * Get the format of the stream.
     *
     * @return format of the stream.
     */
    inline int format() const
    {
        return _sampleSpec.getFormat();
    }

    /**
     * Get the channel count of the stream.
     *
     * @return channel count of the stream.
     */
    inline uint32_t channelCount() const
    {
        return _sampleSpec.getChannelCount();
    }

    /**
     * Get the channels of the stream.
     * Channels is a mask, each bit represents a specific channel.
     *
     * @return channel mask of the stream.
     */
    inline uint32_t channels() const
    {
        return _sampleSpec.getChannelMask();
    }

    /**
     * Get the size of the buffer.
     * It calibrates the transfert size between audio flinger and the stream.
     *
     * @return size of the buffer in bytes
     */
    size_t getBufferSize() const;

    /**
     * Check the stream status.
     * Inherited from Stream class
     *
     * @return true if stream is started, false if stream is in standby.
     */
    virtual bool isStarted() const;

    /**
     * Get the stream direction.
     * Inherited from Stream class
     *
     * @return true if output, false if input.
     */
    virtual bool isOut() const = 0;

    /**
     * Set the stream state.
     *
     * @param[in] isSet: true if the client requests the stream to enter standby, false to start
     *
     * @return OK if stream started/standbyed successfully, error code otherwise.
     */
    android::status_t setStandby(bool isSet);

    /**
     * Get the stream devices mask.
     * Stream Sample specification is the sample spec in which the client gives/receives samples
     *
     * @return _devices specifications.
     */
    uint32_t getDevices() const
    {
        return _devices;
    }

    /**
     * Set the stream devices.
     *
     * @param[in] devices: mask in which each bit represents a device.
     */
    void setDevices(uint32_t devices);

    /**
     * Applicability mask.
     * From Stream class
     *
     * @return ID of input source if input, stream flags if output
     */
    virtual uint32_t getApplicabilityMask() const
    {
        return _applicabilityMask;
    }

    void setApplicabilityMask(uint32_t applicabilityMask);

    /**
     * Get the stream sample specification.
     * Stream Sample specification is the sample spec in which the client gives/receives samples
     *
     * @return sample specifications.
     */
    SampleSpec streamSampleSpec() const
    {
        return _sampleSpec;
    }
protected:
    AudioStream(AudioIntelHAL *parent);

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
     * @return status OK is conversion is successfull, error code otherwise.
     */
    android::status_t applyAudioConversion(const void *src, void **dst,
                                           uint32_t inFrames, uint32_t *outFrames);

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
    android::status_t getConvertedBuffer(void *dst, const uint32_t outFrames,
                                         android::AudioBufferProvider *bufferProvider);

    /**
     * Generate silence.
     * According to the direction, the meaning is different. For an output stream, it means
     * trashing audio samples, while for an input stream, it means providing zeroed samples.
     * To emulate the behavior of the HW and to keep time sync, this function will sleep the time
     * the HW would have used to read/write the amount of requested bytes.
     *
     * @param[in] bytes amount of byte to set to 0 within the buffer.
     * @param[in|out] buffer: if provided, need to fill with 0 (expected for input)
     *
     * @return size of the sample trashed / filled with 0.
     */
    size_t generateSilence(size_t bytes, void *buffer = NULL);

    /**
     * Get the latency of the stream.
     * Latency returns the worst case, ie the latency introduced by the alsa ring buffer.
     *
     * @return latency in milliseconds.
     */
    uint32_t latencyMs() const;

    /**
     * Update the latency according to the flag.
     * Request will be done to the route manager to informs the latency introduced by the route
     * supporting this stream flags.
     *
     */
    void updateLatency();

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
    HALAudioDump *getDumpObjectBeforeConv() const
    {
        return _dumpBeforeConv;
    }


    /**
     * Get audio dump objects after conversion for debug purposes
     *
     * @return a HALAudioDump object after conversion
     */
    HALAudioDump *getDumpObjectAfterConv() const
    {
        return _dumpAfterConv;
    }

    AudioIntelHAL *_parent; /**< Audio HAL singleton handler. */
private:
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


    bool _standby; /**< state of the stream, true if standby, false if started. */

    uint32_t _devices; /**< devices mask selected by the policy for this stream.*/

    SampleSpec _sampleSpec; /**< stream sample specifications. */

    AudioConversion *_audioConversion; /**< Audio Conversion utility class. */

    uint32_t _latencyMs; /**< Latency associated with the current flag of the stream. */

    /**
     * Applicability mask is either:
     *  -for output streams: stream flags, from audio_output_flags_t in audio.h file.
     *  -for input streams: input source (bitfield done from audio_source_t in audio.h file.
     *          Note that 0 will be taken as none.
     */
    uint32_t _applicabilityMask;

    static const uint32_t DEFAULT_SAMPLE_RATE = 48000; /**< Default HAL sample rate. */
    static const uint32_t DEFAULT_CHANNEL_COUNT = 2; /**< Default HAL nb of channels. */
    static const uint32_t DEFAULT_FORMAT = AUDIO_FORMAT_PCM_16_BIT; /**< Default HAL format. */

    /**
     * Audio dump object used if one of the dump property before
     * conversion is true (check init.rc file)
     */
    HALAudioDump *_dumpBeforeConv;

    /**
     * Audio dump object used if one of the dump property after
     * conversion is true (check init.rc file)
     */
    HALAudioDump *_dumpAfterConv;

    /**
     * Array of property names before conversion
     */
    static const std::string dumpBeforeConvProps[audio_comms::utilities::Direction::_nbDirections];

    /**
     * Array of property names after conversion
     */
    static const std::string dumpAfterConvProps[audio_comms::utilities::Direction::_nbDirections];
};
}         // namespace android
