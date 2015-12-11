/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (c) 2013-2016 Intel Corporation All Rights Reserved.
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

#include "Subsystem.h"
#include <NonCopyable.hpp>

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
