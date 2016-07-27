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
#pragma once

#include <SampleSpec.hpp>
#include <system/audio.h>
#include <utils/RWLock.h>
#include <string>

typedef android::RWLock::AutoRLock AutoR;
typedef android::RWLock::AutoWLock AutoW;

namespace intel_audio
{

class IStreamRoute;
class IAudioDevice;

class IoStream
{
public:
    IoStream()
        : mCurrentStreamRoute(NULL),
          mNewStreamRoute(NULL),
          mEffectsRequestedMask(0)
    {}

    /**
     * indicates if the stream has been routed (ie audio device available and the routing is done)
     *
     * @return true if stream is routed, false otherwise
     */
    bool isRouted() const;

    /**
     * indicates if the stream has been routed (ie audio device available and the routing is done).
     * Must be called from locked context.
     *
     * @return true if stream is routed, false otherwise
     */
    bool isRoutedL() const;

    /**
     * indicates if the stream has already been attached to a new route.
     * Note that the routing is still not done.
     *
     * @return true if stream has a new route assigned, false otherwise
     */
    bool isNewRouteAvailable() const;

    /**
     * get stream direction.
     * Note that true=playback, false=capture.
     * @return true if playback, false otherwise.
     */
    virtual bool isOut() const = 0;

    /**
     * A stream is considered as a MIX Port with a given role.
     * An output stream is a Source Mix Port, an input stream is a Sink Mix Port.
     *
     * @return role of the stream (mix port).
     */
    virtual audio_port_role_t getRole() const = 0;

    /**
     * get stream state.
     * Note that true=playing, false=standby|stopped.
     * @return true if started, false otherwise.
     */
    virtual bool isStarted() const = 0;

    /**
     * Checks if a stream has been routed from a policy point of view..
     *
     * @return true if the stream is routed by policy, false otherwise.
     */
    virtual bool isRoutedByPolicy() const = 0;

    /**
     * Flag mask.
     * For an input stream, the input flags are the prefered attribute like fast tracks, hotword
     * For an output stream, the output flags are the attribute like primary, fast, deep, ...
     *
     * @return flags mask.
     */
    virtual uint32_t getFlagMask() const = 0;

    /**
     * Checks if a stream has been created with DIRECT flag attribute, i.e. bypassing Audio Flinger
     * mixer thread. This flag is only applicable for output streams.
     * @return true if the stream is an output stream flagged as direct, false otherwise.
     */
    inline bool isDirect() const { return isOut() && (getFlagMask() & AUDIO_OUTPUT_FLAG_DIRECT); }

    /**
     * Use Case.
     * For an input stream, use case is known as the input source.
     * For an output stream: still not propagated to audio hal.
     *
     * @return use case.
     */
    virtual uint32_t getUseCaseMask() const = 0;

    /**
     * Get output silence to be appended before playing.
     * Some route may require to append silence in the ring buffer as powering on of components
     * involved in this route may take a while, and audio sample might be lost. It will result in
     * loosing the beginning of the audio samples.
     *
     * @return silence to append in milliseconds.
     */
    uint32_t getOutputSilencePrologMs() const;

    /**
     * Adds an effect to the mask of requested effect.
     *
     * @param[in] effectId Id of the requested effect.
     */
    void addRequestedEffect(uint32_t effectId);

    /**
     * Removes an effect from the mask of requested effect.
     *
     * @param[in] effectId Id of the requested effect.
     */
    void removeRequestedEffect(uint32_t effectId);

    /**
     * Get effects requested for this stream.
     * The route manager will select the route that supports all requested effects.
     *
     * @return mask with Id of requested effects
     */
    uint32_t getEffectRequested() const { return mEffectsRequestedMask; }

    /**
     * Get the sample specifications of the stream route.
     *
     * @return sample specifications.
     */
    SampleSpec routeSampleSpec() const { return mRouteSampleSpec; }

    /**
     * Get the stream sample specification.
     * Stream Sample specification is the sample spec in which the client gives/receives samples
     *
     * @return sample specifications.
     */
    SampleSpec streamSampleSpec() const
    {
        return mSampleSpec;
    }

    /**
     * Set an audio configuration to a stream. It is limited to the sample rate, channel mask and
     * format. The config must be valid, the check must be performed by the stream manager.
     *
     * @param[in] config to be set for the stream.
     * @param[in] direction of the stream (true=playback, false=capture).
     */
    inline void setConfig(const audio_config_t &config, bool isOut)
    {
        setSampleRate(config.sample_rate);
        setFormat(config.format);
        setChannels(config.channel_mask, isOut);
    }

    /**
     * Get the sample rate of the stream.
     *
     * @return sample rate of the stream.
     */
    inline uint32_t getSampleRate() const
    {
        return mSampleSpec.getSampleRate();
    }

    /**
     * Set the sample rate of the stream.
     *
     * @param[in] rate of the stream.
     */
    inline void setSampleRate(uint32_t rate)
    {
        mSampleSpec.setSampleRate(rate);
    }

    /**
     * Get the format of the stream.
     *
     * @return format of the stream.
     */
    inline audio_format_t getFormat() const
    {
        return mSampleSpec.getFormat();
    }

    /**
     * Set the format of the stream.
     *
     * @param[in] format of the stream.
     */
    inline void setFormat(audio_format_t format)
    {
        mSampleSpec.setFormat(format);
    }

    /**
     * Get the channel count of the stream.
     *
     * @return channel count of the stream.
     */
    inline uint32_t getChannelCount() const
    {
        return mSampleSpec.getChannelCount();
    }

    /**
     * Get the channel count of the stream.
     *
     * @return channel count of the stream.
     */
    inline void setChannelCount(uint32_t channels)
    {
        return mSampleSpec.setChannelCount(channels);
    }


    /**
     * Get the channels of the stream.
     * Channels is a mask, each bit represents a specific channel.
     *
     * @return channel mask of the stream.
     */
    inline audio_channel_mask_t getChannels() const
    {
        return mSampleSpec.getChannelMask();
    }

    /**
     * Set the channels of the stream.
     * Channels is a mask, each bit represents a specific channel.
     *
     * @param[in] channel mask of the stream.
     * @param[in] direction of the stream (true=playback, false=capture).
     */
    inline void setChannels(audio_channel_mask_t mask, bool isOut)
    {
        mSampleSpec.setChannelMask(mask, isOut);
    }

    /**
     * Reset the new stream route.
     */
    void resetNewStreamRoute();

    /**
     * Set a new route for this stream.
     * No need to lock as the newStreamRoute is for unique usage of the route manager,
     * so accessed from atomic context.
     *
     * @param[in] newStreamRoute: stream route to be attached to this stream.
     */
    void setNewStreamRoute(IStreamRoute *newStreamRoute);

    virtual uint32_t getBufferSizeInBytes() const = 0;

    virtual size_t getBufferSizeInFrames() const = 0;

    /**
     * Read frames from audio device.
     *
     * @param[in] buffer: audio samples buffer to fill from audio device.
     * @param[out] frames: number of frames to read.
     * @param[out] error: string containing readable error, if any is set
     *
     * @return status_t error code of the pcm read operation.
     */
    virtual android::status_t pcmReadFrames(void *buffer, size_t frames,
                                            std::string &error) const = 0;

    /**
     * Write frames to audio device.
     *
     * @param[in] buffer: audio samples buffer to render on audio device.
     * @param[out] frames: number of frames to render.
     * @param[out] error: string containing readable error, if any is set
     *
     * @return status_t error code of the pcm write operation.
     */
    virtual android::status_t pcmWriteFrames(void *buffer, ssize_t frames,
                                             std::string &error) const = 0;

    virtual android::status_t pcmStop() const = 0;

    /**
     * Returns available frames in pcm buffer and corresponding time stamp.
     * For an input stream, frames available are frames ready for the
     * application to read.
     * For an output stream, frames available are the number of empty frames available
     * for the application to write.
     */
    virtual android::status_t getFramesAvailable(size_t &avail,
                                                 struct timespec &tStamp) const = 0;

    IStreamRoute *getCurrentStreamRoute() const { return mCurrentStreamRoute; }

    IStreamRoute *getNewStreamRoute() const { return mNewStreamRoute; }

    /**
     * Attach the stream to its route.
     * Called by the StreamRoute to allow accessing the pcm device.
     * Set the new pcm device and sample spec given by the stream route.
     *
     * @return true if attach successful, false otherwise.
     */
    android::status_t attachRoute();

    /**
     * Detach the stream from its route.
     * Either the stream has been preempted by another stream or the stream has stopped.
     * Called by the StreamRoute to prevent from accessing the device any more.
     *
     * @return true if detach successful, false otherwise.
     */
    android::status_t detachRoute();

    /**
     * Set the device(s) assigned by the audio policy to this stream
     * @param[in] devices selected by the policy
     * @return OK if set correctly, error code otherwise.
     */
    android::status_t setDevices(audio_devices_t devices);

    /**
     * Retrieve the device(s) that has been assigned by the policy to this stream.
     * @return device(s) selected by the policy for this stream.
     */
    audio_devices_t getDevices() const
    {
        AutoR lock(mStreamLock);
        return mDevices;
    }

protected:
    /**
     * Attach the stream to its route.
     * Set the new pcm device and sample spec given by the stream route.
     * Called from locked context.
     *
     * @return true if attach successful, false otherwise.
     */
    virtual android::status_t attachRouteL();

    /**
     * Detach the stream from its route.
     * Either the stream has been preempted by another stream or the stream has stopped.
     * Called from locked context.
     *
     * @return true if detach successful, false otherwise.
     */
    virtual android::status_t detachRouteL();

    /**
     * Lock to protect not only the access to pcm device but also any access to device dependant
     * parameters as sample specification.
     */
    mutable android::RWLock mStreamLock;

    virtual ~IoStream() {}

    SampleSpec mSampleSpec; /**< stream sample specifications. */

private:
    void setCurrentStreamRouteL(IStreamRoute *currentStreamRoute);

    /**
     * Sets the route sample specification.
     * Must be called with stream lock held.
     *
     * @param[in] sampleSpec specifications of the route attached to the stream.
     */
    void setRouteSampleSpecL(SampleSpec sampleSpec);

    IStreamRoute *mCurrentStreamRoute; /**< route assigned to the stream (routed yet). */
    IStreamRoute *mNewStreamRoute; /**< New route assigned to the stream (not routed yet). */

    /**
     * Sample specifications of the route assigned to the stream.
     */
    SampleSpec mRouteSampleSpec;

    uint32_t mEffectsRequestedMask; /**< Mask of requested effects. */

    audio_devices_t mDevices = AUDIO_DEVICE_NONE; /**< devices assgined by the policy.*/
};

} // namespace intel_audio
