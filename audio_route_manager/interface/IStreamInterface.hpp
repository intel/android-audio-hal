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

#include "StreamRouteConfig.hpp"
#include "AudioCapabilities.hpp"
#include <utils/Errors.h>
#include <string>

namespace intel_audio
{

class IoStream;

struct IStreamInterface
{
    /**
     * Starts the route manager service.
     */
    virtual android::status_t startService() = 0;

    /**
     * Stops the route manager service.
     */
    virtual android::status_t stopService() = 0;

    /**
     * Adds a streams to the list of opened streams.
     *
     * @param[in] stream: opened stream to be appended.
     */
    virtual void addStream(IoStream &stream) = 0;

    /**
     * Removes a streams from the list of opened streams.
     *
     * @param[in] stream: closed stream to be removed.
     */
    virtual void removeStream(IoStream &stream) = 0;

    /**
     * Trigs a routing reconsideration.
     *
     * @param[in] synchronous: if set, re routing shall be synchronous.
     */
    virtual void reconsiderRouting(bool isSynchronous = false) = 0;

    /**
     * Sets the voice volume.
     * Called from AudioSystem/Policy to apply the volume on the voice call stream which is
     * platform dependent.
     *
     * @param[in] gain the volume to set in float format in the expected range [0 .. 1.0]
     *                 Note that any attempt to set a value outside this range will return -ERANGE.
     *
     * @return OK if success, error code otherwise.
     */
    virtual android::status_t setVoiceVolume(float gain) = 0;

    /**
     * Get the voice output stream.
     * The purpose of this function is
     * - to retrieve the instance of the voice output stream in order to write playback frames
     * as echo reference for
     *
     * @return voice output stream
     */
    virtual IoStream *getVoiceOutputStream() = 0;

    /**
     * Get the latency.
     * Upon creation, streams need to provide latency. As stream are not attached
     * to any route at creation, they must get a latency dependant of the
     * platform to provide information of latency and buffersize (inferred from ALSA ring buffer).
     * This latency depends on the route that will be assigned to the stream. The route manager
     * will return the route matching those stream attributes. If no route is found, it returns 0.
     *
     * @param[in] stream requesting the latency
     *
     * @return latency in microseconds
     */
    virtual uint32_t getLatencyInUs(const IoStream &stream) const = 0;

    /**
     * Get the period size.
     * Upon creation, streams need to provide buffer size. As stream are not attached
     * to any route at creation, they must get a latency dependant of the
     * platform to provide information of latency and buffersize (inferred from ALSA ring buffer).
     * This period depends on the route that will be assigned to the stream. The route manager
     * will return the route matching those stream attributes. If no route is found, it returns 0.
     *
     * @param[in] stream requesting the period
     *
     * @return period size in microseconds
     */
    virtual uint32_t getPeriodInUs(const IoStream &stream) const = 0;

    /**
     * Checks whether the stream and its audio configuration that it wishes to use match
     * with a stream route.
     *
     * @param[in] stream to be checked for support
     *
     * @return true if the audio configuration of the stream is supported by a route,
     *              false otherwise.
     */
    virtual bool supportStreamConfig(const IoStream &stream) const = 0;

    /**
     * Retrieve the capabilities for a given stream, i.e. what is the list of sample rates, formats
     * and channel masks allowed for a given stream, i.e. for a request to play / capture from a
     * given device, during a given use case (aka input source, not defined for output), in a given
     * manneer (aka with input or output flags).
     *
     * @param[in] stream for which the capabilities are requested
     *
     * @return capabilities supported for this stream.
     */
    virtual AudioCapabilities getCapabilities(const IoStream &stream) const = 0;

    virtual android::status_t setParameters(const std::string &keyValuePair,
                                            bool isSynchronous = false) = 0;

    virtual std::string getParameters(const std::string &keys) const = 0;

    /**
     * Print debug information from target debug files
     */
    virtual void printPlatformFwErrorInfo() const = 0;

    protected:
        virtual ~IStreamInterface() {}
};

} // namespace intel_audio
