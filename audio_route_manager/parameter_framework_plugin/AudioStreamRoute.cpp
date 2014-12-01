/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (c) 2013-2014 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors.
 *
 * Title to the Material remains with Intel Corporation or its suppliers and
 * licensors. The Material contains trade secrets and proprietary and
 * confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and treaty
 * provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or disclosed
 * in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

#include "AudioStreamRoute.hpp"
#include "Tokenizer.h"
#include "RouteMappingKeys.hpp"
#include "RouteSubsystem.hpp"
#include <AudioCommsAssert.hpp>

using std::memcmp;
using std::string;
using namespace intel_audio;

const string AudioStreamRoute::mOutputDirection = "out";
const string AudioStreamRoute::mStreamType = "streamRoute";
const string AudioStreamRoute::mPortDelimiter = "-";
const string AudioStreamRoute::mStringDelimiter = ",";
const string AudioStreamRoute::mChannelPolicyCopy = "copy";
const string AudioStreamRoute::mChannelPolicyIgnore = "ignore";
const string AudioStreamRoute::mChannelPolicyAverage = "average";

const AudioStreamRoute::Config AudioStreamRoute::mDefaultConfig = {
    applicabilityMask :     0,
    effectSupported :       {},
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
    availMin :              0
};

AudioStreamRoute::AudioStreamRoute(const string &mappingValue,
                                   CInstanceConfigurableElement *instanceConfigurableElement,
                                   const CMappingContext &context)
    : CFormattedSubsystemObject(instanceConfigurableElement,
                                mappingValue,
                                MappingKeyAmend1,
                                (MappingKeyAmendEnd - MappingKeyAmend1 + 1),
                                context),
      mRouteSubsystem(static_cast<const RouteSubsystem *>(
                          instanceConfigurableElement->getBelongingSubsystem())),
      mRouteInterface(mRouteSubsystem->getRouteInterface()),
      _config(mDefaultConfig),
      mCardName(context.getItem(MappingKeyCard)),
      mDevice(context.getItemAsInteger(MappingKeyDevice)),
      mIsOut(context.getItem(MappingKeyDirection) == mOutputDirection),
      mIsStreamRoute(context.getItem(MappingKeyType) == mStreamType)
{
    mRouteName = getFormattedMappingValue();

    StreamRouteConfig config;
    config.cardName = mCardName.c_str();
    config.deviceId = mDevice;
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
    Tokenizer mappingTok(ports, mPortDelimiter);
    std::vector<string> subStrings = mappingTok.split();
    AUDIOCOMMS_ASSERT(subStrings.size() <= mDualPorts,
                      "Route cannot be connected to more than 2 ports");

    string portSrc = subStrings.size() >= mSinglePort ? subStrings[0] : string();
    string portDst = subStrings.size() == mDualPorts ? subStrings[1] : string();

    // Declares the stream route
    mRouteInterface->addAudioStreamRoute(context.getItem(MappingKeyAmend1),
                                         portSrc, portDst,
                                         mIsOut);

    // Add the stream route configuration
    mRouteInterface->updateStreamRouteConfig(mRouteName, config);
}

std::vector<SampleSpec::ChannelsPolicy>
AudioStreamRoute::parseChannelPolicyString(const std::string &channelPolicy)
{
    std::vector<SampleSpec::ChannelsPolicy> channelPolicyVector;
    Tokenizer mappingTok(channelPolicy, mStringDelimiter);
    std::vector<string> subStrings = mappingTok.split();

    for (size_t i = 0; i < subStrings.size(); i++) {

        SampleSpec::ChannelsPolicy policy;
        if (subStrings[i] == mChannelPolicyCopy) {

            policy = SampleSpec::Copy;
        } else if (subStrings[i] == mChannelPolicyAverage) {

            policy = SampleSpec::Average;
        } else if (subStrings[i] == mChannelPolicyIgnore) {

            policy = SampleSpec::Ignore;
        } else {

            // Not valid channel policy
            continue;
        }

        channelPolicyVector.push_back(policy);
    }
    return channelPolicyVector;
}

bool AudioStreamRoute::receiveFromHW(string & /*error*/)
{
    blackboardWrite(&_config, sizeof(_config));

    return true;
}

bool AudioStreamRoute::sendToHW(string & /*error*/)
{
    Config config;
    blackboardRead(&config, sizeof(config));

    // Update configuration of Route Mgr
    StreamRouteConfig streamConfig;
    streamConfig.requirePreEnable = config.requirePreEnable;
    streamConfig.requirePostDisable = config.requirePostDisable;
    streamConfig.cardName = mCardName.c_str();
    streamConfig.deviceId = mDevice;
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

    Tokenizer effectTok(string(config.effectSupported), mStringDelimiter);
    std::vector<string> subStrings = effectTok.split();
    for (uint32_t i = 0; i < subStrings.size(); i++) {

        mRouteInterface->addRouteSupportedEffect(mRouteName, subStrings[i]);
    }
    mRouteInterface->updateStreamRouteConfig(mRouteName, streamConfig);

    return true;
}
