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
#include <audio_utils/resampler.h>
#include <list>

namespace intel_audio
{

class AudioResampler : public AudioConverter
{

public:
    /**
     * Class constructor.
     *
     * @param[in] Reference sample specification.
     */
    AudioResampler(SampleSpecItem sampleSpecItem);

    virtual ~AudioResampler();

    static bool supportResample(uint32_t /*srcRate*/, uint32_t /*dstRate*/) { return true; }

private:
    /**
     * Configures the resampler.
     * It configures the resampler that may be used to convert samples from the source
     * to destination sample rate, with option 'RESAMPLER_QUALITY_DEFAULT'.
     *
     * @param[in] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specification.
     *
     * @return status OK, error code otherwise.
     */
    virtual android::status_t configure(const SampleSpec &ssSrc, const SampleSpec &ssDst);

    /**
     * Resamples buffer from source to destination sample rate.
     * Resamples input frames of the provided input buffer into the destination buffer already
     * allocated by the converter or given by the client.
     * Before using this function, configure must have been called.
     *
     * @param[in] src the source buffer.
     * @param[out] dst the destination buffer, caller to ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return error code.
     */
    android::status_t resampleFrames(const void *src,
                                     void *dst,
                                     const size_t inFrames,
                                     size_t *outFrames);

    struct resampler_itfe *mResampler;

};
}  // namespace intel_audio
