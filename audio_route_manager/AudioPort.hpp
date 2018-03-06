/*
 * Copyright (C) 2013-2018 Intel Corporation
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
#include "StreamRouteConfig.hpp"
#include <utils/Log.h>
using namespace std;
namespace intel_audio
{

class IAudioDevice;

// base class of MixPort and DevicePort
class AudioPort
{
public:
    /**
     * Constructor of AudioPort
     * @param[in] name is parsed from the mixPort or devicePort
     * element of audio_policy_configuration.xml
     */
    AudioPort(const std::string &name)
        : mName(name)
    {}

    /**
     * Get the name of AudioPort.
     * @return mName
     */
    std::string getName() { return mName; }

private:
    std::string mName;
};

/**
 * Mix ports describe the possible config profiles
 * for streams that can be opened at the audio HAL for
 * playback and capture
 * MixPort instance is parsed from mixPort element
 * in audio_policy_configuration.xml
 */
class MixPort : public AudioPort
{
public:
    MixPort(const std::string &name, const bool isOut)
        : AudioPort(name), mIsOut(isOut)  {}

    /**
     * Set the device of MixPort
     * @param[in] device is the pointer of AlsaAudioDevcie or
     * TinyAlsaAudioDevice. The device instance is parsed from
     * the card attribute of mixPort element.
     */
    void setAlsaDevice(IAudioDevice *device) { mAudioDevice = device; }

    /**
     * Get the device of MixPort
     * @return the pointer of TinyAlsaAudioDevice or AlsaAudioDevice
     */
    IAudioDevice *getAlsaDevice() { return mAudioDevice; }

    /**
     * Set the stream route config
     * @param[in] config parsed from the profile attribute of
     * mixPort element of audio_policy_configuration.xml.
     */
    void setConfig(const StreamRouteConfig &config) { mConfig = config; }
    StreamRouteConfig &getConfig() { return mConfig; }

    /**
     * Return the direction of MixPort
     * @return true if role of MixPort is source, false if role is sink
     */
    bool isOut() { return mIsOut; }

    /**
     * Set the supported effects parsed from the mixPort
     * element of audio_policy_configuration.
     */
    void setEffectSupported(vector<string> effects)
    {
        mEffects = effects;
    }

private:
    bool mIsOut;
    StreamRouteConfig mConfig;
    IAudioDevice *mAudioDevice;
    vector<string> mEffects;
};

class AudioPorts : public std::vector<AudioPort *>
{
public:
    AudioPort *findByName(const std::string &name) const
    {
        for (auto port : *this) {
            if (port->getName() == name) {
                return port;
            }
        }
        return nullptr;
    }

    void dump()
    {
        for (auto port : *this) {
            ALOGI("port=%s", port->getName().c_str());
        }

    }
};

}
