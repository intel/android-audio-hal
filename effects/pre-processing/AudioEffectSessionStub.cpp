/*
 * INTEL CONFIDENTIAL
 * Copyright © 2013 Intel
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
 * disclosed in any way without Intel’s prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */

#define LOG_TAG "IntelPreProcessingFx/EffectSession"

#include "AudioEffectSessionStub.hpp"
#include "LpePreProcessingStub.hpp"
#include <audio_effects/effect_aec.h>
#include <audio_effects/effect_ns.h>
#include <audio_effects/effect_agc.h>
#include <AudioCommsAssert.hpp>
#include <functional>
#include <algorithm>

#include <cutils/log.h>

using android::status_t;
using android::OK;
using android::BAD_VALUE;

AudioEffectSessionStub::AudioEffectSessionStub(uint32_t sessionId)
    : _id(sessionId)
{
    init();
}

void AudioEffectSessionStub::init()
{
    _ioHandle = _sessionNone;
    _source = AUDIO_SOURCE_DEFAULT;
}

void AudioEffectSessionStub::setIoHandle(int ioHandle)
{
    ALOGD("%s: setting io=%d for session %d", __FUNCTION__, ioHandle, _id);
    _ioHandle = ioHandle;
}

status_t AudioEffectSessionStub::addEffect(AudioEffectStub *effect)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "trying to add null Effect");
    effect->setSession(this);
    _effectsList.push_back(effect);
    return OK;
}

AudioEffectStub *AudioEffectSessionStub::findEffectByUuid(const effect_uuid_t *uuid)
{
    AUDIOCOMMS_ASSERT(uuid != NULL, "Invalid UUID");
    EffectListIterator it;
    it = std::find_if(_effectsList.begin(), _effectsList.end(), std::bind2nd(MatchUuid(), uuid));

    return (it != _effectsList.end()) ? *it : NULL;
}

status_t AudioEffectSessionStub::createEffect(const effect_uuid_t *uuid, effect_handle_t *interface)
{
    AUDIOCOMMS_ASSERT(uuid != NULL, "Invalid UUID");
    AudioEffectStub *effect = findEffectByUuid(uuid);
    if (effect == NULL) {

        ALOGE("%s: could not find effect for requested uuid", __FUNCTION__);
        return BAD_VALUE;
    }
    ALOGD("%s: requesting to create effect %s on session %d",
          __FUNCTION__, effect->getDescriptor()->name, _id);
    // Set the interface handle
    *interface = effect->getHandle();

    _effectsCreatedList.push_back(effect);

    return OK;
}

status_t AudioEffectSessionStub::removeEffect(AudioEffectStub *effect)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "trying to remove null Effect");
    ALOGD("%s: requesting to remove effect %s on session %d",
          __FUNCTION__, effect->getDescriptor()->name, _id);
    _effectsCreatedList.remove(effect);

    // Reset the session if no more effects are created on it.
    if (_effectsCreatedList.empty()) {

        ALOGD("%s: no more effect within session=%d, reinitialising session", __FUNCTION__, _id);
        init();
    }

    return OK;
}
