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
#define LOG_TAG "StreamManager/ParameterHandler"

#include "AudioParameterHandler.hpp"
#include <utilities/Log.hpp>
#include <utils/Errors.h>
#include <cstring>

using android::status_t;
using namespace std;
using audio_comms::utilities::Log;

namespace intel_audio
{

const char AudioParameterHandler::mFilePath[] = "/mnt/asec/media/audio_param.dat";
const int AudioParameterHandler::mReadBufSize = 500;

AudioParameterHandler::AudioParameterHandler()
{
    restore();
}

status_t AudioParameterHandler::saveParameters(const string &keyValuePairs)
{
    add(keyValuePairs);
    return save();
}

void AudioParameterHandler::add(const string &keyValuePairs)
{
    mPairs.add(keyValuePairs);
}

status_t AudioParameterHandler::save()
{
    FILE *fp = fopen(mFilePath, "w+");
    if (!fp) {
        Log::Error() << __FUNCTION__ << ": error " << strerror(errno);
        return android::UNKNOWN_ERROR;
    }

    string param = mPairs.toString();

    size_t ret = fwrite(param.c_str(), sizeof(char), param.length(), fp);
    fclose(fp);

    return ret != param.length() ? android::UNKNOWN_ERROR : android::OK;
}

status_t AudioParameterHandler::restore()
{
    FILE *fp = fopen(mFilePath, "r");
    if (!fp) {
        Log::Error() << __FUNCTION__ << ": error " << strerror(errno);
        return android::UNKNOWN_ERROR;
    }
    char str[mReadBufSize];
    size_t readSize = fread(str, sizeof(char), mReadBufSize - 1, fp);
    fclose(fp);
    if (readSize == 0) {

        return android::UNKNOWN_ERROR;
    }
    str[readSize] = '\0';

    add(str);
    return android::OK;
}

string AudioParameterHandler::getParameters() const
{
    return mPairs.toString();
}
}
