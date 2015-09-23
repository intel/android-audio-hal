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
#include <utils/String8.h>
#include <string>

class AudioEffectSession;

class AudioEffect : private audio_comms::utilities::NonCopyable
{
public:
    AudioEffect(const struct effect_interface_s *itfe, const effect_descriptor_t *descriptor);

    virtual ~AudioEffect();

    /**
     * Create the effect.
     *
     * @return 0 if success, error code otherwise.
     */
    virtual int create();

    /**
     * Initialize the effect.
     *
     * @return 0 if success, error code otherwise.
     */
    virtual int init();

    /**
     * Reset the effect.
     *
     * @return 0 if success, error code otherwise.
     */
    virtual int reset();

    /**
     * Enable the effect.
     * Called by the framework before the first call to process.
     */
    virtual void enable();

    /**
     * Disable the effect.
     * Called by the framework after the last call to process.
     */
    virtual void disable();

    /**
     * Set Parameter to the effect.
     *
     * @param[in] param parameter effect structure
     *
     * @return 0 if success, error code otherwise.
     */
    virtual int setParameter(const effect_param_t *param);

    /**
     * Get the effect parameter.
     *
     * @param[in,out] param parameter effect structure
     *
     * @return 0 if success, error code otherwise.
     */
    virtual int getParameter(effect_param_t *param) const;

    /**
     * Set the effect rendering device.
     *
     * @param[in] device mask of the devices.
     *
     * @return 0 if success, error code otherwise.
     */
    virtual int setDevice(uint32_t device);

    /**
     * Get the effect descriptor.
     *
     * @return effect descriptor structure.
     */
    const effect_descriptor_t *getDescriptor() const { return mDescriptor; }

    /**
     * Get the effect UUID.
     *
     * @return Unique effect ID.
     */
    const effect_uuid_t *getUuid() const;

    /**
     * Set a Session to attach to this effect.
     *
     * @param[in] session to attach.
     */
    android::status_t setSession(AudioEffectSession *session);

    /**
     * Get Session attached to this effect.
     *
     * @return session.
     */
    AudioEffectSession *getSession() const { return mSession; }

    /**
     * Get effect Handle.
     * Note that by design choice of Android, effect interface address is considered
     * as the handle of the effect.
     *
     * @return address the of the effect interface.
     */
    effect_handle_t getHandle() { return (effect_handle_t)(&mItfe); }

private:
    /**
     * Extract from the effect_param_t structure the parameter Id.
     * It supports only single parameter. If more than one paramId is found in the structure,
     * it will return an error code.
     *
     * @param[in] param: Effect Parameter structure
     * @param[out] paramId: retrieve param Id, valid only if return code is 0.
     *
     * @return 0 if the paramId could be extracted, error code otherwise.
     */
    int getParamId(const effect_param_t *param, int32_t &paramId) const;

    /**
     * Format the Parameter key from the effect parameter structure.
     * The followed formalism is:
     *      <human readable type name>-<paramId>[-<subParamId1>-<subParamId2>-...]
     *
     * @param[in] param: Effect Parameter structure
     *
     * @return valid AudioParameter key if success, empty key if failure.
     */
    int formatParamKey(const effect_param_t *param, android::String8 &key) const;

    /**
     * Effect Descriptor structure.
     * The effect descriptor contains necessary information to facilitate the enumeration of the
     * effect.
     */
    const effect_descriptor_t *mDescriptor;
    const struct effect_interface_s *mItfe; /**< Effect control interface structure. */
    uint32_t mPreProcessorId; /**< type of preprocessor. */
    uint32_t mState; /**< state of the effect. */
    AudioEffectSession *mSession; /**< Session on which the effect is on. */
    static const std::string mParamKeyDelimiter; /**< Delimiter chosen to format the key. */
};
