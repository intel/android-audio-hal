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
#include "AudioPlatformState.hpp"
#include <AudioCommsAssert.hpp>
#include "Property.h"
#include <InterfaceProviderLib.h>
#include <hardware/audio_effect.h>
#include <hardware_legacy/power.h>
#include <media/AudioRecord.h>
#include <utils/Log.h>
#include <utils/String8.h>

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
      mBluetoothHFPSupported(TProperty<bool>(mBluetoothHfpSupportedPropName,
                                             mBluetoothHfpSupportedDefaultValue))
{
    /// Get the Stream Interface of the Route manager
    NInterfaceProvider::IInterfaceProvider *interfaceProvider =
        getInterfaceProvider(TProperty<string>(mRouteLibPropName,
                                               mRouteLibPropDefaultValue).getValue().c_str());
    if (!interfaceProvider) {
        ALOGE("%s: Could not connect to interface provider", __FUNCTION__);
        return;
    }
    // Retrieve the Stream Interface
    mStreamInterface = interfaceProvider->queryInterface<IStreamInterface>();
    if (mStreamInterface == NULL) {

        ALOGE("Failed to get Stream Interface on RouteMgr");
        return;
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
    mPlatformState->sync();

    mStreamInterface->reconsiderRouting();

    ALOGD("%s Route Manager Service successfully started", __FUNCTION__);
}

AudioIntelHal::~AudioIntelHal()
{
    // Remove parameter handler
    delete mAudioParameterHandler;
    // Remove Platform State component
    delete mPlatformState;
}

status_t AudioIntelHal::initCheck()
{
    return (mStreamInterface && mPlatformState && mPlatformState->isStarted()) ? OK : NO_INIT;
}

status_t AudioIntelHal::setVoiceVolume(float volume)
{
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

AudioStreamOut *AudioIntelHal::openOutputStreamWithFlags(uint32_t devices,
                                                audio_output_flags_t flags,
                                                int *format,
                                                uint32_t *channels,
                                                uint32_t *sampleRate,
                                                status_t *status)
{
    ALOGD("%s: called for devices: 0x%08x", __FUNCTION__, devices);

    AUDIOCOMMS_ASSERT(status != NULL, "invalid status pointer");

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
    AudioParameter param = AudioParameter();
    param.addInt(String8(AudioPlatformState::mKeyMicMute.c_str()), state);
    return mPlatformState->setParameters(param.toString());
}

status_t AudioIntelHal::getMicMute(bool *state)
{
    if (state == NULL) {
        return NO_ERROR;
    }
    String8 keyMicMute = String8(AudioPlatformState::mKeyMicMute.c_str());
    AudioParameter param(mPlatformState->getParameters(keyMicMute));
    int micMute;
    status_t res = param.getInt(keyMicMute, micMute);
    *state = (micMute != 0);
    return res;
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
    String8 restart;
    String8 key = String8(mRestartingKey);
    status_t status = param.get(key, restart);
    if (status == NO_ERROR) {

        if (restart == mRestartingRequested) {

            // Restore the audio parameters when mediaserver is restarted in case of crash.
            ALOGI("Restore audio parameters as mediaserver is restarted param=%s",
                  mAudioParameterHandler->getParameters().string());
            mPlatformState->setParameters(mAudioParameterHandler->getParameters());
        }
        param.remove(key);
    } else {

        // Set the audio parameters
        status = mPlatformState->setParameters(keyValuePairs);
        if (status == NO_ERROR) {

            ALOGV("%s: saving %s", __FUNCTION__, keyValuePairs.string());
            // Save the audio parameters for recovering audio parameters in case of crash.
            mAudioParameterHandler->saveParameters(keyValuePairs);
        }
    }
    return status;
}

String8 AudioIntelHal::getParameters(const String8 &keys)
{
    ALOGD("%s: requested keys %s", __FUNCTION__, keys.string());
    return mPlatformState->getParameters(keys);
}

status_t AudioIntelHal::startStream(AudioStream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    ALOGD("%s: %s stream", __FUNCTION__, stream->isOut() ? "output" : "input");
    mPlatformState->startStream(stream);
    getStreamInterface()->startStream();
    return OK;
}

status_t AudioIntelHal::stopStream(AudioStream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    ALOGD("%s: %s stream", __FUNCTION__, stream->isOut() ? "output" : "input");
    mPlatformState->stopStream(stream);
    getStreamInterface()->stopStream();
    return OK;
}

void AudioIntelHal::updateRequestedEffect()
{
    mPlatformState->updateRequestedEffect();
    getStreamInterface()->reconsiderRouting();
}

status_t AudioIntelHal::setStreamParameters(AudioStream *stream, const String8 &keyValuePairs)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    AudioParameter param = AudioParameter(keyValuePairs);
    if (stream->isOut()) {
        // Appends the mode key only for output stream, as the policy may only reconsider
        // the mode and selects a new parameter for output
        param.addInt(String8(AudioPlatformState::mKeyAndroidMode.c_str()), mode());
    }
    ALOGD("%s: key value pair %s", __FUNCTION__, param.toString().string());
    return mPlatformState->setParameters(param.toString());
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

void AudioIntelHal::printPlatformFwErrorInfo()
{
    mPlatformState->printPlatformFwErrorInfo();
}

}       // namespace android
