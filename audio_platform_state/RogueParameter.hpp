/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2014 Intel
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
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#pragma once

#include "Parameter.hpp"

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

            ALOGW("%s: unknown parameter value(%s) for %s",
                  __FUNCTION__, androidParamValue.c_str(), getKey().c_str());
            return false;
        }
        ALOGV("%s: %s (%s, %s)", __FUNCTION__, getName().c_str(), androidParamValue.c_str(),
              literalParamValue.c_str());

        return audio_comms::utilities::convertTo<T>(literalParamValue, rogueValue);
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
        return audio_comms::utilities::toString<T>(rogueValue, literalValue) &&
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
        RogueParameter::convertAndroidParamValueToValue<T>(value, typedValue);
        return ParameterMgrHelper::setParameterValue<T>(mParameterMgrConnector,
                                                        getName(), typedValue);
    }

    virtual bool getValue(std::string &value) const
    {
        T typedValue;
        ParameterMgrHelper::getParameterValue<T>(mParameterMgrConnector, getName(), typedValue);
        return RogueParameter::convertValueToAndroidParamValue<T>(typedValue, value);
    }

    virtual bool sync()
    {
        T typedValue;
        return audio_comms::utilities::convertTo<T>(getDefaultLiteralValue(), typedValue) &&
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
        RogueParameter::convertAndroidParamValueToValue<T>(value, typedValue);
        return mStreamInterface->setAudioParameter(getName(), typedValue);
    }

    virtual bool getValue(std::string &value) const
    {
        T typedValue;
        mStreamInterface->getAudioParameter(getName(), typedValue);
        return RogueParameter::convertValueToAndroidParamValue<T>(typedValue, value);
    }

    virtual bool sync()
    {
        T typedValue;
        return audio_comms::utilities::convertTo<T>(getDefaultLiteralValue(), typedValue) &&
               mStreamInterface->setAudioParameter(getName(), typedValue);
    }

private:
    IStreamInterface *mStreamInterface; /**< Handle on stream interface of Route Manager. */
};
