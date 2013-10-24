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
#include "RouteManagerInstance.h"

using namespace std;
using namespace NInterfaceProvider;

RouteManagerInstance::RouteManagerInstance()
    : _audioRouteManager(new AudioRouteManager())
{
    _interfaceProvider.addImplementedInterfaces(*_audioRouteManager);
}

RouteManagerInstance *RouteManagerInstance::getInstance()
{
    static RouteManagerInstance instance;
    return &instance;
}

RouteManagerInstance::~RouteManagerInstance()
{
    delete _audioRouteManager;
}

// Get AudioRouteManager instance
AudioRouteManager *RouteManagerInstance::getAudioRouteManager() const
{
    return _audioRouteManager;
}

// Interface query
IInterface *RouteManagerInstance::queryInterface(const string &strInterfaceName) const
{
    return _interfaceProvider.queryInterface(strInterfaceName);
}

// Interface list
string RouteManagerInstance::getInterfaceList() const
{
    return _interfaceProvider.getInterfaceList();
}
