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


#define LOG_TAG "AudioRemapper"

#include "AudioRemapper.hpp"
#include <utilities/Log.hpp>

using namespace android;
using audio_comms::utilities::Log;

namespace intel_audio
{

template <>
struct AudioRemapper::formatSupported<int16_t> {};
template <>
struct AudioRemapper::formatSupported<uint32_t> {};

static const size_t mono = 1;
static const size_t stereo = 2;
static const size_t quad = 4;
static const size_t multichan8 = 8;

const std::vector < std::pair < uint32_t, uint32_t >> AudioRemapper::mSupportedConversions = {
    { mono, stereo },
    { mono, quad },
    { mono, multichan8 },
    { stereo, stereo },
    { stereo, mono },
    { stereo, quad },
    { stereo, multichan8 },
    { quad, mono },
    { quad, stereo },
    { multichan8, mono },
    { multichan8, stereo }
};

AudioRemapper::AudioRemapper(SampleSpecItem sampleSpecItem)
    : AudioConverter(sampleSpecItem)
{
}

bool AudioRemapper::supportRemap(uint32_t srcChannels, uint32_t dstChannels)
{
    for (auto &candidate : mSupportedConversions) {
        if (candidate.first == srcChannels && dstChannels == candidate.second) {
            return true;
        }
    }
    return false;
}

status_t AudioRemapper::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    status_t ret = AudioConverter::configure(ssSrc, ssDst);
    if (ret != NO_ERROR) {
        return ret;
    }
    switch (ssSrc.getFormat()) {
    case AUDIO_FORMAT_PCM_16_BIT:
        return configure<int16_t>();
    case AUDIO_FORMAT_PCM_8_24_BIT:
    case AUDIO_FORMAT_PCM_32_BIT:
        return configure<uint32_t>();
    default:
        return INVALID_OPERATION;
    }
}


template <typename type>
android::status_t AudioRemapper::configure()
{
    formatSupported<type>();

    if (not supportRemap(mSsSrc.getChannelCount(), mSsDst.getChannelCount())) {
        Log::Error() << __FUNCTION__ << ": remapper not available";
        return INVALID_OPERATION;
    }

    switch (mSsSrc.getChannelCount()) {
    case mono:
        switch (mSsDst.getChannelCount()) {
        case stereo:
        case quad:
        case multichan8:
            mConvertSamplesFct =
                static_cast<SampleConverter>(&AudioRemapper::convertMultiNToMultiM<type> );
            return OK;
        }
        return INVALID_OPERATION;
    case stereo:
        switch (mSsDst.getChannelCount()) {
        case mono:
            mConvertSamplesFct =
                static_cast<SampleConverter>(&AudioRemapper::convertMultiNToMultiM<type> );
            return OK;
        case stereo:
            // Iso channel, checks the channels policy
            if (!SampleSpec::isSampleSpecItemEqual(ChannelCountSampleSpecItem, mSsSrc, mSsDst)) {

                mConvertSamplesFct =
                    static_cast<SampleConverter>(&AudioRemapper::convertChannelsPolicyInStereo<type> );
                return OK;
            }
            return INVALID_OPERATION;
        case quad:
            mConvertSamplesFct =
                static_cast<SampleConverter>(&AudioRemapper::convertStereoToQuad<type> );
            return OK;
        case multichan8:
            mConvertSamplesFct =
                static_cast<SampleConverter>(&AudioRemapper::convertMultiNToMultiM<type> );
            return OK;
        }
        return INVALID_OPERATION;

    case quad:
        switch (mSsDst.getChannelCount()) {
        case mono:
            mConvertSamplesFct =
                static_cast<SampleConverter>(&AudioRemapper::convertMultiNToMultiM<type> );
            return OK;
        case stereo:
            mConvertSamplesFct =
                static_cast<SampleConverter>(&AudioRemapper::convertQuadToStereo<type> );
            return OK;
        }
    case multichan8:
        switch (mSsDst.getChannelCount()) {
        case mono:
        case stereo:
            mConvertSamplesFct =
                static_cast<SampleConverter>(&AudioRemapper::convertMultiNToMultiM<type> );
            return OK;
        }
    }
    return INVALID_OPERATION;
}

template <typename type>
status_t AudioRemapper::convertMultiNToMultiM(const void *src, void *dst, const size_t inFrames,
                                              size_t *outFrames)
{
    const type *srcTyped = static_cast<const type *>(src);
    type *dstTyped = static_cast<type *>(dst);
    size_t srcChannels = mSsSrc.getChannelCount();
    size_t dstChannels = mSsDst.getChannelCount();

    for (size_t frames = 0; frames < inFrames; frames++) {
        size_t srcIndex = srcChannels * frames;
        size_t dstIndex = dstChannels * frames;

        type averagedSrc = getAveragedSrcFrame<type>(&srcTyped[srcIndex]);

        for (size_t channels = 0; channels < dstChannels; channels++) {
            if (mSsDst.getChannelsPolicy(channels) != SampleSpec::Ignore) {
                dstTyped[dstIndex + channels] = averagedSrc;
            }
        }
    }
    // Transformation is "iso" frames
    *outFrames = inFrames;
    return NO_ERROR;
}

template <typename type>
status_t AudioRemapper::convertStereoToQuad(const void *src, void *dst, const size_t inFrames,
                                            size_t *outFrames)
{
    const type *srcTyped = static_cast<const type *>(src);
    type *dstTyped = static_cast<type *>(dst);
    size_t srcChannels = mSsSrc.getChannelCount();
    size_t dstChannels = mSsDst.getChannelCount();

    for (size_t frames = 0; frames < inFrames; frames++) {
        size_t srcIndex = srcChannels * frames;
        size_t dstIndex = dstChannels * frames;

        dstTyped[dstIndex + Left] = srcTyped[Left + srcIndex];
        dstTyped[dstIndex + Right] = srcTyped[Right + srcIndex];
        dstTyped[dstIndex + BackLeft] = srcTyped[Left + srcIndex];
        dstTyped[dstIndex + BackRight] = srcTyped[Right + srcIndex];
    }
    // Transformation is "iso" frames
    *outFrames = inFrames;
    return NO_ERROR;
}

template <typename type>
status_t AudioRemapper::convertQuadToStereo(const void *src, void *dst, const size_t inFrames,
                                            size_t *outFrames)
{
    const type *srcTyped = static_cast<const type *>(src);
    type *dstTyped = static_cast<type *>(dst);
    size_t srcChannels = mSsSrc.getChannelCount();
    size_t dstChannels = mSsDst.getChannelCount();

    for (size_t frames = 0; frames < inFrames; frames++) {
        size_t srcIndex = srcChannels * frames;
        size_t dstIndex = dstChannels * frames;

        type dstRight = 0;
        size_t validSrcRightChannels = 0;
        if (mSsSrc.getChannelsPolicy(srcIndex + Right) != SampleSpec::Ignore) {
            dstRight += srcTyped[srcIndex + Right];
            validSrcRightChannels++;
        }
        if (mSsSrc.getChannelsPolicy(srcIndex + BackRight) != SampleSpec::Ignore) {
            dstRight += srcTyped[srcIndex + BackRight];
            validSrcRightChannels++;
        }
        if (validSrcRightChannels) {
            dstRight = dstRight / validSrcRightChannels;
        }
        dstTyped[dstIndex + Right] = dstRight;

        type dstLeft = 0;
        size_t validSrcLeftChannels = 0;
        if (mSsSrc.getChannelsPolicy(srcIndex + Left) != SampleSpec::Ignore) {
            dstLeft += srcTyped[srcIndex + Left];
            validSrcLeftChannels++;
        }
        if (mSsSrc.getChannelsPolicy(srcIndex + BackLeft) != SampleSpec::Ignore) {
            dstLeft += srcTyped[srcIndex + BackLeft];
            validSrcLeftChannels++;
        }
        if (validSrcLeftChannels) {
            dstLeft = dstLeft / validSrcLeftChannels;
        }
        dstTyped[dstIndex + Left] = dstLeft;
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
