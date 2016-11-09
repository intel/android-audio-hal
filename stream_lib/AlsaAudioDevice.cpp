/*
 * Copyright (C) 2016 Intel Corporation
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
#define LOG_TAG "AlsaAudioDevice"

#include "AlsaAudioDevice.hpp"
#include <AudioUtils.hpp>
#include <SampleSpec.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using audio_comms::utilities::Log;
using namespace std;

namespace intel_audio
{

android::status_t AlsaAudioDevice::open(const char *deviceName, uint32_t /*deviceId*/,
                                        const StreamRouteConfig &routeConfig,  bool isOut)
{
    AUDIOCOMMS_ASSERT(mPcmDevice == NULL, "alsa device already opened");
    AUDIOCOMMS_ASSERT(deviceName != NULL, "Null card name");

    snd_pcm_stream_t stream = (isOut ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE);

    int err = snd_pcm_open(&mPcmDevice, deviceName, stream, SND_PCM_ASYNC);
    if (err) {
        Log::Error() << __FUNCTION__
                     << ": Cannot open alsa (" << deviceName
                     << ") device for "
                     << (isOut ? "output" : "input")
                     << " stream (error=" << snd_strerror(err) << ")";
        goto close_device;
    }

    err = setPcmParams(stream, routeConfig, SND_PCM_ACCESS_RW_INTERLEAVED, 0);

    if (err) {
        Log::Debug() << __FUNCTION__ << " unable to configure properly the pcm device";
        goto close_device;
    }
    Log::Debug() << __FUNCTION__ << ": pcm device successfully initialized: "
                 << "\n\t card (" << deviceName
                 << ") \n\t config (rate=" << routeConfig.rate
                 << " format=" <<
        static_cast<int32_t>(AudioUtils::convertHalToAlsaFormat(routeConfig.format))
                 << " channels=" << routeConfig.channels
                 << ").";

    return android::OK;

close_device:

    close();
    return android::BAD_VALUE;
}

int AlsaAudioDevice::setPcmParams(snd_pcm_stream_t stream, const StreamRouteConfig &config,
                                  snd_pcm_access_t access, int soft_resample)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    const char *s = snd_pcm_stream_name(snd_pcm_stream(mPcmDevice));
    unsigned int latency =
        ((float)config.periodSize * config.periodCount / config.rate) * mUsecToSec;
    snd_pcm_uframes_t period_size = config.periodSize;
    unsigned int period_time = latency / config.periodCount;
    snd_pcm_uframes_t buffer_size = config.periodSize * config.periodCount;
    unsigned int rrate = config.rate;
    int err;

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);

    /* choose all parameters */
    err = snd_pcm_hw_params_any(mPcmDevice, params);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Broken configuration for " << s << " : no configurations available";
        return err;
    }
    /* set software resampling */
    err = snd_pcm_hw_params_set_rate_resample(mPcmDevice, params, soft_resample);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Resampling setup failed for " << s << ": " <<  snd_strerror(err);
        return err;
    }
    /* set the selected read/write format */
    err = snd_pcm_hw_params_set_access(mPcmDevice, params, access);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Access type not available for " << s << " :" << snd_strerror(err);
        return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(mPcmDevice, params, AudioUtils::convertHalToAlsaFormat(config.format));
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Sample format not available for " << s << " :" << snd_strerror(err);
        return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(mPcmDevice, params, config.channels);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Channels count " << config.channels << " not available for " << s << " :" << snd_strerror(err);
        return err;
    }
    /* set the stream rate */
    err = snd_pcm_hw_params_set_rate_near(mPcmDevice, params, &rrate, 0);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Rate " << config.rate << "Hz not available for playback: " << snd_strerror(err);
        return err;
    }
    if (rrate != config.rate) {
        Log::Error() << __FUNCTION__ << " Rate doesn't match (requested " << config.rate << "Hz, got " << rrate << "Hz)";
        return -EINVAL;
    }
    /* set the buffer time */
    Log::Error() << __FUNCTION__ << " setting latency to " << latency;
    err = snd_pcm_hw_params_set_buffer_time_near(mPcmDevice, params, &latency, NULL);
    if (err < 0) {
        /* set the period time */

        err = snd_pcm_hw_params_set_period_time_near(mPcmDevice, params, &period_time, NULL);
        if (err < 0) {
            Log::Error() << __FUNCTION__ << " Unable to set period time " << period_time
                         << " for " << s << " :" << snd_strerror(err);
            return err;
        }
        err = snd_pcm_hw_params_get_period_size(params, &period_size, NULL);
        if (err < 0) {
            Log::Error() << __FUNCTION__ << " Unable to get period size for " << s << " :"
                         << snd_strerror(err);
            return err;
        }
        err = snd_pcm_hw_params_set_buffer_size_near(mPcmDevice, params, &buffer_size);
        if (err < 0) {
            Log::Error() << __FUNCTION__ << " Unable to set buffer size " << buffer_size << " for "
                         << s << " :" << snd_strerror(err);
            return err;
        }
        err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
        if (err < 0) {
            Log::Error() << __FUNCTION__ << " Unable to get buffer size for " << s << " :"
                         << snd_strerror(err);
            return err;
        }
    } else {
        /* standard configuration buffer_time -> periods */
        err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
        if (err < 0) {
            Log::Error() << __FUNCTION__ << " Unable to get buffer size for " << s << " :"
                         << snd_strerror(err);
            return err;
        }
        err = snd_pcm_hw_params_get_buffer_time(params, &latency, NULL);
        if (err < 0) {
            Log::Error() << __FUNCTION__ << " Unable to get buffer time (latency) for " << s << " :"
                         << snd_strerror(err);
            return err;
        }
        /* set the period time */
        err = snd_pcm_hw_params_set_period_time_near(mPcmDevice, params, &period_time, NULL);
        if (err < 0) {
            Log::Error() << __FUNCTION__ << " Unable to set periodz time " << period_time << " for "
                         << s << " :" << snd_strerror(err);
            return err;
        }
        err = snd_pcm_hw_params_get_period_size(params, &period_size, NULL);
        if (err < 0) {
            Log::Error() << __FUNCTION__ << " Unable to get period size for " << s << " :"
                         << snd_strerror(err);
            return err;
        }
    }
    /* write the parameters to device */
    err = snd_pcm_hw_params(mPcmDevice, params);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Unable to set hw params for " << s << " :"
                     << snd_strerror(err);
        return err;
    }

    /* get the current swparams */
    err = snd_pcm_sw_params_current(mPcmDevice, swparams);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Unable to determine current swparams for " << s << " :"
                     << snd_strerror(err);
        return err;
    }

    err = snd_pcm_sw_params_set_start_threshold(mPcmDevice, swparams, config.startThreshold);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Unable to set start threshold mode for " << s << " :"
                     << snd_strerror(err);
        return err;
    }

    err = snd_pcm_sw_params_set_stop_threshold(mPcmDevice, swparams, config.stopThreshold);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Unable to set start threshold mode for " << s << " :"
                     << snd_strerror(err);
        return err;
    }

    /* allow the transfer when at least period_size samples can be processed */
    err = snd_pcm_sw_params_set_avail_min(mPcmDevice, swparams, config.availMin);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Unable to set avail min for " << s << " :"
                     << snd_strerror(err);
        return err;
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(mPcmDevice, swparams);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Unable to set sw params for " << s << " :"
                     << snd_strerror(err);
        return err;
    }
    return 0;
}


bool AlsaAudioDevice::isOpened()
{
    return mPcmDevice != NULL;
}

android::status_t AlsaAudioDevice::close()
{
    if (mPcmDevice == NULL) {
        return android::DEAD_OBJECT;
    }
    Log::Debug() << __FUNCTION__;
    snd_pcm_drain(mPcmDevice);
    snd_pcm_close(mPcmDevice);
    mPcmDevice = NULL;

    return android::OK;
}

/////////////////////////////////////////////////////////////////:::
android::status_t AlsaAudioDevice::pcmReadFrames(void *buffer, size_t frames, string &error) const
{
    if (frames == 0) {
        Log::Error() << "Invalid frame number to read (" << frames << ")";
        return android::BAD_VALUE;
    }

    snd_pcm_sframes_t frames_read;
    frames_read = snd_pcm_readi(mPcmDevice, (char *)buffer, frames);

    if (frames_read < 0) {
        error = snd_strerror(frames_read);
        if (snd_pcm_recover(mPcmDevice, frames_read, 0) != android::OK) {
            Log::Error() << "Unable to recover from pcm_read, error: " << snd_strerror(frames_read);
        }
        return frames_read;
    }

    if ((size_t)frames_read < frames) {
        Log::Warning() << " We read " << frames_read << " instead of " << frames;
    }

    return android::OK;
}

android::status_t AlsaAudioDevice::pcmWriteFrames(void *buffer, ssize_t frames, string &error) const
{
    snd_pcm_sframes_t frames_written = snd_pcm_writei(mPcmDevice, (char *)buffer, frames);
    if (frames_written < 0) {
        error = snd_strerror(frames_written);
        if(snd_pcm_recover(mPcmDevice, frames_written, 0) != android::OK) {
            Log::Error() << "Unable to recover from pcm_write, error: " << snd_strerror(frames_written);
        }
        return frames_written;
    }

    return android::OK;
}

uint32_t AlsaAudioDevice::getBufferSizeInBytes() const
{
    return snd_pcm_frames_to_bytes(mPcmDevice, getBufferSizeInFrames());
}

size_t AlsaAudioDevice::getBufferSizeInFrames() const
{
    snd_pcm_uframes_t buffer_size, period_size;
    int err = snd_pcm_get_params(mPcmDevice, &buffer_size, &period_size);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << " Unable to retrieve buffer size: " << snd_strerror(err);
    }

    return buffer_size;
}

android::status_t AlsaAudioDevice::getFramesAvailable(size_t &avail, struct timespec &tStamp) const
{
    snd_pcm_uframes_t availFrames;
    int err =  snd_pcm_htimestamp(mPcmDevice, &availFrames, &tStamp);
    if (err < 0) {
        Log::Error() << __FUNCTION__ << ": Unable to get available frames";
        return android::INVALID_OPERATION;
    }
    avail = availFrames;
    return android::OK;
}

android::status_t AlsaAudioDevice::pcmStop() const
{
    int err = snd_pcm_drain(mPcmDevice);
    Log::Error() << __FUNCTION__ << " draining samples returned " << snd_strerror(err);
    err = snd_pcm_close(mPcmDevice);
    Log::Error() << __FUNCTION__ << " pcm_close returned " << snd_strerror(err);

    return err;
}

} // namespace intel_audio
