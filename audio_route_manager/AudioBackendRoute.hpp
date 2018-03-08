/*
 * Copyright (C) 2013-2018 Intel Corporation
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

#include "MixPortConfig.hpp"
#include "AudioRoute.hpp"
#include "AudioPort.hpp"
#include <AudioUtils.hpp>
#include <SampleSpec.hpp>
#include <IoStream.hpp>
#include <list>
#include <utils/Errors.h>

namespace intel_audio
{

/**
 * Use to describe the audio backend route which is parsed
 * from route element of audio_policy_configuration.xml.
 * An audio backend route only includes the devicePorts as
 * sources and sinks.
 */
class AudioBackendRoute : public AudioRoute
{
public:
    AudioBackendRoute(std::string name, AudioPorts &sinks, AudioPorts &sources)
        : AudioRoute(name, sinks, sources, ROUTE_TYPE_BACKEND)
    {}

    virtual ~AudioBackendRoute() {}


    bool isMixRoute() { return false; }


    /**
     * Reset the availability of the route.
     */
    virtual void resetAvailability();

    /**
     * Get the supported devices of the route.
     * @return devices mask of android
     * TODO: Currently it isn't used, so return 0.
     * Implement later.
     */
    virtual uint32_t getSupportedDeviceMask() const { return 0; }


    /**
     * Checks if a route needs to be muted / unmuted.
     *
     * It overrides the need reconfigure of Route Parameter Manager to ensure the route was
     * previously used and will be used after the routing reconsideration.
     *
     * @return true if the route needs reconfiguring, false otherwise.
     */
    virtual bool needReflow();

    /**
     * Checks if a route needs to be disabled / enabled.
     *
     * It overrides the need reroute of Route Parameter Manager to ensure the route was
     * previously used and will be used after the routing reconsideration.
     *
     * @return true if the route needs rerouting, false otherwise.
     */
    virtual bool needRepath() const;

    virtual android::status_t dump(const int fd, int spaces = 0) const;

};

} // namespace intel_audio
