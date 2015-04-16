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
#include <convert.hpp>
#include <ParameterMgrHelper.hpp>
#include <utilities/Log.hpp>

namespace intel_audio
{

/**
 * This class intends to address the Android Parameters that are associated to a Rogue Parameter.
 * Each time the key of this android parameters is detected, this class will wrap the accessor
 * of the android parameter to accessors of the rogue parameter.
 */
class RogueParameter : public Parameter
{
public:

    RogueParameter(ParameterChangedObserver *observer,
                   const std::string &key,
                   const std::string &name,
                   const std::string &defaultValue)
        : Parameter(observer, key, name, defaultValue)
    {}

protected:
    /**
     * Helper function to get the value of the android-parameter from a rogue-parameter value.
     * It checks the validity of the rogue value according to the given mapping table found
     * in the configuration file.
     *
     * @tparam T type of the rogue-parameter value.
     * @param[in] androidParamValue android-parameter value to check upon validity.
     * @param[out] rogueValue value of the rogue-parameter. Valid only if true is returned.
     *
     * @return true if value found in the mapping table, false otherwise.
     */
    template <typename T>
    bool convertAndroidParamValueToValue(const std::string &androidParamValue, T &rogueValue) const
    {
        std::string literalParamValue = getDefaultLiteralValue();
        if (!getLiteralValueFromParam(androidParamValue, literalParamValue)) {
            audio_comms::utilities::Log::Warning() << __FUNCTION__
                                                   << ": unknown parameter value "
                                                   << androidParamValue
                                                   << " for " << getKey();
            return false;
        }
        audio_comms::utilities::Log::Verbose() << __FUNCTION__
                                               << ": " << getName() << " (" << androidParamValue
                                               << ", " << literalParamValue << ")";

        return audio_comms::utilities::convertTo(literalParamValue, rogueValue);
    }

    /**
     * Helper function to get the value of the rogue-parameter from an android-parameter value.
     * It checks the validity of the android-parameter according to the given mapping table found
     * in the configuration file.
     *
     * @tparam T type of the rogue-parameter value.
     * @param[in] roguevalue parameter value to check upon validity.
     * @param[out] androidParamValue android-parameter. Valid only if true is returned.
     *
     * @return true if value found in the mapping table, false otherwise.
     */
    template <typename T>
    bool convertValueToAndroidParamValue(const T &rogueValue, std::string &androidParamValue) const
    {
        std::string literalValue = "";
        return audio_comms::utilities::convertTo(rogueValue, literalValue) &&
               getParamFromLiteralValue(androidParamValue, literalValue);
    }
};

/**
 * This class intends to address the Android Parameters that are associated to a Rogue Parameter
 * of the route PFW Instance
 * Each time the key of this android parameters is detected, this class will wrap the accessor
 * of the android parameter to accessors of the rogue parameter of the Route PFW.
 *
 * @tparam T type of the rogue parameter value.
 */
template <class T>
class RouteRogueParameter : public RogueParameter
{
public:
    RouteRogueParameter(ParameterChangedObserver *observer,
                        const std::string &key,
                        const std::string &name,
                        CParameterMgrPlatformConnector *parameterMgrConnector,
                        const std::string &defaultValue = "")
        : RogueParameter(observer, key, name, defaultValue),
          mParameterMgrConnector(parameterMgrConnector)
    {
    }

    virtual bool setValue(const std::string &value)
    {
        T typedValue;
        return RogueParameter::convertAndroidParamValueToValue<T>(value, typedValue) &&
               ParameterMgrHelper::setParameterValue<T>(mParameterMgrConnector,
                                                        getName(), typedValue);
    }

    virtual bool getValue(std::string &value) const
    {
        T typedValue;
        return ParameterMgrHelper::getParameterValue<T>(mParameterMgrConnector, getName(),
                                                        typedValue) &&
               RogueParameter::convertValueToAndroidParamValue<T>(typedValue, value);
    }

    virtual bool sync()
    {
        T typedValue;
        return audio_comms::utilities::convertTo(getDefaultLiteralValue(), typedValue) &&
               ParameterMgrHelper::setParameterValue<T>(mParameterMgrConnector, getName(),
                                                        typedValue);
    }

private:
    CParameterMgrPlatformConnector *mParameterMgrConnector; /**< Route PFW connector. */
};

/**
 * This class intends to address the Android Parameters that are associated to a Rogue Parameter
 * of the Audio PFW Instance
 * Each time the key of this android parameters is detected, this class will wrap the accessor
 * of the android parameter to accessors of the rogue parameter of the Audio PFW.
 *
 * @tparam T type of the rogue parameter value.
 */
template <class T>
class AudioRogueParameter : public RogueParameter
{
public:
    AudioRogueParameter(ParameterChangedObserver *observer,
                        const std::string &key,
                        const std::string &name,
                        IStreamInterface *streamInterface,
                        const std::string &defaultValue = "")
        : RogueParameter(observer, key, name, defaultValue),
          mStreamInterface(streamInterface)
    {
    }

    virtual bool setValue(const std::string &value)
    {
        T typedValue;
        return RogueParameter::convertAndroidParamValueToValue<T>(value, typedValue) &&
               mStreamInterface->setAudioParameter(getName(), typedValue);
    }

    virtual bool getValue(std::string &value) const
    {
        T typedValue;
        return mStreamInterface->getAudioParameter(getName(), typedValue) &&
               RogueParameter::convertValueToAndroidParamValue<T>(typedValue, value);
    }

    virtual bool sync()
    {
        T typedValue;
        return audio_comms::utilities::convertTo(getDefaultLiteralValue(), typedValue) &&
               mStreamInterface->setAudioParameter(getName(), typedValue);
    }

private:
    IStreamInterface *mStreamInterface; /**< Handle on stream interface of Route Manager. */
};

} // namespace intel_audio
