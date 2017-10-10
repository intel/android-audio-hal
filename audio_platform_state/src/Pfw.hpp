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

#include <AudioNonCopyable.hpp>
#include <Parameter.hpp>
#include <Criterion.hpp>
#include <CriterionType.hpp>
#include <cutils/config_utils.h>
#include <utils/Errors.h>
#include <inttypes.h>

class CParameterMgrPlatformConnector;
struct cnode;
class ParameterMgrHelper;
class CParameterHandle;

namespace intel_audio
{

enum pfwtype
{
    Audio
};

template <pfwtype pfw>
class PfwTrait;

class ParameterMgrPlatformConnectorLogger;

template <class Trait>
class Pfw : private audio_comms::utilities::NonCopyable
{
public:
    Pfw();

    virtual ~Pfw();

    android::status_t start();

    void setConfig(Criteria criteria, CriterionTypes criterionTypes)
    {
        mCriteria = criteria;
        mCriterionTypes = criterionTypes;
    }

    /**
     * Add a criterion type value pair to PFW.
     *
     * @param[in] typeName criterion type name to which this value pair is added to.
     * @param[in] numeric part of the value pair.
     * @param[in] literal part of the value pair.
     */
    void addCriterionTypeValuePair(const std::string &typeName, uint32_t numeric,
                                   const std::string &literal);

    /**
     * Set a criterion to PFW. This value will be taken into account at next applyConfiguration.
     *
     * @param[in] name of the PFW criterion.
     * @param[in] value criterion type name to which this criterion is associated to.
     *
     * @return true if the criterion value has changed, false otherwise.
     */
    bool setCriterion(const std::string &name, uint32_t value);

    /**
     * Stage a criterion to PFW.
     * This value will NOT be taken into account at next applyConfiguration unless the criterion
     * is commited (commitCriteriaAndApplyConfiguration).
     *
     * @param[in] name of the PFW criterion.
     * @param[in] value criterion type name to which this criterion is associated to.
     *
     * @return true if the criterion value has changed, false otherwise.
     */
    bool stageCriterion(const std::string &name, uint32_t value);

    /**
     * Commit a criterion to PFW. The value will be taken into account at next applyConfiguration.
     *
     * @param[in] name of the PFW criterion.
     *
     * @return true if the criterion value has changed, false otherwise.
     */
    bool commitCriterion(const std::string &name);

    uint32_t getCriterion(const std::string &name) const;

    /**
     * Apply the configuration of the platform on the route parameter manager.
     * Once all the criteria have been set, the client of the platform state must call
     * this function in order to have the PFW taking into account these criteria.
     *
     */
    void applyConfiguration();

    void commitCriteriaAndApplyConfiguration();

    CParameterHandle *getDynamicParameterHandle(const std::string &dynamicParamPath);

    std::string getFormattedState(const std::string &typeName, uint32_t numeric) const;

    bool getNumericalValue(const std::string &typeName, const std::string &literal,
                           int &numeric) const;
    CParameterMgrPlatformConnector *getConnector() { return mConnector; }

private:
    CriterionTypes mCriterionTypes; /**< Criterion type collection Map. */
    Criteria mCriteria; /**< Criteria collection Map. */
    CParameterMgrPlatformConnector *mConnector; /**< Parameter-Manager connector. */
    ParameterMgrPlatformConnectorLogger *mConnectorLogger; /**< Parameter-Manager logger. */
    ParameterMgrHelper *mParameterHelper;

    const std::string mTag;
};

} // namespace intel_audio
