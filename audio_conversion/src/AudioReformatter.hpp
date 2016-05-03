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

#include "AudioConverter.hpp"

namespace intel_audio
{

class AudioReformatter : public AudioConverter
{

public:
    AudioReformatter(SampleSpecItem sampleSpecItem);

    static bool supportReformat(audio_format_t srcFormat, audio_format_t dstFormat);

private:
    /**
     * Configures the context of reformatting operation to do.
     *
     * Sets the reformatting operation to do, based on destination sample spec as well
     * as source sample spec.
     *
     * @param[in] ssSrc Source sample spec to reformat.
     * @param[in] ssDst Targeted sample spec.
     *
     * @return status NO_ERROR is configuration is successful, error code otherwise.
     */
    virtual android::status_t configure(const SampleSpec &ssSrc, const SampleSpec &ssDst);

    /**
     * Converts (Reformats) audio samples.
     *
     * Reformatting is made from signed 16-bits depth format to signed 24-bits depth format.
     *
     * @param[in]  src Source buffer containing audio samples to reformat.
     * @param[out] dst Destination buffer for reformatted audio samples.
     * @param[in]  inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return status NO_ERROR is always returned.
     */
    android::status_t convertS16toS24over32(const void *src,
                                            void *dst,
                                            const size_t inFrames,
                                            size_t *outFrames);

    /**
     * Converts (Reformats) audio samples.
     *
     * Reformatting is made from signed 16-bits depth format to signed 32-bits depth format.
     * S32 bit is in fact a S24 stored on 32 bits and Left Justified.
     *
     * @param[in]  src Source buffer containing audio samples to reformat.
     * @param[out] dst Destination buffer for reformatted audio samples.
     * @param[in]  inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return status NO_ERROR is always returned.
     */
    android::status_t convertS16toS32(const void *src,
                                      void *dst,
                                      const size_t inFrames,
                                      size_t *outFrames);

    /**
     * Converts (Reformats) audio samples.
     *
     * Reformatting is made from signed 24-bits depth format to signed 16-bits depth format.
     *
     * @param[in]  src Source buffer containing audio samples to reformat.
     * @param[out] dst Destination buffer for reformatted audio samples.
     * @param[in]  inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return status NO_ERROR is always returned.
     */
    android::status_t convertS24over32toS16(const void *src,
                                            void *dst,
                                            const size_t inFrames,
                                            size_t *outFrames);

    /**
     * Converts (Reformats) audio samples.
     *
     * Reformatting is made from signed 32-bits depth format to signed 16-bits depth format.
     * S32 bit is in fact a S24 stored on 32 bits and Left Justified.
     *
     * @param[in]  src Source buffer containing audio samples to reformat.
     * @param[out] dst Destination buffer for reformatted audio samples.
     * @param[in]  inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return status NO_ERROR is always returned.
     */
    android::status_t convertS32toS16(const void *src,
                                      void *dst,
                                      const size_t inFrames,
                                      size_t *outFrames);

    /**
     * Used to do 8-bits right shitfs during reformatting operation.
     */
    static const uint32_t mReformatterShiftRight8;

    /**
     * Used to do 16-bits left shitfs during reformatting operation.
     */
    static const uint32_t mReformatterShiftLeft16;
};
}  // namespace intel_audio
