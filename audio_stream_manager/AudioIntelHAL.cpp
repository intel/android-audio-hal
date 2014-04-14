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
#define LOG_TAG "AudioIntelHAL"

#include "AudioIntelHAL.hpp"
#include "AudioConversion.hpp"
#include "AudioParameterHandler.hpp"
#include "AudioStream.hpp"
#include "AudioStreamInImpl.hpp"
#include "AudioStreamOutImpl.hpp"
#include "AudioPlatformState.hpp"
#include <AudioCommsAssert.hpp>
#include "ModemAudioManagerInterface.h"
#include "Property.h"
#include <InterfaceProviderLib.h>
#include <hardware/audio_effect.h>
#include <hardware_legacy/power.h>
#include <media/AudioRecord.h>
#include <utils/Log.h>
#include <utils/String8.h>

#include "EventThread.h"
#include <cutils/properties.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;
using namespace android;

typedef RWLock::AutoRLock AutoR;
typedef RWLock::AutoWLock AutoW;


namespace android_audio_legacy
{
extern "C"
{
/**
 * Function for dlsym() to look up for creating a new AudioHardwareInterface.
 */
AudioHardwareInterface *createAudioHardware(void)
{
    return AudioIntelHAL::create();
}
}         // extern "C"

AudioHardwareInterface *AudioIntelHAL::create()
{

    ALOGD("Using Audio XML HAL");

    return new AudioIntelHAL();
}

const char *const AudioIntelHAL::_modemLibPropName = "audiocomms.modemLib";
const char *const AudioIntelHAL::_bluetoothHfpSupportedPropName = "Audiocomms.BT.HFP.Supported";
const bool AudioIntelHAL::_bluetoothHfpSupportedDefaultValue = true;
const char *const AudioIntelHAL::_routeLibPropName = "audiocomms.routeLib";
const char *const AudioIntelHAL::_routeLibPropDefaultValue = "audio.routemanager.so";
const char *const AudioIntelHAL::_restartingKey = "restarting";
const char *const AudioIntelHAL::_restartingRequested = "true";

AudioIntelHAL::AudioIntelHAL()
    : _echoReference(NULL),
      _platformState(new AudioPlatformState()),
      _audioParameterHandler(new AudioParameterHandler()),
      _streamInterface(NULL),
      _eventThread(new CEventThread(this)),
      _modemAudioManagerInterface(NULL),
      _bluetoothHFPSupported(TProperty<bool>(_bluetoothHfpSupportedPropName,
                                             _bluetoothHfpSupportedDefaultValue))
{
    /// MAMGR Interface
    // Try to connect a ModemAudioManager Interface
    NInterfaceProvider::IInterfaceProvider *interfaceProvider =
        getInterfaceProvider(TProperty<string>(_modemLibPropName).getValue().c_str());
    if (interfaceProvider == NULL) {

        ALOGI("No MAMGR library.");
    } else {

        // Retrieve the ModemAudioManager Interface
        NInterfaceProvider::IInterface *iface;
        iface = interfaceProvider->queryInterface(IModemAudioManagerInterface::getInterfaceName());
        _modemAudioManagerInterface = static_cast<IModemAudioManagerInterface *>(iface);
        if (_modemAudioManagerInterface == NULL) {

            ALOGE("Failed to get ModemAudioManager interface");
        } else {

            // Declare ourselves as observer
            _modemAudioManagerInterface->setModemAudioManagerObserver(this);
            _platformState->setModemEmbedded(true);
            ALOGI("Connected to a ModemAudioManager interface");
        }
    }

    // Prevents any access to PFW while starting the event thread and initialising
    AutoW lock(_pfwLock);

    // Start Event thread
    _eventThread->start();

    // Start Modem Audio Manager
    if (!startModemAudioManager()) {

        ALOGI("%s: Note that this platform is MODEM-LESS", __FUNCTION__);
    }

    /// Get the Stream Interface of the Route manager
    interfaceProvider =
        getInterfaceProvider(TProperty<string>(_routeLibPropName,
                                               _routeLibPropDefaultValue).getValue().c_str());
    if (interfaceProvider) {

        NInterfaceProvider::IInterface *interface;

        // Retrieve the Stream Interface
        interface = interfaceProvider->queryInterface(IStreamInterface::getInterfaceName());
        _streamInterface = static_cast<IStreamInterface *>(interface);
        if (_streamInterface == NULL) {

            ALOGE("Failed to get Stream Interface on RouteMgr");
            return;
        }

        ALOGD("Stream Interface on RouteMgr successfully loaded");

        /// Start Routing service
        if (_streamInterface->startService() != OK) {

            ALOGE("Could not start Route Manager stream service");
            // Reset interface pointer to give a chance for initCheck to catch any issue
            // with the RouteMgr.
            _streamInterface = NULL;
            return;
        }
        ALOGD("%s Route Manager Service successfully started", __FUNCTION__);
    }
}

AudioIntelHAL::~AudioIntelHAL()
{
    _eventThread->stop();

    // Remove parameter handler
    delete _audioParameterHandler;
    // Remove Platform State component
    delete _platformState;

    delete _eventThread;

    if (_modemAudioManagerInterface != NULL) {

        // Unsuscribe & stop ModemAudioManager
        _modemAudioManagerInterface->setModemAudioManagerObserver(NULL);
        _modemAudioManagerInterface->stop();
    }
}

status_t AudioIntelHAL::initCheck()
{
    return (_streamInterface && _platformState->isStarted()) ? OK : NO_INIT;
}

status_t AudioIntelHAL::setVoiceVolume(float volume)
{
    if (!_platformState->isModemEmbedded()) {

        return NO_ERROR;
    }
    return _streamInterface->setVoiceVolume(volume);
}

status_t AudioIntelHAL::setMasterVolume(float volume)
{
    ALOGW("%s: no implementation provided", __FUNCTION__);
    return NO_ERROR;
}

AudioStreamOut *AudioIntelHAL::openOutputStream(uint32_t devices,
                                                int *format,
                                                uint32_t *channels,
                                                uint32_t *sampleRate,
                                                status_t *status)
{
    ALOGD("%s: called for devices: 0x%08x", __FUNCTION__, devices);

    AUDIOCOMMS_ASSERT(status != NULL, "invalid status pointer");

    /**
     * output flags is passed through status parameter.
     * As changing the API of legacy hal would break backward compatibility and as
     * caracterization of output stream is given at construction only, the flags
     * has to be conveyed from the status.
     */
    audio_output_flags_t flags = static_cast<audio_output_flags_t>(*status);

    status_t &err = *status;

    if (!audio_is_output_device(devices)) {

        ALOGD("%s: called with bad devices", __FUNCTION__);
        err = BAD_VALUE;
        return NULL;
    }

    AudioStreamOutImpl *out = new AudioStreamOutImpl(this, flags);

    err = out->set(format, channels, sampleRate);
    if (err != NO_ERROR) {

        ALOGE("%s: set error.", __FUNCTION__);
        delete out;
        return NULL;
    }

    // Informs the route manager of stream creation
    _streamInterface->addStream(out);

    ALOGD("%s: output created with status=%d", __FUNCTION__, err);
    return out;
}

void AudioIntelHAL::closeOutputStream(AudioStreamOut *out)
{
    // Informs the route manager of stream destruction
    _streamInterface->removeStream(static_cast<AudioStreamOutImpl *>(out));

    delete out;
}

AudioStreamIn *AudioIntelHAL::openInputStream(uint32_t devices,
                                              int *format,
                                              uint32_t *channels,
                                              uint32_t *sampleRate,
                                              status_t *status,
                                              AudioSystem::audio_in_acoustics acoustics)
{
    ALOGD("%s: called for devices: 0x%08x", __FUNCTION__, devices);

    AUDIOCOMMS_ASSERT(status != NULL, "invalid status pointer");

    status_t &err = *status;

    if (!isInputDevice(devices)) {

        err = BAD_VALUE;
        return NULL;
    }

    AudioStreamInImpl *in = new AudioStreamInImpl(this, acoustics);

    err = in->set(format, channels, sampleRate);
    if (err != NO_ERROR) {

        ALOGE("%s: Set err", __FUNCTION__);
        delete in;
        return NULL;
    }
    // Informs the route manager of stream creation
    _streamInterface->addStream(in);

    ALOGD("%s: input created with status=%d", __FUNCTION__, err);
    return in;
}

void AudioIntelHAL::closeInputStream(AudioStreamIn *in)
{
    // Informs the route manager of stream destruction
    _streamInterface->removeStream(static_cast<AudioStreamInImpl *>(in));

    delete in;
}

status_t AudioIntelHAL::setMicMute(bool state)
{
    ALOGD("%s: %s", __FUNCTION__, state ? "true" : "false");
    AutoW lock(_pfwLock);
    _platformState->setMicMute(state);
    _platformState->applyPlatformConfiguration();
    getStreamInterface()->reconsiderRouting();
    return NO_ERROR;
}

status_t AudioIntelHAL::getMicMute(bool *state)
{
    if (state != NULL) {

        AutoR lock(_pfwLock);
        *state = _platformState->isMicMuted();
        ALOGD("%s: %s", __FUNCTION__, *state ? "true" : "false");
    }
    return NO_ERROR;
}

size_t AudioIntelHAL::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
    switch (sampleRate) {
    case 8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        ALOGW("getInputBufferSize bad sampling rate: %d", sampleRate);
        return 0;
    }
    if (format != AudioSystem::PCM_16_BIT) {
        ALOGW("getInputBufferSize bad format: %d", format);
        return 0;
    }

    // multichannel capture is currently supported only for submix
    if ((channelCount < 1) || (channelCount > 8)) {
        ALOGW("getInputBufferSize bad channel count: %d", channelCount);
        return 0;
    }

    // Returns 20ms buffer size per channel
    return (sampleRate / 25) * channelCount;
}

status_t AudioIntelHAL::dump(int fd, const Vector<String16> &args)
{
    return NO_ERROR;
}

status_t AudioIntelHAL::setParameters(const String8 &keyValuePairs)
{
    ALOGD("%s: key value pair %s", __FUNCTION__, keyValuePairs.string());
    AudioParameter param = AudioParameter(keyValuePairs);
    status_t status;
    String8 restart;

    String8 key = String8(_restartingKey);
    status = param.get(key, restart);
    if (status == NO_ERROR) {

        if (restart == _restartingRequested) {

            // Restore the audio parameters when mediaserver is restarted in case of crash.
            ALOGI("Restore audio parameters as mediaserver is restarted param=%s",
                  _audioParameterHandler->getParameters().string());
            doSetParameters(_audioParameterHandler->getParameters());
        }
        param.remove(key);
    } else {

        // Set the audio parameters
        status = doSetParameters(keyValuePairs);
        if (status == NO_ERROR) {

            ALOGV("%s: saving %s", __FUNCTION__, keyValuePairs.string());
            // Save the audio parameters for recovering audio parameters in case of crash.
            _audioParameterHandler->saveParameters(keyValuePairs);
        } else {

            return status;
        }
    }
    return NO_ERROR;
}

void AudioIntelHAL::setDevices(AudioStream *stream, uint32_t devices)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    // Update Platform state: in/out devices
    _platformState->setDevices(devices, stream->isOut());

    // set the new device for this stream
    stream->setDevices(devices);
}

void AudioIntelHAL::setInputSource(AudioStream *streamIn, uint32_t inputSource)
{
    AUDIOCOMMS_ASSERT(streamIn != NULL, "Null stream");
    AUDIOCOMMS_ASSERT(!streamIn->isOut(), "Input stream only");

    ALOGV("%s: inputSource=%d", __FUNCTION__, inputSource);

    AudioStreamInImpl *inputStream = static_cast<AudioStreamInImpl *>(streamIn);
    inputStream->setInputSource(inputSource);

    _platformState->updateActiveInputSources();

    if (inputSource == AUDIO_SOURCE_VOICE_COMMUNICATION) {

        CAudioBand::Type band = CAudioBand::EWide;
        if (streamIn->sampleRate() == _voipRateForNarrowBandProcessing) {

            band = CAudioBand::ENarrow;
        }
        _platformState->setVoIPBandType(band);
    }
}

status_t AudioIntelHAL::startStream(AudioStream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    ALOGD("%s: %s stream", __FUNCTION__, stream->isOut() ? "output" : "input");

    _pfwLock.writeLock();
    _platformState->startStream(stream);

    // Set Criteria to meta PFW
    _platformState->applyPlatformConfiguration();

    _pfwLock.unlock();

    getStreamInterface()->startStream();
    return OK;
}

status_t AudioIntelHAL::stopStream(AudioStream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    ALOGD("%s: %s stream", __FUNCTION__, stream->isOut() ? "output" : "input");

    _pfwLock.writeLock();
    _platformState->stopStream(stream);

    // Set Criteria to meta PFW
    _platformState->applyPlatformConfiguration();

    _pfwLock.unlock();

    getStreamInterface()->stopStream();
    return OK;
}

void AudioIntelHAL::updateRequestedEffect()
{
    _pfwLock.writeLock();

    _platformState->updatePreprocessorRequestedByActiveInput();

    // Set Criteria to meta PFW
    _platformState->applyPlatformConfiguration();

    _pfwLock.unlock();

    getStreamInterface()->reconsiderRouting();
}


void AudioIntelHAL::checkAndSetRoutingStreamParameter(AudioStream *stream, AudioParameter &param)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    int routingDevice;
    String8 key = String8(AudioParameter::keyRouting);

    if (param.getInt(key, routingDevice) == NO_ERROR) {

        setDevices(stream, routingDevice);
        param.remove(key);
    }
    if (stream->isOut()) {

        // For output streams, latch Android Mode
        _platformState->setMode(mode());
    }
}

void AudioIntelHAL::checkAndSetInputSourceStreamParameter(AudioStream *stream,
                                                          AudioParameter &param)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    if (!stream->isOut()) {

        int inputSource;
        String8 key = String8(AudioParameter::keyInputSource);

        if (param.getInt(key, inputSource) == NO_ERROR) {

            setInputSource(stream, inputSource);
            param.remove(key);
        }
    }
}

status_t AudioIntelHAL::setStreamParameters(AudioStream *stream,
                                            const String8 &keyValuePairs)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    ALOGD("%s: key value pair %s", __FUNCTION__, keyValuePairs.string());
    AudioParameter param = AudioParameter(keyValuePairs);
    AutoW lock(_pfwLock);

    checkAndSetRoutingStreamParameter(stream, param);

    checkAndSetInputSourceStreamParameter(stream, param);

    if (_platformState->hasPlatformStateChanged()) {

        // Set Criteria to meta PFW
        _platformState->applyPlatformConfiguration();

        // Release PFW ressource
        _pfwLock.unlock();

        ALOGD("%s: {+++ RECONSIDER ROUTING +++} due to %s stream parameter change",
              __FUNCTION__,
              stream->isOut() ? "output" : "input");
        getStreamInterface()->reconsiderRouting();

        // Relock (as using autolock)
        _pfwLock.writeLock();
    }
    if (param.size()) {

        // No more? Just log!
        ALOGW("%s: Unhandled argument: %s", __FUNCTION__, keyValuePairs.string());
    }
    return NO_ERROR;
}

status_t AudioIntelHAL::doSetParameters(const String8 &keyValuePairs)
{
    AutoW lock(_pfwLock);

    _platformState->setParameters(keyValuePairs);

    if (_platformState->hasPlatformStateChanged()) {

        ALOGD("%s: External parameter change", __FUNCTION__);

        // Apply Configuration
        _platformState->applyPlatformConfiguration();

        // Release PFS ressource
        _pfwLock.unlock();

        // Trig the route manager
        getStreamInterface()->reconsiderRouting();

        // Relock (as using autolock)
        _pfwLock.writeLock();
    }

    return NO_ERROR;
}

void AudioIntelHAL::resetEchoReference(struct echo_reference_itfe *reference)
{
    ALOGD(" %s(reference=%p)", __FUNCTION__, reference);
    // Check that the reset is possible:
    //  - reference and _echoReference shall both point to the same struct (consistency check)
    //  - they should not be NULL because the reset process will remove the reference from
    //    the output voice stream and then free the local echo reference.
    if ((reference == NULL) || (_echoReference == NULL) || (_echoReference != reference)) {

        /* Nothing to do */
        return;
    }

    // Get active voice output stream
    Stream *stream = getStreamInterface()->getVoiceOutputStream();
    if (stream == NULL) {

        ALOGE("%s: no voice output found,"
              " so problem to provide data reference for AEC effect!", __FUNCTION__);
        return;
    }
    AudioStreamOutImpl *out = static_cast<AudioStreamOutImpl *>(stream);
    out->removeEchoReference(_echoReference);
    release_echo_reference(_echoReference);
    _echoReference = NULL;
}

struct echo_reference_itfe *AudioIntelHAL::getEchoReference(const SampleSpec &inputSampleSpec)
{
    ALOGD("%s ()", __FUNCTION__);
    resetEchoReference(_echoReference);

    // Get active voice output stream
    Stream *stream = getStreamInterface()->getVoiceOutputStream();
    if (stream == NULL) {

        ALOGE("%s: no voice output found,"
              " so problem to provide data reference for AEC effect!", __FUNCTION__);
        return NULL;
    }
    AudioStreamOutImpl *out = static_cast<AudioStreamOutImpl *>(stream);
    SampleSpec outputSampleSpec = out->streamSampleSpec();

    if (create_echo_reference(inputSampleSpec.getFormat(),
                              inputSampleSpec.getChannelCount(),
                              inputSampleSpec.getSampleRate(),
                              outputSampleSpec.getFormat(),
                              outputSampleSpec.getChannelCount(),
                              outputSampleSpec.getSampleRate(),
                              &_echoReference) < 0) {

        ALOGE("%s: Could not create echo reference", __FUNCTION__);
        return NULL;
    }
    out->addEchoReference(_echoReference);

    ALOGD(" %s() will return that mEchoReference=%p", __FUNCTION__, _echoReference);
    return _echoReference;
}

void AudioIntelHAL::onModemAudioStatusChanged()
{
    ALOGD("%s", __FUNCTION__);
    _eventThread->trig(UpdateModemAudioStatus);
}

void AudioIntelHAL::onModemStateChanged()
{
    ALOGD("%s", __FUNCTION__);
    _eventThread->trig(UpdateModemState);
}

void AudioIntelHAL::onModemAudioBandChanged()
{
    ALOGD("%s", __FUNCTION__);
    _eventThread->trig(UpdateModemAudioBand);
}

bool AudioIntelHAL::onEvent(int fd)
{
    return false;
}

bool AudioIntelHAL::onError(int fd)
{
    return false;
}

bool AudioIntelHAL::onHangup(int fd)
{
    return false;
}

void AudioIntelHAL::onAlarm()
{
}

void AudioIntelHAL::onPollError()
{
}

bool AudioIntelHAL::onProcess(uint16_t event)
{
    AutoW lock(_pfwLock);

    bool forceResync = false;

    switch (event) {
    case UpdateModemAudioBand:

        ALOGD("%s: Modem Band change", __FUNCTION__);
        _platformState->setCsvBandType(_modemAudioManagerInterface->getAudioBand());
        break;

    case UpdateModemState:
        ALOGD("%s: Modem State change", __FUNCTION__);
        _platformState->setModemAlive(_modemAudioManagerInterface->isModemAlive());
        forceResync = _platformState->isModemAlive();
        break;

    case UpdateModemAudioStatus:
        ALOGD("%s: Modem Audio Status change", __FUNCTION__);
        _platformState->setModemAudioAvailable(
            _modemAudioManagerInterface->isModemAudioAvailable());
        break;

    default:
        ALOGE("%s: Unhandled event.", __FUNCTION__);
        break;
    }
    _platformState->applyPlatformConfiguration();

    getStreamInterface()->reconsiderRouting(forceResync);
    return false;
}


bool AudioIntelHAL::startModemAudioManager()
{
    if (_modemAudioManagerInterface == NULL) {

        return false;
    }
    if (!_modemAudioManagerInterface->start()) {

        ALOGW("%s: could not start ModemAudioManager", __FUNCTION__);
        _modemAudioManagerInterface->setModemAudioManagerObserver(NULL);
        _modemAudioManagerInterface = NULL;
        return false;
    }
    /// Initialize current modem status
    // Modem status
    _platformState->setModemAlive(_modemAudioManagerInterface->isModemAlive());
    // Modem audio availability
    _platformState->setModemAudioAvailable(_modemAudioManagerInterface->isModemAudioAvailable());
    // Modem band
    _platformState->setCsvBandType(_modemAudioManagerInterface->getAudioBand());
    return true;
}

void AudioIntelHAL::printPlatformFwErrorInfo()
{
    ALOGD("%s", __FUNCTION__);

    _pfwLock.writeLock();

    // Dump on console platform hw debug files
    _platformState->printPlatformFwErrorInfo();

    _pfwLock.unlock();
}

}       // namespace android
