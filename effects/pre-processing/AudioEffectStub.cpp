/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
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

#define LOG_TAG "IntelPreProcessingFx/Effect"

#include "AudioEffectStub.hpp"
#include "LpePreProcessingStub.hpp"
#include "AudioEffectSessionStub.hpp"
#include <AudioCommsAssert.hpp>
#include <cutils/log.h>

AudioEffectStub::AudioEffectStub(const effect_interface_s *itfe,
                                 const effect_descriptor_t *descriptor)
    : mDescriptor(descriptor),
      mItfe(itfe),
      mSession(NULL)
{
}

AudioEffectStub::~AudioEffectStub()
{
}

void AudioEffectStub::setSession(AudioEffectSessionStub *session)
{
    AUDIOCOMMS_ASSERT(mSession == NULL, "Effects already in use for a session");
    mSession = session;
}

const effect_uuid_t *AudioEffectStub::getUuid() const
{
    AUDIOCOMMS_ASSERT(mDescriptor != NULL, "Effects descriptor invalid");
    return &mDescriptor->uuid;
}

int AudioEffectStub::create()
{
    ALOGV("%s: NOP", __FUNCTION__);
    return 0;
}

int AudioEffectStub::init()
{
    ALOGV("%s: NOP", __FUNCTION__);
    return 0;
}

int AudioEffectStub::reset()
{
    ALOGV("%s: NOP", __FUNCTION__);
    return 0;
}

void AudioEffectStub::enable()
{
    ALOGV("%s: NOP", __FUNCTION__);
}

void AudioEffectStub::disable()
{
    ALOGV("%s: NOP", __FUNCTION__);
}

int AudioEffectStub::setParameter(void *param, void *value)
{
    ALOGV("%s: NOP", __FUNCTION__);
    return 0;
}

int AudioEffectStub::getParameter(void *param, size_t *size, void *value)
{
    ALOGV("%s: NOP", __FUNCTION__);
    return 0;
}

int AudioEffectStub::setDevice(uint32_t device)
{
    ALOGV("%s: NOP", __FUNCTION__);
    return 0;
}
