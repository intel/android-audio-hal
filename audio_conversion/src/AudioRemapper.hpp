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

class AudioRemapper : public AudioConverter
{
private:
    enum Channel
    {

        Left = 0,
        Right,
        BackLeft,
        BackRight
    };
    static const size_t mono = 1;
    static const size_t stereo = 2;
    static const size_t quad = 4;

public:
    /**
     * Constructor of the remapper.
     *
     * @param[in] sampleSpecItem Sample specification item on which this audio
     *            converter is working on.
     */
    AudioRemapper(SampleSpecItem sampleSpecItem);

private:
    /**
     * Configures the remapper.
     *
     * Selects the appropriate remap operation to use according to the source
     * and destination sample specifications.
     *
     * @param[in] ssSrc the source sample specifications.
     * @param[in] ssDst the destination sample specifications.
     *
     * @return error code.
     */
    virtual android::status_t configure(const SampleSpec &ssSrc, const SampleSpec &ssDst);

    /**
     * Configure the remapper.
     *
     * Selects the appropriate remap operation to use according to the source
     * and destination sample specifications.
     *
     * @tparam type Audio data format from S16 to S32.
     *
     * @return error code.
     */
    template <typename type>
    android::status_t configure();

    /**
     * Remap from X channels to mono in typed format.
     *
     * Convert a stereo source into a mono destination in typed format.
     *
     * @tparam type Audio data format from S16 to S32, no other type allowed.
     * @param[in] src the source buffer.
     * @param[out] dst the destination buffer, the caller must ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return error code.
     */
    template <typename type>
    android::status_t convertMultiToMono(const void *src,
                                         void *dst,
                                         const size_t inFrames,
                                         size_t *outFrames);

    /**
     * Remap from mono to X channels in typed format.
     *
     * Convert a mono source into a stereo destination in typed format.
     *
     * @tparam type Audio data format from S16 to S32, no other type allowed.
     * @param[in] src the source buffer.
     * @param[out] dst the destination buffer, the caller must ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return error code.
     */
    template <typename type>
    android::status_t convertMonoToMulti(const void *src,
                                         void *dst,
                                         const size_t inFrames,
                                         size_t *outFrames);

    template <typename type>
    android::status_t convertStereoToQuad(const void *src,
                                          void *dst,
                                          const size_t inFrames,
                                          size_t *outFrames);

    template <typename type>
    android::status_t convertQuadToStereo(const void *src,
                                          void *dst,
                                          const size_t inFrames,
                                          size_t *outFrames);

    /**
     * Remap channels policy in typed format.
     *
     * Convert a stereo source into a stereo destination in typed format
     * with different channels policy.
     *
     * @tparam type Audio data format from S16 to S32, no other type allowed.
     * @param[in] src the source buffer.
     * @param[out] dst the destination buffer, the caller must ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return error code.
     */
    template <typename type>
    android::status_t convertChannelsPolicyInStereo(const void *src,
                                                    void *dst,
                                                    const size_t inFrames,
                                                    size_t *outFrames);

    /**
     * Convert a source sample in typed format.
     *
     * Gets destination channel from the source sample according to the destination
     * channel policy.
     *
     * @tparam type Audio data format from S16 to S32, no other type allowed.
     * @param[in] src16 the source frame.
     * @param[in] channel the channel of the destination.
     *
     * @return destination channel sample.
     */
    template <typename type>
    type convertSample(const type *src, Channel channel) const;

    /**
     * Average source frame in typed format.
     *
     * Gets an averaged value of the source audio frame taking into
     * account the policy of the source channels.
     *
     * @tparam type Audio data format from S16 to S32, no other type allowed.
     * @param[in] src16 the source frame.
     *
     * @return destination channel sample.
     */
    template <typename type>
    type getAveragedSrcFrame(const type *src16) const;

    /**
     * provide a compile time error if no specialization is provided for a given type.
     *
     * @tparam T: type of the audio data.
     */
    template <typename T>
    struct formatSupported;
};
}  // namespace intel_audio
