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
#define LOG_TAG "TinyAlsaStream"

#include "TinyAlsaAudioDevice.h"
#include "TinyAlsaStream.h"
#include <IStreamRoute.h>
#include <AudioCommsAssert.hpp>
#include <cutils/log.h>

using android::status_t;
using android::OK;

pcm *TinyAlsaStream::getPcmDevice() const
{
    AUDIOCOMMS_ASSERT(_device != NULL, "Null audio device attached to stream");
    return _device->getPcmDevice();
}

android::status_t TinyAlsaStream::attachRouteL()
{
    _device = static_cast<TinyAlsaAudioDevice *>(getNewStreamRoute()->getAudioDevice());
    Stream::attachRouteL();
    return OK;
}

android::status_t TinyAlsaStream::detachRouteL()
{
    Stream::detachRouteL();
    _device = NULL;
    return OK;
}

bool TinyAlsaStream::safeSleep(uint32_t sleepTimeUs)
{
    struct timespec tim, tim2;

    if (sleepTimeUs > MAX_SLEEP_TIME) {
        sleepTimeUs = MAX_SLEEP_TIME;
    }

    tim.tv_sec = 0;
    tim.tv_nsec = sleepTimeUs * NSEC_PER_USEC;

    return nanosleep(&tim, &tim2) > 0;
}

ssize_t TinyAlsaStream::pcmReadFrames(void *buffer, size_t frames)
{
    int ret;
    uint32_t retryCount = 0;

    do {
        ret = pcm_read(getPcmDevice(),
                       (char *)buffer,
                       routeSampleSpec().convertFramesToBytes(frames));

        ALOGV("%s %d %d", __FUNCTION__, ret, pcm_frames_to_bytes(routeSampleSpec(), frames));

        if (ret) {
            ALOGE("%s: read error: requested %d (bytes=%d) frames %s",
                  __FUNCTION__,
                  frames,
                  routeSampleSpec().convertFramesToBytes(frames),
                  pcm_get_error(getPcmDevice()));
            AUDIOCOMMS_ASSERT(++retryCount < MAX_READ_WRITE_RETRIES,
                              "Hardware not responding, restarting media server");

            // Get the number of microseconds to sleep, inferred from the number of
            // frames to write.
            size_t sleepUSecs = routeSampleSpec().convertFramesToUsec(frames);

            // Go sleeping before trying I/O operation again.
            if (safeSleep(sleepUSecs)) {
                // If some error arises when trying to sleep, try I/O operation anyway.
                // Error counter will provoke the restart of mediaserver.
                ALOGE("%s:  Error while calling nanosleep interface", __FUNCTION__);
            }
        }
    } while (ret);

    return frames;
}

ssize_t TinyAlsaStream::pcmWriteFrames(void *buffer, ssize_t frames)
{
    int ret;
    uint32_t retryCount = 0;

    do {
        ret = pcm_write(getPcmDevice(),
                        (char *)buffer,
                        pcm_frames_to_bytes(getPcmDevice(), frames));

        ALOGV("%s %d %d", __FUNCTION__, ret, pcm_frames_to_bytes(mHandle, frames));

        if (ret) {
            ALOGE("%s: write error: %d %s", __FUNCTION__, ret, pcm_get_error(getPcmDevice()));
            AUDIOCOMMS_ASSERT(++retryCount < MAX_READ_WRITE_RETRIES,
                              "Hardware not responding, restarting media server");

            // Get the number of microseconds to sleep, inferred from the number of
            // frames to write.
            size_t sleepUsecs = routeSampleSpec().convertFramesToUsec(frames);

            // Go sleeping before trying I/O operation again.
            if (safeSleep(sleepUsecs)) {
                // If some error arises when trying to sleep, try I/O operation anyway.
                // Error counter will provoke the restart of mediaserver.
                ALOGE("%s:  Error while calling nanosleep interface", __FUNCTION__);
            }
        }
    } while (ret);

    return frames;
}

uint32_t TinyAlsaStream::getBufferSizeInBytes() const
{
    return pcm_frames_to_bytes(getPcmDevice(), getBufferSizeInFrames());
}

size_t TinyAlsaStream::getBufferSizeInFrames() const
{
    return pcm_get_buffer_size(getPcmDevice());
}

status_t TinyAlsaStream::getFramesAvailable(uint32_t &avail, struct timespec &tStamp)
{
    return pcm_get_htimestamp(getPcmDevice(), &avail, &tStamp);
}

status_t TinyAlsaStream::pcmStop()
{
    return pcm_stop(getPcmDevice());
}
