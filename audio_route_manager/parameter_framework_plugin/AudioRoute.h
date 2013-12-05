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
#pragma once


#include "FormattedSubsystemObject.h"
#include "InstanceConfigurableElement.h"
#include "MappingContext.h"
#include <RouteInterface.h>

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
        uint8_t forcedRoutingStage; /**< allow to force a routing stage. */
    } __attribute__((packed));
public:
    AudioRoute(const string &mappingValue,
               CInstanceConfigurableElement *instanceConfigurableElement,
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
     * @param[out] error: if return code is false, it contains it contains the description
     *                     of the error, empty string otherwise.
     *
     * @return true if success, false otherwise.
     */
    virtual bool sendToHW(string &error);
private:
    /**
     * Converts stage (coming from EnumParameter) to Route Manager routing stage
     *
     * @param[in] stage routing stage read from XML
     *
     * @return routing stage in Route Manager format
     */
    RoutingStage toRoutingStage(uint8_t stage);

    const RouteSubsystem *_routeSubsystem; /**< Route subsytem plugin. */
    IRouteInterface *_routeInterface; /**< Route Interface to communicate with Route Mgr. */

    static const Status DEFAULT_STATUS; /**< default status at object creation. */

    Status _status; /**< status of a route. */

    string _routeName; /**< Name of the audio route. */
    uint32_t _routeId; /**< Identifier of the audio route. */
    bool _isStreamRoute; /**< qualifier of the audio route. */
    bool _isOut; /**< direction qualifier of the audio route. */

    static const std::string ROUTE_CRITERION_TYPE; /**< Route criterion type name. */
    static const std::string OUTPUT_DIRECTION; /**< string key to identify output routes. */
    static const std::string STREAM_TYPE; /**< key to identify stream route. */
    static const uint32_t SINGLE_PORT = 1;  /**< only one port is mentionned for this route. */
    static const uint32_t DUAL_PORTS = 2; /**< both port are mentionnent for this route. */
    static const std::string PORT_DELIMITER; /**< Delimiter to parse a list of ports. */

};
