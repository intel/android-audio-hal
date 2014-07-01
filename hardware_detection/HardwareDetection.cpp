/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
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
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#define LOG_TAG "AudioHardwareDetection"

#include "HardwareDetection.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <log/log.h>

using namespace std;

namespace HardwareDetection
{
const char *ConfigurationLocator::mCardsPath = "/proc/asound/cards";
const string ConfigurationLocator::mUnknownCardName = "";

ConfigurationLocator::ConfigurationLocator()
{
    mConfigurationFilePaths["bytcrrt5642t100"] = make_pair(
        "/etc/parameter-framework/ParameterFrameworkConfiguration-bytcr-rt5642-t100.xml",
        "/etc/parameter-framework/ParameterFrameworkConfigurationRoute-bytcr-rt5642-t100.xml");

    mConfigurationFilePaths["bytcrrt5640"] = make_pair(
        "/etc/parameter-framework/ParameterFrameworkConfiguration-bytcr-rt5640-default.xml",
        "/etc/parameter-framework/ParameterFrameworkConfigurationRoute-bytcr-rt5640-default.xml");

    mConfigurationFilePaths["bytcrrt5651"] = make_pair(
        "/etc/parameter-framework/ParameterFrameworkConfiguration-bytcr-rt5651-default.xml",
        "/etc/parameter-framework/ParameterFrameworkConfigurationRoute-bytcr-rt5651-default.xml");

    mSupportedCard = findSupportedCard();
}

string ConfigurationLocator::getAudioConfigurationFile()
{
    return mConfigurationFilePaths[mSupportedCard].first;
}

string ConfigurationLocator::getRouteConfigurationFile()
{
    return mConfigurationFilePaths[mSupportedCard].second;
}

string ConfigurationLocator::findSupportedCard()
{
    string availableCards = readCardsFromFile();

    for (NameToConfigurationsMap::iterator configuration =
             mConfigurationFilePaths.begin();
         configuration != mConfigurationFilePaths.end(); ++configuration) {

        string supportedCardName = configuration->first;
        ALOGD("Supported card: %s", supportedCardName.c_str());

        if (isSupportedCardAvailable(supportedCardName, availableCards)) {
            return supportedCardName;
        }
    }

    ALOGE("Error audio card is not supported %s", availableCards.c_str());
    return mUnknownCardName;
}

bool ConfigurationLocator::isSupportedCardAvailable(string supportedCard, string availableCards)
{
    return availableCards.find(supportedCard) != string::npos;
}

string ConfigurationLocator::readCardsFromFile()
{
    ifstream fileStream(mCardsPath);

    if (!fileStream.is_open()) {
        ALOGE("Error opening file %s", mCardsPath);
        return mUnknownCardName;
    }

    stringstream fileContent;
    fileContent << fileStream.rdbuf();

    if (fileStream.fail()) {
        ALOGE("Error reading file %s", mCardsPath);
        return mUnknownCardName;
    }

    return fileContent.str();
}

}
