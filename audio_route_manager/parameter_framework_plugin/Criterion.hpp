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


#include "SubsystemObject.h"
#include "InstanceConfigurableElement.h"
#include "MappingContext.h"
#include <RouteInterface.hpp>

class RouteSubsystem;

class Criterion : public CSubsystemObject
{

public:
    Criterion(const std::string &mappingValue,
              CInstanceConfigurableElement *instanceConfigurableElement,
              const CMappingContext &context, core::log::Logger &logger);

protected:
    /**
     * Sync to HW.
     * From CSubsystemObject
     *
     * @param[out] error: if return code is false, it contains the description
     *                     of the error, empty std::string otherwise.
     *
     * @return true if success, false otherwise.
     */
    virtual bool sendToHW(std::string &error);

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

    const RouteSubsystem *mRouteSubsystem; /**< Route subsytem plugin. */
    intel_audio::IRouteInterface *mRouteInterface; /**< Interface to communicate with Route Mgr. */

    static const uint32_t mDefaultValue = 0; /**< default numerical value of the criterion. */
    std::string mCriterionName; /**< Name of the criterion. */
    std::string mCriterionType; /**< Type name of the criterion. */
    uint32_t mValue; /**< numerical value of the criterion. */

    static const std::string mValuePairCriterionType; /**< Value pair criterion type name. */
    static const std::string mBitParamCriterionType; /**< Bit Parameter criterion type name. */

};
