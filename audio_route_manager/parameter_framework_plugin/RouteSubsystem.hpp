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

#pragma once

#include "Subsystem.h"
#include <NonCopyable.hpp>

namespace intel_audio
{
class IRouteInterface;
}

class RouteSubsystem : public CSubsystem, private audio_comms::utilities::NonCopyable
{
public:
    RouteSubsystem(const string &strName);

    /**
     * Retrieve Route Manager interface.
     *
     * @return RouteManager interface for the route plugin.
     */
    intel_audio::IRouteInterface *getRouteInterface() const;

private:
    intel_audio::IRouteInterface *_routeInterface; /**< Route Interface. */

    static const char *const ROUTE_LIB_PROP_NAME; /**< Route Manager name property. */
    static const char *const ROUTE_LIBRARY_NAME; /**< Route Manager library name. */

    static const char *const KEY_NAME; /**< name key mapping string. */
    static const char *const KEY_ID; /**< id key mapping string. */
    static const char *const KEY_DIRECTION; /**< direction key mapping string. */
    static const char *const KEY_TYPE; /**< type key mapping string. */
    static const char *const KEY_CARD; /**< card key mapping string. */
    static const char *const KEY_DEVICE; /**< device key mapping string. */
    static const char *const KEY_PORT; /**< port key mapping string. */
    static const char *const KEY_GROUPS; /**< groups key mapping string. */
    static const char *const KEY_INCLUSIVE; /**< inclusive key mapping string. */
    static const char *const KEY_AMEND_1; /**< amend1 key mapping string. */
    static const char *const KEY_AMEND_2; /**< amend2 key mapping string. */
    static const char *const KEY_AMEND_3; /**< amend3 key mapping string. */

    static const char *const PORT_COMPONENT_NAME; /**< name of port component. */
    static const char *const ROUTE_COMPONENT_NAME; /**< name of route component. */
    static const char *const STREAM_ROUTE_COMPONENT_NAME; /**< name of stream route component. */
    static const char *const CRITERION_COMPONENT_NAME; /**< name of criterion component. */
};
