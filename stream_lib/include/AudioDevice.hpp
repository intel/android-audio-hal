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

#include <StreamRouteConfig.hpp>
#include <stdint.h>
#include <utils/Errors.h>

namespace intel_audio
{

class IAudioDevice
{
public:

    virtual android::status_t open(const char *cardName, uint32_t deviceId,
                                   const StreamRouteConfig &config, bool isOut) = 0;

    virtual android::status_t close() = 0;

    /**
     * Checks the state of the audio device.
     *
     * @return true if the device is opened and ready to be used, false otherwise.
     */
    virtual bool isOpened() = 0;

    virtual ~IAudioDevice() {}

    virtual android::status_t pcmReadFrames(void *buffer, size_t frames,
                                            std::string &error) const = 0;

    virtual android::status_t pcmWriteFrames(void *buffer, ssize_t frames,
                                             std::string &error) const = 0;

    virtual uint32_t getBufferSizeInBytes() const = 0;

    virtual size_t getBufferSizeInFrames() const = 0;

    virtual android::status_t getFramesAvailable(size_t &avail, struct timespec &tStamp) const = 0;

    virtual android::status_t pcmStop() const = 0;
};

} // namespace intel_audio
