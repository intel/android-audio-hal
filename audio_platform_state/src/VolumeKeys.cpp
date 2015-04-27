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
#define LOG_TAG "VolumeKeys"

#include "VolumeKeys.hpp"
#include <utilities/Log.hpp>
#include <fcntl.h>

using audio_comms::utilities::Log;

namespace intel_audio
{

const char *const VolumeKeys::mGpioKeysWakeupEnable =
    "/sys/devices/platform/gpio-keys/enabled_wakeup";
const char *const VolumeKeys::mGpioKeysWakeupDisable =
    "/sys/devices/platform/gpio-keys/disabled_wakeup";

const char *const VolumeKeys::mKeyVolumeDown = "114";
const char *const VolumeKeys::mKeyVolumeUp = "115";

bool VolumeKeys::mWakeupEnabled = false;

int VolumeKeys::wakeup(bool isEnabled)
{
    if (mWakeupEnabled == isEnabled) {

        // Nothing to do, bailing out
        return 0;
    }
    Log::Debug() << __FUNCTION__ << ": volume keys wakeup=" << (isEnabled ? "enabled" : "disabled");

    int fd;
    int rc;

    const char *const gpioKeysWakeup =
        isEnabled ? mGpioKeysWakeupEnable : mGpioKeysWakeupDisable;
    fd = open(gpioKeysWakeup, O_RDWR);
    if (fd < 0) {
        Log::Error() << __FUNCTION__ << ": " << (isEnabled ? "enable" : "disable")
                     << " failed: Cannot open sysfs gpio-keys interface ("
                     << fd << ")";
        return -1;
    }
    rc = write(fd, mKeyVolumeDown, sizeof(mKeyVolumeDown));
    rc += write(fd, mKeyVolumeUp, sizeof(mKeyVolumeUp));
    close(fd);
    if (rc != (sizeof(mKeyVolumeDown) + sizeof(mKeyVolumeUp))) {
        Log::Error() << __FUNCTION__ << ": " << (isEnabled ? "enable" : "disable")
                     << " failed: sysfs gpio-keys write error";
        return -1;
    }
    mWakeupEnabled = isEnabled;
    Log::Debug() << __FUNCTION__ << ": " << (isEnabled ? "enable" : "disable")
                 << ": OK";
    return 0;
}

} // namespace intel_audio
