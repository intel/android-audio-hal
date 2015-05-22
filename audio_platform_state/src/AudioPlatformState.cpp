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
#define LOG_TAG "AudioIntelHal/AudioPlatformState"

#include "AudioPlatformState.hpp"
#include "Pfw.hpp"
#include "CriterionParameter.hpp"
#include "RogueParameter.hpp"
#include "ParameterMgrPlatformConnector.h"
#include "VolumeKeys.hpp"
#include <IoStream.hpp>
#include <Criterion.hpp>
#include <CriterionType.hpp>
#include <ParameterMgrHelper.hpp>
#include <property/Property.hpp>
#include "NaiveTokenizer.h"
#include <algorithm>
#include <convert.hpp>
#include <cutils/bitops.h>
#include <cutils/config_utils.h>
#include <cutils/misc.h>
#include <utilities/Log.hpp>
#include <fstream>

using namespace std;
using audio_comms::utilities::convertTo;
using android::status_t;
using audio_comms::utilities::Log;
using audio_comms::utilities::Property;

namespace intel_audio
{

template <>
Pfw<PfwTrait<Route> > *AudioPlatformState::getPfw()
{
    return mRoutePfw;
}

template <>
Pfw<PfwTrait<Audio> > *AudioPlatformState::getPfw()
{
    return mAudioPfw;
}

template <>
Pfw<PfwTrait<Route> > *AudioPlatformState::getPfw() const
{
    return mRoutePfw;
}

template <>
Pfw<PfwTrait<Audio> > *AudioPlatformState::getPfw() const
{
    return mAudioPfw;
}

const std::string AudioPlatformState::mHwDebugFilesPathList =
    "/Route/debug_fs/debug_files/path_list/";

// For debug purposes. This size is enough for dumping relevant informations
const uint32_t AudioPlatformState::mMaxDebugStreamSize = 998;

AudioPlatformState::AudioPlatformState()
{
    /// Audio PFW must be created first
    mAudioPfw = new Pfw<PfwTrait<Audio> >();

    /// Route PFW must be created after Audio PFW
    mRoutePfw = new Pfw<PfwTrait<Route> >();

    /// Load Audio HAL configuration file to populate Criterion types, criteria and rogues.
    if ((loadAudioHalConfig(gAudioHalVendorConfFilePath) != android::OK) &&
        (loadAudioHalConfig(gAudioHalConfFilePath) != android::OK)) {
        Log::Error() << "Neither vendor conf file (" << gAudioHalVendorConfFilePath
                     << ") nor system conf file (" << gAudioHalConfFilePath << ") could be found";
    }
}

/**
 * This class defines a unary function to be used when deleting the object pointer by vector
 * of parameters.
 */
class DeleteParamHelper
{
public:
    DeleteParamHelper() {}

    void operator()(Parameter *param)
    {
        delete param;
    }
};

AudioPlatformState::~AudioPlatformState()
{
    // Delete all parameters (Rogue & CriterionParameters)
    std::for_each(mParameterVector.begin(), mParameterVector.end(), DeleteParamHelper());

    delete mAudioPfw;
    delete mRoutePfw;
}

status_t AudioPlatformState::start()
{
    // Route PFW must be started first
    if (getPfw<Route>()->start() != android::OK) {
        return android::NO_INIT;
    }
    // Audio PFW must be created after Route PFW
    if (getPfw<Audio>()->start() != android::OK) {
        return android::NO_INIT;
    }
    sync();
    return android::OK;
}

status_t AudioPlatformState::loadAudioHalConfig(const char *path)
{
    if (path == NULL) {
        Log::Error() << __FUNCTION__ << ": invalid path ";
        return android::BAD_VALUE;
    }
    cnode *root;
    char *data;
    Log::Debug() << __FUNCTION__ << ": loading configuration file " << path;
    data = (char *)load_file(path, NULL);
    if (data == NULL) {
        return -ENODEV;
    }
    root = config_node("", "");
    if (root == NULL) {
        Log::Error() << __FUNCTION__ << ": failed to parse configuration file";
        return android::BAD_VALUE;
    }
    config_load(root, data);

    getPfw<Audio>()->loadConfig(*root, mParameterVector);
    getPfw<Route>()->loadConfig(*root, mParameterVector);

    config_free(root);
    free(root);
    free(data);
    return android::OK;
}

void AudioPlatformState::sync()
{
    std::for_each(mParameterVector.begin(), mParameterVector.end(), SyncParameterHelper());
    applyConfiguration<Route>();
}

void AudioPlatformState::clearKeys(KeyValuePairs *pairs)
{
    std::for_each(mParameterVector.begin(), mParameterVector.end(),
                  ClearKeyAndroidParameterHelper(pairs));
    if (pairs->size()) {
        Log::Warning() << __FUNCTION__ << ": Unhandled argument: " << pairs->toString();
    }
}

/**
 * This class defines a unary function to be used when looping on the vector of parameters
 * It will help checking if the key received in the AudioParameter structure is associated
 * to a Parameter object and if found, the value will be set to this parameter.
 */
class SetFromAndroidParameterHelper
{
public:
    SetFromAndroidParameterHelper(KeyValuePairs *pairs, bool &hasChanged, status_t &ret,
                                  AudioPlatformState *parent)
        : mPairs(pairs), mHasChanged(hasChanged), mRet(ret), mParent(parent)
    {}

    void operator()(Parameter *param)
    {
        std::string key(param->getKey());
        std::string value;
        if (mPairs->get(key, value) == android::OK) {
            if (!param->setValue(value)) {
                mRet = android::BAD_VALUE;
                return;
            }
            mHasChanged = true;
            // Do not remove the key as nothing forbid to give the same key for 2
            if (param->getType() == Parameter::CriterionParameter) {
                // Handle particular cases, event is the criterion name, not the key
                mParent->criterionHasChanged(param->getName());
            }
        }
    }

private:
    KeyValuePairs *mPairs;
    bool &mHasChanged;
    status_t &mRet;
    AudioPlatformState *mParent;
};

status_t AudioPlatformState::setParameters(const string &keyValuePairs, bool &hasChanged)
{
    Log::Debug() << __FUNCTION__ << ": key value pair " << keyValuePairs;
    KeyValuePairs pairs(keyValuePairs);
    status_t ret = android::OK;
    std::for_each(mParameterVector.begin(), mParameterVector.end(),
                  SetFromAndroidParameterHelper(&pairs, hasChanged, ret, this));
    clearKeys(&pairs);
    if (!hasChanged) {
        return ret;
    }
    // Apply Configuration on route PFW only, any change of Audio PFW must be done
    // within the 5-steps routing.
    getPfw<Route>()->commitCriteriaAndApplyConfiguration();
    // Reset stateChanged criterion
    stageCriterion<Route>(gStateChangedCriterion, 0);
    return ret;
}

void AudioPlatformState::criterionHasChanged(const std::string &event)
{
    if (event == gAndroidModeCriterion) {
        VolumeKeys::wakeup(getPfw<Route>()->getCriterion(event) == AUDIO_MODE_IN_CALL);
    }
    setPlatformStateEvent(event);
}

string AudioPlatformState::getParameters(const string &keys)
{
    KeyValuePairs pairs(keys);
    KeyValuePairs returnedPairs;

    std::for_each(mParameterVector.begin(), mParameterVector.end(),
                  GetFromAndroidParameterHelper(&pairs, &returnedPairs));

    return returnedPairs.toString();
}

void AudioPlatformState::setPlatformStateEvent(const string &eventStateName)
{
    uint32_t stateChanged = getPfw<Route>()->getCriterion(gStateChangedCriterion);
    int eventId;
    // Checks if eventStateName is a possible value of StateChanged criterion
    if (!getPfw<Route>()->getNumericalValue(gStateChangedCriterion, eventStateName, eventId)) {
        return;
    }
    stageCriterion<Route>(gStateChangedCriterion, stateChanged | eventId);
}

void AudioPlatformState::printPlatformFwErrorInfo() const
{
    Log::Error() << "^^^^  Print platform Audio firmware error info  ^^^^";

    string paramValue;

    /**
     * Get the list of files path we wish to print. This list is represented as a
     * string defined in the route manager RouteDebugFs plugin.
     */
    CParameterHandle *handle = getPfw<Route>()->getDynamicParameterHandle(mHwDebugFilesPathList);
    if (handle == NULL) {
        Log::Error() << "Could not get path list from XML configuration";
        return;
    }
    string error;
    if (handle->getAsString(paramValue, error)) {
        Log::Error() << "Unable to get value: " << error
                     << ", from parameter path: " << handle->getPath();
        return;
    }

    vector<std::string> debugFiles;
    char *debugFile;
    string debugFileString;
    char *tokenString = static_cast<char *>(alloca(paramValue.length() + 1));
    vector<std::string>::const_iterator it;

    strncpy(tokenString, paramValue.c_str(), paramValue.length() + 1);

    while ((debugFile = NaiveTokenizer::getNextToken(&tokenString)) != NULL) {
        debugFileString = string(debugFile);
        debugFileString = debugFile;
        debugFiles.push_back(debugFileString);
    }

    for (it = debugFiles.begin(); it != debugFiles.end(); ++it) {
        ifstream debugStream;
        Log::Error() << "Opening file " << *it << " and reading it.";
        debugStream.open(it->c_str(), ifstream::in);

        if (debugStream.fail()) {
            Log::Error() << __FUNCTION__ << ": Unable to open file" << *it
                         << " with failbit "
                         << static_cast<int>(debugStream.rdstate() & ifstream::failbit)
                         << " and badbit "
                         << static_cast<int>(debugStream.rdstate() & ifstream::badbit);
            debugStream.close();
            continue;
        }

        while (debugStream.good()) {
            char dataToRead[mMaxDebugStreamSize];

            debugStream.read(dataToRead, mMaxDebugStreamSize);
            Log::Error() << dataToRead;
        }

        debugStream.close();
    }
}

} // namespace intel_audio
