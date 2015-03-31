/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2015 Intel
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

#include "Patch.hpp"
#include "Port.hpp"
#include <InterfaceProviderImpl.h>
#include <IStreamInterface.hpp>
#include <audio_effects/effect_aec.h>
#include <audio_utils/echo_reference.h>
#include <hardware/audio_effect.h>
#include <hardware/hardware.h>
#include <DeviceInterface.hpp>
#include <AudioUtils.hpp>
#include <SampleSpec.hpp>
#include <NonCopyable.hpp>
#include <Mutex.hpp>
#include <AudioCommsAssert.hpp>
#include <string>

struct echo_reference_itfe;

namespace intel_audio
{

class CAudioConversion;
class StreamOut;
class StreamIn;
class Stream;
class AudioPlatformState;
class AudioParameterHandler;

class Device : public DeviceInterface,
               public PatchInterface,
               private audio_comms::utilities::NonCopyable
{
private:
    typedef std::map<audio_io_handle_t, Stream *> StreamCollection;
    typedef std::map<audio_patch_handle_t, Patch> PatchCollection;
    typedef std::map<audio_port_handle_t, Port> PortCollection;

public:
    Device();
    virtual ~Device();

    // From AudioHwDevice
    virtual android::status_t openOutputStream(audio_io_handle_t handle,
                                               audio_devices_t devices,
                                               audio_output_flags_t flags,
                                               audio_config_t &config,
                                               StreamOutInterface * &stream,
                                               const std::string &address);
    virtual android::status_t openInputStream(audio_io_handle_t handle,
                                              audio_devices_t devices,
                                              audio_config_t &config,
                                              StreamInInterface * &stream,
                                              audio_input_flags_t flags,
                                              const std::string &address,
                                              audio_source_t source);
    virtual void closeOutputStream(StreamOutInterface *stream);
    virtual void closeInputStream(StreamInInterface *stream);
    virtual android::status_t initCheck() const;
    virtual android::status_t setVoiceVolume(float volume);
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t setMasterVolume(float /* volume */)
    {
        return android::INVALID_OPERATION;
    }
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t getMasterVolume(float &) const
    {
        return android::INVALID_OPERATION;
    }
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t setMasterMute(bool /*mute*/)
    {
        return android::INVALID_OPERATION;
    }
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t getMasterMute(bool & /*muted*/) const
    {
        return android::INVALID_OPERATION;
    }
    virtual android::status_t setMode(audio_mode_t mode);
    virtual android::status_t setMicMute(bool mute);
    virtual android::status_t getMicMute(bool &muted) const;
    virtual android::status_t setParameters(const std::string &keyValuePairs);
    virtual std::string getParameters(const std::string &keys) const;
    virtual size_t getInputBufferSize(const audio_config_t &config) const;
    /** @note API not implemented in our Audio HAL */
    virtual android::status_t dump(const int /* fd */) const { return android::OK; }
    /** @note Routing Control API used for routing with AUDIO_DEVICE_API_VERSION >= 3.0. */
    virtual android::status_t createAudioPatch(size_t sourcesCount,
                                               const struct audio_port_config sources[],
                                               size_t sinksCount,
                                               const struct audio_port_config sinks[],
                                               audio_patch_handle_t &handle);
    /** @note Routing Control API used for routing with AUDIO_DEVICE_API_VERSION >= 3.0. */
    virtual android::status_t releaseAudioPatch(audio_patch_handle_t handle);
    /** @note Routing Control API used for routing with AUDIO_DEVICE_API_VERSION >= 3.0. */
    virtual android::status_t getAudioPort(struct audio_port &port) const;
    /** @note Routing Control API used for routing with AUDIO_DEVICE_API_VERSION >= 3.0. */
    virtual android::status_t setAudioPortConfig(const struct audio_port_config &config);

protected:
    /**
     * Returns the stream interface of the route manager.
     * As a AudioHAL creator must ensure HAL is started to performs any action on AudioHAL,
     * this function leaves if the stream interface was not created successfully upon start of HAL.
     *
     * @return stream interface of the route manager.
     */
    IStreamInterface *getStreamInterface()
    {
        AUDIOCOMMS_ASSERT(mStreamInterface != NULL, "Invalid stream interface");
        return mStreamInterface;
    }

    friend class StreamOut;
    friend class StreamIn;
    friend class Stream;

    /**
     * Print debug information from hw registers files.
     * Dump on console platform hw debug files.
     */
    void printPlatformFwErrorInfo();

private:
    /**
     * @return true if the collection of stream managed by the HW Device has a stream tracked by the
     *         given stream handle, false otherwise.
     */
    bool hasStream(const audio_io_handle_t &streamHandle);

    /**
     * @return true if the collection of port managed by the HW Device has a port tracked by the
     *         given port handle, false otherwise.
     */
    bool hasPort(const audio_port_handle_t &portHandle);

    /**
     * It must be called with patch collection lock held.
     *
     * @return true if the collection of patch managed by the HW Device has a patch tracked by the
     *         given patch handle, false otherwise.
     */
    bool hasPatchUnsafe(const audio_patch_handle_t &patchHandle);

    /**
     * Retrieve a stream from a stream handle from the collection managed by the HW Device.
     *
     * @param[in] streamHandle unique stream identifier
     * @param[out] stream instance that is matching the handle, may be NULL.
     *
     * @return true if the stream given as output parameter is valid, false otherwise.
     */
    bool getStream(const audio_io_handle_t &streamHandle, Stream * &stream);

    /**
     * Retrieve a Port from a Port handle from the collection managed by the HW Device.
     * If the handle refers to an unknown Port, this function asserts.
     *
     * @return Port tracked by the given Port handle.
     */
    Port &getPort(const audio_port_handle_t &portHandle);

    /**
     * Retrieve a Patch from a Patch handle from the collection managed by the HW Device.
     * If the handle refers to an unknown Patch, this function asserts.
     * It must be called with patch collection lock held.
     *
     * @return Patch tracked by the given Patch handle.
     */
    Patch &getPatchUnsafe(const audio_patch_handle_t &patchHandle);

    virtual Port &getPortFromHandle(const audio_port_handle_t &portHandle);
    virtual void onPortAttached(const audio_patch_handle_t &patchHandle,
                                const audio_port_handle_t &portHandle);
    virtual void onPortReleased(const audio_patch_handle_t &patchHandle,
                                const audio_port_handle_t &portHandle);

    /**
     * Get the android telephony mode.
     *
     * @return android telephony mode, members of the mother class AudioHardwareBase
     *                                 so coding style is the one from this class...
     */
    int mode() const
    {
        return mMode;
    }

    /**
     * Handle a stream start request.
     * It results in a routing reconsideration. It must be SYNCHRONOUS to avoid loosing samples.
     *
     * @param[in] stream requester Stream.
     *
     * @return OK if stream started successfully, error code otherwise.
     */
    android::status_t startStream(Stream *stream);

    /**
     * Handle a stream stop request.
     * It results in a routing reconsideration.
     *
     * @param[in] stream requester Stream.
     *
     * @return OK if stream stopped successfully, error code otherwise.
     */
    android::status_t stopStream(Stream *stream);

    /**
     * Handle a change of requested effect.
     * It results in a routing reconsideration.
     */
    void updateRequestedEffect();

    /**
     * Resets an echo reference.
     *
     * @param[in] reference: echo reference to reset.
     */
    void resetEchoReference(struct echo_reference_itfe *reference);

    /**
     * Get the echo reference for AEC effect.
     * Called by an input stream on which SW echo cancellation is performed.
     * Audio HAL needs to provide the echo reference (output stream) to the input stream.
     *
     * @param[in] inputSampleSpec: input stream sample specification.
     *
     * @return valid echo reference is found, NULL otherwise.
     */
    struct echo_reference_itfe *getEchoReference(const SampleSpec &inputSampleSpec);

    struct echo_reference_itfe *mEchoReference; /**< Echo reference to use for AEC effect. */

    AudioPlatformState *mPlatformState; /**< Platform state pointer. */

    AudioParameterHandler *mAudioParameterHandler; /**< backup and restore audio parameters. */

    IStreamInterface *mStreamInterface; /**< Route Manager Stream Interface pointer. */

    audio_mode_t mMode; /**< Android telephony mode. */

    StreamCollection mStreams; /**< Collection of opened streams. */
    PatchCollection mPatches; /**< Collection of connected patches. */
    PortCollection mPorts; /**< Collection of audio ports. */

    static const char *const mDefaultGainPropName; /**< Gain property name. */
    static const float mDefaultGainValue; /**< Default gain value if empty property. */
    static const char *const mRouteLibPropName;  /**< Route Manager name property. */
    static const char *const mRouteLibPropDefaultValue;  /**< Route Manager lib default value. */
    static const char *const mRestartingKey; /**< Restart key parameter. */
    static const char *const mRestartingRequested; /**< Restart key parameter value. */

    static const uint32_t mRecordingBufferTimeUsec = 20000;

    /**
     * Protect concurrent access to routing control API to protect concurrent access to
     * patches collection.
     */
    mutable audio_comms::utilities::Mutex mPatchCollectionLock;
};

} // namespace intel_audio
