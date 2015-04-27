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
#define LOG_TAG "TinyAlsaStream"

#include "TinyAlsaAudioDevice.hpp"
#include "TinyAlsaIoStream.hpp"
#include <IStreamRoute.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using audio_comms::utilities::Log;
using std::string;
using android::status_t;
using android::OK;
using android::INVALID_OPERATION;

namespace intel_audio
{

pcm *TinyAlsaIoStream::getPcmDevice() const
{
    AUDIOCOMMS_ASSERT(mDevice != NULL, "Null audio device attached to stream");
    return mDevice->getPcmDevice();
}

android::status_t TinyAlsaIoStream::attachRouteL()
{
    mDevice = static_cast<TinyAlsaAudioDevice *>(getNewStreamRoute()->getAudioDevice());
    IoStream::attachRouteL();
    return OK;
}

android::status_t TinyAlsaIoStream::detachRouteL()
{
    IoStream::detachRouteL();
    mDevice = NULL;
    return OK;
}

status_t TinyAlsaIoStream::pcmReadFrames(void *buffer, size_t frames, string &error) const
{
    if (frames == 0) {
        Log::Error() << "Invalid frame number to read (" << frames << ")";
        return android::BAD_VALUE;
    }

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

status_t TinyAlsaIoStream::pcmWriteFrames(void *buffer, ssize_t frames, string &error) const
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

uint32_t TinyAlsaIoStream::getBufferSizeInBytes() const
{
    return pcm_frames_to_bytes(getPcmDevice(), getBufferSizeInFrames());
}

size_t TinyAlsaIoStream::getBufferSizeInFrames() const
{
    return pcm_get_buffer_size(getPcmDevice());
}

status_t TinyAlsaIoStream::getFramesAvailable(size_t &avail, struct timespec &tStamp) const
{
    unsigned int availFrames;
    int err =  pcm_get_htimestamp(getPcmDevice(), &availFrames, &tStamp);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << ": Unable to get available frames";
        return INVALID_OPERATION;
    }
    avail = availFrames;
    return OK;
}

status_t TinyAlsaIoStream::pcmStop() const
{
    return pcm_stop(getPcmDevice());
}

} // namespace intel_audio
