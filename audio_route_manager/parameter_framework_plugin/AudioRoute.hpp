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
               const CMappingContext &context,
               core::log::Logger &logger);

protected:
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
