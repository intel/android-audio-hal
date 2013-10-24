/*
 * INTEL CONFIDENTIAL
 * Copyright © 2013 Intel
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
 * disclosed in any way without Intel’s prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#define LOG_TAG "StreamManager/ParameterHandler"

#include "AudioParameterHandler.h"
#include <media/AudioParameter.h>
#include <utils/Log.h>
#include <utils/String8.h>
#include <utils/Errors.h>

using namespace android;

namespace android_audio_legacy
{

const char AudioParameterHandler::FILE_PATH[] = "/mnt/asec/media/audio_param.dat";
const int AudioParameterHandler::READ_BUF_SIZE = 500;

AudioParameterHandler::AudioParameterHandler()
{
    restore();
}

status_t AudioParameterHandler::saveParameters(const String8 &keyValuePairs)
{
    add(keyValuePairs);

    return save();
}

void AudioParameterHandler::add(const String8 &keyValuePairs)
{
    AudioParameter newParameters(keyValuePairs);
    uint32_t parameter;

    for (parameter = 0; parameter < newParameters.size(); parameter++) {

        String8 key, value;

        // Retrieve new parameter
        newParameters.getAt(parameter, key, value);

        // Add / merge it with stored ones
        _audioParameter.add(key, value);
    }
}

status_t AudioParameterHandler::save()
{
    FILE *fp = fopen(FILE_PATH, "w+");
    if (!fp) {

        ALOGE("%s: error %s", __FUNCTION__, strerror(errno));
        return UNKNOWN_ERROR;
    }

    String8 param = _audioParameter.toString();

    size_t ret = fwrite(param.string(), sizeof(char), param.length(), fp);
    fclose(fp);

    return ret != param.length() ? UNKNOWN_ERROR : NO_ERROR;
}

status_t AudioParameterHandler::restore()
{
    FILE *fp = fopen(FILE_PATH, "r");
    if (!fp) {

        ALOGE("%s: error %s", __FUNCTION__, strerror(errno));
        return UNKNOWN_ERROR;
    }
    char str[READ_BUF_SIZE];
    size_t readSize = fread(str, sizeof(char), READ_BUF_SIZE - 1, fp);
    fclose(fp);
    if (readSize == 0) {

        return UNKNOWN_ERROR;
    }
    str[readSize] = '\0';

    add(String8(str));
    return NO_ERROR;
}

String8 AudioParameterHandler::getParameters() const
{
    return _audioParameter.toString();
}
}
