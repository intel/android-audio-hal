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


#include "SubsystemObject.h"
#include "InstanceConfigurableElement.h"
#include "MappingContext.h"
#include <RouteInterface.hpp>

class RouteSubsystem;

class Criterion : public CSubsystemObject
{

public:
    Criterion(const string &mappingValue,
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
     * @param[out] error: if return code is false, it contains the description
     *                     of the error, empty string otherwise.
     *
     * @return true if success, false otherwise.
     */
    virtual bool sendToHW(string &error);
private:
    /**
     * Returns the index of an element.
     * According to the type of the element, index has different meaning.
     * For a value-pair, it is simply the numerical part of the pair.
     * For a bit parameter, it is the mask of the bit position.
     * For an element on which it has no meaning, it returns 0 and prints a warning log.
     *
     * @param[in] element: element from which needs to get the index
     *
     * @return index of the element
     */
    uint32_t getIndex(const CElement *element) const;

    const RouteSubsystem *_routeSubsystem; /**< Route subsytem plugin. */
    IRouteInterface *_routeInterface; /**< Route Interface to communicate with Route Mgr. */

    static const uint32_t DEFAULT_VALUE = 0; /**< default numerical value of the criterion. */
    string _criterionName; /**< Name of the criterion. */
    string _criterionType; /**< Type name of the criterion. */
    uint32_t _value; /**< numerical value of the criterion. */

    static const std::string VALUE_PAIR_CRITERION_TYPE; /**< Value pair criterion type name. */
    static const std::string BIT_PARAM_CRITERION_TYPE; /**< Bit Parameter criterion type name. */

};
