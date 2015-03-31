/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2015 Intel
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
#pragma once

#include <system/audio.h>
#include <utils/Errors.h>

namespace intel_audio
{

class Port
{
public:
    Port();
    ~Port();

    /**
     * @return true if the patch is involved in at least a patch, false otherwise.
     */
    bool isUsed() const { return mRefCount != 0; }

    /**
     * Attaching a port to a patch. It doesn't do much than incrementing the reference counter.
     */
    void attach();

    /**
     * Releasing a port from a patch. It doesn't do much than decrementing the reference counter.
     */
    void release();

    /**
     * Set or Update (if existing port) the configuration of an audio port.
     * If trying to overwrite the port id, it asserts.
     * If trying to reassign the same mix port to another io handle mix, it warns.
     *
     * @param[in] config to be applyed to the port.
     */
    void updateConfig(const audio_port_config &config);

    /**
     * Get the configuration associated to the port.
     *
     * @return configuration of the audio port
     */
    const audio_port_config &getConfig() const { return mConfig; }

    /**
     * returns the handle, which is a unique ID, handle allocated by policy.
     */
    audio_port_handle_t getHandle() const { return mConfig.id; }
    audio_port_role_t getRole() const { return mConfig.role; }
    audio_port_type_t getType() const { return mConfig.type; }
    audio_devices_t getDeviceType() const { return mConfig.ext.device.type; }
    audio_io_handle_t getMixIoHandle() const { return mConfig.ext.mix.handle; }
    audio_source_t getMixUseCaseSource() const { return mConfig.ext.mix.usecase.source; }

private:
    /**
     * Audio Port configuration structure used to specify a particular configuration of an
     * audio port.
     */
    audio_port_config mConfig;

    uint32_t mRefCount; /**< Port usage reference counter. */
};

} // namespace intel_audio
