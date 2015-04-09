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


#define LOG_TAG "AudioConverter"

#include "AudioConverter.hpp"
#include "AudioUtils.hpp"
#include <utilities/Log.hpp>
#include <stdlib.h>

using audio_comms::utilities::Log;
using namespace android;

namespace intel_audio
{

AudioConverter::AudioConverter(SampleSpecItem sampleSpecItem)
    : mConvertSamplesFct(NULL),
      mSsSrc(),
      mSsDst(),
      mConvertBuf(NULL),
      mConvertBufSize(0),
      mSampleSpecItem(sampleSpecItem)
{
}

AudioConverter::~AudioConverter()
{
    delete[] mConvertBuf;
}

//
// This function gets an output buffer suitable
// to convert inFrames input frames
//
void *AudioConverter::getOutputBuffer(ssize_t inFrames)
{
    status_t ret = NO_ERROR;
    size_t outBufSizeInBytes = mSsDst.convertFramesToBytes(convertSrcToDstInFrames(inFrames));

    if (outBufSizeInBytes > mConvertBufSize) {

        ret = allocateConvertBuffer(outBufSizeInBytes);
        if (ret != NO_ERROR) {
            Log::Error() << __FUNCTION__ << ": could not allocate memory for operation";
            return NULL;
        }
    }

    return (void *)mConvertBuf;
}

status_t AudioConverter::allocateConvertBuffer(ssize_t bytes)
{
    status_t ret = NO_ERROR;

    // Allocate one more frame for resampler
    mConvertBufSize = bytes +
                      (audio_bytes_per_sample(mSsDst.getFormat()) * mSsDst.getChannelCount());

    free(mConvertBuf);
    mConvertBuf = NULL;

    mConvertBuf = new char[mConvertBufSize];

    if (!mConvertBuf) {
        Log::Error() << "cannot allocate resampler tmp buffers.";
        mConvertBufSize = 0;
        ret = NO_MEMORY;
    }
    return ret;
}

status_t AudioConverter::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    mSsSrc = ssSrc;
    mSsDst = ssDst;

    for (int i = 0; i < NbSampleSpecItems; i++) {

        if (i == mSampleSpecItem) {

            if (SampleSpec::isSampleSpecItemEqual(static_cast<SampleSpecItem>(i), ssSrc, ssDst)) {

                // The Sample spec items on which the converter is working
                // are the same...
                return INVALID_OPERATION;
            }
            continue;
        }

        if (!SampleSpec::isSampleSpecItemEqual(static_cast<SampleSpecItem>(i), ssSrc, ssDst)) {

            // The Sample spec items on which the converter is NOT working
            // MUST BE the same...
            Log::Error() << __FUNCTION__ << ": not supported";
            return INVALID_OPERATION;
        }
    }

    // Reset the convert function pointer
    mConvertSamplesFct = NULL;

    // force the size to 0 to clear the buffer
    mConvertBufSize = 0;

    return NO_ERROR;
}

status_t AudioConverter::convert(const void *src,
                                 void **dst,
                                 const size_t inFrames,
                                 size_t *outFrames)
{
    void *outBuf;
    status_t ret = NO_ERROR;

    // output buffer might be provided by the caller
    outBuf = *dst != NULL ? *dst : getOutputBuffer(inFrames);
    if (!outBuf) {

        return NO_MEMORY;
    }

    if (mConvertSamplesFct != NULL) {

        ret = (this->*mConvertSamplesFct)(src, outBuf, inFrames, outFrames);
    }

    *dst = outBuf;

    return ret;
}

size_t AudioConverter::convertSrcToDstInFrames(ssize_t frames) const
{
    return AudioUtils::convertSrcToDstInFrames(frames, mSsSrc, mSsDst);
}

size_t AudioConverter::convertSrcFromDstInFrames(ssize_t frames) const
{
    return AudioUtils::convertSrcToDstInFrames(frames, mSsDst, mSsSrc);
}
}  // namespace intel_audio
