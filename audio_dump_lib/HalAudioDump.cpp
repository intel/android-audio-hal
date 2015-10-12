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

#include "HalAudioDump.hpp"

#define LOG_TAG "HALAudioDump"

#include <utilities/Log.hpp>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <utils/Errors.h>

using namespace android;
using namespace std;
using audio_comms::utilities::Log;

const char *HalAudioDump::mStreamDirections[] = {
    "in", "out"
};
const char *HalAudioDump::mDumpDirPath = "/data/misc/media";
const uint32_t HalAudioDump::mMaxNumberOfFiles = 4;

HalAudioDump::HalAudioDump()
    : mDumpFile(0), mFileCount(0)
{
    if (mkdir(mDumpDirPath, S_IRWXU | S_IRGRP | S_IROTH) != 0) {
        Log::Error() << "Cannot create audio dumps directory at " << mDumpDirPath
                     << " : " << strerror(errno);
    }
}

HalAudioDump::~HalAudioDump()
{
    if (mDumpFile) {
        close();
    }
}

void HalAudioDump::dumpAudioSamples(const void *buffer,
                                    ssize_t bytes,
                                    bool isOutput,
                                    uint32_t sRate,
                                    uint32_t chNb,
                                    const std::string &nameContext)
{
    status_t write_status;

    if (mDumpFile == NULL) {
        char *audio_file_name;

        /**
         * A new dump file is created for each stream relevant to the dump needs
         */
        asprintf(&audio_file_name,
                 "%s/audio_%s_%dKhz_%dch_%s_%d.pcm",
                 mDumpDirPath,
                 streamDirectionStr(isOutput),
                 sRate,
                 chNb,
                 nameContext.c_str(),
                 ++mFileCount);

        if (!audio_file_name) {
            return;
        }

        mDumpFile = fopen(audio_file_name, "wb");

        if (mDumpFile == NULL) {
            Log::Error() << __FUNCTION__
                         << ": Cannot open dump file " << audio_file_name
                         << " errno " << errno << ", reason: " << strerror(errno);
            free(audio_file_name);
            return;
        }
        Log::Info() << __FUNCTION__
                    << ": Audio " << streamDirectionStr(isOutput)
                    << "put stream dump file " << audio_file_name
                    << ", fh " << mDumpFile << " opened.";
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
                 mDumpDirPath,
                 streamDirectionStr(isOutput),
                 sRate,
                 chNb,
                 nameContext.c_str(),
                 mFileCount);

        if (!fileToRemove) {
            return;
        }

        mFileCount--;

        remove(fileToRemove);
        free(fileToRemove);
        close();
    }
}

void HalAudioDump::close()
{
    fclose(mDumpFile);
    mDumpFile = NULL;
}

const char *HalAudioDump::streamDirectionStr(bool isOut) const
{
    return mStreamDirections[isOut];
}

status_t HalAudioDump::checkDumpFile(ssize_t bytes)
{
    struct stat stDump;
    /** klocwork complains it may access st_size unitialized, even if fstat succeeded.
     * This failure pattern happens when given object as output parameter by reference.
     */
    stDump.st_size = 0;
    if (mDumpFile && fstat(fileno(mDumpFile), &stDump) == 0
        && (stDump.st_size + bytes) > mMaxDumpFileSize) {
        Log::Error() << __FUNCTION__ << ": Max size reached";
        return BAD_VALUE;
    }
    if (mFileCount >= mMaxNumberOfFiles) {
        Log::Error() << __FUNCTION__ << ": Max number of allowed files reached";
        return INVALID_OPERATION;
    }
    return OK;
}

status_t HalAudioDump::writeDumpFile(const void *buffer, ssize_t bytes)
{
    status_t ret;

    ret = checkDumpFile(bytes);
    if (ret != OK) {

        return ret;
    }
    if (fwrite(buffer, bytes, 1, mDumpFile) != 1) {
        Log::Error() << __FUNCTION__
                     << ": Error writing PCM in audio dump file : " << strerror(errno);
        return BAD_VALUE;
    }
    return OK;
}
