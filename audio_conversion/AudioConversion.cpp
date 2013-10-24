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
#define LOG_TAG "AudioConversion"

#include "AudioConversion.h"
#include "AudioConverter.h"
#include "AudioReformatter.h"
#include "AudioRemapper.h"
#include "AudioResampler.h"
#include "AudioUtils.h"
#include <AudioCommsAssert.hpp>
#include <cutils/log.h>
#include <media/AudioBufferProvider.h>
#include <stdlib.h>

using namespace android;
using namespace std;

namespace android_audio_legacy
{

const uint32_t AudioConversion::MAX_RATE = 92000;

const uint32_t AudioConversion::MIN_RATE = 8000;

const uint32_t AudioConversion::ALLOC_BUFFER_MULT_FACTOR = 2;

AudioConversion::AudioConversion()
    : _convOutBufferIndex(0),
      _convOutFrames(0),
      _convOutBufferSizeInFrames(0),
      _convOutBuffer(NULL)
{
    _audioConverter[ChannelCountSampleSpecItem] = new AudioRemapper(ChannelCountSampleSpecItem);
    _audioConverter[FormatSampleSpecItem] = new AudioReformatter(FormatSampleSpecItem);
    _audioConverter[RateSampleSpecItem] = new AudioResampler(RateSampleSpecItem);
}

AudioConversion::~AudioConversion()
{
    for (int i = 0; i < NbSampleSpecItems; i++) {

        delete _audioConverter[i];
        _audioConverter[i] = NULL;
    }

    free(_convOutBuffer);
    _convOutBuffer = NULL;
}

status_t AudioConversion::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    status_t ret = NO_ERROR;

    emptyConversionChain();

    free(_convOutBuffer);

    _convOutBuffer = NULL;
    _convOutBufferIndex = 0;
    _convOutFrames = 0;
    _convOutBufferSizeInFrames = 0;

    _ssSrc = ssSrc;
    _ssDst = ssDst;

    if (ssSrc == ssDst) {

        LOGD("%s: no convertion required", __FUNCTION__);
        return ret;
    }

    SampleSpec tmpSsSrc = ssSrc;

    // Start by adding the remapper, it will add consequently the reformatter and resampler
    // This function may alter the source sample spec
    ret = configureAndAddConverter(ChannelCountSampleSpecItem, &tmpSsSrc, &ssDst);
    if (ret != NO_ERROR) {

        return ret;
    }

    // Assert the temporary sample spec equals the destination sample spec
    AUDIOCOMMS_ASSERT(tmpSsSrc == ssDst, "Could not provide converter");

    return ret;
}

status_t AudioConversion::getConvertedBuffer(void *dst,
                                             const uint32_t outFrames,
                                             AudioBufferProvider *bufferProvider)
{
    AUDIOCOMMS_ASSERT(bufferProvider != NULL, "NULL source buffer");
    AUDIOCOMMS_ASSERT(dst != NULL, "NULL destination buffer");

    status_t status = NO_ERROR;

    if (_activeAudioConvList.empty()) {

        LOGE("%s: conversion called with empty converter list", __FUNCTION__);
        return NO_INIT;
    }

    //
    // Realloc the Output of the conversion if required (with margin of the worst case)
    //
    if (_convOutBufferSizeInFrames < outFrames) {

        _convOutBufferSizeInFrames = outFrames +
                                     (MAX_RATE / MIN_RATE) * ALLOC_BUFFER_MULT_FACTOR;
        int16_t *reallocBuffer =
            static_cast<int16_t *>(realloc(_convOutBuffer,
                                           _ssDst.convertFramesToBytes(
                                               _convOutBufferSizeInFrames)));
        if (reallocBuffer == NULL) {

            ALOGE(" %s(frames=%ld): realloc failed", __FUNCTION__,
                  static_cast<long int>(outFrames));
            return NO_MEMORY;
        }
        _convOutBuffer = reallocBuffer;
    }

    size_t framesRequested = outFrames;

    //
    // Frames are already available from the ConvOutBuffer, empty it first!
    //
    if (_convOutFrames) {

        size_t frameToCopy = min(framesRequested, _convOutFrames);
        framesRequested -= frameToCopy;
        _convOutBufferIndex += _ssDst.convertFramesToBytes(frameToCopy);
    }

    //
    // Frames still needed? (_pConvOutBuffer emptied!)
    //
    while (framesRequested != 0) {

        //
        // Outputs in the convOutBuffer
        //
        AudioBufferProvider::Buffer &buffer(_convInBuffer);

        // Calculate the frames we need to get from buffer provider
        // (Runs at ssSrc sample spec)
        // Note that is is rounded up.
        buffer.frameCount = AudioUtils::convertSrcToDstInFrames(framesRequested, _ssDst, _ssSrc);

        //
        // Acquire next buffer from buffer provider
        //
        status = bufferProvider->getNextBuffer(&buffer);
        if (status != NO_ERROR) {

            return status;
        }

        //
        // Convert
        //
        size_t convertedFrames;
        char *convBuf = reinterpret_cast<char *>(_convOutBuffer) + _convOutBufferIndex;
        status = convert(buffer.raw, reinterpret_cast<void **>(&convBuf),
                         buffer.frameCount, &convertedFrames);
        if (status != NO_ERROR) {

            bufferProvider->releaseBuffer(&buffer);
            return status;
        }

        _convOutFrames += convertedFrames;
        _convOutBufferIndex += _ssDst.convertFramesToBytes(convertedFrames);

        size_t framesToCopy = min(framesRequested, convertedFrames);
        framesRequested -= framesToCopy;

        //
        // Release the buffer
        //
        bufferProvider->releaseBuffer(&buffer);
    }

    //
    // Copy requested outFrames from the output buffer of the conversion.
    // Move the remaining frames to the beginning of the convOut buffer
    //
    memcpy(dst, _convOutBuffer, _ssDst.convertFramesToBytes(outFrames));
    _convOutFrames -= outFrames;

    //
    // Move the remaining frames to the beginning of the convOut buffer
    //
    if (_convOutFrames) {

        memmove(_convOutBuffer,
                reinterpret_cast<char *>(_convOutBuffer) + _ssDst.convertFramesToBytes(outFrames),
                _ssDst.convertFramesToBytes(_convOutFrames));
    }

    // Reset buffer Index
    _convOutBufferIndex = 0;

    return NO_ERROR;
}

status_t AudioConversion::convert(const void *src,
                                  void **dst,
                                  const uint32_t inFrames,
                                  uint32_t *outFrames)
{
    AUDIOCOMMS_ASSERT(src != NULL, "NULL source buffer");
    const void *srcBuf = src;
    void *dstBuf = NULL;
    size_t srcFrames = inFrames;
    size_t dstFrames = 0;
    status_t status = NO_ERROR;

    if (_activeAudioConvList.empty()) {

        // Empty converter list -> No need for convertion
        // Copy the input on the ouput if provided by the client
        // or points on the imput buffer
        if (*dst) {

            memcpy(*dst, src, _ssSrc.convertFramesToBytes(inFrames));
            *outFrames = inFrames;
        } else {

            *dst = (void *)src;
            *outFrames = inFrames;
        }
        return NO_ERROR;
    }

    AudioConverterListIterator it;
    for (it = _activeAudioConvList.begin(); it != _activeAudioConvList.end(); ++it) {

        AudioConverter *pConv = *it;
        dstBuf = NULL;
        dstFrames = 0;

        if (*dst && (pConv == _activeAudioConvList.back())) {

            // Last converter must output within the provided buffer (if provided!!!)
            dstBuf = *dst;
        }
        status = pConv->convert(srcBuf, &dstBuf, srcFrames, &dstFrames);
        if (status != NO_ERROR) {

            return status;
        }

        srcBuf = dstBuf;
        srcFrames = dstFrames;
    }

    *dst = dstBuf;
    *outFrames = dstFrames;

    return status;
}

void AudioConversion::emptyConversionChain()
{
    _activeAudioConvList.clear();
}

status_t AudioConversion::doConfigureAndAddConverter(SampleSpecItem sampleSpecItem,
                                                     SampleSpec *ssSrc,
                                                     const SampleSpec *ssDst)
{
    AUDIOCOMMS_ASSERT(sampleSpecItem < NbSampleSpecItems, "Sample Spec item out of range");

    SampleSpec tmpSsDst = *ssSrc;
    tmpSsDst.setSampleSpecItem(sampleSpecItem, ssDst->getSampleSpecItem(sampleSpecItem));

    if (sampleSpecItem == ChannelCountSampleSpecItem) {

        tmpSsDst.setChannelsPolicy(ssDst->getChannelsPolicy());
    }

    status_t ret = _audioConverter[sampleSpecItem]->configure(*ssSrc, tmpSsDst);
    if (ret != NO_ERROR) {

        return ret;
    }
    _activeAudioConvList.push_back(_audioConverter[sampleSpecItem]);
    *ssSrc = tmpSsDst;

    return NO_ERROR;
}

status_t AudioConversion::configureAndAddConverter(SampleSpecItem sampleSpecItem,
                                                   SampleSpec *ssSrc,
                                                   const SampleSpec *ssDst)
{
    AUDIOCOMMS_ASSERT(sampleSpecItem < NbSampleSpecItems, "Sample Spec item out of range");

    // If the input format size is higher, first perform the reformat
    // then add the resampler
    // and perform the reformat (if not already done)
    if (ssSrc->getSampleSpecItem(sampleSpecItem) > ssDst->getSampleSpecItem(sampleSpecItem)) {

        status_t ret = doConfigureAndAddConverter(sampleSpecItem, ssSrc, ssDst);
        if (ret != NO_ERROR) {

            return ret;
        }
    }

    if ((sampleSpecItem + 1) < NbSampleSpecItems) {
        // Dive
        status_t ret = configureAndAddConverter((SampleSpecItem)(sampleSpecItem + 1), ssSrc,
                                                ssDst);
        if (ret != NO_ERROR) {

            return ret;
        }
    }

    // Handle the case of destination sample spec item is higher than input sample spec
    // or destination and source channels policy are different
    if (!SampleSpec::isSampleSpecItemEqual(sampleSpecItem, *ssSrc, *ssDst)) {

        return doConfigureAndAddConverter(sampleSpecItem, ssSrc, ssDst);
    }
    return NO_ERROR;
}
}  // namespace android
