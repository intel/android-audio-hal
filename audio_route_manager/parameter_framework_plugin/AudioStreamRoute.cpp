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

#include "AudioStreamRoute.hpp"
#include "Tokenizer.h"
#include "RouteMappingKeys.hpp"
#include "RouteSubsystem.hpp"
#include <AudioCommsAssert.hpp>
#include <cstring>

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

AudioStreamRoute::AudioStreamRoute(const std::string &mappingValue,
                                   CInstanceConfigurableElement *instanceConfigurableElement,
                                   const CMappingContext &context,
                                   core::log::Logger &logger)
    : CFormattedSubsystemObject(instanceConfigurableElement,
                                logger,
                                mappingValue,
                                MappingKeyAmend1,
                                (MappingKeyAmendEnd - MappingKeyAmend1 + 1),
                                context),
      mRouteSubsystem(static_cast<const RouteSubsystem *>(
                          instanceConfigurableElement->getBelongingSubsystem())),
      mRouteInterface(mRouteSubsystem->getRouteInterface()),
      mCardName(context.getItem(MappingKeyCard)),
      mDevice(context.getItemAsInteger(MappingKeyDevice)),
      mDeviceAddress(""),
      mIsOut(context.getItem(MappingKeyDirection) == mOutputDirection),
      mIsStreamRoute(context.getItem(MappingKeyType) == mStreamType)
{
    mRouteName = getFormattedMappingValue();
    if (context.iSet(MappingKeyDeviceAddress)) {
        mDeviceAddress = context.getItem(MappingKeyDeviceAddress);
    }

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
    streamConfig.deviceAddress = mDeviceAddress;
    streamConfig.channels = config.channel;
    streamConfig.rate = config.rate;
    streamConfig.periodSize = config.periodSize;
    streamConfig.periodCount = config.periodCount;
    streamConfig.format = static_cast<audio_format_t>(config.format);
    streamConfig.startThreshold = config.startThreshold;
    streamConfig.stopThreshold = config.stopThreshold;
    streamConfig.silenceThreshold = config.silenceThreshold;
    streamConfig.silencePrologInMs = config.silencePrologInMs;
    streamConfig.flagMask = config.flagMask;
    streamConfig.useCaseMask = config.useCaseMask;
    streamConfig.supportedDeviceMask = config.supportedDeviceMask;
    streamConfig.dynamicChannelMapsControl = config.dynamicChannelMapsControl;
    streamConfig.dynamicFormatsControl = config.dynamicFormatsControl;
    streamConfig.dynamicRatesControl = config.dynamicRatesControl;
    streamConfig.availMin = config.availMin;

    streamConfig.channelsPolicy.erase(streamConfig.channelsPolicy.begin(),
                                      streamConfig.channelsPolicy.end());
    streamConfig.channelsPolicy = parseChannelPolicyString(std::string(config.channelsPolicy));

    Tokenizer effectTok(string(config.effectSupported), mStringDelimiter);
    std::vector<string> subStrings = effectTok.split();
    for (uint32_t i = 0; i < subStrings.size(); i++) {

        mRouteInterface->addRouteSupportedEffect(mRouteName, subStrings[i]);
    }
    mRouteInterface->updateStreamRouteConfig(mRouteName, streamConfig);

    return true;
}
