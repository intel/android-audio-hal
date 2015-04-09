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
#pragma once

namespace intel_audio
{

class VolumeKeys
{
public:
    /**
     * Enable or disable the wakeup from volume keys.
     *
     * @param[in] isEnabled true if enable from volume keys is requested, false otherwise.
     *
     * @return 0 is succesfull operation, -1 otherwise.
     */
    static int wakeup(bool isEnabled);

private:
    /**< tracks down state of wakeup property */
    static bool mWakeupEnabled;

    /**< used to represent the filepath to enabled_wakeup property file */
    static const char *const mGpioKeysWakeupEnable;

    /**< used to represent the filepath to disabled_wakeup property file */
    static const char *const mGpioKeysWakeupDisable;

    /**< id of volume down key */
    static const char *const mKeyVolumeDown;

    /**< id of volume up key */
    static const char *const mKeyVolumeUp;
};

} // namespace intel_audio
