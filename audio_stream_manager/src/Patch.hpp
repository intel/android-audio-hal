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
    typedef std::vector<const Port *>::const_iterator PortConstIterator;

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
     * Returns the sink device(s) involved in this patch.
     *
     * @return valid sink device(s) or NONE if no sink device port is attached to this patch
     */
    audio_devices_t getSinkDevices() const;

    /**
     * Returns the source device(s) involved in this patch.
     *
     * @return valid source device(s) or NONE if no sink device port is attached to this patch
     */
    audio_devices_t getSourceDevices() const;

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
     * Returns the device(s) involved in this patch according to the role requested
     *
     * @param[in] role for which the devices are requested, i.e. sink or source.
     *
     * @return valid device(s) of the requested role,
     *         NONE if no device port of the requested role attached to this patch
     */
    audio_devices_t getDevices(audio_port_role_t role) const;

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
