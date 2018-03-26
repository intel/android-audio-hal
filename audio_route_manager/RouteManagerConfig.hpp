/*
 * Copyright (C) 2017-2018 Intel Corporation
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

#include "AudioPort.hpp"
#include "StreamRouteCollection.hpp"
#include <Criterion.hpp>
#include <CriterionType.hpp>
#include <Parameter.hpp>
#include <CriterionParameter.hpp>
#include <RogueParameter.hpp>

namespace intel_audio
{

class RouteManagerConfig
{
public:
    RouteManagerConfig(StreamRouteCollection &routes,
                       Criteria &criteria,
                       CriterionTypes &criterionTypes,
                       Parameters &parameters,
                       CParameterMgrPlatformConnector *connector)
        : mRoutes(routes),
          mCriteria(criteria),
          mCriterionTypes(criterionTypes),
          mParameters(parameters),
          mConnector(connector)
    {}

    void addParameter(CriterionParameter *cp)
    {
        mParameters.push_back(cp);
    }

    CriterionType *getCriterionType(const std::string &criterionTypeName)
    {
        return mCriterionTypes.getByName(criterionTypeName);
    }

    Criterion *getCriterion(const std::string &criterionName)
    {
        return mCriteria.getByName(criterionName);
    }

    CParameterMgrPlatformConnector *getConnector() { return mConnector; }

    AudioPorts mMixPorts;
    AudioPorts mDevicePorts;
    StreamRouteCollection &mRoutes;
    Criteria &mCriteria;
    CriterionTypes &mCriterionTypes;
    Parameters &mParameters;
    CParameterMgrPlatformConnector *mConnector; /**< Parameter-Manager connector. */
};

}  // namespace intel_audio
