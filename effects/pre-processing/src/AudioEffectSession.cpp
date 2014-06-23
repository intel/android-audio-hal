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

#define LOG_TAG "IntelPreProcessingFx/EffectSession"

#include "AudioEffectSession.hpp"
#include "LpePreProcessing.hpp"
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

AudioEffectSession::AudioEffectSession(uint32_t sessionId)
    : mId(sessionId)
{
    init();
}

void AudioEffectSession::init()
{
    mIoHandle = mSessionNone;
    mSource = AUDIO_SOURCE_DEFAULT;
}

void AudioEffectSession::setIoHandle(int ioHandle)
{
    ALOGD("%s: setting io=%d for session %d", __FUNCTION__, ioHandle, mId);
    mIoHandle = ioHandle;
}

status_t AudioEffectSession::addEffect(AudioEffect *effect)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "trying to add null Effect");
    effect->setSession(this);
    mEffectsList.push_back(effect);
    return OK;
}

AudioEffect *AudioEffectSession::findEffectByUuid(const effect_uuid_t *uuid)
{
    AUDIOCOMMS_ASSERT(uuid != NULL, "Invalid UUID");
    EffectListIterator it;
    it = std::find_if(mEffectsList.begin(), mEffectsList.end(), std::bind2nd(MatchUuid(), uuid));

    return (it != mEffectsList.end()) ? *it : NULL;
}

status_t AudioEffectSession::createEffect(const effect_uuid_t *uuid, effect_handle_t *interface)
{
    AUDIOCOMMS_ASSERT(uuid != NULL, "Invalid UUID");
    AudioEffect *effect = findEffectByUuid(uuid);
    if (effect == NULL) {

        ALOGE("%s: could not find effect for requested uuid", __FUNCTION__);
        return BAD_VALUE;
    }
    ALOGD("%s: requesting to create effect %s on session %d",
          __FUNCTION__, effect->getDescriptor()->name, mId);
    // Set the interface handle
    *interface = effect->getHandle();

    mEffectsCreatedList.push_back(effect);

    return OK;
}

status_t AudioEffectSession::removeEffect(AudioEffect *effect)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "trying to remove null Effect");
    ALOGD("%s: requesting to remove effect %s on session %d",
          __FUNCTION__, effect->getDescriptor()->name, mId);
    mEffectsCreatedList.remove(effect);

    // Reset the session if no more effects are created on it.
    if (mEffectsCreatedList.empty()) {

        ALOGD("%s: no more effect within session=%d, reinitialising session", __FUNCTION__, mId);
        init();
    }

    return OK;
}
