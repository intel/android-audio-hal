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

#include "Subsystem.h"
#include <AudioNonCopyable.hpp>

namespace intel_audio
{
struct IRouteInterface;
}

class RouteSubsystem : public CSubsystem, private audio_comms::utilities::NonCopyable
{
public:
    RouteSubsystem(const std::string &strName, core::log::Logger &logger);

    /**
     * Retrieve Route Manager interface.
     *
     * @return RouteManager interface for the route plugin.
     */
    intel_audio::IRouteInterface *getRouteInterface() const;

private:
    intel_audio::IRouteInterface *mRouteInterface; /**< Route Interface. */

    static const char *const mKeyName; /**< name key mapping string. */
    static const char *const mKeyId; /**< id key mapping string. */
    static const char *const mKeyDirection; /**< direction key mapping string. */
    static const char *const mKeyType; /**< type key mapping string. */
    static const char *const mKeyCard; /**< card key mapping string. */
    static const char *const mKeyDevice; /**< device key mapping string. */
    static const char *const mKeyDeviceAddress; /**< device address key mapping string. */
    static const char *const mKeyPort; /**< port key mapping string. */
    static const char *const mKeyGroups; /**< groups key mapping string. */
    static const char *const mKeyInclusive; /**< inclusive key mapping string. */
    static const char *const mKeyAmend1; /**< amend1 key mapping string. */
    static const char *const mKeyAmend2; /**< amend2 key mapping string. */
    static const char *const mKeyAmend3; /**< amend3 key mapping string. */

    static const char *const mPortComponentName; /**< name of port component. */
    static const char *const mRouteComponentName; /**< name of route component. */
    static const char *const mStreamRouteComponentName; /**< name of stream route component. */
    static const char *const mCriterionComponentName; /**< name of criterion component. */
};
