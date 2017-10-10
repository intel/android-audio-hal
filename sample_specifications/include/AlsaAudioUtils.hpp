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
#pragma once

#include <alsa/asoundlib.h>
#include <hardware/audio.h>
#include <utilities/Log.hpp>


namespace intel_audio
{

class AlsaAudioUtils
{
public:
    /**
     * Converts a format from alsa to HAL domains.
     *
     * @param[in] format in alsa domain.
     *
     * @return format in HAL (aka Android) domain.
     *         It returns AUDIO_FORMAT_INVALID in case of unsupported alsa format.
     */
    static audio_format_t convertAlsaToHalFormat(snd_pcm_format_t format);

    /**
     * Converts a format from HAL to alsa domains.
     *
     * @param[in] format in HAL domain.
     *
     * @return format in tiny alsa domain.
     *         It returns SND_PCM_FORMAT_S16_LE format in case of unrecognized alsa format.
     */
    static snd_pcm_format_t convertHalToAlsaFormat(audio_format_t format);
};
}  // namespace intel_audio
