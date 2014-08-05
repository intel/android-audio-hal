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

#include "FormattedSubsystemObject.h"
#include "InstanceConfigurableElement.h"
#include "MappingContext.h"
#include <RouteInterface.hpp>

class RouteSubsystem;

class AudioPort : public CFormattedSubsystemObject
{

public:
    AudioPort(const string &mappingValue,
              CInstanceConfigurableElement *pInstanceConfigurableElement,
              const CMappingContext &context);

protected:
    /**
     * Sync from HW.
     * From CSubsystemObject
     *
     * @param[out] error: if return code is false, it contains the description
     *                     of the error, empty string otherwise.
     *
     * @return true if success, false otherwise.
     */
    virtual bool receiveFromHW(string &error);

    /**
     * Sync to HW.
     * From CSubsystemObject
     *
     * @param[out] error: if return code is false, it contains the description
     *                     of the error, empty string otherwise.
     *
     * @return true if success, false otherwise.
     */
    virtual bool sendToHW(string &error);

private:
    string mName; /**< Name of an audio port. */
    uint32_t mId; /**< Identifier of an audio port. */
    bool mIsBlocked; /**< Blocked attribute of the port, ie, port must not be used. */

    const RouteSubsystem *mRouteSubsystem; /**< Route subsytem plugin. */
    intel_audio::IRouteInterface *mRouteInterface; /**< Interface to communicate with Route Mgr. */

    static const std::string mDelimiter; /**< Delimiter to parse a concatenated list of ports. */
};
