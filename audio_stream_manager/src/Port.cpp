/*
 * Copyright (C) 2015-2017 Intel Corporation
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

#define LOG_TAG "AudioPort"

#include "Port.hpp"
#include <typeconverter/TypeConverter.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <utils/String8.h>

using android::status_t;
using audio_comms::utilities::Log;
using namespace std;

namespace intel_audio
{

Port::Port()
    : mRefCount(0)
{
    mConfig.id = AUDIO_PORT_HANDLE_NONE;
    mConfig.ext.mix.handle = AUDIO_IO_HANDLE_NONE;
}

Port::~Port()
{
    // On port deletion, i.e. ref count is 0, remove the associated device from the
    // routed devices.
}

void Port::updateConfig(const audio_port_config &config)
{
    if (getHandle() != AUDIO_PORT_HANDLE_NONE) {
        // We are dealing with an existing port
        AUDIOCOMMS_ASSERT(getHandle() == config.id, "trying to overwrite port Id");

        if (getType() == AUDIO_PORT_TYPE_MIX && getMixIoHandle() != config.ext.mix.handle) {
            Log::Warning() << __FUNCTION__ << ": Changing mix port (Id= " << config.id << ") handle"
                           << "current=" << getMixIoHandle() << ", new=" << config.ext.mix.handle;
        }
    }
    Log::Verbose() << __FUNCTION__ << ": update config of port " << mConfig.id;
    mConfig = config;
}

void Port::attach()
{
    Log::Verbose() << __FUNCTION__ << ": increment counter of port " << mConfig.id;
    ++mRefCount;
}

void Port::release()
{
    if (mRefCount == 0) {
        return;
    }
    Log::Verbose() << __FUNCTION__ << ": decrement counter of port " << mConfig.id;
    if (--mRefCount == 0) {
        Log::Warning() << __FUNCTION__ << ": Port UNUSED " << mConfig.id;
    }
}

android::status_t Port::dump(const int fd, int spaces) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    android::String8 result;

    snprintf(buffer, SIZE, "%*s Port:\n", spaces, "");
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- handle: %d\n", spaces + 2, "", getHandle());
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- role: %s\n", spaces + 2, "",
             PortRoleConverter::toString(getRole()).c_str());
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- type: %s\n", spaces + 2, "",
             PortTypeConverter::toString(getType()).c_str());
    result.append(buffer);

    switch (getType()) {
    case AUDIO_PORT_TYPE_DEVICE:
        snprintf(buffer, SIZE, "%*s- device type: %s\n", spaces + 2, "",
                 DeviceConverter::maskToString(getDeviceType()).c_str());
        result.append(buffer);
        snprintf(buffer,
                 SIZE,
                 "%*s- device address: %s\n",
                 spaces + 2,
                 "",
                 mConfig.ext.device.address);
        result.append(buffer);
        break;
    case AUDIO_PORT_TYPE_MIX:
        snprintf(buffer, SIZE, "%*s- Mix IO handle: %d\n", spaces + 2, "", getMixIoHandle());
        result.append(buffer);
        if (getRole() == AUDIO_PORT_ROLE_SINK) {
            snprintf(buffer, SIZE, "%*s- Mix Use Case source: %s\n", spaces + 2, "",
                     InputSourceConverter::toString(getMixUseCaseSource()).c_str());
            result.append(buffer);
        } else {
            snprintf(buffer, SIZE, "%*s- Mix Use Case stream: %s\n", spaces + 2, "",
                     StreamTypeConverter::toString(mConfig.ext.mix.usecase.stream).c_str());
            result.append(buffer);
        }
        break;
    case AUDIO_PORT_TYPE_SESSION:
        snprintf(buffer, SIZE, "%*s- session: %d\n", spaces + 2, "", mConfig.ext.session.session);
        result.append(buffer);
        break;
    default:
        break;
    }
    snprintf(buffer, SIZE, "%*s- RefCount: %d\n", spaces + 2, "", mRefCount);
    result.append(buffer);

    write(fd, result.string(), result.size());
    return android::OK;
}

} // namespace intel_audio
