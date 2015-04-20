/*
 * Copyright (C) 2014-2015 Intel Corporation
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

#include "Parameter.hpp"

class CriterionType;
class Criterion;
class CParameterMgrPlatformConnector;

namespace intel_audio
{

/**
 * This class intends to address the Android Parameters that are associated to a criterion.
 * Each time the key of these android parameters is detected, this class will wrap the accessor
 * of the android parameter to accessors of the PFW criterion.
 */
class CriterionParameter : public Parameter
{
public:

    CriterionParameter(const std::string &key,
                       const std::string &name,
                       CriterionType *criterionType,
                       CParameterMgrPlatformConnector *connector,
                       const std::string &defaultValue);

    virtual Type getType() const { return Parameter::CriterionParameter; }

    virtual ~CriterionParameter();

    virtual bool setValue(const std::string &value);

    virtual bool getValue(std::string &value) const;

    /**
     * Sync the criterion parameter, i.e. apply the default value set at construction
     * time.
     *
     * @return true if synchronised correctly, false otherwise.
     */
    virtual bool sync();

    Criterion *getCriterion() { return mCriterion; }

private:
    Criterion *mCriterion; /**< Associated Route PFW criterion to the android parameter. */
};

} // namespace intel_audio
