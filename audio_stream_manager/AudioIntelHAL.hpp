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
#pragma once

#include "ModemAudioManagerObserver.h"
#include <EventListener.h>
#include <InterfaceProviderImpl.h>
#include <NonCopyable.hpp>
#include <StreamInterface.hpp>
#include <audio_effects/effect_aec.h>
#include <audio_utils/echo_reference.h>
#include <hardware/audio_effect.h>
#include <hardware/hardware.h>
#include <hardware_legacy/AudioHardwareBase.h>

#include <AudioUtils.hpp>
#include <SampleSpec.hpp>
#include <list>
#include <string>
#include <utils/List.h>
#include <utils/threads.h>
#include <vector>

struct echo_reference_itfe;
struct IModemAudioManagerInterface;
class CEventThread;

namespace android_audio_legacy
{

class CAudioConversion;
class AudioStreamOutImpl;
class AudioStreamInImpl;
class AudioStream;
class AudioPlatformState;
class AudioParameterHandler;

using android::RWLock;
using android::Mutex;

class AudioIntelHAL : public AudioHardwareBase, private IModemAudioManagerObserver,
                      public IEventListener,
                      public audio_comms::utilities::NonCopyable
{
private:
    enum EventType
    {
        UpdateModemAudioBand,   /**< Modem Audio Band change event. */
        UpdateModemState,       /**< Modem State change event. */
        UpdateModemAudioStatus  /**< Modem Audio status change event. */
    };

public:
    AudioIntelHAL();
    virtual ~AudioIntelHAL();

    /**
     * check to see if the audio hardware interface has been initialized.
     * return status based on values defined in include/utils/Errors.h.
     */
    virtual android::status_t initCheck();

    /**
     * set the audio volume of a voice call.
     *
     * @param[in] volume: voice volume to set, range is between 0.0 and 1.0.
     *
     * @return OK if volume inside range and applied successfully, error code otherwise.
     */
    virtual android::status_t setVoiceVolume(float volume);

    /**
     * set the audio volume for all audio activities other than voice call.
     * the software mixer will emulate this capability.
     *
     * @param[in] volume: voice volume to set, range is between 0.0 and 1.0.
     *
     * @return OK if volume inside range and applied successfully, error code otherwise.
     */
    virtual android::status_t setMasterVolume(float volume);

    /**
     * set the microphone in mute state.
     *
     * @param[in] state: true to request mute, false to unmute.
     *
     * @return OK if mute/unmute successfull, error code otherwise.
     */
    virtual android::status_t setMicMute(bool state);

    /**
     * Get the microphone in mute state.
     *
     * @param[out] state: true to request mute, false to unmute.
     *
     * @return OK if mute/unmute state retrieved successfully, error code otherwise.
     */
    virtual android::status_t getMicMute(bool *state);

    /**
     * set the global parameters on Audio HAL.
     * This function is called by Audio System to inform of such parameter like BT enabled, TTY,
     * HAC, etc...
     * It will backup the parameters given in order to restore them when AudioHAL is restarted.
     *
     * @param[in] keyValuePairs: one or more value pair "name:value", coma separated.
     *
     * @return OK if set is successfull, error code otherwise.
     */
    virtual android::status_t setParameters(const String8 &keyValuePairs);

    /**
     * Get the global parameters of Audio HAL.
     *
     * @param[out] keys: one or more value pair "name:value", coma separated.
     *
     * @return OK if set is successfull, error code otherwise.
     */
    // virtual String8 getParameters(const String8 &keys);

    /**
     * Get the input buffer size of Audio HAL.
     *
     * @param[in] sampleRate: sample rate of the samples of the record client.
     * @param[in] format: format of the samples of the record client.
     * @param[in] channels: channels of the samples of the record client.
     *
     * @return size of input buffer if parameters of the client supported, 0 otherwise.
     */
    virtual size_t getInputBufferSize(uint32_t sampleRate, int format, int channels);

    /**
     * Creates and opens the audio output stream.
     *
     * @param[in] devices: audio devices requested by the playback client.
     * @param[in|out] format: format of the samples of the playback client.
     *                        If not set, AudioHAL returns format chosen.
     * @param[in|out] channels: channels of the samples of the playback client.
     *                          If not set, AudioHAL returns channels chosen.
     * @param[in|out] sampleRate: sample rate of the samples of the playback client.
     *                            If not set, AudioHAL returns sample rate chosen.
     * @param[out] status: error code to set if parameters given by playback client not supported.
     *.
     * @return valid output stream if status set to OK, NULL otherwise
     */
    virtual AudioStreamOut *openOutputStream(
        uint32_t devices,
        int *format = NULL,
        uint32_t *channels = NULL,
        uint32_t *sampleRate = NULL,
        status_t *status = NULL);

    /**
     * Closes and deletes an audio output stream.
     *
     * @param[in] out: output stream to close / delete.
     */
    virtual void closeOutputStream(AudioStreamOut *out);

    /**
     * Creates and opens the audio input stream.
     *
     * @param[in] devices: audio devices requested by the record client.
     * @param[in|out] format: format of the samples of the record client.
     *                        If not set, AudioHAL returns format chosen.
     * @param[in|out] channels: channels of the samples of the record client.
     *                          If not set, AudioHAL returns channels chosen.
     * @param[in|out] sampleRate: sample rate of the samples of the record client.
     *                            If not set, AudioHAL returns sample rate chosen.
     * @param[out] status: error code to set if parameters given by record client not supported.
     *
     * @return valid input stream if status set to OK, NULL otherwise.
     */
    virtual AudioStreamIn *openInputStream(
        uint32_t devices,
        int *format,
        uint32_t *channels,
        uint32_t *sampleRate,
        status_t *status,
        AudioSystem::audio_in_acoustics acoustics);

    /**
     * Closes and deletes an audio input stream.
     *
     * @param[in] out: input stream to close / delete.
     */
    virtual void closeInputStream(AudioStreamIn *in);

    /**
     * Entry point to create AudioHAL.
     */
    static AudioHardwareInterface *create();

protected:
    virtual status_t dump(int fd, const Vector<String16> &args);

    /**
     * Returns the stream interface of the route manager.
     * As a AudioHAL creator must ensure HAL is started to performs any action on AudioHAL,
     * this function leaves if the stream interface was not created successfully upon start of HAL.
     *
     * @return stream interface of the route manager.
     */
    IStreamInterface *getStreamInterface()
    {
        LOG_ALWAYS_FATAL_IF(_streamInterface == NULL);
        return _streamInterface;
    }

    friend class AudioStreamOutImpl;
    friend class AudioStreamInImpl;
    friend class AudioStream;

private:
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
     * Starts the modem audio manager service. Once started, AudioHAL will be notified of
     *      -state of the modem.
     *      -status of audio in the modem.
     *      -any change of band for CSV call.
     *
     * @return true if modem audio manager successfully started, false otherwise
     */
    bool startModemAudioManager();

    /**
     * Modem Audio Manager callback to notify of a change of the audio status on the modem.
     * Inherited from IModemAudioManagerObserver.
     */
    virtual void onModemAudioStatusChanged();
    /**
     * Modem Audio Manager callback to notify of a change of the modem state.
     * Inherited from IModemAudioManagerObserver.
     */
    virtual void onModemStateChanged();
    /**
     * Modem Audio Manager callback to notify of a change of the audio band of the CSV call.
     * Inherited from IModemAudioManagerObserver.
     */
    virtual void onModemAudioBandChanged();

    /**
     * Event Thread callback to notify of an event.
     * Inherited from IEventListener.
     *
     * @param[in] fd file descriptor from which event is originated.
     *
     * @return true if the file descriptor state has changed.
     */
    virtual bool onEvent(int fd);

    /**
     * Event Thread callback to notify of error.
     * Inherited from IEventListener.
     *
     * @param[in] fd file descriptor from which error is originated.
     *
     * @return true if the file descriptor state has changed.
     */
    virtual bool onError(int fd);

    /**
     * Event Thread callback to notify of a hangup.
     * Inherited from IEventListener.
     *
     * @param[in] fd file descriptor from which hangup is originated.
     *
     * @return true if the file descriptor state has changed.
     */
    virtual bool onHangup(int fd);

    /**
     * Event Thread callback to notify of an alarm.
     * Inherited from IEventListener.
     */
    virtual void onAlarm();

    /**
     * Event Thread callback to notify of a polling error.
     * Inherited from IEventListener.
     */
    virtual void onPollError();

    /**
     * Event Thread callback to notify of a request to process an event.
     * Inherited from IEventListener.
     *
     * @param[in] event event to treat.
     *
     * @return true if the file descriptor state has changed.
     */
    virtual bool onProcess(uint16_t event);

    /**
     * Checks if a device is only one input device.
     *
     * @param[in] device: device to check that conforms to HAL API REV 1.0.
     *
     * @return true if input false otherwise.
     */
    inline bool isInputDevice(uint32_t device)
    {
        return (popcount(device) == 1) && ((device & ~AudioSystem::DEVICE_IN_ALL) == 0);
    }

    /**
     * Set the global parameters on Audio HAL.
     * It may lead to a routing reconsideration.
     *
     * @param[in] keyValuePairs: one or more value pair "name:value", coma separated.
     *
     * @return OK if set is successfull, error code otherwise.
     */
    android::status_t doSetParameters(const String8 &keyValuePairs);

    /**
     * Handle any setParameters called from the streams.
     * It may result in a routing reconsideration.
     *
     * @param[in] stream Stream from which the setParameters is originated.
     * @param[in] keyValuePairs: one or more value pair "name:value", coma separated.
     *
     * @return OK if parameters successfully taken into account, error code otherwise.
     */
    status_t setStreamParameters(AudioStream *stream, const String8 &keyValuePairs);


    /**
     * Check and set the routing stream parameter (ie the devices).
     * It checks if routing key is found in the parameters, read the value and set the platform
     * state accordingly and removes the value pair from the parameter.
     * It takes a chance to catch any change of android mode from the policy during any routing
     * request of output stream. Audio Policy will always force a rerouting of the output upon mode
     * change.
     *
     * @param[in] stream Stream from which the setParameters is originated.
     * @param[in|out] param: contains all the key value pairs to check.
     */
    void checkAndSetRoutingStreamParameter(AudioStream *stream, AudioParameter &param);

    /**
     * Check and set the input source stream parameters.
     * It checks if input source key is found in the parameters, read the value and set the platform
     * state accordingly and removes the value pair from the parameter
     *
     * @param[in] stream Stream from which the setParameters is originated.
     * @param[in|out] param: contains all the key value pairs to check.
     */
    void checkAndSetInputSourceStreamParameter(AudioStream *stream, AudioParameter &param);

    /**
     * Handle a stream start request.
     * It results in a routing reconsideration. It must be SYNCHRONOUS to avoid loosing samples.
     *
     * @param[in] stream requester Stream.
     *
     * @return OK if stream started successfully, error code otherwise.
     */
    status_t startStream(AudioStream *stream);

    /**
     * Handle a stream stop request.
     * It results in a routing reconsideration.
     *
     * @param[in] stream requester Stream.
     *
     * @return OK if stream stopped successfully, error code otherwise.
     */
    status_t stopStream(AudioStream *stream);

    /**
     * Handle a change of requested effect.
     * It results in a routing reconsideration.
     */
    void updateRequestedEffect();

    /**
     * Update devices attribute of the stream.
     *
     * @param[in] stream: stream from which the new device[s] is[are] requested.
     * @param[in] devices: mask of devices to be used.
     */
    void setDevices(AudioStream *stream, uint32_t devices);

    /**
     * Set input source attribute of an input stream.
     *
     * @param[in] streamIn: input stream on which the input source has to be set.
     * @param[in] inputSource: input source to set.
     */
    void setInputSource(AudioStream *streamIn, uint32_t inputSource);

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

    struct echo_reference_itfe *_echoReference; /**< Echo reference to use for AEC effect. */

    AudioPlatformState *_platformState; /**< Platform state pointer. */

    AudioParameterHandler *_audioParameterHandler; /**< backup and restore audio parameters. */

    IStreamInterface *_streamInterface; /**< Route Manager Stream Interface pointer. */

    android::RWLock _pfwLock; /**< PFW concurrency protection - to garantee atomic operation only. */

    CEventThread *_eventThread; /**< Worker Thread. */

    IModemAudioManagerInterface *_modemAudioManagerInterface; /**< Audio AT Manager interface. */

    bool _bluetoothHFPSupported; /**< track if platform supports Bluetooth HFP. */

    static const char *const _defaultGainPropName; /**< Gain property name. */
    static const float _defaultGainValue; /**< Default gain value if empty property. */
    /**
     * Stream Rate associated with narrow band in case of VoIP.
     */
    static const uint32_t _voipRateForNarrowBandProcessing = 8000;
    static const char *const _bluetoothHfpSupportedPropName; /**< BT HFP property name. */
    static const bool _bluetoothHfpSupportedDefaultValue; /**< Default BT HFP value. */

    static const char *const _routeLibPropName;  /**< Route Manager name property. */
    static const char *const _routeLibPropDefaultValue;  /**< Route Manager lib default value. */

    static const char *const _modemLibPropName; /**< MAMGR library name property. */

    static const char *const _restartingKey; /**< Restart key parameter. */
    static const char *const _restartingRequested; /**< Restart key parameter value. */
};
}         // namespace android
