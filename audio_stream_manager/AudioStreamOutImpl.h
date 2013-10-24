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

#include "AudioIntelHAL.h"
#include "AudioStream.h"

struct echo_reference_itfe;

namespace android_audio_legacy
{

class AudioStreamOutImpl : public AudioStreamOut, public AudioStream
{
public:
    AudioStreamOutImpl(AudioIntelHAL *parent);
    virtual ~AudioStreamOutImpl();

    virtual uint32_t sampleRate() const
    {
        return AudioStream::sampleRate();
    }

    virtual size_t bufferSize() const;

    virtual uint32_t channels() const;

    virtual int format() const
    {
        return AudioStream::format();
    }

    /**
     * Get the latency of the output stream.
     * Android wants latency in milliseconds. It must match the latency introduced by
     * driver buffering (Size of ring buffer). Audio Flinger will use this value to
     * calculate each track buffer depth.
     *
     * @return latency in ms
     */
    virtual uint32_t latency() const;

    virtual ssize_t write(const void *buffer, size_t bytes);
    virtual status_t dump(int fd, const Vector<String16> &args);

    virtual status_t setVolume(float left, float right);

    virtual status_t standby();

    virtual status_t setParameters(const String8 &keyValuePairs);

    virtual String8 getParameters(const String8 &keys)
    {
        return AudioStream::getParameters(keys);
    }

    /**
     * Get the render position.
     *
     * @param[out] dspFrames: the number of audio frames written by AudioFlinger to audio HAL to
     * to Audio Codec since the output on which the specified stream is playing
     * has exited standby.
     *
     * @return NO_ERROR if successfyll operation, error code otherwise.
     */
    virtual status_t getRenderPosition(uint32_t *dspFrames);

    /**
     * Request to provide Echo Reference.
     *
     * @param[in] echo reference structure pointer.
     */
    void addEchoReference(struct echo_reference_itfe *reference);

    /**
     * Cancel the request to provide Echo Reference.
     * Called from locked context.
     *
     * @param[in] echo reference structure pointer.
     */
    void removeEchoReferenceL(struct echo_reference_itfe *reference);

    /**
     * Cancel the request to provide Echo Reference.
     *
     * @param[in] echo reference structure pointer.
     */
    void removeEchoReference(struct echo_reference_itfe *reference);

    /**
     * Get the echo reference for AEC effect.
     * Called by an input stream on which SW echo cancellation is performed.
     *
     * @return valid echo reference is found, NULL otherwise.
     */
    struct echo_reference_itfe *getEchoReference() { return _echoReference; }

    /**
     * flush the data down the flow. It is similar to drop.
     */
    virtual status_t flush();

    /**
     * Get stream direction. From Stream class.
     *
     * @return always true for ouput, false for input.
     */
    virtual bool isOut() const { return true; }
protected:
    /**
     * Callback of route attachement called by the stream lib. (and so route manager)
     * Inherited from Stream class
     *
     * @return OK if streams attached successfully to the route, error code otherwise.
     */
    virtual status_t attachRouteL();

    /**
     * Callback of route detachement called by the stream lib. (and so route manager)
     * Inherited from Stream class
     *
     * @return OK if streams detached successfully from the route, error code otherwise.
     */
    virtual status_t detachRouteL();
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

    uint32_t _frameCount; /**< number of audio frames written by AudioFlinger. */

    struct echo_reference_itfe *_echoReference; /**< echo reference pointer, for SW AEC effect. */

    static const uint32_t MAX_AGAIN_RETRY; /**< Max retry for write operations before recovering. */
    static const uint32_t WAIT_BEFORE_RETRY_US; /**< Time to wait before retrial. */
    static const uint32_t USEC_PER_MSEC; /**< time conversion constant. */
};
}         // namespace android
