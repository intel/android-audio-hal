/*
 * Copyright (C) 2013-2016 Intel Corporation
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

#define LOG_TAG "IntelPreProcessingFx"

#include "LpeAgc.hpp"
#include <audio_effects/effect_agc.h>

AgcAudioEffect::AgcAudioEffect(const effect_interface_s *itfe)
    : AudioEffect(itfe, &mAgcDescriptor)
{

}

const effect_descriptor_t AgcAudioEffect::mAgcDescriptor = {
    .type =         FX_IID_AGC_,
    .uuid =         {
        .timeLow = 0x4e188f80,
        .timeMid = 0x3c8b,
        .timeHiAndVersion = 0x11e3,
        .clockSeq = 0xa20d,
        .node = { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b }
    },
    .apiVersion =   EFFECT_CONTROL_API_VERSION,
    .flags =        (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND),
    .cpuLoad =      0,
    .memoryUsage =  0,
    "Automatic Gain Control",  /**< name. */
    "IntelLPE"                 /**< implementor. */
};
