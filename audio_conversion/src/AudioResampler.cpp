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

#define LOG_TAG "AudioResampler"

#include "AudioResampler.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <audio_utils/resampler.h>
#include <limits.h>

using audio_comms::utilities::Log;
using namespace android;

namespace intel_audio
{

AudioResampler::AudioResampler(SampleSpecItem sampleSpecItem)
    : AudioConverter(sampleSpecItem),
      mResampler(NULL)
{
}

AudioResampler::~AudioResampler()
{
    if (mResampler != NULL) {
        release_resampler(mResampler);
        mResampler = NULL;
    }
}

status_t AudioResampler::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    if ((ssSrc.getSampleRate() == mSsSrc.getSampleRate()) &&
        (ssDst.getSampleRate() == mSsDst.getSampleRate()) &&
        (mResampler != NULL)) {
        mResampler->reset(mResampler);
        return NO_ERROR;
    }

    if ((ssSrc.getSampleRate() == 0) ||
        (ssDst.getSampleRate() == 0)) {
        return BAD_VALUE;
    }

    status_t status = AudioConverter::configure(ssSrc, ssDst);
    if (status != NO_ERROR) {

        return status;
    }
    if (mResampler != NULL) {
        release_resampler(mResampler);
        mResampler = NULL;
    }
    //  resampler_buffer_provider is NULL since we will be driven by the input...
    status = create_resampler(ssSrc.getSampleRate(), ssDst.getSampleRate(),
                              ssSrc.getChannelCount(), RESAMPLER_QUALITY_DEFAULT, NULL,
                              &mResampler);
    if (status != OK) {
        Log::Error() << "cannot instantiate resampler handle, status=" << status;
        return status;
    }

    AUDIOCOMMS_ASSERT(mResampler != NULL, "Audio Utils failed to instantiate a resampler");

    mConvertSamplesFct = static_cast<SampleConverter>(&AudioResampler::resampleFrames);
    return OK;
}

status_t AudioResampler::resampleFrames(const void *src,
                                        void *dst,
                                        const size_t inFrames,
                                        size_t *outFrames)
{
    // here we need to cast src and inFrames due to resample_from_input interface
    int16_t *bufferSrc = static_cast<int16_t *>(const_cast<void *>(src));
    size_t frameSrc = inFrames;
    status_t status;

    *outFrames = convertSrcToDstInFrames(inFrames);

    status = mResampler->resample_from_input(mResampler,
                                             bufferSrc,
                                             &frameSrc,
                                             static_cast<int16_t *>(dst), outFrames);

    /* According to resample_from_input() documentation: "[*frameSrc]
     * and [*outFrames] are updated respectively with the number of
     * frames remaining in input and written to output."
     * According to resample_from_input() code, *framesSrc is updated
     * with the number of frames processed (not "remaining") from
     * input and *outFrames is updated with the number of frames
     * successfully written to output.
     * Relying on the code, on success frameSrc is equal to inFrames.
     */
    if (frameSrc != inFrames) {
        Log::Warning() << __FUNCTION__ << ": buffer not fully consumed ("
                       << frameSrc << " frames left)";
    }

    return status;
}
}  // namespace intel_audio
