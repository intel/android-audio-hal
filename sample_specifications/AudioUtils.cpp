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
#define LOG_TAG "AudioUtils"

#include "AudioUtils.h"
#include "AudioUtils.h"
#include "SampleSpec.h"
#include "SampleSpec.h"
#include <AudioCommsAssert.hpp>
#include <cerrno>
#include <convert.hpp>
#include <hardware_legacy/AudioSystemLegacy.h>
#include <AudioCommsAssert.hpp>
#include <limits.h>
#include <limits>
#include <stdint.h>
#include <stdlib.h>
#include <system/audio.h>
#include <utils/Log.h>

using namespace android;
using namespace std;
using audio_comms::utilities::convertTo;

namespace android_audio_legacy
{

uint32_t AudioUtils::alignOn16(uint32_t u)
{
    AUDIOCOMMS_ASSERT((u / FRAME_ALIGNEMENT_ON_16) <=
                      (numeric_limits<uint32_t>::max() / FRAME_ALIGNEMENT_ON_16),
                      "value to align exceeding limit");
    return (u + (FRAME_ALIGNEMENT_ON_16 - 1)) & ~(FRAME_ALIGNEMENT_ON_16 - 1);
}

size_t AudioUtils::convertSrcToDstInBytes(size_t bytes,
                                          const SampleSpec &ssSrc,
                                          const SampleSpec &ssDst)
{
    return ssDst.convertFramesToBytes(convertSrcToDstInFrames(ssSrc.convertBytesToFrames(bytes),
                                                              ssSrc,
                                                              ssDst));
}

size_t AudioUtils::convertSrcToDstInFrames(size_t frames,
                                           const SampleSpec &ssSrc,
                                           const SampleSpec &ssDst)
{
    AUDIOCOMMS_ASSERT(ssSrc.getSampleRate() != 0, "Sample Rate not set");
    AUDIOCOMMS_COMPILE_TIME_ASSERT(sizeof(uint64_t) >= (2 * sizeof(ssize_t)));
    int64_t dstFrames = ((uint64_t)frames * ssDst.getSampleRate() + ssSrc.getSampleRate() - 1) /
                        ssSrc.getSampleRate();
    AUDIOCOMMS_ASSERT(dstFrames <= numeric_limits<ssize_t>::max(),
                      "conversion exceeding limit");
    return dstFrames;
}

audio_format_t AudioUtils::convertTinyToHalFormat(pcm_format format)
{
    audio_format_t convFormat;

    switch (format) {

    case PCM_FORMAT_S16_LE:
        convFormat = AUDIO_FORMAT_PCM_16_BIT;
        break;
    case PCM_FORMAT_S32_LE:
        convFormat = AUDIO_FORMAT_PCM_8_24_BIT;
        break;
    default:
        ALOGE("%s: format not recognized", __FUNCTION__);
        convFormat = AUDIO_FORMAT_INVALID;
        break;
    }
    return convFormat;
}

pcm_format AudioUtils::convertHalToTinyFormat(audio_format_t format)
{
    pcm_format convFormat;

    switch (format) {

    case AUDIO_FORMAT_PCM_16_BIT:
        convFormat = PCM_FORMAT_S16_LE;
        break;
    case AUDIO_FORMAT_PCM_8_24_BIT:
        convFormat = PCM_FORMAT_S32_LE;
        break;
    default:
        ALOGE("%s: format not recognized", __FUNCTION__);
        convFormat = PCM_FORMAT_S16_LE;
        break;
    }
    return convFormat;
}

int AudioUtils::getCardIndexByName(const char *name)
{
    AUDIOCOMMS_ASSERT(name != NULL, "Null card name");
    char id_filepath[PATH_MAX] = {
        0
    };
    char number_filepath[PATH_MAX] = {
        0
    };
    const int cardLen = strlen("card");

    ssize_t written;

    snprintf(id_filepath, sizeof(id_filepath), "/proc/asound/%s", name);

    written = readlink(id_filepath, number_filepath, sizeof(number_filepath));
    if (written < 0) {

        ALOGE("Sound card %s does not exist", name);
        return -errno;
    } else if (written >= (ssize_t)sizeof(id_filepath)) {

        // This will probably never happen
        return -ENAMETOOLONG;
    } else if (written <= cardLen) {

        // Waiting at least card length + index of the card
        return -EBADFD;
    }

    int indexCard = 0;
    if (!convertTo<int>(number_filepath + cardLen, indexCard)) {

        return -EINVAL;
    }
    return indexCard;
}

uint32_t AudioUtils::convertUsecToMsec(uint32_t timeUsec)
{
    // Round up to the nearest Msec
    return (static_cast<uint64_t>(timeUsec) + USEC_PER_MSEC - 1) / USEC_PER_MSEC;
}

bool AudioUtils::isAudioInputDevice(uint32_t devices)
{
    return (popcount(devices) == 1) && ((devices & ~AudioSystem::DEVICE_IN_ALL) == 0);
}
}  // namespace android
