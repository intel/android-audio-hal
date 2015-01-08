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

#define LOG_TAG "AudioPort"

#include "Port.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>

using android::status_t;
using audio_comms::utilities::Log;
using namespace std;

namespace intel_audio
{

Port::Port()
    : mRefCount(0)
{
    mConfig.id = AUDIO_PORT_HANDLE_NONE;
    mConfig.ext.mix.handle = AUDIO_IO_HANDLE_NONE;
}

Port::~Port()
{
    // On port deletion, i.e. ref count is 0, remove the associated device from the
    // routed devices.
    AUDIOCOMMS_ASSERT(!isUsed(), "Removing still used port is not allowed, please analyse");
}

status_t Port::updateConfig(const audio_port_config &config)
{
    if (getHandle() != AUDIO_PORT_HANDLE_NONE && getHandle() != config.id) {
        Log::Error() << __FUNCTION__ << ": configuration mismaching port handle";
        return android::INVALID_OPERATION;
    }
    if (getType() == AUDIO_PORT_TYPE_MIX && getMixIoHandle() != config.ext.mix.handle) {
        Log::Error() << __FUNCTION__ << ": not allowed to change mix io handle";
        return android::INVALID_OPERATION;
    }
    Log::Verbose() << __FUNCTION__ << ": update config of port " << mConfig.id;
    mConfig = config;
    return android::OK;
}

void Port::attach()
{
    Log::Verbose() << __FUNCTION__ << ": increment counter of port " << mConfig.id;
    ++mRefCount;
}

void Port::release()
{
    if (mRefCount == 0) {
        return;
    }
    Log::Verbose() << __FUNCTION__ << ": decrement counter of port " << mConfig.id;
    if (--mRefCount == 0) {
        Log::Warning() << __FUNCTION__ << ": Port UNUSED " << mConfig.id;
    }
}

} // namespace intel_audio
