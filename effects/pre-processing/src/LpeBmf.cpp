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

#include "LpeBmf.hpp"

BmfAudioEffect::BmfAudioEffect(const effect_interface_s *itfe)
    : AudioEffect(itfe, &mBmfDescriptor)
{
}

static const effect_uuid_t FX_IID_BMF_ = {
    .timeLow = 0x30927220,
    .timeMid = 0xbfb0,
    .timeHiAndVersion = 0x11e3,
    .clockSeq = 0xb03a,
    .node = { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b }
};

const effect_descriptor_t BmfAudioEffect::mBmfDescriptor = {
    .type =         FX_IID_BMF_,
    .uuid =         {
        .timeLow = 0xff619c00,
        .timeMid = 0xc458,
        .timeHiAndVersion = 0x11e3,
        .clockSeq = 0x9af1,
        .node = { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b }
    },
    .apiVersion =   EFFECT_CONTROL_API_VERSION,
    .flags =        (EFFECT_FLAG_TYPE_PRE_PROC | EFFECT_FLAG_DEVICE_IND),
    .cpuLoad =      0,
    .memoryUsage =  0,
    "Beam Forming",                 /**< name. */
    "IntelLPE"                 /**< implementor. */
};
