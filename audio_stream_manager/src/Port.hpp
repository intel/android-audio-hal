/*
 * Copyright (C) 2015 Intel Corporation
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

#include <system/audio.h>
#include <utils/Errors.h>
#include <string>

namespace intel_audio
{

class Port
{
public:
    Port();
    ~Port();

    /**
     * @return true if the patch is involved in at least a patch, false otherwise.
     */
    bool isUsed() const { return mRefCount != 0; }

    /**
     * Attaching a port to a patch. It doesn't do much than incrementing the reference counter.
     */
    void attach();

    /**
     * Releasing a port from a patch. It doesn't do much than decrementing the reference counter.
     */
    void release();

    /**
     * Set or Update (if existing port) the configuration of an audio port.
     * If trying to overwrite the port id, it asserts.
     * If trying to reassign the same mix port to another io handle mix, it warns.
     *
     * @param[in] config to be applyed to the port.
     */
    void updateConfig(const audio_port_config &config);

    /**
     * Get the configuration associated to the port.
     *
     * @return configuration of the audio port
     */
    const audio_port_config &getConfig() const { return mConfig; }

    /**
     * returns the handle, which is a unique ID, handle allocated by policy.
     */
    audio_port_handle_t getHandle() const { return mConfig.id; }
    audio_port_role_t getRole() const { return mConfig.role; }
    audio_port_type_t getType() const { return mConfig.type; }
    audio_devices_t getDeviceType() const { return mConfig.ext.device.type; }
    audio_io_handle_t getMixIoHandle() const { return mConfig.ext.mix.handle; }
    audio_source_t getMixUseCaseSource() const { return mConfig.ext.mix.usecase.source; }
    std::string getDeviceAddress() const { return mConfig.ext.device.address; }

private:
    /**
     * Audio Port configuration structure used to specify a particular configuration of an
     * audio port.
     */
    audio_port_config mConfig;

    uint32_t mRefCount; /**< Port usage reference counter. */
};

} // namespace intel_audio
