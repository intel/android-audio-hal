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
#pragma once

#include <string>
#include <map>
#include <utility> // for std::pair

using namespace std;

namespace HardwareDetection
{
class ConfigurationLocator
{
public:
    ConfigurationLocator();
    string getAudioConfigurationFile();
    string getRouteConfigurationFile();

private:
    string findSupportedCard();
    string readCardsFromFile();
    bool isSupportedCardAvailable(string supportedCard, string availableCards);

    typedef map<const string, pair<string, string> > NameToConfigurationsMap;

    // As a convention, first is AudioFilePath, second is RouteFilePath
    NameToConfigurationsMap mConfigurationFilePaths;
    string mSupportedCard;
    static const string mUnknownCardName;
    static const char *mCardsPath;
};
}
