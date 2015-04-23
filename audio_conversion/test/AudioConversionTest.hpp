/*
 * Copyright (C) 2014-2015 Intel Corporation
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

#include <stddef.h>
#include <media/AudioBufferProvider.h>
#include <NonCopyable.hpp>
#include <gtest/gtest.h>
#include <stdint.h>

namespace intel_audio
{

class MyAudioBufferProvider
    : public android::AudioBufferProvider,
      private audio_comms::utilities::NonCopyable
{

public:
    MyAudioBufferProvider(const uint16_t *src, uint32_t samples)
        : readPos(0)
    {

        sourceBuffer = new uint16_t[samples];
        memcpy(sourceBuffer, src, samples * sizeof(uint16_t));
    }

    ~MyAudioBufferProvider()
    {

        delete[] sourceBuffer;
    }

    /**
     * Provides new buffer to the client.
     * From AudioBufferProvider interface.
     *
     * @param[out] buffer audio data to be provided to the client.
     *                    Buffer shall not be taken into account by the client if
     *                    error code is returned.
     * @param[in] pts local time when the next sample yielded by getNextBuffer will be rendered.
     *
     * @return status_t OK if buffer provided, error code otherwise.
     */
    virtual android::status_t getNextBuffer(android::AudioBufferProvider::Buffer *buffer,
                                            int64_t /*pts*/)
    {

        buffer->raw = &sourceBuffer[readPos];
        readPos += buffer->frameCount;
        return android::NO_ERROR;
    }

    /**
     * Release buffer provided to the client on previous getNextBuffer call.
     * From AudioBufferProvider interface.
     *
     * @param[out] buffer client buffer to be released. Audio data have been consumed by the client
     *                    and the provider memory can now be released.
     */
    virtual void releaseBuffer(Buffer */*buffer*/) {}

private:
    uint32_t readPos; /**< Position within the source buffer. */
    uint16_t *sourceBuffer; /**< Source buffer to convert. */

};

}  // namespace intel_audio
