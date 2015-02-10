/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2015 Intel
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
 *
 */

#include "Device.hpp"
#include <DeviceWrapper.hpp>
#include <hardware/hardware.h>

extern "C"
{

static struct hw_module_methods_t audio_module_methods = {
open: intel_audio::DeviceWrapper<intel_audio::Device, AUDIO_DEVICE_API_VERSION_2_0>::open
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
tag: HARDWARE_MODULE_TAG,
module_api_version: AUDIO_MODULE_API_VERSION_0_1,
hal_api_version: HARDWARE_HAL_API_VERSION,
id: AUDIO_HARDWARE_MODULE_ID,
name: "Intel Audio HW HAL",
author: "Intel Corporation",
methods: &audio_module_methods,
dso: NULL,
reserved:
    {
        0
    },
};
} // extern "C"
