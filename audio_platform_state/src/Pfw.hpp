/*
 * Copyright (C) 2013-2015 Intel Corporation
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

#include <NonCopyable.hpp>
#include "Parameter.hpp"
#include <cutils/config_utils.h>
#include <utils/Errors.h>
#include <inttypes.h>
#include <list>
#include <map>
#include <string>
#include <vector>

class CParameterMgrPlatformConnector;
class Criterion;
class CriterionType;
class cnode;
class ParameterMgrHelper;
class CParameterHandle;

namespace intel_audio
{

enum pfwtype
{
    Route,
    Audio
};

template <pfwtype pfw>
class PfwTrait;

class ParameterMgrPlatformConnectorLogger;

typedef std::pair<std::string, std::string> AndroidParamMappingValuePair;

template <class Trait>
class Pfw : private audio_comms::utilities::NonCopyable
{
private:
    typedef std::map<std::string, CriterionType *> CriterionTypes;
    typedef std::map<std::string, Criterion *> Criteria;
    typedef std::pair<int, const char *> CriterionTypeValuePair;
    typedef Criteria::iterator CriterionMapIterator;
    typedef Criteria::const_iterator CriterionMapConstIterator;
    typedef CriterionTypes::iterator CriterionTypeMapIterator;
    typedef CriterionTypes::const_iterator CriteriaTypeMapConstIterator;
    typedef std::vector<Parameter *>::iterator ParamIterator;

public:
    Pfw();

    virtual ~Pfw();

    android::status_t start();

    /**
     * Add a criterion type to PFW.
     *
     * @param[in] typeName of the PFW criterion type.
     * @param[in] isInclusive attribute of the criterion type.
     */
    bool addCriterionType(const std::string &typeName, bool isInclusive);

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
     * Add a criterion to PFW.
     *
     * @param[in] name of the PFW criterion.
     * @param[in] typeName criterion type name to which this criterion is associated to.
     * @param[in] defaultLiteralValue of the PFW criterion.
     */
    void addCriterion(const std::string &name,
                      const std::string &typeName,
                      const std::string &defaultLiteralValue);

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

    /**
     * Parse and load the inclusive criterion type from configuration file.
     *
     * @param[in] root node of the configuration file.
     */
    void loadInclusiveCriterionType(cnode &root);

    /**
     * Parse and load the exclusive criterion type from configuration file.
     *
     * @param[in] root node of the configuration file.
     */
    void loadExclusiveCriterionType(cnode &root);

    /**
     * Add a parameter and its mapping pairs to the Platform state.
     *
     * @param[in] param object to add.
     * @param[in] valuePairs Mapping table between android-parameter values and PFW parameter values
     */
    void addParameter(Parameter *param,
                      const std::vector<AndroidParamMappingValuePair> &valuePairs,
                      std::vector<Parameter *> &parameterVector);

    /**
     * Add a Rogue parameter to PFW.
     *
     * @param[in] typeName parameter type.
     * @param[in] paramKey android-parameter key to which this PFW parameter is associated to.
     * @param[in] name of the PFW parameter.
     * @param[in] defaultValue of the PFW parameter.
     * @param[in] valuePairs Mapping table between android-parameter values and PFW parameter values
     */
    void addRogueParameter(const std::string &typeName, const std::string &paramKey,
                           const std::string &name, const std::string &defaultValue,
                           const std::vector<AndroidParamMappingValuePair> &valuePairs,
                           std::vector<Parameter *> &parameterVector);

    /**
     * Add a Criterion parameter to PFW.
     *
     * @param[in] typeName parameter type.
     * @param[in] paramKey android-parameter key to which this PFW parameter is associated to.
     * @param[in] name of the PFW parameter.
     * @param[in] defaultValue of the PFW parameter.
     * @param[in] valuePairs Mapping table between android-parameter values and PFW parameter values
     */
    void addCriterionParameter(const std::string &typeName, const std::string &paramKey,
                               const std::string &name, const std::string &defaultValue,
                               const std::vector<AndroidParamMappingValuePair> &valuePairs,
                               std::vector<Parameter *> &parameterVector);
    /**
     * Parse and load the rogue parameter type from configuration file.
     *
     * @param[in] root node of the configuration file.
     */
    void loadRogueParameterType(cnode &root, std::vector<Parameter *> &parameterVector);

    /**
     * Parse and load the rogue parameters type from configuration file and push them into a list.
     *
     * @param[in] root node of the configuration file.
     */
    void loadRogueParameterTypeList(cnode &root, std::vector<Parameter *> &parameterVector);

    /**
     * Parse and load the criteria from configuration file.
     *
     * @param[in] root node of the configuration file.
     */
    void loadCriteria(cnode &root, std::vector<Parameter *> &parameterVector);

    /**
     * Parse and load a criterion from configuration file.
     *
     * @param[in] root node of the configuration file.
     */
    void loadCriterion(cnode &root, std::vector<Parameter *> &parameterVector);

    /**
     * Parse and load the criterion types from configuration file.
     *
     * @param[in] root node of the configuration file
     * @param[in] isInclusive true if inclusive, false is exclusive.
     */
    void loadCriterionType(cnode &root, bool isInclusive);

    /**
     * Parse and load the chidren node from a given root node.
     *
     * @param[in] root node of the configuration file
     * @param[out] path of the parameter manager element to retrieve.
     * @param[out] defaultValue of the parameter manager element to retrieve.
     * @param[out] key of the android parameter to retrieve.
     * @param[out] type of the parameter manager element to retrieve.
     * @param[out] valuePairs pair of android value / parameter Manager value.
     */
    void parseChildren(cnode &root, std::string &path, std::string &defaultValue, std::string &key,
                       std::string &type, std::vector<AndroidParamMappingValuePair> &valuePairs);

    void loadConfig(cnode &root, std::vector<Parameter *> &parameterVector);

private:
    CParameterMgrPlatformConnector *getConnector();

    /**
     * Parse and load the mapping table of a criterion from configuration file.
     * A mapping table associates the Android Parameter values to the criterion values.
     *
     * @param[in] values: csv list of param value/criterion values to parse.
     *
     * @return vector of value pairs.
     */
    std::vector<AndroidParamMappingValuePair> parseMappingTable(const char *values);

    /**
     * Check if the given collection has the element indexed by the name key
     *
     * @tparam T type of element to search.
     * @param[in] name name of the element to find.
     * @param[in] elementsMap maps of elements to search into.
     *
     * @return true if element found within collection, false otherwise.
     */
    template <typename T>
    bool collectionHasElement(const std::string &name,
                              const std::map<std::string, T> &collection) const;

    /**
     * Retrieve an element from a map by its name.
     *
     * @tparam T type of element to search.
     * @param[in] name name of the element to find.
     * @param[in] elementsMap maps of elements to search into.
     *
     * @return valid pointer on element if found, NULL otherwise.
     */
    template <typename T>
    T *getElement(const std::string &name, std::map<std::string, T *> &elementsMap);

    /**
     * Retrieve an element from a map by its name. Const version.
     *
     * @tparam T type of element to search.
     * @param[in] name name of the element to find.
     * @param[in] elementsMap maps of elements to search into.
     *
     * @return valid pointer on element if found, NULL otherwise.
     */
    template <typename T>
    const T *getElement(const std::string &name,
                        const std::map<std::string, T *> &elementsMap) const;

private:
    CriterionTypes mCriterionTypes; /**< Criterion type collection Map. */
    Criteria mCriteria; /**< Criteria collection Map. */
    CParameterMgrPlatformConnector *mConnector; /**< Parameter-Manager connector. */
    ParameterMgrPlatformConnectorLogger *mConnectorLogger; /**< Parameter-Manager logger. */
    ParameterMgrHelper *mParameterHelper;

    const std::string mTag;
};

} // namespace intel_audio
