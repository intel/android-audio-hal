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
#define LOG_TAG "AudioIntelHal"

#include "AudioIntelHal.hpp"
#include "AudioConversion.hpp"
#include "AudioParameterHandler.hpp"
#include "AudioStream.hpp"
#include "AudioStreamInImpl.hpp"
#include "AudioStreamOutImpl.hpp"
#include <AudioPlatformState.hpp>
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
    return AudioIntelHal::create();
}
}         // extern "C"

AudioHardwareInterface *AudioIntelHal::create()
{

    ALOGD("Using Audio XML HAL");

    return new AudioIntelHal();
}

const char *const AudioIntelHal::mModemLibPropName = "audiocomms.modemLib";
const char *const AudioIntelHal::mBluetoothHfpSupportedPropName = "Audiocomms.BT.HFP.Supported";
const bool AudioIntelHal::mBluetoothHfpSupportedDefaultValue = true;
const char *const AudioIntelHal::mRouteLibPropName = "audiocomms.routeLib";
const char *const AudioIntelHal::mRouteLibPropDefaultValue = "audio.routemanager.so";
const char *const AudioIntelHal::mRestartingKey = "restarting";
const char *const AudioIntelHal::mRestartingRequested = "true";

AudioIntelHal::AudioIntelHal()
    : mEchoReference(NULL),
      mPlatformState(NULL),
      mAudioParameterHandler(new AudioParameterHandler()),
      mStreamInterface(NULL),
      mEventThread(new CEventThread(this)),
      mModemAudioManagerInterface(NULL),
      _bluetoothHFPSupported(TProperty<bool>(mBluetoothHfpSupportedPropName,
                                             mBluetoothHfpSupportedDefaultValue))
{
    // Prevents any access to PFW during whole construction of the Audio HAL.
    AutoW lock(mPfwLock);

    bool platformHasModem = false;

    /// MAMGR Interface
    // Try to connect a ModemAudioManager Interface
    NInterfaceProvider::IInterfaceProvider *interfaceProvider =
        getInterfaceProvider(TProperty<string>(mModemLibPropName).getValue().c_str());
    if (interfaceProvider == NULL) {

        ALOGI("No MAMGR library.");
    } else {

        // Retrieve the ModemAudioManager Interface
        NInterfaceProvider::IInterface *iface;
        iface = interfaceProvider->queryInterface(IModemAudioManagerInterface::getInterfaceName());
        mModemAudioManagerInterface = static_cast<IModemAudioManagerInterface *>(iface);
        if (mModemAudioManagerInterface == NULL) {

            ALOGE("Failed to get ModemAudioManager interface");
        } else {

            // Declare ourselves as observer
            mModemAudioManagerInterface->setModemAudioManagerObserver(this);
            platformHasModem = true;
            ALOGI("Connected to a ModemAudioManager interface");
        }
    }

    // Start Event thread
    mEventThread->start();

    // Start Modem Audio Manager
    if (!startModemAudioManager()) {

        ALOGI("%s: Note that this platform is MODEM-LESS", __FUNCTION__);
    }

    /// Get the Stream Interface of the Route manager
    interfaceProvider =
        getInterfaceProvider(TProperty<string>(mRouteLibPropName,
                                               mRouteLibPropDefaultValue).getValue().c_str());
    if (interfaceProvider) {

        NInterfaceProvider::IInterface *interface;

        // Retrieve the Stream Interface
        interface = interfaceProvider->queryInterface(IStreamInterface::getInterfaceName());
        mStreamInterface = static_cast<IStreamInterface *>(interface);
        if (mStreamInterface == NULL) {

            ALOGE("Failed to get Stream Interface on RouteMgr");
            return;
        }
    }

    /// Construct the platform state component and start it
    mPlatformState = new AudioPlatformState(mStreamInterface);
    if (mPlatformState->start() != OK) {

        ALOGE("%s: could not start Platform State", __FUNCTION__);
        mStreamInterface = NULL;
        delete mPlatformState;
        mPlatformState = NULL;
        return;
    }

    /// Initialize current modem status
    // Has Modem
    mPlatformState->setModemEmbedded(platformHasModem);
    // Modem status
    mPlatformState->setModemAlive(mModemAudioManagerInterface->isModemAlive());
    // Modem audio availability
    mPlatformState->setModemAudioAvailable(mModemAudioManagerInterface->isModemAudioAvailable());
    // Modem band
    mPlatformState->setCsvBandType(mModemAudioManagerInterface->getAudioBand());

    /// Start Routing service
    if (mStreamInterface->startService() != OK) {

        ALOGE("Could not start Route Manager stream service");
        // Reset interface pointer to give a chance for initCheck to catch any issue
        // with the RouteMgr.
        mStreamInterface = NULL;
        delete mPlatformState;
        mPlatformState = NULL;
        return;
    }
    ALOGD("%s Route Manager Service successfully started", __FUNCTION__);
}

AudioIntelHal::~AudioIntelHal()
{
    mEventThread->stop();

    // Remove parameter handler
    delete mAudioParameterHandler;
    // Remove Platform State component
    delete mPlatformState;

    delete mEventThread;

    if (mModemAudioManagerInterface != NULL) {

        // Unsuscribe & stop ModemAudioManager
        mModemAudioManagerInterface->setModemAudioManagerObserver(NULL);
        mModemAudioManagerInterface->stop();
    }
}

status_t AudioIntelHal::initCheck()
{
    return (mStreamInterface && mPlatformState->isStarted()) ? OK : NO_INIT;
}

status_t AudioIntelHal::setVoiceVolume(float volume)
{
    if (!mPlatformState->isModemEmbedded()) {

        return NO_ERROR;
    }
    return mStreamInterface->setVoiceVolume(volume);
}

status_t AudioIntelHal::setMasterVolume(float volume)
{
    ALOGW("%s: no implementation provided", __FUNCTION__);
    return NO_ERROR;
}

AudioStreamOut *AudioIntelHal::openOutputStream(uint32_t devices,
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
    mStreamInterface->addStream(out);

    ALOGD("%s: output created with status=%d", __FUNCTION__, err);
    return out;
}

void AudioIntelHal::closeOutputStream(AudioStreamOut *out)
{
    // Informs the route manager of stream destruction
    mStreamInterface->removeStream(static_cast<AudioStreamOutImpl *>(out));

    delete out;
}

AudioStreamIn *AudioIntelHal::openInputStream(uint32_t devices,
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
    mStreamInterface->addStream(in);

    ALOGD("%s: input created with status=%d", __FUNCTION__, err);
    return in;
}

void AudioIntelHal::closeInputStream(AudioStreamIn *in)
{
    // Informs the route manager of stream destruction
    mStreamInterface->removeStream(static_cast<AudioStreamInImpl *>(in));

    delete in;
}

status_t AudioIntelHal::setMicMute(bool state)
{
    ALOGD("%s: %s", __FUNCTION__, state ? "true" : "false");
    AutoW lock(mPfwLock);
    mPlatformState->setMicMute(state);
    mPlatformState->applyPlatformConfiguration();
    getStreamInterface()->reconsiderRouting();
    return NO_ERROR;
}

status_t AudioIntelHal::getMicMute(bool *state)
{
    if (state != NULL) {

        AutoR lock(mPfwLock);
        *state = mPlatformState->isMicMuted();
        ALOGD("%s: %s", __FUNCTION__, *state ? "true" : "false");
    }
    return NO_ERROR;
}

size_t AudioIntelHal::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
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

status_t AudioIntelHal::dump(int fd, const Vector<String16> &args)
{
    return NO_ERROR;
}

status_t AudioIntelHal::setParameters(const String8 &keyValuePairs)
{
    ALOGD("%s: key value pair %s", __FUNCTION__, keyValuePairs.string());
    AudioParameter param = AudioParameter(keyValuePairs);
    status_t status;
    String8 restart;

    String8 key = String8(mRestartingKey);
    status = param.get(key, restart);
    if (status == NO_ERROR) {

        if (restart == mRestartingRequested) {

            // Restore the audio parameters when mediaserver is restarted in case of crash.
            ALOGI("Restore audio parameters as mediaserver is restarted param=%s",
                  mAudioParameterHandler->getParameters().string());
            doSetParameters(mAudioParameterHandler->getParameters());
        }
        param.remove(key);
    } else {

        // Set the audio parameters
        status = doSetParameters(keyValuePairs);
        if (status == NO_ERROR) {

            ALOGV("%s: saving %s", __FUNCTION__, keyValuePairs.string());
            // Save the audio parameters for recovering audio parameters in case of crash.
            mAudioParameterHandler->saveParameters(keyValuePairs);
        } else {

            return status;
        }
    }
    return NO_ERROR;
}

String8 AudioIntelHal::getParameters(const String8 &keys)
{
    ALOGD("%s: requested keys %s", __FUNCTION__, keys.string());
    AutoR lock(mPfwLock);

    return mPlatformState->getParameters(keys);
}

void AudioIntelHal::setDevices(AudioStream *stream, uint32_t devices)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    // Update Platform state: in/out devices
    mPlatformState->setDevices(devices, stream->isOut());

    // set the new device for this stream
    stream->setDevices(devices);
}

void AudioIntelHal::setInputSource(AudioStream *streamIn, uint32_t inputSource)
{
    AUDIOCOMMS_ASSERT(streamIn != NULL, "Null stream");
    AUDIOCOMMS_ASSERT(!streamIn->isOut(), "Input stream only");

    ALOGV("%s: inputSource=%d", __FUNCTION__, inputSource);

    AudioStreamInImpl *inputStream = static_cast<AudioStreamInImpl *>(streamIn);
    inputStream->setInputSource(inputSource);

    mPlatformState->updateActiveInputSources();
}

status_t AudioIntelHal::startStream(AudioStream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    ALOGD("%s: %s stream", __FUNCTION__, stream->isOut() ? "output" : "input");

    mPfwLock.writeLock();
    mPlatformState->startStream(stream);

    // Set Criteria to meta PFW
    mPlatformState->applyPlatformConfiguration();

    mPfwLock.unlock();

    getStreamInterface()->startStream();
    return OK;
}

status_t AudioIntelHal::stopStream(AudioStream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    ALOGD("%s: %s stream", __FUNCTION__, stream->isOut() ? "output" : "input");

    mPfwLock.writeLock();
    mPlatformState->stopStream(stream);

    // Set Criteria to meta PFW
    mPlatformState->applyPlatformConfiguration();

    mPfwLock.unlock();

    getStreamInterface()->stopStream();
    return OK;
}

void AudioIntelHal::updateRequestedEffect()
{
    mPfwLock.writeLock();

    mPlatformState->updateParametersFromActiveInput();

    // Set Criteria to meta PFW
    mPlatformState->applyPlatformConfiguration();

    mPfwLock.unlock();

    getStreamInterface()->reconsiderRouting();
}


void AudioIntelHal::checkAndSetRoutingStreamParameter(AudioStream *stream, AudioParameter &param)
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
        mPlatformState->setMode(mode());
    }
}

void AudioIntelHal::checkAndSetInputSourceStreamParameter(AudioStream *stream,
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

status_t AudioIntelHal::setStreamParameters(AudioStream *stream,
                                            const String8 &keyValuePairs)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    ALOGD("%s: key value pair %s", __FUNCTION__, keyValuePairs.string());
    AudioParameter param = AudioParameter(keyValuePairs);
    AutoW lock(mPfwLock);

    checkAndSetRoutingStreamParameter(stream, param);

    checkAndSetInputSourceStreamParameter(stream, param);

    if (mPlatformState->hasPlatformStateChanged()) {

        // Set Criteria to meta PFW
        mPlatformState->applyPlatformConfiguration();

        // Release PFW ressource
        mPfwLock.unlock();

        ALOGD("%s: {+++ RECONSIDER ROUTING +++} due to %s stream parameter change",
              __FUNCTION__,
              stream->isOut() ? "output" : "input");
        getStreamInterface()->reconsiderRouting();

        // Relock (as using autolock)
        mPfwLock.writeLock();
    }
    if (param.size()) {

        // No more? Just log!
        ALOGW("%s: Unhandled argument: %s", __FUNCTION__, keyValuePairs.string());
    }
    return NO_ERROR;
}

status_t AudioIntelHal::doSetParameters(const String8 &keyValuePairs)
{
    AutoW lock(mPfwLock);

    mPlatformState->setParameters(keyValuePairs);

    if (mPlatformState->hasPlatformStateChanged()) {

        ALOGD("%s: External parameter change", __FUNCTION__);

        // Apply Configuration
        mPlatformState->applyPlatformConfiguration();

        // Release PFS ressource
        mPfwLock.unlock();

        // Trig the route manager
        getStreamInterface()->reconsiderRouting();

        // Relock (as using autolock)
        mPfwLock.writeLock();
    }

    return NO_ERROR;
}

void AudioIntelHal::resetEchoReference(struct echo_reference_itfe *reference)
{
    ALOGD(" %s(reference=%p)", __FUNCTION__, reference);
    // Check that the reset is possible:
    //  - reference and _echoReference shall both point to the same struct (consistency check)
    //  - they should not be NULL because the reset process will remove the reference from
    //    the output voice stream and then free the local echo reference.
    if ((reference == NULL) || (mEchoReference == NULL) || (mEchoReference != reference)) {

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
    out->removeEchoReference(mEchoReference);
    release_echo_reference(mEchoReference);
    mEchoReference = NULL;
}

struct echo_reference_itfe *AudioIntelHal::getEchoReference(const SampleSpec &inputSampleSpec)
{
    ALOGD("%s ()", __FUNCTION__);
    resetEchoReference(mEchoReference);

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
                              &mEchoReference) < 0) {

        ALOGE("%s: Could not create echo reference", __FUNCTION__);
        return NULL;
    }
    out->addEchoReference(mEchoReference);

    ALOGD(" %s() will return that mEchoReference=%p", __FUNCTION__, mEchoReference);
    return mEchoReference;
}

void AudioIntelHal::onModemAudioStatusChanged()
{
    ALOGD("%s", __FUNCTION__);
    mEventThread->trig(UpdateModemAudioStatus);
}

void AudioIntelHal::onModemStateChanged()
{
    ALOGD("%s", __FUNCTION__);
    mEventThread->trig(UpdateModemState);
}

void AudioIntelHal::onModemAudioBandChanged()
{
    ALOGD("%s", __FUNCTION__);
    mEventThread->trig(UpdateModemAudioBand);
}

bool AudioIntelHal::onEvent(int fd)
{
    return false;
}

bool AudioIntelHal::onError(int fd)
{
    return false;
}

bool AudioIntelHal::onHangup(int fd)
{
    return false;
}

void AudioIntelHal::onAlarm()
{
}

void AudioIntelHal::onPollError()
{
}

bool AudioIntelHal::onProcess(uint16_t event)
{
    AutoW lock(mPfwLock);

    bool forceResync = false;

    switch (event) {
    case UpdateModemAudioBand:

        ALOGD("%s: Modem Band change", __FUNCTION__);
        mPlatformState->setCsvBandType(mModemAudioManagerInterface->getAudioBand());
        break;

    case UpdateModemState:
        ALOGD("%s: Modem State change", __FUNCTION__);
        mPlatformState->setModemAlive(mModemAudioManagerInterface->isModemAlive());
        forceResync = mPlatformState->isModemAlive();
        break;

    case UpdateModemAudioStatus:
        ALOGD("%s: Modem Audio Status change", __FUNCTION__);
        mPlatformState->setModemAudioAvailable(
            mModemAudioManagerInterface->isModemAudioAvailable());
        break;

    default:
        ALOGE("%s: Unhandled event.", __FUNCTION__);
        break;
    }
    mPlatformState->applyPlatformConfiguration();

    getStreamInterface()->reconsiderRouting(forceResync);
    return false;
}


bool AudioIntelHal::startModemAudioManager()
{
    if (mModemAudioManagerInterface == NULL) {

        return false;
    }
    if (!mModemAudioManagerInterface->start()) {

        ALOGW("%s: could not start ModemAudioManager", __FUNCTION__);
        mModemAudioManagerInterface->setModemAudioManagerObserver(NULL);
        mModemAudioManagerInterface = NULL;
        return false;
    }
    return true;
}

void AudioIntelHal::printPlatformFwErrorInfo()
{
    ALOGD("%s", __FUNCTION__);

    mPfwLock.writeLock();

    // Dump on console platform hw debug files
    mPlatformState->printPlatformFwErrorInfo();

    mPfwLock.unlock();
}

}       // namespace android
