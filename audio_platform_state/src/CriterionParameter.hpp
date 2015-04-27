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

class IStreamInterface;

/**
 * This class intends to address the Android Parameters that are associated to a criterion.
 * Each time the key of these android parameters is detected, this class will wrap the accessor
 * of the android parameter to accessors of the PFW criterion.
 */
class CriterionParameter : public Parameter
{
public:

    CriterionParameter(ParameterChangedObserver *observer,
                       const std::string &key,
                       const std::string &name,
                       const std::string &defaultValue)
        : Parameter(observer, key, name, defaultValue)
    {}

    virtual bool set(const std::string &androidParamValue);
};

/**
 * This class intends to address the Android Parameters that are associated to a criterion
 * of the Route PFW Instance
 * Each time the key of these android parameters is detected, this class will wrap the accessor
 * of the android parameter to accessors of the Route PFW criterion.
 */
class RouteCriterionParameter : public CriterionParameter
{
public:
    RouteCriterionParameter(ParameterChangedObserver *observer,
                            const std::string &key,
                            const std::string &name,
                            CriterionType *criterionType,
                            CParameterMgrPlatformConnector *connector,
                            const std::string &defaultValue = "");

    virtual ~RouteCriterionParameter();

    virtual bool setValue(const std::string &value);

    virtual bool getValue(std::string &value) const;

    /**
     * No need to sync route criterion parameter, as the default value is set at construction
     * time.
     *
     * @return always true.
     */
    virtual bool sync() { return true; }

    Criterion *getCriterion() { return mCriterion; }

private:
    Criterion *mCriterion; /**< Associated Route PFW criterion to the android parameter. */
};

/**
 * This class intends to address the Android Parameters that are associated to a criterion
 * of the Audio PFW Instance
 * Each time the key of this android parameters is detected, this class will wrap the accessor
 * of the android parameter to accessors of the Audio PFW criterion.
 */
class AudioCriterionParameter : public CriterionParameter
{
public:
    AudioCriterionParameter(ParameterChangedObserver *observer,
                            const std::string &key,
                            const std::string &name,
                            const std::string &typeName,
                            IStreamInterface *streamInterface,
                            const std::string &defaultValue = "");

    virtual bool setValue(const std::string &value);

    virtual bool getValue(std::string &value) const;

    virtual bool sync();

private:
    IStreamInterface *mStreamInterface; /**< Handle on stream interface of Route Manager. */
};

} // namespace intel_audio
