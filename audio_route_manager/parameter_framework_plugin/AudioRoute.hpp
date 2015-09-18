/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (c) 2013-2015 Intel Corporation All Rights Reserved.
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

#pragma once


#include "FormattedSubsystemObject.h"
#include "InstanceConfigurableElement.h"
#include "MappingContext.h"
#include <RouteInterface.hpp>

class RouteSubsystem;

class AudioRoute : public CFormattedSubsystemObject
{
private:
    /**
     * Mapped control structure - must be packed as required by PFW.
     */
    struct Status
    {
        bool isApplicable; /**< applicable attribute of a route, ie route can be enabled. */
        bool needReconfigure; /**< route needs to be reconfigured in a glitch free way. */
        bool needReroute; /**< route needs to be closed/reopened. */
    } __attribute__((packed));

public:
    AudioRoute(const std::string &mappingValue,
               CInstanceConfigurableElement *instanceConfigurableElement,
               const CMappingContext &context);

protected:
    /**
     * Sync from HW.
     * From CSubsystemObject
     *
     * @param[out] error: if return code is false, it contains the description
     *                     of the error, empty std::string otherwise.
     *
     * @return true if success, false otherwise.
     */
    virtual bool receiveFromHW(std::string &error);

    /**
     * Sync to HW.
     * From CSubsystemObject
     *
     * @param[out] error: if return code is false, it contains it contains the description
     *                     of the error, empty std::string otherwise.
     *
     * @return true if success, false otherwise.
     */
    virtual bool sendToHW(std::string &error);

private:
    const RouteSubsystem *mRouteSubsystem; /**< Route subsytem plugin. */
    intel_audio::IRouteInterface *mRouteInterface; /**< Interface to communicate with Route Mgr. */

    static const Status mDefaultStatus; /**< default status at object creation. */

    Status mStatus; /**< status of a route. */

    std::string mRouteName; /**< Name of the audio route. */
    bool mIsStreamRoute; /**< qualifier of the audio route. */
    bool mIsOut; /**< direction qualifier of the audio route. */
    static const std::string mOutputDirection; /**< string key to identify output routes. */
    static const std::string mStreamType; /**< key to identify stream route. */
    static const uint32_t mSinglePort = 1;  /**< only one port is mentionned for this route. */
    static const uint32_t mDualPorts = 2; /**< both port are mentionnent for this route. */
    static const std::string mPortDelimiter; /**< Delimiter to parse a list of ports. */

};
