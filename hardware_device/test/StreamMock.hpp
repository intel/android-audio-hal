/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2014-2015 Intel
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
