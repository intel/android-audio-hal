/*
 * Copyright (C) 2014-2015 Intel Corporation
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

#pragma once

#include <StreamInterface.hpp>


namespace intel_audio
{

class StreamOutMock : public StreamOutInterface
{
public:
    virtual uint32_t getSampleRate() const { return 1234u; }
    virtual android::status_t setSampleRate(uint32_t rate) { return android::OK; }
    virtual size_t getBufferSize() const { return 54321u; }
    virtual audio_channel_mask_t getChannels() const { return 7; }
    virtual audio_format_t getFormat() const { return AUDIO_FORMAT_PCM_32_BIT; }
    virtual android::status_t setFormat(audio_format_t format) { return android::OK; }
    virtual android::status_t standby() { return android::OK; }
    virtual android::status_t dump(int fd) const { return android::OK; }
    virtual audio_devices_t getDevice() const { return AUDIO_DEVICE_OUT_HDMI; }
    virtual android::status_t setDevice(audio_devices_t device) { return android::OK; }
    virtual std::string getParameters(const std::string &keys) const { return "glaaaa"; }
    virtual android::status_t setParameters(const std::string &keyValuePairs)
    {
        return android::OK;
    }
    virtual android::status_t addAudioEffect(effect_handle_t effect) { return android::OK; }
    virtual android::status_t removeAudioEffect(effect_handle_t effect) { return android::OK; }

    virtual uint32_t getLatency() { return 888u; }
    virtual android::status_t setVolume(float left, float right) { return android::OK; }
    virtual android::status_t write(const void *buffer, size_t &bytes) { return android::OK; }
    virtual android::status_t getRenderPosition(uint32_t &dspFrames) const { return android::OK; }
    virtual android::status_t getNextWriteTimestamp(int64_t &timestamp) const
    {
        return android::OK;
    }
    virtual android::status_t flush() { return android::OK; }
    virtual android::status_t setCallback(stream_callback_t callback,
                                          void *cookie) { return android::OK; }
    virtual android::status_t pause() { return android::OK; }
    virtual android::status_t resume() { return android::OK; }
    virtual android::status_t drain(audio_drain_type_t type) { return android::OK; }
    virtual android::status_t getPresentationPosition(uint64_t &frames,
                                                      struct timespec &timestamp) const
    {
        return android::OK;
    }
};

class StreamInMock : public StreamInInterface
{
public:
    virtual uint32_t getSampleRate() const { return 1234u; }
    virtual android::status_t setSampleRate(uint32_t rate) { return android::OK; }
    virtual size_t getBufferSize() const { return 11155u; }
    virtual audio_channel_mask_t getChannels() const { return 5; }
    virtual audio_format_t getFormat() const { return AUDIO_FORMAT_PCM_16_BIT; }
    virtual android::status_t setFormat(audio_format_t format) { return android::OK; }
    virtual android::status_t standby() { return android::OK; }
    virtual android::status_t dump(int fd) const { return android::OK; }
    virtual audio_devices_t getDevice() const { return AUDIO_DEVICE_OUT_AUX_DIGITAL; }
    virtual android::status_t setDevice(audio_devices_t device) { return android::OK; }
    virtual std::string getParameters(const std::string &keys) const { return "Input"; }
    virtual android::status_t setParameters(const std::string &keyValuePairs)
    {
        return android::OK;
    }
    virtual android::status_t addAudioEffect(effect_handle_t effect) { return android::OK; }
    virtual android::status_t removeAudioEffect(effect_handle_t effect) { return android::OK; }

    virtual android::status_t getPresentationPosition(uint64_t &frames,
                                                      struct timespec &timestamp) const
    {
        return android::OK;
    }
    virtual android::status_t setGain(float gain) { return android::OK; }
    virtual android::status_t read(void *buffer, size_t &bytes) { return android::OK; }
    virtual uint32_t getInputFramesLost() const { return 15; }
};

} // namespace intel_audio
