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

#include <SampleSpec.hpp>
#include <string>

namespace intel_audio
{

struct StreamRouteConfig;
class IAudioDevice;

class IStreamRoute
{
public:
    /**
     * Get the sample specifications of the stream route.
     *
     * @return sample specifications.
     */
    virtual const SampleSpec getSampleSpec() const = 0;

    /**
     * Get output silence to be appended before playing.
     * Some route may require to append silence in the ring buffer as powering on of components
     * involved in this route may take a while, and audio sample might be lost. It will result in
     * loosing the beginning of the audio samples.
     *
     * @return silence to append in milliseconds.
     */
    virtual uint32_t getOutputSilencePrologMs() const = 0;

    virtual IAudioDevice *getAudioDevice() = 0;

    virtual ~IStreamRoute() {}
};

} // namespace intel_audio
