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

#include "Device.hpp"
#include "AudioConversion.hpp"
#include "AudioParameterHandler.hpp"
#include "StreamIn.hpp"
#include "StreamOut.hpp"
#include <AudioPlatformState.hpp>
#include <AudioCommsAssert.hpp>
#include <hardware/audio.h>
#include "Property.h"
#include <InterfaceProviderLib.h>
#include <hardware/audio_effect.h>
#include <media/AudioRecord.h>
#include <utils/Log.h>
#include <string>
#include <cutils/properties.h>

using namespace std;
using android::status_t;

namespace intel_audio
{
extern "C"
{
/**
 * Function for dlsym() to look up for creating a new DeviceInterface.
 */
DeviceInterface *createAudioHardware(void)
{
    return Device::create();
}
}         // extern "C"

DeviceInterface *Device::create()
{
    ALOGD("Using Audio XML HAL");
    return new Device();
}

const char *const Device::mBluetoothHfpSupportedPropName = "Audiocomms.BT.HFP.Supported";
const bool Device::mBluetoothHfpSupportedDefaultValue = true;
const char *const Device::mRouteLibPropName = "audiocomms.routeLib";
const char *const Device::mRouteLibPropDefaultValue = "audio.routemanager.so";
const char *const Device::mRestartingKey = "restarting";
const char *const Device::mRestartingRequested = "true";

Device::Device()
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
    if (mPlatformState->start() != android::OK) {

        ALOGE("%s: could not start Platform State", __FUNCTION__);
        mStreamInterface = NULL;
        delete mPlatformState;
        mPlatformState = NULL;
        return;
    }

    /// Start Routing service
    if (mStreamInterface->startService() != android::OK) {

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

Device::~Device()
{
    // Remove parameter handler
    delete mAudioParameterHandler;
    // Remove Platform State component
    delete mPlatformState;
}

status_t Device::initCheck() const
{
    return (mStreamInterface && mPlatformState && mPlatformState->isStarted()) ?
           android::OK : android::NO_INIT;
}

status_t Device::setVoiceVolume(float volume)
{
    return mStreamInterface->setVoiceVolume(volume);
}

android::status_t Device::openOutputStream(audio_io_handle_t /*handle*/,
                                           audio_devices_t devices,
                                           audio_output_flags_t flags,
                                           audio_config_t &config,
                                           StreamOutInterface *&stream,
                                           const std::string &/*address*/)
{
    ALOGD("%s: called for devices: 0x%08x", __FUNCTION__, devices);

    if (!audio_is_output_devices(devices)) {
        ALOGE("%s: called with bad devices", __FUNCTION__);
        return android::BAD_VALUE;
    }
    StreamOut *out = new StreamOut(this, flags);
    status_t err = out->set(config);
    if (err != android::OK) {
        ALOGE("%s: set error.", __FUNCTION__);
        delete out;
        return err;
    }
    // Informs the route manager of stream creation
    mStreamInterface->addStream(out);
    stream = out;

    ALOGD("%s: output created with status=%d", __FUNCTION__, err);
    return android::OK;
}

void Device::closeOutputStream(StreamOutInterface *out)
{
    // Informs the route manager of stream destruction
    AUDIOCOMMS_ASSERT(out != NULL, "Invalid output stream to remove");
    mStreamInterface->removeStream(static_cast<StreamOut *>(out));
    delete out;
}

android::status_t Device::openInputStream(audio_io_handle_t /*handle*/,
                                          audio_devices_t devices,
                                          audio_config_t &config,
                                          StreamInInterface *&stream,
                                          audio_input_flags_t /*flags*/,
                                          const std::string &/*address*/,
                                          audio_source_t source)
{
    ALOGD("%s: called for devices: 0x%08x and input source: 0x%08x", __FUNCTION__, devices, source);

    if (!audio_is_input_device(devices)) {
        ALOGE("%s: called with bad device 0x%08x", __FUNCTION__, devices);
        return android::BAD_VALUE;
    }

    StreamIn *in = new StreamIn(this, source);
    status_t err = in->set(config);
    if (err != android::OK) {
        ALOGE("%s: Set err", __FUNCTION__);
        delete in;
        return err;
    }
    // Informs the route manager of stream creation
    mStreamInterface->addStream(in);
    stream = in;

    ALOGD("%s: input created with status=%d", __FUNCTION__, err);
    return android::OK;
}

void Device::closeInputStream(StreamInInterface *in)
{
    // Informs the route manager of stream destruction
    AUDIOCOMMS_ASSERT(in != NULL, "Invalid input stream to remove");
    mStreamInterface->removeStream(static_cast<StreamIn *>(in));
    delete in;
}

status_t Device::setMicMute(bool mute)
{
    ALOGV("%s: %s", __FUNCTION__, mute ? "true" : "false");
    KeyValuePairs pair;
    pair.add(AudioPlatformState::mKeyMicMute, mute);
    return mPlatformState->setParameters(pair.toString());
}

status_t Device::getMicMute(bool &muted) const
{
    KeyValuePairs pair(mPlatformState->getParameters(AudioPlatformState::mKeyMicMute));
    status_t res = pair.get(AudioPlatformState::mKeyMicMute, muted);
    return res;
}

size_t Device::getInputBufferSize(const struct audio_config &config) const
{
    switch (config.sample_rate) {
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
        ALOGW("%s bad sampling rate: %d", __FUNCTION__, config.sample_rate);
        return 0;
    }
    if (config.format != AUDIO_FORMAT_PCM_16_BIT) {
        ALOGW("%s bad format: %d", __FUNCTION__, config.format);
        return 0;
    }
    uint32_t channelCount = popcount(config.channel_mask);

    // multichannel capture is currently supported only for submix
    if ((channelCount < 1) || (channelCount > 8)) {
        ALOGW("%s bad channel count: %d", __FUNCTION__, channelCount);
        return 0;
    }

    // Returns 20ms buffer size per channel
    return (config.sample_rate / 25) * channelCount;
}

status_t Device::setParameters(const string &keyValuePairs)
{
    ALOGV("%s: key value pair %s", __FUNCTION__, keyValuePairs.c_str());
    KeyValuePairs pairs(keyValuePairs);
    string restart;
    string key(mRestartingKey);
    status_t status = pairs.get(key, restart);
    if (status == android::OK) {

        if (restart == mRestartingRequested) {

            // Restore the audio parameters when mediaserver is restarted in case of crash.
            ALOGI("Restore audio parameters as mediaserver is restarted param=%s",
                  mAudioParameterHandler->getParameters().c_str());
            mPlatformState->setParameters(mAudioParameterHandler->getParameters());
        }
        pairs.remove(key);
    } else {

        // Set the audio parameters
        status = mPlatformState->setParameters(keyValuePairs);
        if (status == android::OK) {

            ALOGV("%s: saving %s", __FUNCTION__, keyValuePairs.string());
            // Save the audio parameters for recovering audio parameters in case of crash.
            mAudioParameterHandler->saveParameters(keyValuePairs);
        }
    }
    return status;
}

string Device::getParameters(const string &keys) const
{
    ALOGV("%s: requested keys %s", __FUNCTION__, keys.c_str());
    return mPlatformState->getParameters(keys);
}

android::status_t Device::setMode(audio_mode_t mode)
{
    mMode = mode;
    return android::OK;
}


status_t Device::startStream(Stream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    ALOGD("%s: %s stream", __FUNCTION__, stream->isOut() ? "output" : "input");
    mPlatformState->startStream(stream);
    getStreamInterface()->startStream();
    return android::OK;
}

status_t Device::stopStream(Stream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    ALOGD("%s: %s stream", __FUNCTION__, stream->isOut() ? "output" : "input");
    mPlatformState->stopStream(stream);
    getStreamInterface()->stopStream();
    return android::OK;
}

void Device::updateRequestedEffect()
{
    mPlatformState->updateRequestedEffect();
    getStreamInterface()->reconsiderRouting();
}

status_t Device::setStreamParameters(Stream *stream, const string &keyValuePairs)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Null stream");
    KeyValuePairs pairs(keyValuePairs);
    if (stream->isOut()) {
        // Appends the mode key only for output stream, as the policy may only reconsider
        // the mode and selects a new parameter for output
        pairs.add(AudioPlatformState::mKeyAndroidMode, mode());
    }
    ALOGV("%s: key value pair %s", __FUNCTION__, pairs.toString().c_str());
    return mPlatformState->setParameters(pairs.toString());
}

void Device::resetEchoReference(struct echo_reference_itfe *reference)
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
    IoStream *stream = getStreamInterface()->getVoiceOutputStream();
    if (stream == NULL) {

        ALOGE("%s: no voice output found,"
              " so problem to provide data reference for AEC effect!", __FUNCTION__);
        return;
    }
    StreamOut *out = static_cast<StreamOut *>(stream);
    out->removeEchoReference(mEchoReference);
    release_echo_reference(mEchoReference);
    mEchoReference = NULL;
}

struct echo_reference_itfe *Device::getEchoReference(const SampleSpec &inputSampleSpec)
{
    ALOGD("%s ()", __FUNCTION__);
    resetEchoReference(mEchoReference);

    // Get active voice output stream
    IoStream *stream = getStreamInterface()->getVoiceOutputStream();
    if (stream == NULL) {

        ALOGE("%s: no voice output found,"
              " so problem to provide data reference for AEC effect!", __FUNCTION__);
        return NULL;
    }
    StreamOut *out = static_cast<StreamOut *>(stream);
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

void Device::printPlatformFwErrorInfo()
{
    mPlatformState->printPlatformFwErrorInfo();
}

} // namespace intel_audio
