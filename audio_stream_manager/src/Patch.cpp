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

#define LOG_TAG "AudioPatch"

#include "Patch.hpp"
#include "Port.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <utils/Errors.h>
#include <utils/Atomic.h>
#include <vector>

using android::status_t;
using audio_comms::utilities::Log;
using namespace std;

namespace intel_audio
{

Patch::Patch(const audio_patch_handle_t handle, PatchInterface *patchInterface)
    : mHandle(handle), mPatchInterface(patchInterface)
{
    Log::Verbose() << __FUNCTION__ << ": adding a new patch";
}

Patch::~Patch()
{
    release();
}

audio_devices_t Patch::getSinkDevices() const
{
    return getDevices(AUDIO_PORT_ROLE_SINK);
}

audio_devices_t Patch::getSourceDevices() const
{
    return getDevices(AUDIO_PORT_ROLE_SOURCE) & ~AUDIO_DEVICE_BIT_IN;
}

audio_devices_t Patch::getDevices(audio_port_role_t role) const
{
    audio_devices_t devices = AUDIO_DEVICE_NONE;
    for (PortConstIterator portIter = mPorts.begin(); portIter != mPorts.end(); ++portIter) {
        const Port *port = *portIter;
        AUDIOCOMMS_ASSERT(port != NULL, "Invalid Port");
        if (port->getRole() == role &&
            port->getType() == AUDIO_PORT_TYPE_DEVICE) {
            devices |= port->getDeviceType();
        }
    }
    return devices;
}

const Port *Patch::getMixPort(audio_port_role_t role) const
{
    for (PortConstIterator portIter = mPorts.begin(); portIter != mPorts.end(); ++portIter) {
        const Port *port = *portIter;
        AUDIOCOMMS_ASSERT(port != NULL, "Invalid Port");
        if (port->getRole() == role &&
            port->getType() == AUDIO_PORT_TYPE_MIX) {
            return port;
        }
    }
    return NULL;
}

void Patch::release(bool notify)
{
    for (PortIterator portIter = mPorts.begin(); portIter != mPorts.end();) {
        Port *portToRelease = *portIter;
        AUDIOCOMMS_ASSERT(portToRelease != NULL, "Invalid Port");
        portToRelease->release();
        if (notify) {
            getPatchInterface()->onPortReleased(getHandle(), portToRelease->getHandle());
        }
        portIter = mPorts.erase(portIter);
    }
}

audio_patch_handle_t Patch::nextUniqueHandle()
{
    static volatile int32_t gNextUniqueId = 1;
    return static_cast<audio_patch_handle_t>(android_atomic_inc(&gNextUniqueId));
}

PatchInterface *Patch::getPatchInterface()
{
    AUDIOCOMMS_ASSERT(mPatchInterface != NULL, "Invalid Patch Interface");
    return mPatchInterface;
}

void Patch::addPort(Port &port)
{
    mPorts.push_back(&port);
    port.attach();
    getPatchInterface()->onPortAttached(getHandle(), port.getHandle());
    Log::Verbose() << __FUNCTION__ << ": adding port to patch=" << mHandle;
}

void Patch::addPorts(size_t sourcesCount,
                     const struct audio_port_config sources[],
                     size_t sinksCount,
                     const struct audio_port_config sinks[])
{
    // Simply release the patch and all its port without notify the user.
    // The same patch may be requested to be created just to update the port configuration(s).
    release(false);
    addPorts(sourcesCount, sources);
    addPorts(sinksCount, sinks);
}

void Patch::addPorts(size_t portCount, const struct audio_port_config portConfig[])
{
    for (size_t portConfigIndex = 0; portConfigIndex < portCount; portConfigIndex++) {
        const audio_port_handle_t &portHandle = portConfig[portConfigIndex].id;
        Port &port = getPatchInterface()->getPortFromHandle(portHandle);
        port.updateConfig(portConfig[portConfigIndex]);
        addPort(port);
    }
}

} // namespace intel_audio
