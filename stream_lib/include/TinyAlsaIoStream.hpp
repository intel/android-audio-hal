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

#include "IoStream.hpp"
#include <SampleSpec.hpp>
#include <utils/RWLock.h>

namespace intel_audio
{

class TinyAlsaAudioDevice;

class TinyAlsaIoStream : public IoStream
{
public:
    TinyAlsaIoStream()
        : IoStream::IoStream(), mDevice(NULL)
    {}

    virtual uint32_t getBufferSizeInBytes() const;

    virtual size_t getBufferSizeInFrames() const;

    virtual android::status_t pcmReadFrames(void *buffer, size_t frames, std::string &error) const;

    virtual android::status_t pcmWriteFrames(void *buffer, ssize_t frames,
                                             std::string &error) const;

    virtual android::status_t pcmStop() const;

    /**
     * Returns available frames in pcm buffer and corresponding time stamp.
     * For an input stream, frames available are frames ready for the
     * application to read.
     * For an output stream, frames available are the number of empty frames available
     * for the application to write.
     */
    virtual android::status_t getFramesAvailable(size_t &avail, struct timespec &tStamp) const;

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
     * Get the pcm device handle.
     * Must only be called if isRouteAvailable returns true.
     * and any access to the device must be called with Lock held.
     *
     * @return pcm handle on alsa tiny device.
     */
    pcm *getPcmDevice() const;

    TinyAlsaAudioDevice *mDevice;

    /** Ratio between microseconds and milliseconds */
    static const uint32_t mUsecPerMsec = 1000;
};

} // namespace intel_audio
