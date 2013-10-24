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
#define LOG_TAG "Resampler"

#include "Resampler.h"
#include <AudioCommsAssert.hpp>
#include <cutils/log.h>
#include <iasrc_resampler.h>
#include <limits.h>

using namespace android;

namespace android_audio_legacy
{

Resampler::Resampler(SampleSpecItem sampleSpecItem)
    : AudioConverter(sampleSpecItem),
      _maxFrameCnt(0),
      _context(NULL), _floatInp(NULL), _floatOut(NULL)
{
}

Resampler::~Resampler()
{
    if (_context) {
        iaresamplib_reset(_context);
        iaresamplib_delete(&_context);
    }

    delete[] _floatInp;
    delete[] _floatOut;
}

status_t Resampler::allocateBuffer()
{
    if (_maxFrameCnt == 0) {
        _maxFrameCnt = BUF_SIZE;
    } else {
        _maxFrameCnt *= 2; // simply double the buf size
    }

    delete[] _floatInp;
    delete[] _floatOut;

    _floatInp = new float[(_maxFrameCnt + 1) * _ssSrc.getChannelCount()];
    _floatOut = new float[(_maxFrameCnt + 1) * _ssSrc.getChannelCount()];

    if (!_floatInp || !_floatOut) {

        LOGE("cannot allocate resampler tmp buffers.\n");
        delete[] _floatInp;
        delete[] _floatOut;

        return NO_MEMORY;
    }
    return NO_ERROR;
}

status_t Resampler::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    ALOGD("%s: SOURCE rate=%d format=%d channels=%d",  __FUNCTION__, ssSrc.getSampleRate(),
          ssSrc.getFormat(), ssSrc.getChannelCount());
    ALOGD("%s: DST rate=%d format=%d channels=%d", __FUNCTION__, ssDst.getSampleRate(),
          ssDst.getFormat(), ssDst.getChannelCount());

    if ((ssSrc.getSampleRate() == _ssSrc.getSampleRate()) &&
        (ssDst.getSampleRate() == _ssDst.getSampleRate()) && _context) {

        return NO_ERROR;
    }

    status_t status = AudioConverter::configure(ssSrc, ssDst);
    if (status != NO_ERROR) {

        return status;
    }

    if (_context) {
        iaresamplib_reset(_context);
        iaresamplib_delete(&_context);
        _context = NULL;
    }

    if (!iaresamplib_supported_conversion(ssSrc.getSampleRate(), ssDst.getSampleRate())) {

        ALOGE("%s: SRC lib doesn't support this conversion", __FUNCTION__);
        return INVALID_OPERATION;
    }

    iaresamplib_new(&_context, ssSrc.getChannelCount(),
                    ssSrc.getSampleRate(), ssDst.getSampleRate());
    if (!_context) {
        ALOGE("cannot create resampler handle for lacking of memory.\n");
        return BAD_VALUE;
    }

    _convertSamplesFct = static_cast<SampleConverter>(&Resampler::resampleFrames);
    return NO_ERROR;
}

void Resampler::convertShort2Float(int16_t *inp, float *out, size_t sz) const
{
    AUDIOCOMMS_ASSERT(inp != NULL && out != NULL, "Invalid input and/or output buffer(s)");
    size_t i;
    for (i = 0; i < sz; i++) {
        *out++ = (float)*inp++;
    }
}

void Resampler::convertFloat2Short(float *inp, int16_t *out, size_t sz) const
{
    AUDIOCOMMS_ASSERT(inp != NULL && out != NULL, "Invalid input and/or output buffer(s)");
    size_t i;
    for (i = 0; i < sz; i++) {
        if (*inp > SHRT_MAX) {
            *inp = SHRT_MAX;
        } else if (*inp < SHRT_MIN) {
            *inp = SHRT_MIN;
        }
        *out++ = (short)*inp++;
    }
}

status_t Resampler::resampleFrames(const void *src,
                                   void *dst,
                                   const uint32_t inFrames,
                                   uint32_t *outFrames)
{
    size_t outFrameCount = convertSrcToDstInFrames(inFrames);

    while (outFrameCount > _maxFrameCnt) {

        status_t ret = allocateBuffer();
        if (ret != NO_ERROR) {

            ALOGE("%s: could not allocate memory for resampling operation", __FUNCTION__);
            return ret;
        }
    }
    unsigned int outNbFrames;
    convertShort2Float((short *)src, _floatInp, inFrames * _ssSrc.getChannelCount());
    iaresamplib_process_float(_context, _floatInp, inFrames, _floatOut, &outNbFrames);
    convertFloat2Short(_floatOut, (short *)dst, outNbFrames * _ssSrc.getChannelCount());

    *outFrames = outNbFrames;

    return NO_ERROR;
}
}  // namespace android
