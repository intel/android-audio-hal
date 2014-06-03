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

#define LOG_TAG "IntelPreProcessingFx"

#include "LpePreProcessingStub.hpp"
#include "AudioEffectSessionStub.hpp"
#include <LpeAec.hpp>
#include <LpeBmf.hpp>
#include <LpeWnr.hpp>
#include <LpeNs.hpp>
#include <LpeAgc.hpp>
#include <utils/Errors.h>
#include <fcntl.h>
#include <functional>
#include <algorithm>

// #define LOG_NDEBUG 0
#include <cutils/log.h>


using android::status_t;
using android::OK;
using android::BAD_VALUE;
using audio_comms::utilities::Mutex;

const struct effect_interface_s LpePreProcessingStub::effectInterface = {
    NULL, /**< process. Not implemented as this lib deals with HW effects. */
    /**
     * Send a command and receive a response to/from effect engine.
     */
    &LpePreProcessingStub::intelLpeFxCommand,
    &LpePreProcessingStub::intelLpeFxGetDescriptor, /**< get the effect descriptor. */
    NULL /**< process reverse. Not implemented as this lib deals with HW effects. */
};

int LpePreProcessingStub::intelLpeFxCommand(effect_handle_t interface,
                                            uint32_t cmdCode,
                                            uint32_t cmdSize,
                                            void *cmdData,
                                            uint32_t *replySize,
                                            void *replyData)
{
    LpePreProcessingStub *self = getInstance();
    AudioEffectStub *effect = self->findEffectByInterface(interface);
    if (effect == NULL) {

        ALOGE("%s: could not find effect for requested interface", __FUNCTION__);
        return BAD_VALUE;
    }

    switch (cmdCode) {
    case EFFECT_CMD_INIT:
        if (replyData == NULL || *replySize != sizeof(int)) {
            return -EINVAL;
        }
        effect->init();
        *static_cast<int *>(replyData) = 0;
        break;

    case EFFECT_CMD_SET_CONFIG:
        if (cmdData == NULL ||
            cmdSize != sizeof(effect_config_t) ||
            replyData == NULL ||
            *replySize != sizeof(int)) {
            ALOGV("%s EFFECT_CMD_SET_CONFIG: ERROR", __FUNCTION__);
            return -EINVAL;
        }
        *static_cast<int *>(replyData) = 0;
        break;

    case EFFECT_CMD_GET_CONFIG:
        if (replyData == NULL || *replySize != sizeof(effect_config_t)) {
            ALOGV("%s EFFECT_CMD_GET_CONFIG: ERROR", __FUNCTION__);
            return -EINVAL;
        }
        break;

    case EFFECT_CMD_SET_CONFIG_REVERSE:
        if (cmdData == NULL ||
            cmdSize != sizeof(effect_config_t) ||
            replyData == NULL ||
            *replySize != sizeof(int)) {
            ALOGV("%s EFFECT_CMD_SET_CONFIG_REVERSE: ERROR", __FUNCTION__);
            return -EINVAL;
        }
        *static_cast<int *>(replyData) = 0;
        break;

    case EFFECT_CMD_GET_CONFIG_REVERSE:
        if (replyData == NULL ||
            *replySize != sizeof(effect_config_t)) {
            ALOGV("%s EFFECT_CMD_GET_CONFIG_REVERSE: ERROR", __FUNCTION__);
            return -EINVAL;
        }
        break;

    case EFFECT_CMD_RESET:
        effect->reset();
        break;

    case EFFECT_CMD_GET_PARAM: {
        if (cmdData == NULL ||
            cmdSize < static_cast<int>(sizeof(effect_param_t)) ||
            replyData == NULL ||
            *replySize < static_cast<int>(sizeof(effect_param_t))) {
            ALOGV("%s EFFECT_CMD_GET_PARAM: ERROR", __FUNCTION__);
            return -EINVAL;
        }
        effect_param_t *p = static_cast<effect_param_t *>(cmdData);
        memcpy(replyData, cmdData, sizeof(effect_param_t) + p->psize);
        p = static_cast<effect_param_t *>(replyData);

        /**
         * The start of value field inside the data field is always on a 32 bit boundary
         * Need to take padding into consideration to point on value.
         * cf to audio_effect.h for the strucutre of the effect_params_s structure.
         */
        int voffset = ((p->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);

        effect->getParameter(p->data, static_cast<size_t  *>(&p->vsize), p->data + voffset);
        *replySize = sizeof(effect_param_t) + voffset + p->vsize;
        break;
    }

    case EFFECT_CMD_SET_PARAM: {
        if (cmdData == NULL ||
            cmdSize < static_cast<int>(sizeof(effect_param_t)) ||
            replyData == NULL ||
            *replySize != sizeof(int32_t)) {
            ALOGV("%s EFFECT_CMD_SET_PARAM: ERROR", __FUNCTION__);
            return -EINVAL;
        }
        effect_param_t *p = static_cast<effect_param_t *>(cmdData);

        if (p->psize != sizeof(int32_t)) {
            ALOGV("cmdCode Case: "
                  "EFFECT_CMD_SET_PARAM: ERROR, parameter size is not sizeof(int32_t)");
            return -EINVAL;
        }
        *static_cast<int *>(replyData) =
            effect->setParameter(static_cast<void *>(p->data), p->data + p->psize);
        break;
    }

    case EFFECT_CMD_ENABLE:
        if (replyData == NULL || *replySize != sizeof(int)) {
            ALOGV("%s EFFECT_CMD_ENABLE: ERROR", __FUNCTION__);
            return -EINVAL;
        }
        *static_cast<int *>(replyData) = 0;
        break;

    case EFFECT_CMD_DISABLE:
        if (replyData == NULL || *replySize != sizeof(int)) {
            ALOGV("%s EFFECT_CMD_DISABLE: ERROR", __FUNCTION__);
            return -EINVAL;
        }
        *static_cast<int *>(replyData) = 0;
        break;

    case EFFECT_CMD_SET_DEVICE:
    case EFFECT_CMD_SET_INPUT_DEVICE:
        if (cmdData == NULL ||
            cmdSize != sizeof(uint32_t)) {
            ALOGV("%s EFFECT_CMD_SET_INPUT_DEVICE: ERROR", __FUNCTION__);
            return -EINVAL;
        }
        effect->setDevice(*static_cast<uint32_t *>(cmdData));
        break;

    case EFFECT_CMD_SET_VOLUME:
    case EFFECT_CMD_SET_AUDIO_MODE:
    case EFFECT_CMD_SET_FEATURE_CONFIG:
        break;

    case EFFECT_CMD_SET_AUDIO_SOURCE:
        if (cmdData == NULL ||
            cmdSize != sizeof(uint32_t)) {
            ALOGV("%s EFFECT_CMD_SET_AUDIO_SOURCE: ERROR", __FUNCTION__);
            return -EINVAL;
        }
        *static_cast<int *>(replyData) = 0;
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

int LpePreProcessingStub::intelLpeFxGetDescriptor(effect_handle_t interface,
                                                  effect_descriptor_t *descriptor)
{
    LpePreProcessingStub *self = getInstance();
    AudioEffectStub *effect = self->findEffectByInterface(interface);
    if (effect == NULL) {

        ALOGE("%s: could not find effect for requested interface", __FUNCTION__);
        return BAD_VALUE;
    }
    memcpy(descriptor, effect->getDescriptor(), sizeof(effect_descriptor_t));
    return 0;
}

LpePreProcessingStub::LpePreProcessingStub()
{
    ALOGD("%s", __FUNCTION__);
    init();
}

status_t LpePreProcessingStub::getEffectDescriptor(const effect_uuid_t *uuid,
                                                   effect_descriptor_t *descriptor)
{
    if (descriptor == NULL || uuid == NULL) {

        ALOGE("%s: invalue interface and/or uuid", __FUNCTION__);
        return BAD_VALUE;
    }
    const AudioEffectStub *effect = findEffectByUuid(uuid);
    if (effect == NULL) {

        ALOGE("%s: could not find effect for requested uuid", __FUNCTION__);
        return BAD_VALUE;
    }
    const effect_descriptor_t *desc = effect->getDescriptor();
    if (desc == NULL) {

        ALOGE("%s: invalid effect descriptor", __FUNCTION__);
        return BAD_VALUE;
    }
    memcpy(descriptor, desc, sizeof(effect_descriptor_t));
    return OK;
}


status_t LpePreProcessingStub::createEffect(const effect_uuid_t *uuid,
                                            int32_t sessionId,
                                            int32_t ioId,
                                            effect_handle_t *interface)
{
    ALOGD("%s", __FUNCTION__);
    if (interface == NULL || uuid == NULL) {

        ALOGE("%s: invalue interface and/or uuid", __FUNCTION__);
        return BAD_VALUE;
    }
    Mutex::Locker Locker(_lpeEffectsLock);
    AudioEffectSessionStub *session = getSession(ioId);
    if (session == NULL) {

        ALOGE("%s: could not get session for sessionId=%d, ioHandleId=%d",
              __FUNCTION__, sessionId, ioId);
        return BAD_VALUE;
    }
    return session->createEffect(uuid, interface) == OK ?
           OK :
           BAD_VALUE;
}


status_t LpePreProcessingStub::releaseEffect(effect_handle_t interface)
{
    ALOGD("%s", __FUNCTION__);
    Mutex::Locker Locker(_lpeEffectsLock);
    AudioEffectStub *effect = findEffectByInterface(interface);
    if (effect == NULL) {

        ALOGE("%s: could not find effect for requested interface", __FUNCTION__);
        return BAD_VALUE;
    }
    AudioEffectSessionStub *session = effect->getSession();
    if (session == NULL) {

        ALOGE("%s: no session for effect", __FUNCTION__);
        return BAD_VALUE;
    }
    return session->removeEffect(effect);
}

status_t LpePreProcessingStub::init()
{
    for (uint32_t i = 0; i < _maxEffectSessions; i++) {

        AudioEffectSessionStub *effectSession = new AudioEffectSessionStub(i);

        // Each session has an instance of effects provided by LPE
        AgcAudioEffect *agc = new AgcAudioEffect(&effectInterface);
        _effectsList.push_back(agc);
        effectSession->addEffect(agc);
        NsAudioEffect *ns = new NsAudioEffect(&effectInterface);
        _effectsList.push_back(ns);
        effectSession->addEffect(ns);
        AecAudioEffect *aec = new AecAudioEffect(&effectInterface);
        _effectsList.push_back(aec);
        effectSession->addEffect(aec);

        BmfAudioEffect *bmf = new BmfAudioEffect(&effectInterface);
        _effectsList.push_back(bmf);
        effectSession->addEffect(bmf);
        WnrAudioEffect *wnr = new WnrAudioEffect(&effectInterface);
        _effectsList.push_back(wnr);
        effectSession->addEffect(wnr);

        _effectSessionsList.push_back(effectSession);
    }
    return OK;
}

// Function to be used as the predicate in find_if call.
struct MatchUuid : public std::binary_function<AudioEffectStub *, const effect_uuid_t *, bool>
{

    bool operator()(const AudioEffectStub *effect,
                    const effect_uuid_t *uuid) const
    {

        return memcmp(effect->getUuid(), uuid, sizeof(effect_uuid_t)) == 0;
    }
};

AudioEffectStub *LpePreProcessingStub::findEffectByUuid(const effect_uuid_t *uuid)
{
    EffectListIterator it;
    it = std::find_if(_effectsList.begin(), _effectsList.end(), std::bind2nd(MatchUuid(), uuid));

    return (it != _effectsList.end()) ? *it : NULL;
}

/**
 * Function to be used as the predicate in find_if call.
 */
struct MatchInterface : public std::binary_function<AudioEffectStub *, const effect_handle_t, bool>
{

    bool operator()(AudioEffectStub *effect,
                    const effect_handle_t interface) const
    {

        return effect->getHandle() == interface;
    }
};

AudioEffectStub *LpePreProcessingStub::findEffectByInterface(const effect_handle_t interface)
{
    EffectListIterator it;
    it = std::find_if(_effectsList.begin(), _effectsList.end(),
                      std::bind2nd(MatchInterface(), interface));

    return (it != _effectsList.end()) ? *it : NULL;
}

/**
 * Function to be used as the predicate in find_if call.
 */
struct MatchSession : public std::binary_function<AudioEffectSessionStub *, int, bool>
{

    bool operator()(const AudioEffectSessionStub *effectSession,
                    int ioId) const
    {
        return effectSession->getIoHandle() == ioId;
    }
};

AudioEffectSessionStub *LpePreProcessingStub::findSession(int ioId)
{
    EffectSessionListIterator it;
    it = std::find_if(_effectSessionsList.begin(), _effectSessionsList.end(),
                      std::bind2nd(MatchSession(), ioId));

    return (it != _effectSessionsList.end()) ? *it : NULL;
}

AudioEffectSessionStub *LpePreProcessingStub::getSession(uint32_t ioId)
{
    AudioEffectSessionStub *session = findSession(ioId);
    if (session == NULL) {

        ALOGD("%s: no session assigned for io=%d, try to get one...", __FUNCTION__, ioId);
        session = findSession(AudioEffectSessionStub::_sessionNone);
        if (session != NULL) {

            session->setIoHandle(ioId);
        }
    }
    return session;
}

LpePreProcessingStub *LpePreProcessingStub::getInstance()
{
    static LpePreProcessingStub instance;
    return &instance;
}
