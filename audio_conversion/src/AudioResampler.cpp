/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2015 Intel
 * Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
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
    Log::Debug() << __FUNCTION__ << ": SOURCE rate=" << ssSrc.getSampleRate()
                 << " format=" << static_cast<int32_t>(ssSrc.getFormat())
                 << " channels=" << ssSrc.getChannelCount();
    Log::Debug() << __FUNCTION__ << ": DST rate=" << ssDst.getSampleRate()
                 << " format=" << static_cast<int32_t>(ssDst.getFormat())
                 << " channels=" << ssDst.getChannelCount();

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

    AUDIOCOMMS_ASSERT(mResampler != NULL, "NULL resampler handler");

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
