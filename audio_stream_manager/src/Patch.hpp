/*
 * Copyright (C) 2015-2017 Intel Corporation
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

#include "Port.hpp"
#include <system/audio.h>
#include <vector>

namespace intel_audio
{

class PatchInterface
{
public:
    /**
     * Retrieve the port object from a port handle.
     *
     * @param[out] portHandle for which the patch is requesting a port object.
     *
     * @return Port object tracked by the given unique port handle.
     */
    virtual Port &getPortFromHandle(const audio_port_handle_t &portHandle) = 0;

    /**
     * Notify that a port is now attached to the patch.
     *
     * @param[out] patchHandle patch refered by its handle attaching the port to itself0
     * @param[out] portHandle port refered by its handle attached to this patch.
     */
    virtual void onPortAttached(const audio_patch_handle_t &patchHandle,
                                const audio_port_handle_t &portHandle) = 0;

    /**
     * Notify that a port is now released by the patch.
     *
     * @param[out] patchHandle patch refered by its handle releasing the port
     * @param[out] portHandle port refered by its handle released by the patch.
     */
    virtual void onPortReleased(const audio_patch_handle_t &patchHandle,
                                const audio_port_handle_t &portHandle) = 0;

protected:
    virtual ~PatchInterface() {}
};

class Patch
{
private:
    typedef std::vector<Port *> PortCollection;
    typedef PortCollection::iterator PortIterator;
    typedef std::vector<Port *>::const_iterator PortConstIterator;

public:
    Patch(const audio_patch_handle_t handle = AUDIO_PATCH_HANDLE_NONE,
          PatchInterface *mPatchInterface = NULL);

    /**
     * A patch might be deleted in 2 cases:
     *  - explicit call to release audio patch from the audio policy
     *  - implicit call to release an audio patch when a new one is created for the SAME MIX Port.
     * Upon patch deletion, release all the ports involved in this patch, it will decrease their
     * usage reference counter.
     */
    ~Patch();

    /**
     * Returns the device(s) involved in this patch according to the role requested
     * Note that it removes the sign bit introduced by the policy for input device in order to keep
     * the cardinality between a bit and a device within the devices mask.
     *
     * @param[in] role for which the devices are requested, i.e. sink or source.
     *
     * @return valid device(s) of the requested role,
     *         NONE if no device port of the requested role attached to this patch
     */
    audio_devices_t getDevices(audio_port_role_t role) const;

    /**
     * Returns the device address involved in this patch according to the role requested
     * Note that it removes the sign bit introduced by the policy for input device in order to keep
     * the cardinality between a bit and a device within the devices mask.
     *
     * @param[in] role for which the devices are requested, i.e. sink or source.
     *
     * @return valid device address of the requested role,
     *         empty string if no device port of the requested role attached to this patch
     */
    std::string getDeviceAddress(audio_port_role_t role) const;

    /**
     * Checks if a port is involving port devices in the given role.
     *
     * @param[in] role of the port device to check.
     *
     * @return true if the patch is involving port device with the given role, false otherwise.
     */
    bool hasDevice(audio_port_role_t role) const;

    /** Add source and sink ports involved within this patch, i.e. connected from one another
     * by this audio patch).
     *
     * @param[in] sourcesCount number of source ports to connect.
     * @param[in] sources array of source port configurations.
     * @param[in] sinksCount number of sink ports to connect.
     * @param[in] sinks array of sink port configurations.
     */
    void addPorts(size_t sourcesCount,
                  const struct audio_port_config sources[],
                  size_t sinksCount,
                  const struct audio_port_config sinks[]);

    /**
     * Get if exist the mix port involved in this patch according to the role requested.
     *
     * @param[in] role for which the devices are requested, i.e. sink or source.
     *
     * @return valid Port of the requested role if found, NULL otherwise
     */
    const Port *getMixPort(audio_port_role_t role) const;

    /**
     * Release the patch and all the ports involved by this patch.
     * It also clears the collection of ports.
     *
     * @param[in] notify: if set, the patch will notify the client through the PatchInterface
     *            of any release event on its ports.
     */
    void release(bool notify = true);

    /**
     * @return the unique handle identifier of this patch.
     */
    audio_patch_handle_t getHandle() const { return mHandle; }

    /**
     * @return unique patch handle, which is in fact a unique identifier
     */
    static audio_patch_handle_t nextUniqueHandle();

    android::status_t dump(const int fd, int spaces = 0) const;

private:
    PatchInterface *getPatchInterface();

    /**
     *  Adds ports to the collection of ports that are connected to one another by this patch.
     *
     * @param[in] configCount Number of port configuration to be updated for this patch
     * @param[in] config Array of port configurations
     */
    void addPorts(size_t portCount, const struct audio_port_config portConfig[]);

    /**
     * Adds a port to the collection of ports that are connected to one another by this patch.
     *
     * @param[in] port to be added to the collection of ports involved in this patch.
     */
    void addPort(Port &port);

    /**
     * Collection of ports connected by this patch.
     */
    PortCollection mPorts;

    audio_patch_handle_t mHandle; /**< Unique Patch Handle ID. */

    /**
     * Patch Interface used to notify patch event such as add/release ports.
     */
    PatchInterface *mPatchInterface;
};

} // namespace intel_audio
