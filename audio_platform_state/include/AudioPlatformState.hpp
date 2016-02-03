/*
 * Copyright (C) 2013-2016 Intel Corporation
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

#include "AudioHalConf.hpp"
#include "Parameter.hpp"
#include <Pfw.hpp>
#include <AudioNonCopyable.hpp>
#include <KeyValuePairs.hpp>
#include <AudioCommsAssert.hpp>
#include <Direction.hpp>
#include <utils/Errors.h>
#include <utils/RWLock.h>
#include <list>
#include <string>
#include <vector>

class CParameterMgrPlatformConnector;
class Criterion;
class CriterionType;
struct cnode;
class ParameterMgrHelper;
class CParameterHandle;

namespace intel_audio
{

class ParameterMgrPlatformConnectorLogger;

class AudioPlatformState : private audio_comms::utilities::NonCopyable
{
private:
    /**
     * This class defines a unary function to be used when looping on the vector of parameters
     * It will help checking if the key received in the AudioParameter structure is associated
     * to a Parameter object and if found, the value will be get from this parameter.
     */
    class GetFromAndroidParameterHelper
    {
    public:
        GetFromAndroidParameterHelper(KeyValuePairs *pairsIn,
                                      KeyValuePairs *pairsOut)
            : mPairsIn(pairsIn), mPairsOut(pairsOut)
        {}

        void operator()(Parameter *param)
        {
            std::string key(param->getKey());
            if (mPairsIn->hasKey(key)) {

                std::string literalValue;
                if (param->getValue(literalValue)) {

                    mPairsOut->add(key, literalValue);

                    // The key can be safely removed now. Even if the key appears twice in the
                    // config file (meaning associated to more than one criterion/rogue), the value
                    // of the android parameter will be the same.
                    mPairsIn->remove(key);
                }
            }
        }
        KeyValuePairs *mPairsIn;
        KeyValuePairs *mPairsOut;
    };

    /**
     * This class defines a unary function to be used when looping on the vector of parameters
     * It will help checking if the key received in the AudioParameter structure is associated
     * to a Parameter object and if found, the key will be removed in order to be sure all keys
     * received have been taken into account.
     */
    class ClearKeyAndroidParameterHelper
    {
    public:
        ClearKeyAndroidParameterHelper(KeyValuePairs *pairs)
            : mPairs(pairs)
        {}

        void operator()(Parameter *param)
        {
            std::string key(param->getKey());
            if (mPairs->hasKey(key)) {
                mPairs->remove(key);
            }
        }
        KeyValuePairs *mPairs;
    };

    /**
     * This class defines a unary function to be used when looping on the vector of parameters
     * It will help syncing all the parameters.
     */
    class SyncParameterHelper
    {
    public:
        SyncParameterHelper() {}

        void operator()(Parameter *param)
        {
            param->sync();
        }
    };

public:
    AudioPlatformState();
    virtual ~AudioPlatformState();

    /**
     * Starts the platform state service.
     *
     * @return OK if success, error code otherwise.
     */
    android::status_t start();

    /**
     * Generic setParameter handler.
     * It can for example:
     *      -Set the TTY mode.
     * (Direction of the TTY is a bitfield with Downlink and Uplink fields.)
     *      - Set the HAC mode.
     *      - Set the BT headset NREC. (BT device embeds its acoustic algorithms).
     *      - Set the BT headset negociated Band Type.
     * (Band Type results of the negociation between device and the BT HFP headset.)
     *      - Set the BT Enabled flag.
     *      - Set the context awareness status.
     *      - Set the FM state.
     *      - Set the screen state.
     *
     * @param[in] keyValuePairs semicolon separated list of key=value.
     * @param[out] hasChanged: return true if the platform has changed due to new param,
     *                         false otherwise
     *
     * @return OK if these parameters were applyied correctly, error code otherwise.
     */
    android::status_t setParameters(const std::string &keyValuePairs, bool &hasChanged);

    /**
     * Get the global parameters of Audio HAL.
     *
     * @param[out] keys: one or more value pair "name=value", semicolon-separated.
     *
     * @return OK if set is successful, error code otherwise.
     */
    std::string getParameters(const std::string &keys);

    /**
     * Print debug information from target debug files
     */
    void printPlatformFwErrorInfo() const;

    /**
     * Apply the configuration of the platform on the parameter manager.
     * Once all the criteria have been set, the client of the platform state must call
     * this function in order to have the PFW taking into account these criteria.
     *
     * @tparam pfw instance of Parameter Manager targeted for this call.
     */
    template <pfwtype pfw>
    void applyConfiguration()
    {
        getPfw<pfw>()->applyConfiguration();
    }

    /**
     * Commit the criteria and apply the configuration of the platform on the parameter manager.
     *
     * @tparam pfw instance of Parameter Manager targeted for this call.
     */
    template <pfwtype pfw>
    void commitCriteriaAndApplyConfiguration()
    {
        getPfw<pfw>()->commitCriteriaAndApplyConfiguration();
    }

    /**
     * Set a criterion to PFW. This value will be taken into account at next applyConfiguration.
     *
     * @tparam pfw instance of Parameter Manager targeted for this call.
     * @param[in] name of the PFW criterion.
     * @param[in] value criterion type name to which this criterion is associated to.
     *
     * @return true if the criterion value has changed, false otherwise.
     */
    template <pfwtype pfw>
    bool setCriterion(const std::string &name, uint32_t value)
    {
        return getPfw<pfw>()->setCriterion(name, value);
    }

    /**
     * Stage a criterion to PFW.
     * This value will NOT be taken into account at next applyConfiguration unless the criterion
     * is commited (commitCriteriaAndApplyConfiguration).
     *
     * @tparam pfw instance of Parameter Manager targeted for this call.
     * @param[in] name of the PFW criterion.
     * @param[in] value criterion type name to which this criterion is associated to.
     *
     * @return true if the criterion value has changed, false otherwise.
     */
    template <pfwtype pfw>
    bool stageCriterion(const std::string &name, uint32_t value)
    {
        return getPfw<pfw>()->stageCriterion(name, value);
    }

    /**
     * Add a criterion type to PFW.
     *
     * @tparam pfw instance of Parameter Manager targeted for this call.
     * @param[in] typeName of the PFW criterion type.
     * @param[in] isInclusive attribute of the criterion type.
     */
    template <pfwtype pfw>
    bool addCriterionType(const std::string &name, bool isInclusive)
    {
        return getPfw<pfw>()->addCriterionType(name, isInclusive);
    }

    /**
     * Add a criterion type value pair to PFW.
     *
     * @tparam pfw instance of Parameter Manager targeted for this call.
     * @param[in] typeName criterion type name to which this value pair is added to.
     * @param[in] numeric part of the value pair.
     * @param[in] literal part of the value pair.
     */
    template <pfwtype pfw>
    void addCriterionTypeValuePair(const std::string &name, const std::string &literal,
                                   uint32_t value)
    {
        getPfw<pfw>()->addCriterionTypeValuePair(name, value, literal);
    }

    /**
     * Add a criterion to PFW.
     *
     * @tparam pfw instance of Parameter Manager targeted for this call.
     * @param[in] name of the PFW criterion.
     * @param[in] typeName criterion type name to which this criterion is associated to.
     * @param[in] defaultLiteralValue of the PFW criterion.
     */
    template <pfwtype pfw>
    void addCriterion(const std::string &name, const std::string &criterionType,
                      const std::string &defaultLiteralValue = "")
    {
        getPfw<pfw>()->addCriterion(name, criterionType, defaultLiteralValue);
    }

    /**
     * Get a parameter handle on the targetted PFW.
     *
     * @tparam pfw instance of Parameter Manager targeted for this call.
     * @param[in] dynamicParamPath path of the parameter for which the handle is requested.
     *
     * @return handle on the parameter, NULL if not found.
     */
    template <pfwtype pfw>
    CParameterHandle *getDynamicParameterHandle(const std::string &dynamicParamPath)
    {
        return getPfw<pfw>()->getDynamicParameterHandle(dynamicParamPath);
    }

    /**
     * Get the literal representation of a numeric value of a criterion type.
     *
     * @param[in] typeName criterion type name.
     * @param[in] numeric value of the criterion type.
     *
     * @return string representation of the numeric value, empty if any error occured.
     */
    template <pfwtype pfw>
    std::string getFormattedState(const std::string &typeName, uint32_t numeric)
    {
        return getPfw<pfw>()->getFormattedState(typeName, numeric);
    }

    /**
     * Notify a change of criterion.
     *
     * @param[in] name criterion that has changed.
     */
    void criterionHasChanged(const std::string &name);

private:
    /**
     * Synchronise all parameters (rogue / criteria on Route and Audio PFW)
     * and apply the configuration on Route PFW.
     */
    void sync();

    template <pfwtype pfw>
    Pfw<PfwTrait<pfw> > *getPfw() const;
    template <pfwtype pfw>
    Pfw<PfwTrait<pfw> > *getPfw();

    /**
     * Clear all the keys found into AudioParameter and in the configuration file.
     * If unknown keys remain, it prints a warn log.
     *
     * @param[in,out] pairs: {key, value} collection.
     */
    void clearKeys(KeyValuePairs *pairs);

    /**
     * Load the criterion configuration file.
     *
     * @param[in] path Criterion conf file path.
     *
     * @return OK is parsing successful, error code otherwise.
     */
    android::status_t loadAudioHalConfig(const char *path);

    /**
     * Sets a platform state event.
     *
     * @param[in] eventStateName name of the event that happened.
     */
    void setPlatformStateEvent(const std::string &eventStateName);

    Pfw<PfwTrait<Audio> > *mAudioPfw;
    Pfw<PfwTrait<Route> > *mRoutePfw;

    std::vector<Parameter *> mParameterVector; /**< Map of parameters. */

    /**
     * String containing a list of paths to the hardware debug files on target
     * to debug the audio firmware/driver in case of EIO error. Defined in pfw.
     */
    static const std::string mHwDebugFilesPathList;

    /**
     * Max size of the output debug stream in characters
     */
    static const uint32_t mMaxDebugStreamSize;

    /**
     * PFW concurrency protection - to garantee atomic operation only.
     */
    mutable android::RWLock mPfwLock;
};

} // namespace intel_audio
