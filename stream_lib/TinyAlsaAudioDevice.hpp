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

#include "AudioDevice.hpp"
#include <tinyalsa/asoundlib.h>

namespace intel_audio
{

class StreamRouteConfig;

class TinyAlsaAudioDevice : public IAudioDevice
{
public:
    TinyAlsaAudioDevice()
        : mPcmDevice(NULL) {}

    /**
     * Get the pcm device handle.
     * Must only be called if isRouteAvailable returns true.
     * and any access to the device must be called with Lock held.
     *
     * @return pcm handle on alsa tiny device.
     */
    pcm *getPcmDevice();

    virtual android::status_t open(const char *cardName, uint32_t deviceId,
                                   const StreamRouteConfig &config, bool isOut);

    virtual bool isOpened();

    virtual android::status_t close();

private:
    pcm *mPcmDevice; /**< Handle on tiny alsa PCM device. */
};

} // namespace intel_audio
