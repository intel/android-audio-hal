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
#pragma once

#include <utils/Errors.h>
#include <utils/String8.h>
#include <media/AudioParameter.h>

namespace android_audio_legacy
{

class AudioParameterHandler
{
public:
    AudioParameterHandler();

    /**
     * Backup the parameters
     *
     * @param[in] keyValuePairs parameters to backup
     *
     * @return OK if backup is successfull, error code otherwise
     */
    android::status_t saveParameters(const android::String8 &keyValuePairs);

    /**
     * Return the stored parameters from filesystem
     *
     * @return parameters backuped previously.
     */
    android::String8 getParameters() const;

private:
    /**
     * Save the AudioParameter into filesystem.
     *
     * @return OK if saving is successfull, error code otherwise
     */
    android::status_t save();

    /**
     * Read the parameters from filesystem and add into AudioParameters.
     *
     * @return OK if restoring is successfull, error code otherwise
     */
    android::status_t restore();

    /**
     * Add parameters into list of backuped parameters
     *
     * @param[in] keyValuePairs parameters to backup
     */
    void add(const android::String8 &keyValuePairs);

    mutable android::AudioParameter _audioParameter; /**< parameters backuped. */

    static const char _filePath[]; /**< path of the backup file. */
    static const int _readBufSize; /**< size of the buffer to read the backup file. */
};
}
