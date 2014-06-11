/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
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
#include "AudioBand.h"
#include "VolumeKeys.hpp"
#include <NonCopyable.hpp>
#include <Direction.hpp>
#include <hardware_legacy/AudioHardwareBase.h>
#include <StreamInterface.hpp>
#include <AudioCommsAssert.hpp>
#include <list>
#include <map>
#include <string>
#include <utils/Errors.h>
#include <utils/String8.h>
#include <vector>

class CParameterMgrPlatformConnector;
class Criterion;
class CriterionType;
class cnode;
class Stream;

namespace android_audio_legacy
{

class ParameterMgrPlatformConnectorLogger;



class AudioPlatformState
    : public ParameterChangedObserver,
      private audio_comms::utilities::NonCopyable
{
private:
    typedef std::pair<const char *, const char *> AndroidParamMappingValuePair;
    typedef std::pair<int, const char *> CriterionTypeValuePair;
    typedef std::vector<Parameter *>::iterator ParamIterator;
    typedef std::map<std::string, Criterion *>::iterator CriterionMapIterator;
    typedef std::map<std::string, Criterion *>::const_iterator CriterionMapConstIterator;
    typedef std::map<std::string, CriterionType *>::iterator CriterionTypeMapIterator;
    typedef std::map<std::string, CriterionType *>::const_iterator CriteriaTypeMapConstIterator;

    typedef std::list<Stream *>::iterator StreamListIterator;
    typedef std::list<const Stream *>::const_iterator StreamListConstIterator;

    /**
     * This class defines a unary function to be used when looping on the vector of value pairs
     * of a parameter.
     * It will help applying all the pairs (Android-Parameter value - PFW-Parameter value)
     * for the mapping table.
     */
    class SetAndroidParamMappingPairHelper
    {
    public:
        SetAndroidParamMappingPairHelper(Parameter *param)
            : mParamCriterion(param)
        {}

        void operator()(AndroidParamMappingValuePair pair)
        {
            mParamCriterion->setMappingValuePair(pair.first, pair.second);
        }

        Parameter *mParamCriterion;
    };

    /**
     * This class defines a unary function to be used when looping on the vector of parameters
     * It will help checking if the key received in the AudioParameter structure is associated
     * to a Parameter object and if found, the value will be set to this parameter.
     */
    class SetFromAndroidParameterHelper
    {
    public:
        SetFromAndroidParameterHelper(AudioParameter *param, int *errorCount)
            : mParam(param),
              mErrorCount(errorCount)
        {}

        void operator()(Parameter *param)
        {

            String8 key(param->getKey().c_str());
            String8 value;
            if (mParam->get(key, value) == NO_ERROR) {
                if (!param->setValue(value.string())) {
                    *mErrorCount += 1;
                }
                // Do not remove the key as nothing forbid to give the same key for 2
                // criteria / rogue parameters...
            }
        }

        AudioParameter *mParam;
        int *mErrorCount;
    };

    /**
     * This class defines a unary function to be used when looping on the vector of parameters
     * It will help checking if the key received in the AudioParameter structure is associated
     * to a Parameter object and if found, the value will be get from this parameter.
     */
    class GetFromAndroidParameterHelper
    {
    public:
        GetFromAndroidParameterHelper(AudioParameter *paramIn, AudioParameter *paramOut)
            : mParamIn(paramIn), mParamOut(paramOut)
        {}

        void operator()(Parameter *param)
        {

            String8 key(param->getKey().c_str());
            String8 value;
            if (mParamIn->get(key, value) == NO_ERROR) {

                std::string strValue;
                if (param->getValue(strValue)) {

                    mParamOut->add(key, String8(strValue.c_str()));

                    // The key can be safely removed now. Even if the key appears twice in the
                    // config file (meaning associated to more than one criterion/rogue), the value
                    // of the android parameter will be the same.
                    mParamIn->remove(key);
                }
            }
        }
        AudioParameter *mParamIn;
        AudioParameter *mParamOut;
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
        ClearKeyAndroidParameterHelper(AudioParameter *param)
            : mParam(param)
        {}

        void operator()(Parameter *param)
        {
            String8 key(param->getKey().c_str());
            String8 value;
            if (mParam->get(key, value) == NO_ERROR) {
                mParam->remove(key);
            }
        }
        AudioParameter *mParam;
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
    AudioPlatformState(IStreamInterface *streamInterface);
    virtual ~AudioPlatformState();

    /**
     * Starts the platform state service.
     *
     * @return OK if success, error code otherwise.
     */
    android::status_t start();

    /**
     * Synchronise all parameters (rogue / criteria on Route and Audio PFW)
     * and apply the configuration on Route PFW.
     */
    void sync();

    /**
     * Apply the configuration of the platform on the route parameter manager.
     * Once all the criteria have been set, the client of the platform state must call
     * this function in order to have the route PFW taking into account these criteria.
     */
    void applyPlatformConfiguration();

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
     */
    android::status_t setParameters(const android::String8 &keyValuePairs);

    /**
     * Get the global parameters of Audio HAL.
     *
     * @param[out] keys: one or more value pair "name=value", semicolon-separated.
     *
     * @return OK if set is successful, error code otherwise.
     */
    String8 getParameters(const String8 &keys);

    /**
     * Checks if the platform state was correctly started (ie the route parameter manager
     * has been instantiated and started correctly).
     *
     * @return true if platform state is started correctly, false otherwise.
     */
    bool isStarted();

    /**
     * Set the modem status.
     *
     * @param[in] isAlive true if modem is UP, false otherwise (DOWN or RESET).
     */
    void setModemAlive(bool isAlive)
    {
        setValue(isAlive, mModemState);
    }
    /**
     * Get the modem status.
     *
     * @return true if modem is UP, false otherwise (DOWN or RESET).
     */
    bool isModemAlive() const
    {
        return getValue(mModemState);
    }

    /**
     * Set the modem audio call status.
     *
     * @param[in] isAudioAvailable true if modem is ready for enabling audio link.
     */
    void setModemAudioAvailable(bool isAudioAvailable)
    {
        setValue(isAudioAvailable, mModemAudioStatus);
    }
    /**
     * Get the modem audio call status.
     *
     * @return true if modem is ready for enabling audio link.
     */
    bool isModemAudioAvailable() const
    {
        return getValue(mModemAudioStatus);
    }

    /**
     * Set the modem embedded status.
     *
     * @param[in] isPresent true if platform embeds a modem, false otherwise.
     */
    void setModemEmbedded(bool isPresent)
    {
        setValue(isPresent, mHasModem);
    }

    /**
     * Get the modem embedded status.
     *
     * @return true if platform embeds a modem, false otherwise.
     */
    bool isModemEmbedded() const
    {
        return getValue(mHasModem);
    }

    /**
     * Set the android telephony mode.
     * Mode are defined by AudioSystem.
     *
     * @param[in] mode android mode selected by the policy.
     */
    void setMode(int mode)
    {
        VolumeKeys::wakeup(mode == AudioSystem::MODE_IN_CALL);
        setValue(mode, mAndroidMode);
    }

    /**
     * Get the android telephony mode.
     * Mode are defined by AudioSystem.
     *
     * @return android mode selected by the policy.
     */
    int getMode() const
    {
        return getValue(mAndroidMode);
    }

    /**
     * Set the devices.
     *
     * @param[in] devices: devices enabled mask
     * @param[in] isOut: true for output devices, false for input devices.
     */
    void setDevices(uint32_t devices, bool isOut)
    {
        setValue(devices, isOut ? mOutputDevice : mInputDevice);
    }
    /**
     * Get the devices.
     *
     * @param[in] isOut: true for output devices, false for input devices.
     * @return devices enabled mask.
     */
    uint32_t getDevices(bool isOut) const
    {
        return getValue(isOut ? mOutputDevice : mInputDevice);
    }

    /**
     * Set the CSV Band Type.
     * Voice Call Band Type is given by the modem itself.
     *
     * @param[in] bandType: the band type to be used for Voice Call.
     */
    void setCsvBandType(CAudioBand::Type bandType)
    {
        setValue(bandType, mCsvBand);
    }
    /**
     * Get the CSV Band Type.
     * Voice Call Band Type is given by the modem itself.
     *
     * @return the band type to be used for Voice Call.
     */
    CAudioBand::Type getCsvBandType() const
    {
        return static_cast<CAudioBand::Type>(getValue(mCsvBand));
    }

    /**
     * Update Input Sources.
     * It computes the input sources criteria as a mask of input source of all active input streams.
     */
    void updateActiveInputSources() { updateApplicabilityMask(false); }
    /**
     * Get Input Sources.
     *
     * @return computed input sources mask ie input source of all active input streams.
     */
    uint32_t getInputSource() const
    {
        return getValue(mInputSources);
    }

    /**
     * Update Output flags.
     * It computes the output flags criteria as a mask of output flags of all active output streams.
     */
    void updateActiveOutputFlags() { updateApplicabilityMask(true); }
    /**
     * Get Output flags.
     * return computed output flags maks ie mask of output flags of all active output streams.
     */
    uint32_t getOutputFlags() const
    {
        return getValue(mOutputFlags);
    }

    /**
     * Set the mic to muted/unmuted state.
     *
     * @param[in] muted: true to mute, false to unmute.
     */
    void setMicMute(bool muted)
    {
        setValue(muted, mMicMute);
    }
    /**
     * Get the mic to muted/unmuted state.
     *
     * @return true if muted, false if unmuted.
     */
    bool isMicMuted() const
    {
        return getValue(mMicMute);
    }

    /**
     * Informs that a stream is started.
     * It adds the stream to active stream list.
     * Platform states use this list to provide the outputflags/inputsource bitfield criteria
     * only when the stream using the flag/source is active.
     *
     * @param startedStream stream to be added to the list.
     */
    void startStream(const Stream *startedStream);

    /**
     * Informs that a stream is stopped.
     * It removes the stream to active stream list and update the outputflags or input sources.
     * criterion according to the direction of the stream.
     *
     * @param startedStream stream to be added to the list.
     */
    void stopStream(const Stream *stoppedStream);

    /**
     * Update all the parameters of the active input.
     * It not only updates the requested preproc criterion but also the band type.
     * Only one input stream may be active at one time.
     * However, it does not mean that both are not started, but only one has a valid
     * device given by the policy so that the other may not be routed.
     * Find this active stream with valid device and set the parameters
     * according to what was requested from this input.
     */
    void updateParametersFromActiveInput();

    /**
     * Set the BT headset negociated Band Type.
     * Band Type results of the negociation between device and the BT HFP headset.
     *
     * @param[in] eBtHeadsetBandType: the band type to be set.
     */
    bool hasPlatformStateChanged(int iEvents = -1) const;

    /**
     * Print debug information from target debug files
     */
    void printPlatformFwErrorInfo() const;

private:
    virtual void parameterHasChanged(const std::string &name);

    /**
     * Clear all the keys found into AudioParameter and in the configuration file.
     * If unknown keys remain, it prints a warn log.
     *
     * @param[in,out] param: AudioParameter structure.
     */
    void clearParamKeys(AudioParameter *param);

    /**
     * Set the Voice Band Type.
     * Voice band type is inferred by the rate of the input stream (which is a "direct" stream, ie
     * running at the same rate than the VoIP application).
     *
     * @param[in] activeStream: current active input stream (i.e. input stream that has a valid
     *                          input device as per policy implementation.
     */
    void setVoipBandType(const Stream *activeStream);

    /**
     * Update the applicability mask.
     * This function parses all active streams and concatenate their mask into a bit field.
     *
     * @param[in] isOut direction of streams.
     *
     */
    void updateApplicabilityMask(bool isOut);

    /**
     * Load the criterion configuration file.
     *
     * @param[in] path Criterion conf file path.
     *
     * @return OK is parsing successful, error code otherwise.
     */
    android::status_t loadAudioHalConfig(const char *path);

    /**
     * Returns a human readable name of the PFW instance associated to a given type.
     *
     * @tparam pfw Instance of parameter framework to which the type is linked.
     *
     * @return name of the PFW Instance
     */
    template <PfwInstance pfw>
    const std::string &getPfwInstanceName() const;

    /**
     * Add a criterion type to AudioPlatformState.
     *
     * @tparam pfw Instance of parameter framework to which the criterion type is linked.
     * @param[in] typeName of the PFW criterion type.
     * @param[in] isInclusive attribute of the criterion type.
     */
    template <PfwInstance pfw>
    void addCriterionType(const std::string &typeName, bool isInclusive);

    /**
     * Add a criterion type value pair to AudioPlatformState.
     *
     * @tparam pfw Instance of parameter framework to which the criterion is linked.
     * @param[in] typeName criterion type name to which this value pair is added to.
     * @param[in] numeric part of the value pair.
     * @param[in] literal part of the value pair.
     */
    template <PfwInstance pfw>
    void addCriterionTypeValuePair(const std::string &typeName, uint32_t numeric,
                                   const std::string &literal);

    /**
     * Add a criterion to AudioPlatformState.
     *
     * @tparam pfw Instance of parameter framework to which the parameter is linked.
     * @param[in] name of the PFW criterion.
     * @param[in] typeName criterion type name to which this criterion is associated to.
     * @param[in] defaultLiteralValue of the PFW criterion.
     */
    template <PfwInstance pfw>
    void addCriterion(const std::string &name,
                      const std::string &typeName,
                      const std::string &defaultLiteralValue);
    /**
     * Parse and load the inclusive criterion type from configuration file.
     *
     * @tparam[in] pfw Target Parameter framework for this element.
     * @param[in] root node of the configuration file.
     */
    template <PfwInstance pfw>
    void loadInclusiveCriterionType(cnode *root);

    /**
     * Parse and load the exclusive criterion type from configuration file.
     *
     * @tparam[in] pfw Target Parameter framework for this element.
     * @param[in] root node of the configuration file.
     */
    template <PfwInstance pfw>
    void loadExclusiveCriterionType(cnode *root);

    /**
     * Add a parameter and its mapping pairs to the Platform state.
     *
     * @param[in] param object to add.
     * @param[in] valuePairs Mapping table between android-parameter values and PFW parameter values
     */
    void addParameter(Parameter *param,
                      const std::vector<AndroidParamMappingValuePair> &valuePairs);

    /**
     * Add a parameter to AudioPlatformState.
     *
     * @tparam pfw Instance of parameter framework to which the parameter is linked.
     * @tparam type of the parameter (i.e. rogue or criterion).
     * @param[in] typeName parameter type.
     * @param[in] paramKey android-parameter key to which this PFW parameter is associated to.
     * @param[in] name of the PFW parameter.
     * @param[in] defaultValue of the PFW parameter.
     * @param[in] valuePairs Mapping table between android-parameter values and PFW parameter values
     */
    template <PfwInstance pfw, ParameterType type>
    void addParameter(const std::string &typeName, const std::string &paramKey,
                      const std::string &name, const std::string &defaultValue,
                      const std::vector<AndroidParamMappingValuePair> &valuePairs);
    /**
     * Parse and load the rogue parameter type from configuration file.
     *
     * @tparam[in] pfw Target Parameter framework for this element.
     * @param[in] root node of the configuration file.
     */
    template <PfwInstance pfw>
    void loadRogueParameterType(cnode *root);

    /**
     * Parse and load the rogue parameters type from configuration file and push them into a list.
     *
     * @tparam[in] pfw Target Parameter framework for this element.
     * @param[in] root node of the configuration file.
     */
    template <PfwInstance pfw>
    void loadRogueParameterTypeList(cnode *root);

    /**
     * Parse and load the criteria from configuration file.
     *
     * @tparam[in] pfw Target Parameter framework for this element.
     * @param[in] root node of the configuration file.
     */
    template <PfwInstance pfw>
    void loadCriteria(cnode *root);

    /**
     * Parse and load a criterion from configuration file.
     *
     * @tparam[in] pfw Target Parameter framework for this element.
     * @param[in] root node of the configuration file.
     */
    template <PfwInstance pfw>
    void loadCriterion(cnode *root);

    /**
     * Parse and load the criterion types from configuration file.
     *
     * @tparam[in] pfw Target Parameter framework for this element.
     * @param[in] root node of the configuration file
     * @param[in] isInclusive true if inclusive, false is exclusive.
     */
    template <PfwInstance pfw>
    void loadCriterionType(cnode *root, bool isInclusive);

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
    void parseChildren(cnode *root, std::string &path, std::string &defaultValue, std::string &key,
                       std::string &type, std::vector<AndroidParamMappingValuePair> &valuePairs);

    /**
     * Load the configuration file.
     *
     * @tparam[in] pfw Target Parameter framework for this element.
     * @param[in] root node of the configuration file.
     */
    template <PfwInstance pfw>
    void loadConfig(cnode *root);

    IStreamInterface *mStreamInterface; /**< Route Manager Stream Interface pointer. */

    /**
     * Parse and load the mapping table of a criterion from configuration file.
     * A mapping table associates the Android Parameter values to the criterion values.
     *
     * @param[in] values string of the list of param value/criterion values comma separated to parse.
     *
     * @return vector of value pairs.
     */
    std::vector<AndroidParamMappingValuePair> parseMappingTable(const char *values);

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

    /**
     * set the value of a component state.
     *
     * @param[in] value new value to set to the component state.
     * @param[in] stateName of the component state.
     */
    void setValue(int value, const char *stateName);

    /**
     * get the value of a component state.
     *
     * @param[in] name of the component state.
     *
     * @return value of the component state
     */
    int getValue(const char *stateName) const;

    /**
     * Resets the platform state events.
     */
    void clearPlatformStateEvents();

    /**
     * Sets a platform state event.
     *
     * @param[in] eventStateName name of the event that happened.
     */
    void setPlatformStateEvent(const std::string &eventStateName);

    /**
     * Update the streams mask.
     * This function parses all active streams and concatenate their mask into a bit field.
     *
     * @param[in] isOut direction of streams.
     *
     * @return concatenated mask of active streams.
     */
    uint32_t updateStreamsMask(bool isOut);

    /**
     * Input/Output Streams list.
     */
    std::list<const Stream *>
    mActiveStreamsList[audio_comms::utilities::Direction::_nbDirections];

    std::map<std::string, CriterionType *> mCriterionTypeMap;
    std::map<std::string, Criterion *> mCriterionMap;
    std::vector<Parameter *> mParameterVector; /**< Map of parameters. */

    CParameterMgrPlatformConnector *mRoutePfwConnector; /**< Route Parameter Manager connector. */
    ParameterMgrPlatformConnectorLogger *mRoutePfwConnectorLogger; /**< Route PFW logger. */

    /**
     * Defines the name of the Android property describing the name of the PFW configuration file.
     */
    static const char *const mRoutePfwConfFileNamePropName;
    static const char *const mRoutePfwDefaultConfFileName; /**< default PFW conf file name. */
    static const char *const mOutputDevice; /**< Output device criterion name. */
    static const char *const mInputDevice; /**< Input device criterion name. */
    static const char *const mInputSources; /**< Input sources criterion name. */
    static const char *const mOutputFlags; /**< Output flags criterion name. */
    static const char *const mModemAudioStatus; /**< Modem audio status criterion name. */
    static const char *const mAndroidMode; /**< Android Mode criterion name. */
    static const char *const mHasModem; /**< has modem criterion name. */
    static const char *const mModemState; /**< Modem State criterion name. */
    static const char *const mStateChanged; /**< State Changed criterion name. */
    static const char *const mCsvBand; /**< CSV Band criterion name. */
    static const char *const mVoipBand; /**< VoIP band criterion name. */
    static const char *const mMicMute; /**< Mic Mute criterion name. */
    static const char *const mPreProcessorRequestedByActiveInput; /**< requested preproc. */
    /**
     * Stream Rate associated with narrow band in case of VoIP.
     */
    static const uint32_t mVoiceStreamRateForNarrowBandProcessing = 8000;

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
     * provide a compile time error if no specialization is provided for a given type.
     *
     * @tparam T: type of the parameter manager element. Supported one are:
     *                      - Criterion
     *                      - CriterionType.
     */
    template <typename T>
    struct parameterManagerElementSupported;

    /**
     * Boolean to indicate that at least one of the audio PFW criterion has changed and the
     * routing must be reconsidered in order to apply configurations that may depend on these
     * criteria.
     */
    bool mAudioPfwHasChanged;
};
}         // namespace android
