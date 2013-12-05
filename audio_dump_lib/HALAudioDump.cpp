/*
 * Copyright 2013 Intel Corporation
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

#include "HALAudioDump.hpp"

#define LOG_TAG "HALAudioDump"

#include <cutils/log.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <utils/Errors.h>

using namespace android;
using namespace std;

const char *HALAudioDump::_streamDirections[] = {
    "in", "out"
};
const char *HALAudioDump::_dumpDirPath = "/logs/audio_dumps";
const uint32_t HALAudioDump::_maxNumberOfFiles = 4;

HALAudioDump::HALAudioDump()
    : _dumpFile(0), _fileCount(0)
{
    if (mkdir(_dumpDirPath, S_IRWXU | S_IRGRP | S_IROTH) != 0) {

        ALOGE("Cannot create audio dumps directory at %s : %s.",
              _dumpDirPath, strerror(errno));
    }
}

HALAudioDump::~HALAudioDump()
{
    if (_dumpFile) {
        close();
    }
}

void HALAudioDump::dumpAudioSamples(const void *buffer,
                                    ssize_t bytes,
                                    bool isOutput,
                                    uint32_t sRate,
                                    uint32_t chNb,
                                    const std::string &nameContext)
{
    status_t write_status;

    if (_dumpFile == NULL) {
        char *audio_file_name;

        /**
         * A new dump file is created for each stream relevant to the dump needs
         */
        asprintf(&audio_file_name,
                 "%s/audio_%s_%dKhz_%dch_%s_%d.pcm",
                 _dumpDirPath,
                 streamDirectionStr(isOutput),
                 sRate,
                 chNb,
                 nameContext.c_str(),
                 ++_fileCount);

        if (!audio_file_name) {
            return;
        }

        _dumpFile = fopen(audio_file_name, "wb");

        if (_dumpFile == NULL) {
            ALOGE("Cannot open dump file %s, errno %d, reason: %s",
                  audio_file_name,
                  errno,
                  strerror(errno));
            free(audio_file_name);
            return;
        }

        ALOGI("Audio %put stream dump file %s, fh %p opened.", streamDirectionStr(isOutput),
              audio_file_name,
              _dumpFile);
        free(audio_file_name);
    }

    write_status = writeDumpFile(buffer, bytes);

    if (write_status == BAD_VALUE) {

        /**
         * Max file size reached. Close file to open another one
         * Up to 4 dump files are dumped. When 4 dump files are
         * reached the last file is reused circularly.
         */
        close();
    }

    /**
     * Roll on 4 files, to keep at least the last audio dumps
     * and to split dumps for more convenience.
     */
    if (write_status == INVALID_OPERATION) {
        char *fileToRemove;

        asprintf(&fileToRemove,
                 "%s/audio_%s_%dKhz_%dch_%s_%d.pcm",
                 _dumpDirPath,
                 streamDirectionStr(isOutput),
                 sRate,
                 chNb,
                 nameContext.c_str(),
                 _fileCount);

        if (!fileToRemove) {
            return;
        }

        _fileCount--;

        remove(fileToRemove);
        free(fileToRemove);
        close();
    }
}

void HALAudioDump::close()
{
    fclose(_dumpFile);
    _dumpFile = NULL;
}

const char *HALAudioDump::streamDirectionStr(bool isOut) const
{
    return _streamDirections[isOut];
}

status_t HALAudioDump::checkDumpFile(ssize_t bytes)
{
    struct stat stDump;
    if (_dumpFile && fstat(fileno(_dumpFile), &stDump) == 0
        && (stDump.st_size + bytes) < _maxDumpFileSize) {

        ALOGE("%s: Max size reached", __FUNCTION__);
        return BAD_VALUE;
    }
    if (_fileCount >= _maxNumberOfFiles) {

        ALOGE("%s: Max number of allowed files reached", __FUNCTION__);
        return INVALID_OPERATION;
    }
    return OK;
}

status_t HALAudioDump::writeDumpFile(const void *buffer, ssize_t bytes)
{
    status_t ret;

    ret = checkDumpFile(bytes);
    if (ret != OK) {

        return ret;
    }
    if (fwrite(buffer, bytes, 1, _dumpFile) != 1) {

        ALOGE("Error writing PCM in audio dump file : %s", strerror(errno));
        return BAD_VALUE;
    }
    return OK;
}
