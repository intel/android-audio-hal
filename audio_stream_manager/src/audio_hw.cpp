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

#include "Device.hpp"
#include <DeviceWrapper.hpp>
#include <hardware/hardware.h>

extern "C"
{

static struct hw_module_methods_t audio_module_methods = {
    .open = intel_audio::DeviceWrapper<intel_audio::Device, AUDIO_DEVICE_API_VERSION_3_0>::open
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
    .hal_api_version = HARDWARE_HAL_API_VERSION,
    .id = AUDIO_HARDWARE_MODULE_ID,
    .name = "Intel Audio HW HAL",
    .author = "Intel Corporation",
    .methods = &audio_module_methods,
    .dso = NULL,
    .reserved = {
        0
    },
};
} // extern "C"
