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

#include <SampleSpec.hpp>
#include <string>

namespace intel_audio
{

struct StreamRouteConfig;
class IAudioDevice;

class IStreamRoute
{
public:
    IStreamRoute(const std::string &name, bool isOut) : mName(name), mIsOut(isOut) {}

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

    /**
     * Gets the direction of the route.
     *
     * @return true if the route an output route, false if input route.
     */
    bool isOut() const
    {
        return mIsOut;
    }

    /**
     * Returns identifier of current routing element
     *
     * @returns string representing the name of the routing element
     */
    const std::string &getName() const { return mName; }

private:
    std::string mName;

    bool mIsOut; /**< Tells whether the route is an output or not. */
};

} // namespace intel_audio
