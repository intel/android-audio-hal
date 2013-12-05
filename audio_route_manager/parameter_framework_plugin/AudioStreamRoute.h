/*
 * INTEL CONFIDENTIAL
 * Copyright © 2013 Intel
 * Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel’s prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#pragma once

#include "FormattedSubsystemObject.h"
#include "StreamRouteConfig.h"
#include "InstanceConfigurableElement.h"
#include "MappingContext.h"
#include <RouteInterface.h>

class RouteSubsystem;

class AudioStreamRoute : public CFormattedSubsystemObject
{
private:
    static const uint32_t MAX_STRING_SIZE = 257; /**< max string size (plus zero terminal). */

    /**
     * Mapped control structure - must be packed
     */
    struct Config
    {
        bool requirePreEnable; /**< require pre enable attribute. */
        bool requirePostDisable; /**< require post disable attribute. */
        uint32_t silencePrologInMs; /**< prolog silence to be added when opening the device in ms.*/
        uint32_t channel; /**< pcm channels supported. */
        char channelsPolicy[MAX_STRING_SIZE]; /**< channel policy supported. */
        uint32_t rate; /**< pcm rate supported. */
        uint8_t format; /**< pcm format supported. */
        uint32_t periodSize; /**< period size. */
        uint32_t periodCount; /**< period count. */
        uint32_t startThreshold; /**< start threshold. */
        uint32_t stopThreshold; /**< stop threshold. */
        uint32_t silenceThreshold; /**< silence threshold. */
        uint32_t availMin; /** avail min. */
        uint32_t applicabilityMask; /**< applicability mask, either input source or output flags. */
        char effectSupported[MAX_STRING_SIZE]; /**< effects supported by the stream route. */
    } __attribute__((packed));
public:
    AudioStreamRoute(const string &mappingValue,
                     CInstanceConfigurableElement *instanceConfigurableElement,
                     const CMappingContext &context);
protected:
    /**
     * Sync from HW.
     * From CSubsystemObject
     *
     * @param[out] error: if return code is false, it contains the description
     *                     of the error, empty string otherwise.
     *
     * @return true if success, false otherwise.
     */
    virtual bool receiveFromHW(string &error);

    /**
     * Sync to HW.
     * From CSubsystemObject
     *
     * @param[out] error: if return code is false, it contains the description
     *                     of the error, empty string otherwise.
     *
     * @return true if success, false otherwise.
     */
    virtual bool sendToHW(string &error);
private:
    /**
     * Parse a concatenated list of channel policy separated by a coma.
     *
     * @param[in] channelPolicy string of concatenated channel policy.
     *
     * @return vector of channel policy.
     */
    std::vector<android_audio_legacy::SampleSpec::ChannelsPolicy>
    parseChannelPolicyString(const std::string &channelPolicy);

    const RouteSubsystem *_routeSubsystem; /**< Route subsytem plugin. */
    IRouteInterface *_routeInterface; /**< Route Interface to communicate with Route Mgr. */

    static const Config _defaultConfig; /**< default route stream configuration at construction. */
    Config _config; /**< stream route configuration. */

    string _routeName; /**< stream route name. */
    uint32_t _routeId;   /**< stream route Id. */
    string _cardName; /**< card name used by the stream route. */
    int32_t _device; /**< audio device used by the stream route. */
    bool _isOut; /**< direction qualifier of the stream route. */
    bool _isStreamRoute; /**< qualifier of the stream route. */

    static const std::string OUTPUT_DIRECTION; /**< string key to identify output routes. */
    static const std::string STREAM_TYPE; /**< key to identify stream route. */
    static const uint32_t SINGLE_PORT = 1;  /**< only one port is mentionned for this route. */
    static const uint32_t DUAL_PORTS = 2; /**< both port are mentionnent for this route. */
    static const std::string PORT_DELIMITER; /**< Delimiter to parse a list of ports. */
    static const std::string STRING_DELIMITER; /**< Delimiter to parse strings. */
    static const std::string CHANNEL_POLICY_COPY; /**< copy channel policy tag. */
    static const std::string CHANNEL_POLICY_IGNORE; /**< ignore channel policy tag. */
    static const std::string CHANNEL_POLICY_AVERAGE; /**< average channel policy tag. */
};
