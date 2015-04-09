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


#define LOG_TAG "AudioRemapper"

#include "AudioRemapper.hpp"

using namespace android;

namespace intel_audio
{

template <>
struct AudioRemapper::formatSupported<int16_t> {};
template <>
struct AudioRemapper::formatSupported<uint32_t> {};

AudioRemapper::AudioRemapper(SampleSpecItem sampleSpecItem)
    : AudioConverter(sampleSpecItem)
{
}

status_t AudioRemapper::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    status_t ret = AudioConverter::configure(ssSrc, ssDst);
    if (ret != NO_ERROR) {

        return ret;
    }

    switch (ssSrc.getFormat()) {

    case AUDIO_FORMAT_PCM_16_BIT:

        ret = configure<int16_t>();
        break;

    case AUDIO_FORMAT_PCM_8_24_BIT:

        ret = configure<uint32_t>();
        break;

    default:

        ret = INVALID_OPERATION;
        break;
    }

    return ret;
}


template <typename type>
android::status_t AudioRemapper::configure()
{
    formatSupported<type>();

    if (mSsSrc.isMono() && mSsDst.isStereo()) {

        mConvertSamplesFct =
            static_cast<SampleConverter>(&AudioRemapper::convertMonoToStereo<type> );
    } else if (mSsSrc.isStereo() && mSsDst.isMono()) {

        mConvertSamplesFct =
            static_cast<SampleConverter>(&AudioRemapper::convertStereoToMono<type> );
    } else if (mSsSrc.isStereo() && mSsDst.isStereo()) {

        // Iso channel, checks the channels policy
        if (!SampleSpec::isSampleSpecItemEqual(ChannelCountSampleSpecItem, mSsSrc, mSsDst)) {

            mConvertSamplesFct =
                static_cast<SampleConverter>(&AudioRemapper::convertChannelsPolicyInStereo<type> );
        }
    } else {

        return INVALID_OPERATION;
    }

    return OK;
}

template <typename type>
status_t AudioRemapper::convertStereoToMono(const void *src,
                                            void *dst,
                                            const size_t inFrames,
                                            size_t *outFrames)
{
    const type *srcTyped = static_cast<const type *>(src);
    type *dstTyped = static_cast<type *>(dst);
    uint32_t srcChannels = mSsSrc.getChannelCount();
    size_t frames;

    for (frames = 0; frames < inFrames; frames++) {

        dstTyped[frames] = getAveragedSrcFrame<type>(&srcTyped[srcChannels * frames]);
    }

    // Transformation is "iso" frames
    *outFrames = inFrames;
    return NO_ERROR;
}

template <typename type>
status_t AudioRemapper::convertMonoToStereo(const void *src,
                                            void *dst,
                                            const size_t inFrames,
                                            size_t *outFrames)
{
    const type *srcTyped = static_cast<const type *>(src);
    type *dstTyped = static_cast<type *>(dst);
    size_t frames = 0;
    uint32_t dstChannels = mSsDst.getChannelCount();

    for (frames = 0; frames < inFrames; frames++) {

        uint32_t channels;
        for (channels = 0; channels < dstChannels; channels++) {

            if (mSsDst.getChannelsPolicy(channels) != SampleSpec::Ignore) {

                dstTyped[dstChannels * frames + channels] = srcTyped[frames];
            }
        }
    }

    // Transformation is "iso" frames
    *outFrames = inFrames;
    return NO_ERROR;
}

template <typename type>
status_t AudioRemapper::convertChannelsPolicyInStereo(const void *src,
                                                      void *dst,
                                                      const size_t inFrames,
                                                      size_t *outFrames)
{
    const type *srcTyped = static_cast<const type *>(src);
    size_t frames = 0;
    uint32_t srcChannels = mSsSrc.getChannelCount();

    struct Stereo
    {
        type leftCh;
        type rightCh;
    } *dstTyped = static_cast<Stereo *>(dst);

    for (frames = 0; frames < inFrames; frames++) {

        dstTyped[frames].leftCh = convertSample(&srcTyped[srcChannels * frames], Left);
        dstTyped[frames].rightCh = convertSample(&srcTyped[srcChannels * frames], Right);
    }

    // Transformation is "iso" frames
    *outFrames = inFrames;
    return NO_ERROR;
}


template <typename type>
type AudioRemapper::convertSample(const type *src, Channel channel) const
{
    SampleSpec::ChannelsPolicy dstPolicy = mSsDst.getChannelsPolicy(channel);

    if (dstPolicy == SampleSpec::Ignore) {

        // Destination policy is Ignore, so set to null dest sample
        return 0;
    } else if (dstPolicy == SampleSpec::Average) {

        // Destination policy is average, so average on all channels of the source frame
        return getAveragedSrcFrame<type>(src);
    }

    // Destination policy is Copy
    // so copy only if source channel policy is not ignore
    if (mSsSrc.getChannelsPolicy(channel) != SampleSpec::Ignore) {

        return src[channel];
    }

    // Even if policy is Copy, if the source channel is Ignore,
    // take the average of the other source channels
    return getAveragedSrcFrame<type>(src);
}

template <typename type>
type AudioRemapper::getAveragedSrcFrame(const type *src) const
{
    uint32_t validSrcChannels = 0;
    uint64_t dst = 0;

    // Loops on source channels, checks upon the channel policy to take it into account
    // or not.
    // Average on all valid source channels
    for (uint32_t iSrcChannels = 0; iSrcChannels < mSsSrc.getChannelCount(); iSrcChannels++) {

        if (mSsSrc.getChannelsPolicy(iSrcChannels) != SampleSpec::Ignore) {

            dst += src[iSrcChannels];
            validSrcChannels += 1;
        }
    }

    if (validSrcChannels) {

        dst = dst / validSrcChannels;
    }

    return dst;
}
}  // namespace intel_audio
