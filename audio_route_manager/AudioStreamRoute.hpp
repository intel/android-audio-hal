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

#include "IStreamRoute.hpp"
#include "StreamRouteConfig.hpp"
#include "AudioCapabilities.hpp"
#include <AudioUtils.hpp>
#include <SampleSpec.hpp>
#include <IoStream.hpp>
#include <list>
#include <utils/Errors.h>

namespace intel_audio
{

class IAudioDevice;

class AudioStreamRoute;

/**
 * Create a route from a given type, with a given name and direction.
 *
 * @tparam T type of audio stream route, default is stream routebase class.
 * @param[in] name of the route to create
 * @param[in] isOut direction indicator: true for playback route, false for capture
 *
 * @return stream route object
 */
template <typename T = AudioStreamRoute>
AudioStreamRoute *createT(const std::string &name, bool isOut)
{
    return new T(name, isOut);
}

class AudioStreamRoute : public IStreamRoute
{
public:
    AudioStreamRoute(const std::string &name, bool isOut);

    virtual ~AudioStreamRoute();

    /**
     * Upon connection of device managed by this route, it loads the capabilities from the device
     * (example: for an HDMI screen, the driver will read EDID to retrieve the screen audio
     * capabilities.
     */
    virtual void loadCapabilities();

    /**
     * For route with dynamic behavior: upon disconnection of device managed by this route,
     * the capabilities shall be resetted.
     */
    void resetCapabilities();

    /**
     * Get the sample specifications of this route.
     * From IStreamRoute, intended to be called by the stream.
     *
     * @return sample specifications supported by the stream route.
     */
    virtual const SampleSpec getSampleSpec() const
    {
        return SampleSpec(mConfig.getChannelCount(), mConfig.getFormat(),
                          mConfig.getRate(), mConfig.channelsPolicy);
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
     * Set an effect supported by this route.
     * This API is intended to be called by the Route Parameter Manager to add an audio effect
     * supported by this route according to the platform settings (XML configuration).
     *
     * @param[in] effect Audio Effect Name supported by this route.
     */
    void setEffectSupported(const std::vector<std::string> &effects);

    /**
     * Update the stream route configuration.
     * This API is intended to be called by the Route Parameter Manager to set the configuration
     * of this route according to the platform settings (XML configuration).
     *
     * @param[in] config Stream Route configuration.
     */
    void setConfig(const StreamRouteConfig &config) { mConfig = config; }

    /**
     * Set the audio device managed by this stream route
     *
     * @param[in] device audio to be associated to this stream route.
     */
    void setDevice(IAudioDevice *device) { mAudioDevice = device; }

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
    android::status_t route(bool isPreEnable);

    /**
     * unroute hook point.
     * Called by the route manager at disable step.
     */
    void unroute(bool isPostDisable);

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
     * Checks if the stream route capabilities are matching with the stream sample specification
     * i.e. format, subformat, sample rate, channel count, specific encoded format...
     * For non dynamic settings, return true if supported converter can be used
     * For dynamic settings (intend to adress direct stream), the list of supported setting must
     * contain the stream audio configuration.
     * Either natively by the stream route
     * (checks done with its capabilities) or using a converter from the given config to
     * the default route config.
     *
     * @param[in] stream to be checked against this route
     *
     * @return true if the config is supported, false otherwise.
     */
    inline bool supportStreamConfig(const IoStream &stream) const
    {
        return mConfig.supportSampleSpec(stream.streamSampleSpec());
    }

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
     * Sets the route as in use.
     *
     * Calling this API will propagate the in use attribute to the ports belonging to this route.
     *
     */
    void setUsed() { mIsUsed = true; }

    bool isUsed() const
    {
        return mIsUsed;
    }

    /**
     * Checks if the route was previously used.
     *
     * Previously used means if the route was in use before the routing
     * reconsideration of the route manager.
     *
     * @return true if the route was in use, false otherwise.
     */
    bool previouslyUsed() const
    {
        return mPreviouslyUsed;
    }

    inline bool stillUsed() const { return previouslyUsed() && isUsed(); }

    uint32_t getMask() const { return mMask; }

    /**
     * Check if the route requires pre enabling.
     * Pre enabling means that the path must be configured before the pcm device is opened.
     *
     * @return true if the route needs pre enabling, false otherwise.
     */
    bool isPreEnableRequired() const { return mConfig.requirePreEnable; }

    /**
     * Check if the route requires post disabling.
     * Post disabling means that the path must be deconfigured before the pcm device is closed.
     *
     * @return true if the route needs pre enabling, false otherwise.
     */
    bool isPostDisableRequired() const { return mConfig.requirePostDisable; }

    /**
     * Get the pcm configuration associated with this route.
     *
     * @return pcm configuration of the route (from Route Parameter Manager settings).
     */
    const StreamRouteConfig &getRouteConfig() const { return mConfig; }

    uint32_t getSupportedDeviceMask() const { return mConfig.supportedDeviceMask; }

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

    /**
     * Checks if the devices assigned by the policy to the stream are matching the devices supported
     * by the stream route
     * @param[in] streamDeviceMask Mask of devices of the stream
     * @return true if supported, false otherwise.
     */
    bool supportDevices(audio_devices_t streamDeviceMask) const;

    /**
     * Checks if a route needs to be muted / unmuted.
     *
     * It overrides the need reconfigure of Route Parameter Manager to ensure the route was
     * previously used and will be used after the routing reconsideration.
     *
     * @return true if the route needs reconfiguring, false otherwise.
     */
    bool needReflow();

    /**
     * Checks if a route needs to be disabled / enabled.
     *
     * It overrides the need reroute of Route Parameter Manager to ensure the route was
     * previously used and will be used after the routing reconsideration.
     *
     * @return true if the route needs rerouting, false otherwise.
     */
    bool needRepath() const
    {
        return stillUsed() && (mCurrentStream != mNewStream);
    }

    AudioCapabilities getCapabilities() const { return mConfig.mAudioCapabilities; }

protected:
    IoStream *mCurrentStream; /**< Current stream attached to this route. */
    IoStream *mNewStream; /**< New stream that will be attached to this route after rerouting. */

    std::list<std::string> mEffectSupported; /**< list of name of supported effects. */
    uint32_t mEffectSupportedMask; /**< Mask of supported effects. */
    StreamRouteConfig mConfig; /**< Configuration of the audio stream route. */

private:
    bool supportDeviceAddress(const std::string &streamDeviceAddress, audio_devices_t device) const;

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
        return mConfig.cardName.c_str();
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

    uint32_t mMask; /**< A route is identified with a mask, it helps for criteria representation. */

    bool mIsUsed; /**< Route will be used after reconsidering the routing. */
    bool mPreviouslyUsed; /**< Route was used before reconsidering the routing. */

    IAudioDevice *mAudioDevice; /**< Platform dependant audio device. */
};

} // namespace intel_audio
