/*
 * Copyright (C) 2014-2016 Intel Corporation
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
#include <map>
#include <string>

namespace intel_audio
{

/**
 * This class materialize a wrapping object between an android-parameter refered by its key
 * and a Parameter Manager element (criterion or rogue) refered by its name (for a criterion)
 * or path (for a rogue parameter). This object will allow to wrap setter and getter on this
 * android-parameter to the associated element in the Parameter Manager.
 */
class Parameter : private audio_comms::utilities::NonCopyable
{
public:
    enum Type
    {
        RogueParameter,
        CriterionParameter
    };

    typedef std::map<std::string, std::string>::iterator MappingValuesMapIterator;
    typedef std::map<std::string, std::string>::const_iterator MappingValuesMapConstIterator;

    Parameter(const std::string &key,
              const std::string &name,
              const std::string &defaultValue)
        : mDefaultLiteralValue(defaultValue),
          mAndroidParameterKey(key),
          mAndroidParameter(name)
    {
    }

    virtual ~Parameter() {}

    virtual Type getType() const = 0;

    /**
     * Get the key of the android parameter associated to the PFW parameter.
     *
     * @return android-parameter key.
     */
    const std::string &getKey() const { return mAndroidParameterKey; }

    /**
     * Returns the name of the Android Parameter.
     *
     * @return name of the android parameter.
     */
    const std::string &getName() const { return mAndroidParameter; }

    /**
     * Returns the default literal value of the parameter (in the PFW domain, not in android).
     *
     * @return default literal value of the PFW parameter
     */
    const std::string &getDefaultLiteralValue() const { return mDefaultLiteralValue; }

    /**
     * Adds a mapping value pair, i.e. a pair or Android Value, PFW Parameter Value.
     *
     * @param[in] name android parameter value.
     * @param[in] value PFW parameter value.
     */
    void setMappingValuePair(const std::string &name, const std::string &value);

    /**
     * Sets a value to the parameter.
     *
     * @param[in] value to set (received from the keyValue pair).
     *
     * @return true if set is successful, false otherwise.
     */
    virtual bool setValue(const std::string &value) = 0;

    /**
     * Gets the value from the Parameter. The value returned must be in the domain
     * of the android parameter.
     *
     * @param[out] value to return (associated to the key of the android parameter).
     *
     * @return true if get is successful, false otherwise.
     */
    virtual bool getValue(std::string &value) const = 0;

    /**
     * Synchronise the pfw Parameter associated to an android-parameter.
     *
     * @return true if sync is successful, false otherwise.
     */
    virtual bool sync() = 0;

protected:
    /**
     * Checks the validity of an android parameter value.
     *
     * @param[in] androidParam android parameter to wrap.
     * @param[out] literalValue associated to the androidParam value. Set only if return is true.
     *
     * @return true if value found in the mapping table, false otherwise.
     */
    bool getLiteralValueFromParam(const std::string &androidParam, std::string &literalValue) const;

    /**
     * Checks the validity of a rogue parameter value.
     *
     * @param[out] androidParam android-parameter associated to the. Set only if return is true.
     * @param[in] literalValue to wrap.
     *
     * @return true if value found in the mapping table, false otherwise.
     */
    bool getParamFromLiteralValue(std::string &androidParam, const std::string &literalValue) const;

    /**
     * associate android parameter values to PFW element values (it may be criterion literal values
     * or rogue parameter literal values).
     * This Map may be empty, in this case, any value of the Android Parameter will be applied as it
     * to the parameter / criterion without any validity check.
     */
    std::map<std::string, std::string> mMappingValuesMap;

private:
    /**
     * Default literal value.
     * The default is in the parameter domain.
     */
    const std::string mDefaultLiteralValue;

    const std::string mAndroidParameterKey; /**< key name of the parameter. */

    const std::string mAndroidParameter; /**< Name of the parameter. */
};

} // namespace intel_audio
