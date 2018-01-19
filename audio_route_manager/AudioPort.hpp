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
#include <utils/Log.h>
using namespace std;
namespace intel_audio
{
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
