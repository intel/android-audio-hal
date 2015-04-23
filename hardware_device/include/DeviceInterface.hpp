/*
 * Copyright (C) 2014-2015 Intel Corporation
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

#include <StreamInterface.hpp>
#include <hardware/audio.h>
#include <utils/Errors.h>
#include <string>


namespace intel_audio
{

/**
 * Class that handles audio HAL interface.
 *
 * That interface needs to be implemented by a derived class.
 * Stream classes also need to be implemented.
 *
 * Important note: Implementation must define an "extern "C" createAudioHardware(void)" function
 *                 that constructs a HAL instance.
 */
class DeviceInterface
{
public:
    virtual ~DeviceInterface() {}

    /** Creates and opens the audio hardware output stream.
     *
     * @param[in] handle to IO audio stream
     * @param[in] devices attached to the stream
     * @param[in] flags of output stream
     * @param[in,out] config sample specification
     * @param[out] stream that is created, NULL if failure
     *
     * @return OK if succeed, error code else.
     */
    virtual android::status_t openOutputStream(audio_io_handle_t handle,
                                               audio_devices_t devices,
                                               audio_output_flags_t flags,
                                               audio_config_t &config,
                                               StreamOutInterface * &stream,
                                               const std::string &address) = 0;

    /** Closes and frees the audio hardware output stream.
     *
     * @param[in] stream to be closed.
     */
    virtual void closeOutputStream(StreamOutInterface *stream) = 0;

    /** Creates and opens the audio hardware input stream.
     *
     * @param[in] handle to IO audio stream
     * @param[in] devices attached to the stream
     * @param[in,out] config sample specification
     * @param[out] stream that is created, NULL if failure
     * @return OK if succeed, error code else.
     */
    virtual android::status_t openInputStream(audio_io_handle_t handle,
                                              audio_devices_t devices,
                                              audio_config_t &config,
                                              StreamInInterface * &stream,
                                              audio_input_flags_t flags,
                                              const std::string &address,
                                              audio_source_t source) = 0;

    /** Closes and frees the audio hardware input stream.
     *
     * @param[in] stream to be closed.
     */
    virtual void closeInputStream(StreamInInterface *stream) = 0;

    /** Check to see if the audio hardware interface has been initialized.
     *
     * @return OK if succeed, NO_INIT else.
     */
    virtual android::status_t initCheck() const = 0;

    /** Set the audio volume of a voice call.
     *
     * @param[in] volume range is between 0.0 and 1.0
     * @return OK if succeed, error code else.
     */
    virtual android::status_t setVoiceVolume(float volume) = 0;

    /** Set the audio volume for all audio activities other than voice call.
     *
     * @param[in] volume range between 0.0 and 1.0.
     * @return OK if succeed, error code else.
     *         If an error is returned, the software mixer will emulate this capability.
     */
    virtual android::status_t setMasterVolume(float volume) = 0;

    /** Get the current master volume value for the HAL, if the HAL supports master volume control.
     * AudioFlinger will query this value from the primary audio HAL when the service starts
     * and use the value for setting the initial master volume across all HALs.
     *
     * @param[out] volume value
     *             HALs which do not support this method may leave it set to NULL.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t getMasterVolume(float &volume) const = 0;

    /** Set the audio mute status for all audio activities.
     *
     * @param[in] mute true for mute request, false for unmute request
     * @return OK if succeed, error code else.
     *         If an error is returned, the software mixer will emulate this capability.
     */
    virtual android::status_t setMasterMute(bool mute) = 0;

    /** Get the current master mute status for the HAL, if the HAL supports master mute control.
     * AudioFlinger will query this value from the primary audio HAL when the service starts
     * and use the value for setting the initial master mute across all HALs.
     *
     * @param[out] muted true when muted, false when unmuted
     *             HALs which do not support this method may leave it set to NULL.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t getMasterMute(bool &muted) const = 0;

    /** Called when the audio mode changes.
     * @see audio_mode_t enums for possible values.
     *
     * @param[in] mode that is set.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t setMode(audio_mode_t mode) = 0;

    /** Mute/unmute the microphone.
     *
     * @param[in] state true for mute request, false for unmute request
     * @return OK if succeed, error code else.
     */
    virtual android::status_t setMicMute(bool mute) = 0;

    /** Retrieve microphone mute state.
     *
     * @param[out] muted true when muted, false when unmuted
     * @return OK if succeed, error code else.
     */
    virtual android::status_t getMicMute(bool &muted) const = 0;

    /** Set the global parameters on Audio HAL.
     * Called by Audio System to inform the HAL of parameter value change like BT enabled,
     * TTY state, HAC state, etc...
     * It backups the given parameters in order to restore them when AudioHAL is restarted.
     *
     * @param[in] keyValuePairslist of parameter key value pairs in the form:
     *            key1=value1;key2=value2;...
     * @return OK if succeed, error code else.
     */
    virtual android::status_t setParameters(const std::string &keyValuePairs) = 0;

    /** Get the global parameters of Audio HAL.
     *
     * @param[in] keys list of parameter key value pairs in the form: key1;key2;key3;...
     *            Some keys are reserved for standard parameters (@see AudioParameter class)
     * @return list of parameter key value pairs in the form: key1=value1;key2=value2;...
     */
    virtual std::string getParameters(const std::string &keys) const = 0;

    /** Retrieve audio input buffer size.
     * @see also StreamInterface::getBufferSize which is for a particular stream.
     *
     * @param[in] config
     * @return audio input buffer size according to parameters passed or
     *         0 if one of the parameters is not supported.
     */
    virtual size_t getInputBufferSize(const audio_config_t &config) const = 0;

    /** Dump the state of the audio hardware.
     *
     * @param[in] fd file descriptor used as dump output.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t dump(const int fd) const = 0;

    /** Creates an audio patch between several source ports and sink ports.
     *
     * @param[in] sourcesCount number of source ports to connect.
     * @param[in] sources array of source port configurations.
     * @param[in] sinksCount number of sink ports to connect.
     * @param[in] sinks array of sink port configurations.
     * @param[out] handle allocated by the HAL should be unique for this
     *                    audio HAL module.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t createAudioPatch(size_t sourcesCount,
                                               const struct audio_port_config sources[],
                                               size_t sinksCount,
                                               const struct audio_port_config sinks[],
                                               audio_patch_handle_t &handle) = 0;

    /** Releases an audio patch.
     *
     * @param[in] handle of the patch to release.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t releaseAudioPatch(audio_patch_handle_t handle) = 0;

    /** Fills the list of supported attributes for a given audio port.
     * As input, "port" contains the information (type, role, address etc...)
     * needed by the HAL to identify the port.
     * As output, "port" contains possible attributes (sampling rates, formats,
     * channel masks, gain controllers...) for this port.
     *
     * @param[in,out] port for which attributes are requested to be filled by HAL.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t getAudioPort(struct audio_port &port) const = 0;

    /** Set audio port configuration.
     *
     * @param[out] config port configuration.
     * @return OK if succeed, error code else.
     */
    virtual android::status_t setAudioPortConfig(const struct audio_port_config &config) = 0;
};

} // namespace intel_audio
