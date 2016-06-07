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
#include <utility>
#include <vector>

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
    static const std::vector<std::pair<uint32_t, uint32_t> > mSupportedConversions;

public:
    /**
     * Constructor of the remapper.
     *
     * @param[in] sampleSpecItem Sample specification item on which this audio
     *            converter is working on.
     */
    AudioRemapper(SampleSpecItem sampleSpecItem);

    static bool supportRemap(uint32_t srcChannels, uint32_t dstChannels);

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
     * Remap simply from M-channels to N-channels in typed format.
     *
     * Convert a multi N-channels source in a mutli M-channels destination in typed format
     * by averaging the source and propagating the averaged value on all channels of the destination
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
    android::status_t convertMultiNToMultiM(const void *src, void *dst, const size_t inFrames,
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
