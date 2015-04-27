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

#include "Patch.hpp"
#include "Port.hpp"
#include <IStreamInterface.hpp>
#include <KeyValuePairs.hpp>
#include <Direction.hpp>
#include <audio_effects/effect_aec.h>
#include <audio_utils/echo_reference.h>
#include <hardware/audio_effect.h>
#include <hardware/hardware.h>
#include <DeviceInterface.hpp>
#include <AudioBand.h>
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
     * Update the streams parameters upon start / stop / change of devices events on streams.
     * in a synchronous manner.
     *
     * @param[in] streamMixPortRole role of the stream (acting as a mix port).
     *
     * @return OK if successfully updated streams parameters, error code otherwise.
     */
    android::status_t updateStreamsParametersSync(audio_port_role_t streamPortRole)
    {
        return updateStreamsParameters(streamPortRole, true);
    }

    /**
     * Update the streams parameters upon start / stop / change of devices events on streams.
     * in an asynchronous manner.
     *
     * @param[in] streamMixPortRole role of the stream (acting as a mix port).
     *
     * @return OK if successfully updated streams parameters, error code otherwise.
     */
    android::status_t updateStreamsParametersAsync(audio_port_role_t streamPortRole)
    {
        return updateStreamsParameters(streamPortRole, false);
    }

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
     * Update the streams parameters upon start / stop / change of devices events on streams.
     * This function parses all streams and concatenate their mask into a bit field.
     * For input streams:
     * It not only updates the requested preproc criterion but also the band type.
     * Only one input stream may be active at one time by design of android audio policy.
     * Find this active stream with valid device and set the parameters
     * according to what was requested from this input.
     *
     * @param[in] isOut direction of stream from which the events is issued.
     * @param[in] isSynchronous: need to update the settings in a synchronous way or not.
     *
     * @return OK if successfully updated streams parameters, error code otherwise.
     */
    android::status_t updateStreamsParameters(audio_port_role_t streamPortRole, bool isSynchronous)
    {
        // If a stream is a sink mix port: we need to update source devices.
        // If a stream is a source mix port: we need to update parameters related to sink devices.
        return updateParameters(streamPortRole == AUDIO_PORT_ROLE_SINK,
                                streamPortRole == AUDIO_PORT_ROLE_SOURCE,
                                isSynchronous);
    }

    inline audio_port_role_t getOppositeRole(audio_port_role_t role) const
    {
        return role == AUDIO_PORT_ROLE_SINK ? AUDIO_PORT_ROLE_SOURCE : AUDIO_PORT_ROLE_SINK;
    }

    /**
     * Retrieve the device mask from a given stream.
     *
     * @param[in] stream for which the request applies to.
     *
     * @return devices mask associated to the stream.
     */
    uint32_t getDeviceFromStream(const Stream &stream) const;

    /**
     * Update the streams parameters according to the change of source and or sink devices.
     *
     * @param[in] updateSourceDevice: if set, we shall update the parameter linked to output devices
     * @param[in] updateSinkDevice: if set, we shall update the parameter linked to input devices
     * @param[in] isSynchronous: need to update the settings in a synchronous way or not.
     *
     * @return OK if successfully updated streams parameters, error code otherwise.
     */
    android::status_t updateParameters(bool updateSourceDevice, bool updateSinkDevice,
                                       bool isSynchronous = false);

    /**
     * Prepare the streams parameters to be sent to the parameter framework for routing.
     *
     * @param[in] streamPortRole direction of stream from which the events is issued.
     * @param[out] pairs: parameters as collection of {key,value} pairs.
     */
    void prepareStreamsParameters(audio_port_role_t streamPortRole, KeyValuePairs &pairs);

    /**
     * Extract from a given stream the parameters that needs to be updated.
     *
     * @param[in] stream for which the request apply to.
     * @param[in|out] flagMask of the active streams.
     * @param[in|out] useCaseMask of the active streams.
     * @param[in|out] devicesMask involved in stream operations.
     * @param[in|out] requestedEffectMask of the active streams.
     */
    void updateParametersFromStream(const Stream &stream, uint32_t &flagMask,
                                    uint32_t &useCaseMask, uint32_t &deviceMask,
                                    uint32_t &requestedEffectMask);

    /**
     * Selects the output devices from streams devices and internal devices. It also take into
     * account the specific role of the primary output and the compress (as not handled by
     * primary HAL BUT routed by primary).
     *
     * @param[in] internalDeviceMask Devices that are connected internal (without streams).
     * @param[in] streamDeviceMask involved in stream operations.
     *
     * @return selected output devices to be routed.
     */
    uint32_t selectOutputDevices(uint32_t internalDeviceMask, uint32_t streamDeviceMask);

    /**
     * Checks if the stream is the primary output stream, i.e. it has PRIMARY flags.
     *
     * @param[in] stream to check
     *
     * @return true if stream is the primary output, false otherwise.
     */
    bool isPrimaryOutput(const Stream &stream) const;

    /**
     * Infer the band from a stream, using its sample rate information.
     *
     * @return audio band associated to this stream.
     */
    inline CAudioBand::Type getBandFromActiveInput() const;

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
    bool hasPatchUnsafe(const audio_patch_handle_t &patchHandle) const;

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
    const Patch &getPatchUnsafe(const audio_patch_handle_t &patchHandle) const;
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
     * @return true if the system is in CSV call, false otherwise.
     */
    inline bool isInCall() const
    {
        return mMode == AUDIO_MODE_IN_CALL;
    }

    inline Direction::Values getDirectionFromMix(audio_port_role_t streamPortRole) const
    {
        return streamPortRole == AUDIO_PORT_ROLE_SOURCE ? Direction::Output : Direction::Input;
    }

    inline audio_output_flags_t getCompressOffloadFlags() const
    {
        return mCompressOffloadDevices != AUDIO_DEVICE_NONE ?
               AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD : AUDIO_OUTPUT_FLAG_NONE;
    }

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
    Stream *mPrimaryOutput; /**< Primary output stream, which has a leading routing role. */

    /**
     * Until compress is in a separated HAL, keep track of its device as primary HAL is
     * of routing the compress use case.
     */
    audio_devices_t mCompressOffloadDevices;

    static const char *const mDefaultGainPropName; /**< Gain property name. */
    static const float mDefaultGainValue; /**< Default gain value if empty property. */
    static const char *const mRestartingKey; /**< Restart key parameter. */
    static const char *const mRestartingRequested; /**< Restart key parameter value. */

    static const uint32_t mRecordingBufferTimeUsec = 20000;

    /**
     * Stream Rate associated with narrow band in case of VoIP.
     */
    static const uint32_t mVoiceStreamRateForNarrowBandProcessing = 8000;

    /**
     * Protect concurrent access to routing control API to protect concurrent access to
     * patches collection.
     */
    mutable audio_comms::utilities::Mutex mPatchCollectionLock;
};

} // namespace intel_audio
