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
#define LOG_TAG "TinyAlsaAudioDevice"

#include "TinyAlsaAudioDevice.hpp"
#include <StreamRouteConfig.hpp>
#include <AudioUtils.hpp>
#include <SampleSpec.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using audio_comms::utilities::Log;

namespace intel_audio
{

pcm *TinyAlsaAudioDevice::getPcmDevice()
{
    AUDIOCOMMS_ASSERT(mPcmDevice != NULL, "NULL tiny alsa device");

    return mPcmDevice;
}

android::status_t TinyAlsaAudioDevice::open(const char *cardName,
                                            uint32_t deviceId,
                                            const StreamRouteConfig &routeConfig,
                                            bool isOut)
{
    AUDIOCOMMS_ASSERT(mPcmDevice == NULL, "Tiny alsa device already opened");
    AUDIOCOMMS_ASSERT(cardName != NULL, "Null card name");

    pcm_config config;
    config.rate = routeConfig.rate;
    config.channels = routeConfig.channels;
    config.format = AudioUtils::convertHalToTinyFormat(routeConfig.format);
    config.period_size = routeConfig.periodSize;
    config.period_count = routeConfig.periodCount;
    config.start_threshold = routeConfig.startThreshold;
    config.stop_threshold = routeConfig.stopThreshold;
    config.silence_threshold = routeConfig.silenceThreshold;
    config.silence_size = 0;
    config.avail_min = routeConfig.availMin;

    Log::Debug() << __FUNCTION__ << ": card (" << cardName << ", " << deviceId
                 << ") \n\t config (rate=" << config.rate
                 << " format=" << static_cast<int32_t>(config.format)
                 << " channels= " << config.channels
                 << ")."
                 << "\n\t RingBuffer config: periodSize=" << config.period_size
                 << " nbPeriod=" << config.period_count << "startTh=" << config.start_threshold
                 << " stop Th=" << config.stop_threshold
                 << " silence Th=" << config.silence_threshold;
    //
    // Opens the device in BLOCKING mode (default)
    // No need to check for NULL handle, tiny alsa
    // guarantee to return a pcm structure, even when failing to open
    // it will return a reference on a "bad pcm" structure
    //
    uint32_t flags = (isOut ? PCM_OUT : PCM_IN) | PCM_MONOTONIC;
    int cardIndex = AudioUtils::getCardIndexByName(cardName);
    if (cardIndex < 0) {
        return android::BAD_VALUE;
    }
    mPcmDevice = pcm_open(cardIndex, deviceId, flags, &config);
    if (mPcmDevice && !pcm_is_ready(mPcmDevice)) {
        Log::Error() << __FUNCTION__
                     << ": Cannot open tinyalsa (" << cardName
                     << "," << deviceId << ") device for "
                     << (isOut ? "output" : "input")
                     << " stream (error=" << pcm_get_error(mPcmDevice) << ")";
        goto close_device;
    }
    // Prepare the device (ie allocation of the stream)
    if (pcm_prepare(mPcmDevice) != 0) {
        Log::Error() << __FUNCTION__ << ": prepare failed with error " << pcm_get_error(mPcmDevice);
        goto close_device;
    }
    if ((config.period_count * config.period_size) != (pcm_get_buffer_size(mPcmDevice))) {
        Log::Warning() << __FUNCTION__
                       << ": refine done by alsa, ALSA RingBuffer = "
                       << pcm_get_buffer_size(mPcmDevice)
                       << "(frames), expected by AudioHAL and AudioFlinger = "
                       << config.period_count * config.period_size << " (frames)";
    }
    return android::OK;

close_device:

    close();
    return android::NO_MEMORY;
}

bool TinyAlsaAudioDevice::isOpened()
{
    return mPcmDevice != NULL;
}

android::status_t TinyAlsaAudioDevice::close()
{
    if (mPcmDevice == NULL) {

        return android::DEAD_OBJECT;
    }
    Log::Debug() << __FUNCTION__;
    pcm_close(mPcmDevice);
    mPcmDevice = NULL;

    return android::OK;
}

} // namespace intel_audio
