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
#include "AudioStreamRoute.h"
#include "Tokenizer.h"
#include "RouteMappingKeys.h"
#include "RouteSubsystem.h"
#include <AudioCommsAssert.hpp>

using std::memcmp;

const string AudioStreamRoute::OUTPUT_DIRECTION = "out";
const string AudioStreamRoute::STREAM_TYPE = "streamRoute";
const string AudioStreamRoute::PORT_DELIMITER = "-";
const string AudioStreamRoute::STRING_DELIMITER = ",";
const string AudioStreamRoute::CHANNEL_POLICY_COPY = "copy";
const string AudioStreamRoute::CHANNEL_POLICY_IGNORE = "ignore";
const string AudioStreamRoute::CHANNEL_POLICY_AVERAGE = "average";

const AudioStreamRoute::Config AudioStreamRoute::_defaultConfig = {
    requirePreEnable :      false,
    requirePostDisable :    false,
    silencePrologInMs :     0,
    channel :               0,
    channelsPolicy :        {},
    rate :                  0,
    format :                0,
    periodSize :            0,
    periodCount :           0,
    startThreshold :        0,
    stopThreshold :         0,
    silenceThreshold :      0,
    availMin :              0,
    applicabilityMask :     0,
    effectSupported :       {}
};

AudioStreamRoute::AudioStreamRoute(const string &mappingValue,
                                   CInstanceConfigurableElement *instanceConfigurableElement,
                                   const CMappingContext &context)
    : CFormattedSubsystemObject(instanceConfigurableElement,
                                mappingValue,
                                MappingKeyAmend1,
                                (MappingKeyAmendEnd - MappingKeyAmend1 + 1),
                                context),
      _routeSubsystem(static_cast<const RouteSubsystem *>(
                          instanceConfigurableElement->getBelongingSubsystem())),
      _routeInterface(_routeSubsystem->getRouteInterface()),
      _config(_defaultConfig),
      _routeId(context.getItemAsInteger(MappingKeyId)),
      _cardName(context.getItem(MappingKeyCard)),
      _device(context.getItemAsInteger(MappingKeyDevice)),
      _isOut(context.getItem(MappingKeyDirection) == OUTPUT_DIRECTION),
      _isStreamRoute(context.getItem(MappingKeyType) == STREAM_TYPE)
{
    _routeName = getFormattedMappingValue();

    StreamRouteConfig config;
    config.cardName = _cardName.c_str();
    config.deviceId = _device;
    config.requirePreEnable = _config.requirePreEnable;
    config.requirePostDisable = _config.requirePostDisable;
    config.channels = _config.channel;
    config.rate = _config.rate;
    config.periodSize = _config.periodSize;
    config.periodCount = _config.periodCount;
    config.format = static_cast<audio_format_t>(_config.format);
    config.startThreshold = _config.startThreshold;
    config.stopThreshold = _config.stopThreshold;
    config.silenceThreshold = _config.silenceThreshold;
    config.silencePrologInMs = _config.silencePrologInMs;
    config.applicabilityMask = _config.applicabilityMask;

    string ports = context.getItem(MappingKeyPorts);
    Tokenizer mappingTok(ports, PORT_DELIMITER);
    vector<string> subStrings = mappingTok.split();
    AUDIOCOMMS_ASSERT(subStrings.size() <= DUAL_PORTS,
                      "Route cannot be connected to more than 2 ports");

    string portSrc = subStrings.size() >= SINGLE_PORT ? subStrings[0] : string();
    string portDst = subStrings.size() == DUAL_PORTS ? subStrings[1] : string();

    // Declares the stream route
    _routeInterface->addAudioStreamRoute(_routeName,
                                         1 << _routeId,
                                         portSrc, portDst,
                                         _isOut);

    // Add the stream route configuration
    _routeInterface->updateStreamRouteConfig(_routeName, config);
}

std::vector<android_audio_legacy::SampleSpec::ChannelsPolicy>
AudioStreamRoute::parseChannelPolicyString(const std::string &channelPolicy)
{
    std::vector<android_audio_legacy::SampleSpec::ChannelsPolicy> channelPolicyVector;
    Tokenizer mappingTok(channelPolicy, STRING_DELIMITER);
    vector<string> subStrings = mappingTok.split();

    for (size_t i = 0; i < subStrings.size(); i++) {

        android_audio_legacy::SampleSpec::ChannelsPolicy policy;
        if (subStrings[i] == CHANNEL_POLICY_COPY) {

            policy = android_audio_legacy::SampleSpec::Copy;
        } else if (subStrings[i] == CHANNEL_POLICY_AVERAGE) {

            policy = android_audio_legacy::SampleSpec::Average;
        } else if (subStrings[i] == CHANNEL_POLICY_IGNORE) {

            policy = android_audio_legacy::SampleSpec::Ignore;
        } else {

            // Not valid channel policy
            continue;
        }

        channelPolicyVector.push_back(policy);
    }
    return channelPolicyVector;
}

bool AudioStreamRoute::receiveFromHW(string &error)
{
    blackboardWrite(&_config, sizeof(_config));

    return true;
}

bool AudioStreamRoute::sendToHW(string &error)
{
    Config config;
    blackboardRead(&config, sizeof(config));

    // Update configuration of Route Mgr
    StreamRouteConfig streamConfig;
    streamConfig.requirePreEnable = config.requirePreEnable;
    streamConfig.requirePostDisable = config.requirePostDisable;
    streamConfig.cardName = _cardName.c_str();
    streamConfig.deviceId = _device;
    streamConfig.channels = config.channel;
    streamConfig.rate = config.rate;
    streamConfig.periodSize = config.periodSize;
    streamConfig.periodCount = config.periodCount;
    streamConfig.format = static_cast<audio_format_t>(config.format);
    streamConfig.startThreshold = config.startThreshold;
    streamConfig.stopThreshold = config.stopThreshold;
    streamConfig.silenceThreshold = config.silenceThreshold;
    streamConfig.silencePrologInMs = config.silencePrologInMs;
    streamConfig.applicabilityMask = config.applicabilityMask;

    streamConfig.channelsPolicy.erase(streamConfig.channelsPolicy.begin(),
                                      streamConfig.channelsPolicy.end());
    streamConfig.channelsPolicy = parseChannelPolicyString(string(config.channelsPolicy));

    Tokenizer effectTok(string(config.effectSupported), STRING_DELIMITER);
    vector<string> subStrings = effectTok.split();
    for (uint32_t i = 0; i < subStrings.size(); i++) {

        _routeInterface->addRouteSupportedEffect(_routeName, subStrings[i]);
    }
    _routeInterface->updateStreamRouteConfig(_routeName, streamConfig);

    return true;
}
