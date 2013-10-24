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

#include "Stream.h"
#include <SampleSpec.h>
#include <cutils/log.h>
#include <utils/RWLock.h>

class TinyAlsaAudioDevice;

class TinyAlsaStream : public Stream
{
public:
    TinyAlsaStream()
        : Stream::Stream(),
                              _device(NULL)
    {}

    virtual uint32_t getBufferSizeInBytes() const;

    virtual size_t getBufferSizeInFrames() const;

    virtual ssize_t pcmReadFrames(void *buffer, size_t frames);

    /**
     * Write frames to audio device.
     *
     * @param[in] buffer: audio samples buffer to render on audio device.
     * @param[out] frames: number of frames to render.
     *
     * @return number of frames rendered to audio device.
     */
    virtual ssize_t pcmWriteFrames(void *buffer, ssize_t frames);

    virtual android::status_t pcmStop();

    /**
     * Returns available frames in pcm buffer and corresponding time stamp.
     * For an input stream, frames available are frames ready for the
     * application to read.
     * For an output stream, frames available are the number of empty frames available
     * for the application to write.
     */
    virtual android::status_t getFramesAvailable(uint32_t &avail, struct timespec &tStamp);
protected:
    /**
     * Attach the stream to its route.
     * Called by the StreamRoute to allow accessing the pcm device.
     *
     * @return true if attach successful, false otherwise.
     */
    virtual android::status_t attachRouteL();

    /**
     * Detach the stream from its route.
     * Either the stream has been preempted by another stream or the stream has stopped.
     * Called by the StreamRoute to prevent from accessing the device any more.
     *
     * @return true if detach successful, false otherwise.
     */
    virtual android::status_t detachRouteL();
private:
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

    /**
     * Get the pcm device handle.
     * Must only be called if isRouteAvailable returns true.
     * and any access to the device must be called with Lock held.
     *
     * @return pcm handle on alsa tiny device.
     */
    pcm *getPcmDevice() const;

    TinyAlsaAudioDevice *_device;

    /**
     * maximum number of read/write retries.
     *
     * This constant is used to set maximum number of retries to do
     * on write/read operations before stating that error is not
     * recoverable and reset media server.
     */
    static const uint32_t MAX_READ_WRITE_RETRIES = 50;

    /** Ratio between microseconds and milliseconds */
    static const uint32_t USEC_PER_MSEC = 1000;

    /** Ratio between nanoseconds and microseconds */
    static const uint32_t NSEC_PER_USEC = 1000;

    /** maximum sleep time to be allowed by HAL, in microseconds. */
    static const uint32_t MAX_SLEEP_TIME = 1000000UL;
};
