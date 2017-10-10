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
#define LOG_TAG "AudioIntelHal/AudioPlatformState"

#include "AudioPlatformState.hpp"
#include "VolumeKeys.hpp"
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
#include <system/audio.h>

using namespace std;
using audio_comms::utilities::convertTo;
using android::status_t;
using audio_comms::utilities::Log;
using audio_comms::utilities::Property;

static const std::string gAndroidModeCriterion = "AndroidMode";

namespace intel_audio
{

template <>
Pfw<PfwTrait<Audio> > *AudioPlatformState::getPfw()
{
    return mAudioPfw;
}
template <>
Pfw<PfwTrait<Audio> > *AudioPlatformState::getPfw() const
{
    return mAudioPfw;
}

const std::string AudioPlatformState::mHwDebugFilesPathList =
    "/Audio/debug_fs/debug_files/path_list/";

// For debug purposes. This size is enough for dumping relevant informations
const uint32_t AudioPlatformState::mMaxDebugStreamSize = 998;

AudioPlatformState::AudioPlatformState() : mAudioPfw(new Pfw<PfwTrait<Audio> >())
{
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
}

status_t AudioPlatformState::start()
{
    if (getPfw<Audio>()->start() != android::OK) {
        return android::NO_INIT;
    }
    sync();
    return android::OK;
}

void AudioPlatformState::sync()
{
    std::for_each(mParameterVector.begin(), mParameterVector.end(), SyncParameterHelper());
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
            std::string oldValue;
            param->getValue(oldValue);

            if (!param->setValue(value)) {
                if (value.compare(oldValue) != 0) {
                    mRet = android::BAD_VALUE;
                }
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
    return ret;
}

void AudioPlatformState::criterionHasChanged(const std::string &event)
{
    if (event == gAndroidModeCriterion) {
        VolumeKeys::wakeup(getPfw<Audio>()->getCriterion(event) == AUDIO_MODE_IN_CALL);
    }
}

string AudioPlatformState::getParameters(const string &keys)
{
    KeyValuePairs pairs(keys);
    KeyValuePairs returnedPairs;

    std::for_each(mParameterVector.begin(), mParameterVector.end(),
                  GetFromAndroidParameterHelper(&pairs, &returnedPairs));

    return returnedPairs.toString();
}

void AudioPlatformState::printPlatformFwErrorInfo() const
{
    Log::Error() << "^^^^  Print platform Audio firmware error info  ^^^^";

    string paramValue;

    /**
     * Get the list of files path we wish to print. This list is represented as a
     * string defined in the route manager DebugFs plugin.
     */
    CParameterHandle *handle = getPfw<Audio>()->getDynamicParameterHandle(mHwDebugFilesPathList);
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
