/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (c) 2014-2015 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors.
 *
 * Title to the Material remains with Intel Corporation or its suppliers and
 * licensors. The Material contains trade secrets and proprietary and
 * confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and treaty
 * provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or disclosed
 * in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
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
