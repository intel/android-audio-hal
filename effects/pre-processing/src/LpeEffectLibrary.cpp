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

#include "LpePreProcessing.hpp"

extern "C" {

/**
 * Create an effect
 * Creates an effect engine of the specified implementation uuid and
 * returns an effect control interface on this engine. The function will allocate the
 * resources for an instance of the requested effect engine and return
 * a handle on the effect control interface.
 *
 * @param[in] uuid  pointer to the effect uuid.
 * @param[in] sessionId audio session to which this effect instance will be attached. All effects
 *              created with the same session ID are connected in series and process the same signal
 *              stream. Knowing that two effects are part of the same effect chain can help the
 *              library implement some kind of optimizations.
 * @param[in] ioId identifies the output or input stream this effect is directed to at audio HAL.
 *              For future use especially with tunneled HW accelerated effects
 * @param[out] interface effect interface handle.
 *
 * @return 0 if success, effect interface handle is valid,
 *         -ENODEV is library failed to initialize
 *         -ENOENT     no effect with this uuid found
 *         -EINVAL if invalid interface handle
 */
int lpeCreate(const effect_uuid_t *uuid,
              int32_t sessionId,
              int32_t ioId,
              effect_handle_t *interface)
{
    LpePreProcessing *lpeProcessing = LpePreProcessing::getInstance();
    return lpeProcessing->createEffect(uuid, sessionId, ioId, interface);
}

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
int lpeRelease(effect_handle_t interface)
{
    LpePreProcessing *lpeProcessing = LpePreProcessing::getInstance();
    return lpeProcessing->releaseEffect(interface);
}

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
int lpeGetDescriptor(const effect_uuid_t *uuid, effect_descriptor_t *descriptor)
{
    LpePreProcessing *lpeProcessing = LpePreProcessing::getInstance();
    lpeProcessing->getEffectDescriptor(uuid, descriptor);
    return 0;
}

/**
 * Audio Effect Library entry point structure.
 * Every effect library must have a data structure named AUDIO_EFFECT_LIBRARY_INFO_SYM
 * and the fields of this data structure must begin with audio_effect_library_t
 */
audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM = {
    .tag = AUDIO_EFFECT_LIBRARY_TAG,
    .version = EFFECT_LIBRARY_API_VERSION,
    .name = "Intel LPE Preprocessing Library",
    .implementor = "Intel",
    .create_effect = lpeCreate,
    .release_effect = lpeRelease,
    .get_descriptor = lpeGetDescriptor
};

} // extern "C"
