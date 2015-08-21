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

#include "AudioRoute.hpp"
#include "IStreamRoute.hpp"
#include "StreamRouteConfig.hpp"
#include <AudioUtils.hpp>
#include <SampleSpec.hpp>
#include <list>
#include <utils/Errors.h>

namespace intel_audio
{

class IoStream;
class IAudioDevice;

class AudioStreamRoute : public AudioRoute, private IStreamRoute
{
public:
    AudioStreamRoute(const std::string &name, bool isOut, uint32_t mask);

    virtual ~AudioStreamRoute();

    /**
     * Get the sample specifications of this route.
     * From IStreamRoute, intended to be called by the stream.
     *
     * @return sample specifications supported by the stream route.
     */
    virtual const SampleSpec getSampleSpec() const
    {
        return mSampleSpec;
    }

    /**
     * Get Audio Device.
     * From IStreamRoute, intended to be called by the stream.
     *
     * @return IAudioDevice handle.
     */
    virtual IAudioDevice *getAudioDevice()
    {
        return mAudioDevice;
    }

    /**
     * Get amount of silence delay upon stream opening.
     * From IStreamRoute, intended to be called by the stream.
     *
     * @return silence to be appended in milliseconds (from Route Parameter Manager settings).
     */
    virtual uint32_t getOutputSilencePrologMs() const
    {
        return mConfig.silencePrologInMs;
    }

    /**
     * Add an effect supported by this route.
     * This API is intended to be called by the Route Parameter Manager to add an audio effect
     * supported by this route according to the platform settings (XML configuration).
     *
     * @param[in] effect Audio Effect Name supported by this route.
     */
    void addEffectSupported(const std::string &effect);

    /**
     * Update the stream route configuration.
     * This API is intended to be called by the Route Parameter Manager to set the configuration
     * of this route according to the platform settings (XML configuration).
     *
     * @param[in] config Stream Route configuration.
     */
    void updateStreamRouteConfig(const StreamRouteConfig &config);

    /**
     * Assign a new stream to this route.
     * It overrides the applicability of Route Parameter Manager to apply the port strategy
     * and to match the mask of the stream requesting to be routed.
     *
     * @param true if the stream has been attached to the route, falsoe otherwise..
     */
    bool setStream(IoStream &stream);

    /**
     * route hook point.
     * Called by the route manager at enable step.
     */
    virtual android::status_t route(bool isPreEnable);

    /**
     * unroute hook point.
     * Called by the route manager at disable step.
     */
    virtual void unroute(bool isPostDisable);

    /**
     * configure hook point.
     * Called by the route manager at configure step.
     */
    void configure();

    /**
     * Reset the availability of the route.
     */
    virtual void resetAvailability();

    /**
     * Checks if the stream route matches the given stream attributes, i.e. the flags, the use case.
     *
     * @param stream candidate for using this route.
     *
     * @return true if the route matches, false otherwise.
     */
    bool isMatchingWithStream(const IoStream &stream) const;

    /**
     * Returns the applicable flags mask of the route
     */
    uint32_t getFlagsMask() const
    {
        return mConfig.flagMask;
    }

    /**
     * Returns the applicable use cases mask of the route
     * Note that use case mask has a different meaning according to the direction:
     * -inputSource for input route
     * -not used until now for output route.
     */
    uint32_t getUseCaseMask() const
    {
        return mConfig.useCaseMask;
    }

    /**
     * Check if a route needs to go throug flow routing stage.
     * It overrides the XML rules only if the route is used by a different stream.
     *
     * @return true if the route was used before rerouting, will be used after but needs to be
     *              reconfigured.
     */
    virtual bool needReflow() const;

    /**
     * Check if the route requires pre enabling.
     * Pre enabling means that the path must be configured before the pcm device is opened.
     *
     * @return true if the route needs pre enabling, false otherwise.
     */
    bool isPreEnableRequired()
    {
        return mConfig.requirePreEnable;
    }

    /**
     * Check if the route requires post disabling.
     * Post disabling means that the path must be deconfigured before the pcm device is closed.
     *
     * @return true if the route needs pre enabling, false otherwise.
     */
    bool isPostDisableRequired()
    {
        return mConfig.requirePostDisable;
    }

    /**
     * Get the pcm configuration associated with this route.
     *
     * @return pcm configuration of the route (from Route Parameter Manager settings).
     */
    const StreamRouteConfig &getRouteConfig() const
    {
        return mConfig;
    }

    /**
     * Get the latency associated with this route.
     * More precisely, it returns the size of the ring buffer configured when using this stream
     * route, which is a worst case.
     *
     * @return latency in microseconds.
     */
    uint32_t getLatencyInUs() const;

    /**
     * Get the period size associated to this route.
     * More precisely, it returns the size of a period of the ring buffer configured
     * when using this streamroute.
     *
     * @return period in microseconds.
     */
    uint32_t getPeriodInUs() const;

protected:
    IoStream *mCurrentStream; /**< Current stream attached to this route. */
    IoStream *mNewStream; /**< New stream that will be attached to this route after rerouting. */

    std::list<std::string> mEffectSupported; /**< list of name of supported effects. */
    uint32_t mEffectSupportedMask; /**< Mask of supported effects. */

private:
    /**
     * Checks if the use cases supported by this route are matching with the stream use case mask.
     *
     * @param[in] streamUseCaseMask mask of the use case requested by a stream
     *
     * @return true if the route supports the stream use case, false otherwise.
     */
    inline bool areUseCasesMatching(uint32_t streamUseCaseMask) const;

    /**
     * Checks if the flags supported by the route are matching with the given stream flags mask.
     *
     * @param[in] streamFlagMask mask of the flags requested by a stream
     *
     * @return true if the route supports the stream flags, false otherwise.
     */
    inline bool areFlagsMatching(uint32_t streamFlagMask) const;

    /**
     * Checks if route implements all effects in the mask.
     *
     * @param[in] effectMask mask of the effects to check.
     *
     * @return true if all effects in the mask are supported by the stream route,
     *          false otherwise
     */
    bool implementsEffects(uint32_t effectMask) const;

    /**
     * Get the id of current pcm device.
     *
     * @return the id of pcm device.
     */
    uint32_t getPcmDeviceId() const
    {
        return mConfig.deviceId;
    }

    /**
     * Get the audio card name.
     *
     * @return the name of audio card.
     */

    const char *getCardName() const
    {
        return mConfig.cardName;
    }

    /**
     * Attach a new stream to current audio route.
     *
     * @return status. OK if successful, error code otherwise.
     */
    android::status_t attachNewStream();

    /**
     * Dettach a stream from current audio route.
     *
     * @return status. OK if successful, error code otherwise.
     */
    android::status_t detachCurrentStream();

    StreamRouteConfig mConfig; /**< Configuration of the audio stream route. */

    SampleSpec mSampleSpec; /**< Sample specification of the stream route. */

    IAudioDevice *mAudioDevice; /**< Platform dependant audio device. */
};

} // namespace intel_audio
