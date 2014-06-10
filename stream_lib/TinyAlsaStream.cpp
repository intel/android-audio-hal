/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
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
#define LOG_TAG "TinyAlsaStream"

#include "TinyAlsaAudioDevice.hpp"
#include "TinyAlsaStream.hpp"
#include <IStreamRoute.hpp>
#include <AudioCommsAssert.hpp>
#include <cutils/log.h>

using std::string;
using android::status_t;
using android::OK;

pcm *TinyAlsaStream::getPcmDevice() const
{
    AUDIOCOMMS_ASSERT(mDevice != NULL, "Null audio device attached to stream");
    return mDevice->getPcmDevice();
}

android::status_t TinyAlsaStream::attachRouteL()
{
    mDevice = static_cast<TinyAlsaAudioDevice *>(getNewStreamRoute()->getAudioDevice());
    Stream::attachRouteL();
    return OK;
}

android::status_t TinyAlsaStream::detachRouteL()
{
    Stream::detachRouteL();
    mDevice = NULL;
    return OK;
}

status_t TinyAlsaStream::pcmReadFrames(void *buffer, size_t frames, string &error) const
{
    status_t ret;

    ret = pcm_read(getPcmDevice(),
                   (char *)buffer,
                   routeSampleSpec().convertFramesToBytes(frames));

    if (ret < 0) {
        error = pcm_get_error(getPcmDevice());
        return ret;
    }

    return OK;
}

status_t TinyAlsaStream::pcmWriteFrames(void *buffer, ssize_t frames, string &error) const
{
    status_t ret;

    ret = pcm_write(getPcmDevice(),
                    (char *)buffer,
                    pcm_frames_to_bytes(getPcmDevice(), frames));

    if (ret < 0) {
        error = pcm_get_error(getPcmDevice());
        return ret;
    }

    return OK;
}

uint32_t TinyAlsaStream::getBufferSizeInBytes() const
{
    return pcm_frames_to_bytes(getPcmDevice(), getBufferSizeInFrames());
}

size_t TinyAlsaStream::getBufferSizeInFrames() const
{
    return pcm_get_buffer_size(getPcmDevice());
}

status_t TinyAlsaStream::getFramesAvailable(uint32_t &avail, struct timespec &tStamp) const
{
    return pcm_get_htimestamp(getPcmDevice(), &avail, &tStamp);
}

status_t TinyAlsaStream::pcmStop() const
{
    return pcm_stop(getPcmDevice());
}
