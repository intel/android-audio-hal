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

#include "HalAudioDump.hpp"

#define LOG_TAG "HALAudioDump"

#include <cutils/log.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <utils/Errors.h>

using namespace android;
using namespace std;

const char *HalAudioDump::mStreamDirections[] = {
    "in", "out"
};
const char *HalAudioDump::mDumpDirPath = "/logs/audio_dumps";
const uint32_t HalAudioDump::mMaxNumberOfFiles = 4;

HalAudioDump::HalAudioDump()
    : mDumpFile(0), mFileCount(0)
{
    if (mkdir(mDumpDirPath, S_IRWXU | S_IRGRP | S_IROTH) != 0) {

        ALOGE("Cannot create audio dumps directory at %s : %s.",
              mDumpDirPath, strerror(errno));
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
            ALOGE("Cannot open dump file %s, errno %d, reason: %s",
                  audio_file_name,
                  errno,
                  strerror(errno));
            free(audio_file_name);
            return;
        }

        ALOGI("Audio %put stream dump file %s, fh %p opened.", streamDirectionStr(isOutput),
              audio_file_name,
              mDumpFile);
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
    if (mDumpFile && fstat(fileno(mDumpFile), &stDump) == 0
        && (stDump.st_size + bytes) > mMaxDumpFileSize) {

        ALOGE("%s: Max size reached", __FUNCTION__);
        return BAD_VALUE;
    }
    if (mFileCount >= mMaxNumberOfFiles) {

        ALOGE("%s: Max number of allowed files reached", __FUNCTION__);
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

        ALOGE("Error writing PCM in audio dump file : %s", strerror(errno));
        return BAD_VALUE;
    }
    return OK;
}
