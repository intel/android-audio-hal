/*
 * Copyright (C) 2017 Intel Corporation
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
#define LOG_TAG "AlsaAudioUtils"

#include "AlsaAudioUtils.hpp"
#include <hardware/audio.h>

using audio_comms::utilities::Log;

namespace intel_audio
{

audio_format_t AlsaAudioUtils::convertAlsaToHalFormat(snd_pcm_format_t format)
{
    audio_format_t convFormat;

    switch (format) {
    case SND_PCM_FORMAT_S16_LE:
        convFormat = AUDIO_FORMAT_PCM_16_BIT;
        break;
    case SND_PCM_FORMAT_S24_LE:
        convFormat = AUDIO_FORMAT_PCM_8_24_BIT;
        break;
    case SND_PCM_FORMAT_S32_LE:
        convFormat = AUDIO_FORMAT_PCM_32_BIT;
        break;
    default:
        Log::Error() << __FUNCTION__ << ": format not recognized";
        convFormat = AUDIO_FORMAT_INVALID;
        break;
    }
    return convFormat;
}

snd_pcm_format_t AlsaAudioUtils::convertHalToAlsaFormat(audio_format_t format)
{
    snd_pcm_format_t convFormat;

    switch (format) {
    case AUDIO_FORMAT_PCM_16_BIT:
        convFormat = SND_PCM_FORMAT_S16_LE;
        break;
    case AUDIO_FORMAT_PCM_8_24_BIT:
        convFormat = SND_PCM_FORMAT_S24_LE; /* SND_PCM_FORMAT_S24_LE is 24-bits in 4-bytes */
        break;
    case AUDIO_FORMAT_PCM_32_BIT:
        convFormat = SND_PCM_FORMAT_S32_LE;
        break;
    default:
        Log::Error() << __FUNCTION__ << ": format not recognized";
        convFormat = SND_PCM_FORMAT_S16_LE;
        break;
    }
    return convFormat;
}

}  // namespace intel_audio
