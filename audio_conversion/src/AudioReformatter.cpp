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


#define LOG_TAG "AudioReformatter"

#include "AudioReformatter.hpp"
#include <utilities/Log.hpp>
#include <utility>
#include <vector>

using audio_comms::utilities::Log;
using namespace android;

namespace intel_audio
{

static const std::vector<std::pair<audio_format_t, audio_format_t> > mSupportedConversions = {
    { AUDIO_FORMAT_PCM_16_BIT, AUDIO_FORMAT_PCM_8_24_BIT },
    { AUDIO_FORMAT_PCM_16_BIT, AUDIO_FORMAT_PCM_32_BIT },
    { AUDIO_FORMAT_PCM_8_24_BIT, AUDIO_FORMAT_PCM_16_BIT },
    { AUDIO_FORMAT_PCM_32_BIT, AUDIO_FORMAT_PCM_16_BIT }
};


const uint32_t AudioReformatter::mReformatterShiftLeft16 = 16;
const uint32_t AudioReformatter::mReformatterShiftRight8 = 8;

AudioReformatter::AudioReformatter(SampleSpecItem sampleSpecItem)
    : AudioConverter(sampleSpecItem)
{
}

bool AudioReformatter::supportReformat(audio_format_t srcFormat, audio_format_t dstFormat)
{
    for (auto &candidate : mSupportedConversions) {
        if (candidate.first == srcFormat && dstFormat == candidate.second) {
            return true;
        }
    }
    return false;
}

status_t AudioReformatter::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    status_t status = AudioConverter::configure(ssSrc, ssDst);
    if (status != NO_ERROR) {
        return status;
    }
    if (not supportReformat(ssSrc.getFormat(), ssDst.getFormat())) {
        Log::Error() << __FUNCTION__ << ": reformatter not available";
        return INVALID_OPERATION;
    }

    switch (ssSrc.getFormat()) {
    case AUDIO_FORMAT_PCM_16_BIT:
        if (ssDst.getFormat() == AUDIO_FORMAT_PCM_8_24_BIT) {
            mConvertSamplesFct =
                static_cast<SampleConverter>(&AudioReformatter::convertS16toS24over32);
            return OK;
        } else if (ssDst.getFormat() == AUDIO_FORMAT_PCM_32_BIT) {
            mConvertSamplesFct =
                static_cast<SampleConverter>(&AudioReformatter::convertS16toS32);
            return OK;
        }
        return INVALID_OPERATION;
    case AUDIO_FORMAT_PCM_8_24_BIT:
        if (ssDst.getFormat() == AUDIO_FORMAT_PCM_16_BIT) {
            mConvertSamplesFct =
                static_cast<SampleConverter>(&AudioReformatter::convertS24over32toS16);
            return OK;
        }
        return INVALID_OPERATION;
    case AUDIO_FORMAT_PCM_32_BIT:
        if (ssDst.getFormat() == AUDIO_FORMAT_PCM_16_BIT) {
            mConvertSamplesFct =
                static_cast<SampleConverter>(&AudioReformatter::convertS32toS16);
            return OK;
        }
    default:
        return INVALID_OPERATION;
    }
}

status_t AudioReformatter::convertS16toS24over32(const void *src,
                                                 void *dst,
                                                 const size_t inFrames,
                                                 size_t *outFrames)
{
    size_t frameIndex;
    const int16_t *src16 = (const int16_t *)src;
    uint32_t *dst32 = (uint32_t *)dst;
    size_t n = inFrames * mSsSrc.getChannelCount();

    for (frameIndex = 0; frameIndex < n; frameIndex++) {

        *(dst32 + frameIndex) = (uint32_t)((int32_t)*(src16 + frameIndex) <<
                                           mReformatterShiftLeft16) >> mReformatterShiftRight8;
    }

    // Transformation is "iso" frames
    *outFrames = inFrames;

    return NO_ERROR;
}

status_t AudioReformatter::convertS24over32toS16(const void *src,
                                                 void *dst,
                                                 const size_t inFrames,
                                                 size_t *outFrames)
{
    const uint32_t *src32 = (const uint32_t *)src;
    int16_t *dst16 = (int16_t *)dst;
    size_t frameIndex;

    size_t n = inFrames * mSsSrc.getChannelCount();

    for (frameIndex = 0; frameIndex < n; frameIndex++) {

        *(dst16 + frameIndex) = (int16_t)(((int32_t)(*(src32 + frameIndex)) <<
                                           mReformatterShiftRight8) >> mReformatterShiftLeft16);
    }

    // Transformation is "iso" frames
    *outFrames = inFrames;

    return NO_ERROR;
}

status_t AudioReformatter::convertS16toS32(const void *src, void *dst, const size_t inFrames,
                                           size_t *outFrames)
{
    const int16_t *src16 = (const int16_t *)src;
    uint32_t *dst32 = (uint32_t *)dst;

    for (size_t frameIndex = 0; frameIndex < inFrames * mSsSrc.getChannelCount(); frameIndex++) {
        *(dst32 + frameIndex) = (uint32_t)((int32_t)*(src16 + frameIndex) <<
                                           mReformatterShiftLeft16);
    }
    // Transformation is "iso" frames
    *outFrames = inFrames;
    return NO_ERROR;
}

status_t AudioReformatter::convertS32toS16(const void *src, void *dst, const size_t inFrames,
                                           size_t *outFrames)
{
    const uint32_t *src32 = (const uint32_t *)src;
    int16_t *dst16 = (int16_t *)dst;

    for (size_t frameIndex = 0; frameIndex < inFrames * mSsSrc.getChannelCount(); frameIndex++) {
        *(dst16 + frameIndex) = (int16_t)(((int32_t)(*(src32 + frameIndex))) >>
                                          mReformatterShiftLeft16);
    }
    // Transformation is "iso" frames
    *outFrames = inFrames;
    return NO_ERROR;
}
}  // namespace intel_audio
