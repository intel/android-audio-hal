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

#include <KeyValuePairs.hpp>
#include <utils/Errors.h>
#include <string>

namespace intel_audio
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
     * @return OK if backup is successful, error code otherwise
     */
    android::status_t saveParameters(const std::string &keyValuePairs);

    /**
     * Return the stored parameters from filesystem
     *
     * @return parameters backuped previously.
     */
    std::string getParameters() const;

private:
    /**
     * Save the AudioParameter into filesystem.
     *
     * @return OK if saving is successful, error code otherwise
     */
    android::status_t save();

    /**
     * Read the parameters from filesystem and add into AudioParameters.
     *
     * @return OK if restoring is successful, error code otherwise
     */
    android::status_t restore();

    /**
     * Add parameters into list of backuped parameters
     *
     * @param[in] keyValuePairs parameters to backup
     */
    void add(const std::string &keyValuePairs);

    mutable KeyValuePairs mPairs; /**< parameters backuped. */

    static const char mFilePath[]; /**< path of the backup file. */
    static const int mReadBufSize; /**< size of the buffer to read the backup file. */
};
} // namespace intel_audio
