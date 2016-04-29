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

#include "AudioRoute.hpp"
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

class AudioStreamRoute : public AudioRoute, private IStreamRoute
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
        return SampleSpec(popcount(mCurrentChannelMask), mCurrentFormat, mCurrentRate,
                          mConfig.channelsPolicy);
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
        // Ugly WA: the policy tries to open a stream to retrieve the capabilities BEFORE
        // broadcastingcapabilities CONNECT message used by us to trig the discovery of the
        // capabilities. So, as capabilities are still empty, try to populate it...
        // @TODO: remove it once policy patch broadcasting device state before loading the profile
        // has been merged.
        if (mCapabilities.supportedFormats.empty() || mCapabilities.supportedChannelMasks.empty() ||
            mCapabilities.supportedRates.empty()) {
            const_cast<AudioStreamRoute *>(this)->loadCapabilities();
        }
        return supportRate(stream.getSampleRate()) && supportFormat(stream.getFormat()) &&
               supportChannelMask(stream.getChannels());
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

    virtual bool needReflow() const
    {
        return stillUsed() &&
               (mRoutingStageRequested.test(Flow) || mRoutingStageRequested.test(Path));
    }

    virtual bool needRepath() const
    {
        return stillUsed() && (mRoutingStageRequested.test(Path) || mCurrentStream != mNewStream);
    }

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

    /** As prepare is called on both RouteCollection & StreamRouteCollection, Nope it here. */
    virtual void prepare() {}

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
    const StreamRouteConfig getRouteConfig() const;

    uint32_t getSupportedDeviceMask() const
    {
        return mConfig.supportedDeviceMask;
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

    AudioCapabilities getCapabilities() const { return mCapabilities; }

    /**
     * Checks if the devices assigned by the policy to the stream are matching the devices supported
     * by the stream route
     * @param[in] streamDeviceMask Mask of devices of the stream
     * @return true if supported, false otherwise.
     */
    bool supportDevices(audio_devices_t streamDeviceMask) const;

protected:
    IoStream *mCurrentStream; /**< Current stream attached to this route. */
    IoStream *mNewStream; /**< New stream that will be attached to this route after rerouting. */

    std::list<std::string> mEffectSupported; /**< list of name of supported effects. */
    uint32_t mEffectSupportedMask; /**< Mask of supported effects. */

    AudioCapabilities mCapabilities;

private:
    /**
     * Load the capabilities in term of channel mask supported, i.e. it initializes the vector of
     * supported channel mask (stereo, 5.1, 7.1, ...)
     * @return OK is channel masks supported has been set correctly, error code otherwise.
     */
    android::status_t loadChannelMaskCapabilities();

    inline bool remapperSupported(const audio_channel_mask_t mask) const
    {
        // We only support convertion to / from at most 2 channels
        const int maxChannelCountSupported =
            audio_channel_count_from_out_mask(AUDIO_CHANNEL_OUT_QUAD);
        return (popcount(mask) <= maxChannelCountSupported) &&
               (popcount(mCapabilities.getDefaultChannelMask()) <= maxChannelCountSupported);
    }

    inline bool reformatterSupported(const audio_format_t format) const
    {
        // We only support convertion to/from S16 from/to S8_24 respectively
        return ((format == AUDIO_FORMAT_PCM_16_BIT) || (format == AUDIO_FORMAT_PCM_8_24_BIT)) &&
               ((mCapabilities.getDefaultFormat() == AUDIO_FORMAT_PCM_16_BIT) ||
                (mCapabilities.getDefaultFormat() == AUDIO_FORMAT_PCM_8_24_BIT));
    }

    inline bool resamplerSupported(uint32_t rate) const
    {
        return (rate != 0) && (mCapabilities.getDefaultRate() != 0);
    }

    inline bool supportRate(uint32_t rate) const
    {
        return mCapabilities.supportRate(rate) || resamplerSupported(rate);
    }
    inline bool supportFormat(audio_format_t format) const
    {
        return mCapabilities.supportFormat(format) || reformatterSupported(format);
    }
    inline bool supportChannelMask(audio_channel_mask_t channelMask) const
    {
        return mCapabilities.supportChannelMask(channelMask) || remapperSupported(channelMask);
    }

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

    IAudioDevice *mAudioDevice; /**< Platform dependant audio device. */

    uint32_t mCurrentRate = 0;
    audio_format_t mCurrentFormat = AUDIO_FORMAT_DEFAULT;
    audio_channel_mask_t mCurrentChannelMask = AUDIO_CHANNEL_NONE;
};

} // namespace intel_audio
