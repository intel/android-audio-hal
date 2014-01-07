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
#define LOG_TAG "AudioConverter"

#include "AudioConverter.hpp"
#include "AudioUtils.hpp"
#include <cutils/log.h>
#include <stdlib.h>

using namespace android;

namespace android_audio_legacy
{

AudioConverter::AudioConverter(SampleSpecItem sampleSpecItem)
    : _convertSamplesFct(NULL),
      _ssSrc(),
      _ssDst(),
      _convertBuf(NULL),
      _convertBufSize(0),
      _sampleSpecItem(sampleSpecItem)
{
}

AudioConverter::~AudioConverter()
{
    delete[] _convertBuf;
}

//
// This function gets an output buffer suitable
// to convert inFrames input frames
//
void *AudioConverter::getOutputBuffer(ssize_t inFrames)
{
    status_t ret = NO_ERROR;
    size_t outBufSizeInBytes = _ssDst.convertFramesToBytes(convertSrcToDstInFrames(inFrames));

    if (outBufSizeInBytes > _convertBufSize) {

        ret = allocateConvertBuffer(outBufSizeInBytes);
        if (ret != NO_ERROR) {

            LOGE("%s: could not allocate memory for operation", __FUNCTION__);
            return NULL;
        }
    }

    return (void *)_convertBuf;
}

status_t AudioConverter::allocateConvertBuffer(ssize_t bytes)
{
    status_t ret = NO_ERROR;

    // Allocate one more frame for resampler
    _convertBufSize = bytes +
                      (audio_bytes_per_sample(_ssDst.getFormat()) * _ssDst.getChannelCount());

    free(_convertBuf);
    _convertBuf = NULL;

    _convertBuf = new char[_convertBufSize];

    if (!_convertBuf) {

        LOGE("cannot allocate resampler tmp buffers.\n");
        _convertBufSize = 0;
        ret = NO_MEMORY;
    }
    return ret;
}

status_t AudioConverter::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    _ssSrc = ssSrc;
    _ssDst = ssDst;

    for (int i = 0; i < NbSampleSpecItems; i++) {

        if (i == _sampleSpecItem) {

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
            LOGE("%s: not supported", __FUNCTION__);
            return INVALID_OPERATION;
        }
    }

    // Reset the convert function pointer
    _convertSamplesFct = NULL;

    // force the size to 0 to clear the buffer
    _convertBufSize = 0;

    return NO_ERROR;
}

status_t AudioConverter::convert(const void *src,
                                 void **dst,
                                 const uint32_t inFrames,
                                 uint32_t *outFrames)
{
    void *outBuf;
    status_t ret = NO_ERROR;

    // output buffer might be provided by the caller
    outBuf = *dst != NULL ? *dst : getOutputBuffer(inFrames);
    if (!outBuf) {

        return NO_MEMORY;
    }

    if (_convertSamplesFct != NULL) {

        ret = (this->*_convertSamplesFct)(src, outBuf, inFrames, outFrames);
    }

    *dst = outBuf;

    return ret;
}

size_t AudioConverter::convertSrcToDstInFrames(ssize_t frames) const
{
    return AudioUtils::convertSrcToDstInFrames(frames, _ssSrc, _ssDst);
}

size_t AudioConverter::convertSrcFromDstInFrames(ssize_t frames) const
{
    return AudioUtils::convertSrcToDstInFrames(frames, _ssDst, _ssSrc);
}
}  // namespace android
