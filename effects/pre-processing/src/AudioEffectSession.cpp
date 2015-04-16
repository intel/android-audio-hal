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

#define LOG_TAG "IntelPreProcessingFx/EffectSession"

#include "AudioEffectSession.hpp"
#include "LpePreProcessing.hpp"
#include <audio_effects/effect_aec.h>
#include <audio_effects/effect_ns.h>
#include <audio_effects/effect_agc.h>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <functional>
#include <algorithm>

using android::status_t;
using android::OK;
using android::BAD_VALUE;
using audio_comms::utilities::Log;

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
    Log::Debug() << __FUNCTION__ << ": setting io=" << ioHandle << " for session=" << mId;
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
        Log::Error() << __FUNCTION__ << ": could not find effect for requested uuid";
        return BAD_VALUE;
    }
    Log::Debug() << __FUNCTION__ << ": requesting to create effect "
                 << effect->getDescriptor()->name << " on session=" << mId;
    // Set the interface handle
    *interface = effect->getHandle();

    mEffectsCreatedList.push_back(effect);

    return OK;
}

status_t AudioEffectSession::removeEffect(AudioEffect *effect)
{
    AUDIOCOMMS_ASSERT(effect != NULL, "trying to remove null Effect");
    Log::Debug() << __FUNCTION__ << ": requesting to remove effect "
                 << effect->getDescriptor()->name << " on session=" << mId;
    mEffectsCreatedList.remove(effect);

    // Reset the session if no more effects are created on it.
    if (mEffectsCreatedList.empty()) {
        Log::Debug() << __FUNCTION__ << ": no more effect within session=" << mId
                     << ", reinitialising session";
        init();
    }

    return OK;
}
