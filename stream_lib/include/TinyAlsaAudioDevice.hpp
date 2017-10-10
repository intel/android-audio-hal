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

#include "AudioDevice.hpp"
#include <tinyalsa/asoundlib.h>

namespace intel_audio
{

struct StreamRouteConfig;

class TinyAlsaAudioDevice : public IAudioDevice
{
public:
    TinyAlsaAudioDevice() : mPcmDevice(NULL) {}

    virtual android::status_t open(const char *cardName, uint32_t deviceId,
                                   const StreamRouteConfig &config, bool isOut);

    virtual bool isOpened();

    virtual android::status_t close();

    virtual android::status_t pcmReadFrames(void *buffer, size_t frames, std::string &error) const;

    virtual android::status_t pcmWriteFrames(void *buffer, ssize_t frames,
                                             std::string &error) const;

    virtual uint32_t getBufferSizeInBytes() const;

    virtual size_t getBufferSizeInFrames() const;

    virtual android::status_t getFramesAvailable(size_t &avail, struct timespec &tStamp) const;

    virtual android::status_t pcmStop() const;

private:
    pcm *mPcmDevice; /**< Handle on tiny alsa PCM device. */
};

} // namespace intel_audio
