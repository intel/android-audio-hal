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

#include "FormattedSubsystemObject.h"
#include "InstanceConfigurableElement.h"
#include "MappingContext.h"
#include <RouteInterface.hpp>

class RouteSubsystem;

class AudioStreamRoute : public CFormattedSubsystemObject
{
private:
    static const uint32_t mMaxStringSize = 257; /**< max string size (plus zero terminal). */

    /**
     * Mapped control structure - must be packed
     * Note that the component called config is either playback or capture config.
     * Playback will use output flags as applicability mask whereas capture will use input sources
     * as applicability mask.
     * Both Playback and Capture config component type will extend generic config type.
     * Extend mecanism will append the generic config parameter AFTER specific capture / playback
     * parameters.
     */
    struct Config
    {
        uint32_t flagMask; /**< applicable flags mask, either input/output flags. */
        uint32_t useCaseMask; /**< applicable use cases mask, input source, unused for output. */
        char effectSupported[mMaxStringSize]; /**< effects supported by the stream route. */
        uint32_t supportedDeviceMask; /**< Mask of supported devices by the stream route. */
        bool requirePreEnable; /**< require pre enable attribute. */
        bool requirePostDisable; /**< require post disable attribute. */
        uint32_t silencePrologInMs; /**< prolog silence to be added when opening the device in ms.*/
        uint32_t channel; /**< pcm channels supported. */
        char channelsPolicy[mMaxStringSize]; /**< channel policy supported. */
        uint32_t rate; /**< pcm rate supported. */
        uint8_t format; /**< pcm format supported. */
        uint32_t periodSize; /**< period size. */
        uint32_t periodCount; /**< period count. */
        uint32_t startThreshold; /**< start threshold. */
        uint32_t stopThreshold; /**< stop threshold. */
        uint32_t silenceThreshold; /**< silence threshold. */
        uint32_t availMin; /** avail min. */
        char dynamicChannelMapsControl[mMaxStringSize];
        char dynamicFormatsControl[mMaxStringSize];
        char dynamicRatesControl[mMaxStringSize];
    } __attribute__((packed));

public:
    AudioStreamRoute(const std::string &mappingValue,
                     CInstanceConfigurableElement *instanceConfigurableElement,
                     const CMappingContext &context,
                     core::log::Logger &logger);

protected:
    /**
     * Sync to HW.
     * From CSubsystemObject
     *
     * @param[out] error: if return code is false, it contains the description
     *                     of the error, empty std::string otherwise.
     *
     * @return true if success, false otherwise.
     */
    virtual bool sendToHW(std::string &error);

private:
    /**
     * Parse a concatenated list of channel policy separated by a coma.
     *
     * @param[in] channelPolicy std::string of concatenated channel policy.
     *
     * @return std::vector of channel policy.
     */
    std::vector<intel_audio::SampleSpec::ChannelsPolicy>
    parseChannelPolicyString(const std::string &channelPolicy);

    const RouteSubsystem *mRouteSubsystem; /**< Route subsytem plugin. */
    intel_audio::IRouteInterface *mRouteInterface; /**< Interface to communicate with Route Mgr. */

    std::string mRouteName; /**< stream route name. */
    std::string mCardName; /**< card name used by the stream route. */
    int32_t mDevice; /**< audio device used by the stream route. */
    std::string mDeviceAddress; /**< device address supported by this route. */
    bool mIsOut; /**< direction qualifier of the stream route. */
    bool mIsStreamRoute; /**< qualifier of the stream route. */

    static const std::string mOutputDirection; /**< string key to identify output routes. */
    static const std::string mStreamType; /**< key to identify stream route. */
    static const uint32_t mSinglePort = 1;  /**< only one port is mentionned for this route. */
    static const uint32_t mDualPorts = 2; /**< both port are mentionnent for this route. */
    static const std::string mPortDelimiter; /**< Delimiter to parse a list of ports. */
    static const std::string mStringDelimiter; /**< Delimiter to parse strings. */
    static const std::string mChannelPolicyCopy; /**< copy channel policy tag. */
    static const std::string mChannelPolicyIgnore; /**< ignore channel policy tag. */
    static const std::string mChannelPolicyAverage; /**< average channel policy tag. */
};
