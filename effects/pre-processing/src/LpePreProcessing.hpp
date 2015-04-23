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

#include <hardware/audio_effect.h>
#include <NonCopyable.hpp>
#include <list>
#include <utils/Errors.h>
#include <Mutex.hpp>

class AudioEffect;
class AudioEffectSession;

class LpePreProcessing : private audio_comms::utilities::NonCopyable
{
private:
    typedef std::list<AudioEffect *>::iterator EffectListIterator;
    typedef std::list<AudioEffect *>::const_iterator EffectListConstIterator;
    typedef std::list<AudioEffectSession *>::iterator EffectSessionListIterator;
    typedef std::list<AudioEffectSession *>::const_iterator EffectSessionListConstIterator;

public:
    LpePreProcessing();

    /**
     * Create an effect
     * Creates an effect engine of the specified implementation uuid and
     * returns an effect control interface on this engine. The function will allocate the
     * resources for an instance of the requested effect engine and return
     * a handle on the effect control interface.
     *
     * @param[in] uuid  pointer to the effect uuid.
     * @param[in] sessionId audio session to which this effect instance will be attached.
     *              All effects created with the same session ID are connected in series and
     *              process the same signal stream.
     *              Knowing that two effects are part of the same effect chain can help the
     *              library implement some kind of optimizations.
     * @param[in] ioId identifies the output or input stream this effect is directed to audio HAL.
     *              For future use especially with tunneled HW accelerated effects
     * @param[out] interface effect interface handle.
     *
     * @return 0 if success, effect interface handle is valid,
     *         -ENODEV is library failed to initialize
     *         -ENOENT     no effect with this uuid found
     *         -EINVAL if invalid interface handle
     */
    android::status_t createEffect(const effect_uuid_t *uuid,
                                   int32_t sessionId,
                                   int32_t ioId,
                                   effect_handle_t *interface);

    /**
     * Release an effect
     * Releases the effect engine whose handle is given as argument.
     * All resources allocated to this particular instance of the effect are released.
     *
     * @param[in] interface handle on the effect interface to be released
     *
     * @return 0 if success,
     *         -ENODEV is library failed to initialize
     *         -EINVAL if invalid interface handle
     */
    android::status_t releaseEffect(effect_handle_t interface);

    /**
     * get effet descriptor.
     * Returns the descriptor of the effect engine which implementation UUID is
     * given as argument.
     *
     * @param[in] uuid pointer to the effect uuid.
     * @param[out] descriptor effect descriptor
     *
     * @return 0 if success, in this case, descriptor is set correctly,
     *         -ENODEV is library failed to initialize
     *         -EINVAL if invalid descriptor or uuid
     */
    android::status_t getEffectDescriptor(const effect_uuid_t *uuid,
                                          effect_descriptor_t *descriptor);


    static LpePreProcessing *getInstance();

private:
    android::status_t init();

    /**
     * Retrieve the AudioEffect instance from the uuid.
     *
     * @param[in] uuid handle on the audio effect.
     *
     * @return AudioEffect instance if interface is valid, NULL otherwise
     */
    AudioEffect *findEffectByUuid(const effect_uuid_t *uuid);

    /**
     * Retrieve the AudioEffect instance from the handle.
     *
     * @param[in] interface handle on the audio effect.
     *
     * @return AudioEffect instance if interface is valid, NULL otherwise
     */
    AudioEffect *findEffectByInterface(const effect_handle_t interface);

    /**
     * Return the session object for the request Id and IO handle Id
     *
     * @param[in] ioId Id of the input stream
     *
     * @return AudioEffectSession handle
     */
    AudioEffectSession *findSession(int ioId);

    /**
     * Get a session for a ioHandle.
     * Either a session already exists for this handle, so returns this one, or returns the first
     * available session (ie session without iohandle attached to it).
     *
     * @param[in] ioId Id of the input stream
     *
     * @return AudioEffectSession handle
     */
    AudioEffectSession *getSession(uint32_t ioId);

    /**
     * List of Audio Effects available on LPE
     */
    std::list<AudioEffect *> mEffectsList;

    /**
     * List of Audio Effect Sessions available on LPE
     */
    std::list<AudioEffectSession *> mEffectSessionsList;

    static const uint32_t mMaxEffectSessions = 8; /**< Max number of sessions for effect bundle. */

    /**
     * Send a command and receive a response to/from effect engine.
     *
     * @param[in] self handle to the effect interface this function is called on.
     * @param[in] cmdCode command code: the command can be a standardized command defined in
     *                    effect_command_e (see below) or a proprietary command.
     * @param[in] cmdSize size of command in bytes
     * @param[in] cmdData pointer to command data
     * @param[in] replyData pointer to reply data
     * @param[in:out] replySize maximum size of reply data as input
     *                          actual size of reply data as output
     *
     * @return  0       successful operation
     *                           -EINVAL invalid interface handle or
     *                                invalid command/reply size or format according to command code
     *              The return code should be restricted to indicate problems related to the
     *              API specification. Status related to the execution of a particular command
     *              should be indicated as part of the reply field.
     *              replyData updated with command response
     */
    static int intelLpeFxCommand(effect_handle_t  self,
                                 uint32_t cmdCode,
                                 uint32_t cmdSize,
                                 void *cmdData,
                                 uint32_t *replySize,
                                 void *replyData);


    /**
     * Returns the effect descriptor.
     *
     * @param[in] self handle to the effect interface this function is called on.
     * @param[out] descriptor address where to return the effect descriptor.
     *
     * @return 0  successful operation, descriptor is updated with the effect descriptor.
     *         -EINVAL invalid interface handle or invalid pDescriptor
     */
    static int intelLpeFxGetDescriptor(effect_handle_t self,
                                       effect_descriptor_t *descriptor);

    /**
     * Effect interface structure.
     * It will be used as a unique entry point to interact with audio effects by
     * the audio effect module.
     */
    static const struct effect_interface_s mEffectInterface;

    /**
     * Effects are handled in a separated thread, need to lock to protect concurrent
     * access to the input stream.
     */
    audio_comms::utilities::Mutex mLpeEffectsLock;
};
