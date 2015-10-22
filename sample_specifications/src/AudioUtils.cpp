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
#define LOG_TAG "AudioUtils"

#include "AudioUtils.hpp"
#include "SampleSpec.hpp"
#include <AudioCommsAssert.hpp>
#include <cerrno>
#include <convert.hpp>
#include <AudioCommsAssert.hpp>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <hardware/audio.h>
#include <utilities/Log.hpp>

using namespace std;
using audio_comms::utilities::convertTo;
using audio_comms::utilities::Log;

namespace intel_audio
{

uint32_t AudioUtils::alignOn16(uint32_t u)
{
    AUDIOCOMMS_ASSERT((u / mFrameAlignementOn16) <=
                      (numeric_limits<uint32_t>::max() / mFrameAlignementOn16),
                      "value to align exceeding limit");
    return (u + (mFrameAlignementOn16 - 1)) & ~(mFrameAlignementOn16 - 1);
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
    AUDIOCOMMS_ASSERT(ssSrc.getSampleRate() != 0, "Source Sample Rate not set");
    AUDIOCOMMS_ASSERT(ssDst.getSampleRate() != 0, "Destination Sample Rate not set");
    AUDIOCOMMS_ASSERT(frames < (numeric_limits<uint64_t>::max() / ssDst.getSampleRate()),
                      "Overflow detected");
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
    case PCM_FORMAT_S24_LE:
        convFormat = AUDIO_FORMAT_PCM_8_24_BIT;
        break;
    default:
        Log::Error() << __FUNCTION__ << ": format not recognized";
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
        convFormat = PCM_FORMAT_S24_LE; /* PCM_FORMAT_S24_LE is 24-bits in 4-bytes */
        break;
    default:
        Log::Error() << __FUNCTION__ << ": format not recognized";
        convFormat = PCM_FORMAT_S16_LE;
        break;
    }
    return convFormat;
}

int AudioUtils::getCardIndexByName(const char *name)
{
    if (name == NULL) {
        Log::Error() << __FUNCTION__ << ": invalid card name";
        return -1;
    }
    char cardFilePath[PATH_MAX] = {
        0
    };
    char cardNameWithIndex[NAME_MAX] = {
        0
    };
    const char *const card = "card";
    const int cardLen = strlen(card);

    if (!strncmp(name, card, cardLen)) {

        /**
         * Checks first if the card name provided as "cardX". (Must start with 'card').
         */
        snprintf(cardNameWithIndex, sizeof(cardNameWithIndex), "%s", name);
    } else {

        /**
         * The card name might have been provided with user friendly name.
         * To retrieve the index, checks first if this name matches an entry in /proc/asound.
         * This entry, if exists, must be a symbolic link on the real audio card known as "cardX".
         */
        ssize_t written;

        snprintf(cardFilePath, sizeof(cardFilePath), "/proc/asound/%s", name);

        written = readlink(cardFilePath, cardNameWithIndex, sizeof(cardNameWithIndex));
        if (written < 0) {
            Log::Error() << "Sound card " << name << " does not exist";
            return -errno;
        } else if (written >= (ssize_t)sizeof(cardNameWithIndex)) {

            // This will probably never happen
            return -ENAMETOOLONG;
        } else if (written <= cardLen) {

            // Waiting at least card length + index of the card
            return -EBADFD;
        }
    }

    uint32_t indexCard = 0;
    if (!convertTo<string, uint32_t>(cardNameWithIndex + cardLen, indexCard)) {

        return -EINVAL;
    }
    return indexCard;
}

static int compressDeviceFilter(const struct dirent *entry)
{
    static const char *const compressDevicePrefix = "comprC";
    return !strncmp(entry->d_name, compressDevicePrefix, strlen(compressDevicePrefix));
}

int AudioUtils::getCompressDeviceIndex()
{
    struct dirent **fileList;
    int count = scandir("/dev/snd", &fileList, compressDeviceFilter, NULL);
    if (count <= 0) {
        Log::Error() << __FUNCTION__ << ": no compressed devices found";
        return -ENODEV;
    } else if (count > 1) {
        Log::Verbose() << __FUNCTION__ << ": multiple (" << count
                       << ") compressed devices found, using first one";
    }
    const char *devName = fileList[0]->d_name;
    Log::Verbose() << __FUNCTION__ << ": compressed device node: " << devName;

    int dev;
    static const char *compressDevice = "comprCxD";
    if (!convertTo<std::string, int>(devName + strlen(compressDevice), dev)) {
        return -ENODEV;
    }
    return dev;
}

uint32_t AudioUtils::convertUsecToMsec(uint32_t timeUsec)
{
    // Round up to the nearest Msec
    return (static_cast<uint64_t>(timeUsec) + mUsecPerMsec - 1) / mUsecPerMsec;
}

}  // namespace intel_audio
