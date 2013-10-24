/*
 * INTEL CONFIDENTIAL
 * Copyright © 2013 Intel
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
 * disclosed in any way without Intel’s prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#define LOG_TAG "AudioResampler"

#include <cutils/log.h>

#include "AudioResampler.h"
#include "Resampler.h"
#include <AudioCommsAssert.hpp>

using namespace android;

namespace android_audio_legacy
{

AudioResampler::AudioResampler(SampleSpecItem sampleSpecItem)
    : AudioConverter(sampleSpecItem),
      _resampler(new Resampler(RateSampleSpecItem)),
      _pivotResampler(new Resampler(RateSampleSpecItem)),
      _activeResamplerList()
{
}

AudioResampler::~AudioResampler()
{
    _activeResamplerList.clear();
    delete _resampler;
    delete _pivotResampler;
}

status_t AudioResampler::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    _activeResamplerList.clear();

    status_t status = AudioConverter::configure(ssSrc, ssDst);
    if (status != NO_ERROR) {

        return status;
    }

    status = _resampler->configure(ssSrc, ssDst);
    if (status != NO_ERROR) {

        //
        // Our resampling lib does not support all conversions
        // using 2 resamplers
        //
        LOGD("%s: trying to use working sample rate @ 48kHz", __FUNCTION__);
        SampleSpec pivotSs = ssDst;
        pivotSs.setSampleRate(PIVOT_SAMPLE_RATE);

        status = _pivotResampler->configure(ssSrc, pivotSs);
        if (status != NO_ERROR) {

            LOGD("%s: trying to use pivot sample rate @ %dkHz: FAILED",
                 __FUNCTION__, PIVOT_SAMPLE_RATE);
            return status;
        }

        _activeResamplerList.push_back(_pivotResampler);

        status = _resampler->configure(pivotSs, ssDst);
        if (status != NO_ERROR) {

            LOGD("%s: trying to use pivot sample rate @ 48kHz: FAILED", __FUNCTION__);
            return status;
        }
    }

    _activeResamplerList.push_back(_resampler);

    return NO_ERROR;
}

status_t AudioResampler::convert(const void *src,
                                 void **dst,
                                 uint32_t inFrames,
                                 uint32_t *outFrames)
{
    AUDIOCOMMS_ASSERT(src != NULL, "NULL source buffer");
    const void *srcBuf = static_cast<const void *>(src);
    void *dstBuf = NULL;
    size_t srcFrames = inFrames;
    size_t dstFrames = 0;

    ResamplerListIterator it;
    for (it = _activeResamplerList.begin(); it != _activeResamplerList.end(); ++it) {

        Resampler *conv = *it;
        dstFrames = 0;

        if (*dst && (conv == _activeResamplerList.back())) {

            // Last converter must output within the provided buffer (if provided!!!)
            dstBuf = *dst;
        }

        status_t status = conv->convert(srcBuf, &dstBuf, srcFrames, &dstFrames);
        if (status != NO_ERROR) {

            return status;
        }

        srcBuf = dstBuf;
        srcFrames = dstFrames;
    }

    *dst = dstBuf;
    *outFrames = dstFrames;

    return NO_ERROR;
}
}  // namespace android
