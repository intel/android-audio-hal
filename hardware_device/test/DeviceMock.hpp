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

#include <DeviceInterface.hpp>
#include <StreamMock.hpp>

namespace intel_audio
{

class DeviceMock : public DeviceInterface
{
public:
    static const char *AudioHalName;
    virtual android::status_t openOutputStream(audio_io_handle_t /*handle*/,
                                               audio_devices_t /*devices*/,
                                               audio_output_flags_t /*flags*/,
                                               audio_config_t & /*config*/,
                                               StreamOutInterface * &stream,
                                               const std::string & /*address*/)
    {
        stream = new StreamOutMock();
        return android::OK;
    }

    virtual void closeOutputStream(StreamOutInterface *stream)
    {
        delete stream;
    }
    virtual android::status_t openInputStream(audio_io_handle_t handle,
                                              audio_devices_t devices,
                                              audio_config_t &config,
                                              StreamInInterface * &stream,
                                              audio_input_flags_t flags,
                                              const std::string &address,
                                              audio_source_t source)
    {
        stream = new StreamInMock();
        return android::OK;
    }

    virtual void closeInputStream(StreamInInterface *stream)
    {
        delete stream;
    }

    virtual android::status_t initCheck() const
    { return android::OK; }

    virtual android::status_t setVoiceVolume(float volume)
    { return android::OK; }

    virtual android::status_t setMasterVolume(float volume)
    { return android::OK; }

    virtual android::status_t getMasterVolume(float &volume) const
    { return android::OK; }

    virtual android::status_t setMasterMute(bool mute)
    { return android::OK; }

    virtual android::status_t getMasterMute(bool &muted) const
    {
        muted = true;
        return android::OK;
    }

    virtual android::status_t setMode(audio_mode_t mode)
    { return android::OK; }

    virtual android::status_t setMicMute(bool mute)
    { return android::OK; }

    virtual android::status_t getMicMute(bool &muted) const
    {
        return false;
        return android::OK;
    }

    virtual android::status_t setParameters(const std::string &keyValuePairs)
    { return android::OK; }

    virtual std::string getParameters(const std::string &keys) const
    { return strdup("Bla"); }

    virtual size_t getInputBufferSize(const audio_config_t &config) const
    { return 1234u; }

    virtual android::status_t dump(const int fd) const
    { return android::OK; }

    virtual android::status_t createAudioPatch(size_t sourcesCount,
                                               const struct audio_port_config *sources,
                                               size_t sinksCount,
                                               const struct audio_port_config *sinks,
                                               audio_patch_handle_t &handle)
    { return android::OK; }

    virtual android::status_t releaseAudioPatch(audio_patch_handle_t)
    { return android::OK; }

    virtual android::status_t setAudioPortConfig(const struct audio_port_config &)
    { return android::OK; }

    virtual android::status_t getAudioPort(struct audio_port &) const
    { return android::OK; }
};

} // namespace intel_audio
