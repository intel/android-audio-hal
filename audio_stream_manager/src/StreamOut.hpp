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
#pragma once

#include "Stream.hpp"
#include "Device.hpp"
#include <StreamInterface.hpp>

struct echo_reference_itfe;

namespace intel_audio
{

class StreamOut : public StreamOutInterface, public Stream
{
public:
    StreamOut(Device *parent, audio_io_handle_t handle, uint32_t flagMask, audio_devices_t devices,
              const std::string &address);

    virtual ~StreamOut();

    virtual android::status_t set(audio_config_t &config);

    // From AudioStreamOut
    virtual uint32_t getLatency();
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t setVolume(float left, float right);
    virtual android::status_t write(const void *buffer, size_t &bytes);
    virtual android::status_t getRenderPosition(uint32_t &dspFrames) const;
    virtual android::status_t getNextWriteTimestamp(int64_t &ts) const;
    virtual android::status_t flush();
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t setCallback(stream_callback_t, void *) { return android::OK; }
    /** @note API implemented in our Audio HAL only for direct streams */
    virtual android::status_t pause();
    /** @note API implemented in our Audio HAL only for direct streams */
    virtual android::status_t resume();
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t drain(audio_drain_type_t) { return android::OK; }
    virtual android::status_t getPresentationPosition(uint64_t &, struct timespec &) const;
    virtual android::status_t setDevice(audio_devices_t device);

    /**
     * Request to provide Echo Reference.
     *
     * @param[in] echo reference structure pointer.
     */
    void addEchoReference(struct echo_reference_itfe *reference);

    /**
     * Cancel the request to provide Echo Reference.
     *
     * @param[in] echo reference structure pointer.
     *
     */
    void removeEchoReference(struct echo_reference_itfe *reference);

    // From IoStream
    /**
     * Get stream direction. From Stream class.
     *
     * @return always true for ouput, false for input.
     */
    virtual bool isOut() const { return true; }

    virtual audio_port_role_t getRole() const { return AUDIO_PORT_ROLE_SOURCE; }

    /**
     * Checks if a stream has been muted or not by the policy.
     *
     * @return true if the stream has been muted by policy, false otherwise
     * @todo return mIsMuted once AOSP patch within AudioTrack sends the setVolume to the stream.
     */
    virtual bool isMuted() const { return mIsMuted; }

    void mute()
    {
        AutoW lock(mStreamLock);
        mIsMuted = true;
    }

    void unMute()
    {
        AutoW lock(mStreamLock);
        mIsMuted = false;
    }

protected:
    /**
     * Callback of route attachement called by the stream lib. (and so route manager)
     * Inherited from Stream class
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

private:
    /**
     * Push samples to echo reference.
     *
     * @param[in] buffer: output stream audio buffer to be appended to echo reference.
     * @param[in] frames: number of frames to be appended in echo reference.
     */
    void pushEchoReference(const void *buffer, ssize_t frames);

    /**
     * Get the playback delay.
     * Used when SW AEC effect is activated to informs at best the AEC engine of the rendering
     * delay.
     *
     * @param[in] frames: frames pushed in the echo reference.
     * @param[out] buffer: echo reference buffer given to set the timestamp when echo reference
     *                     is provided.
     *
     * @return 0 if success, error code otherwise.
     */
    int getPlaybackDelay(ssize_t frames, struct echo_reference_buffer *buffer);

    uint64_t mFrameCount; /**< number of audio frames written by AudioFlinger. */

    struct echo_reference_itfe *mEchoReference; /**< echo reference pointer, for SW AEC effect. */

    static const uint32_t mMaxAgainRetry; /**< Max retry for write operations before recovering. */
    static const uint32_t mWaitBeforeRetryUs; /**< Time to wait before retrial. */
    static const uint32_t mUsecPerMsec; /**< time conversion constant. */

    bool mIsMuted;
};
} // namespace intel_audio
