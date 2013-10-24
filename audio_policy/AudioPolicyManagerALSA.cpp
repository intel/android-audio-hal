/*
 * INTEL CONFIDENTIAL
 * Copyright © 2013 Intel
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
 * disclosed in any way without Intel’s prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */

#define LOG_TAG "AudioPolicyManagerALSA"

#include <utils/Log.h>
#include "AudioPolicyManagerALSA.h"
#include <media/mediarecorder.h>
#include <Property.h>
#include <AudioCommsAssert.hpp>


namespace android_audio_legacy
{

/**< Note that Audio Policy makes use of HAL_API_REV_2_0
 * Do not use HAL_API_REV_1_0
 * For input devices, not only a direction bit has been added but also the mapping is different
 * As input devices is a bitfield, provides here a macro to remove the direction bit
 */
#define MASK_DEVICE_NO_DIR    ((unsigned int)~AUDIO_DEVICE_BIT_IN)
#define REMOVE_DEVICE_DIR(device) ((unsigned int)device & MASK_DEVICE_NO_DIR)


//  Is voice volume applied after mixing while in mode IN_COMM ?
const String8 AudioPolicyManagerALSA::mVoiceVolumeAppliedAfterMixInCommPropName(
    "voice_volume_applied_after_mixing_in_communication");
// Is voice volume applied after mixing while in mode IN_CALL ?
const String8 AudioPolicyManagerALSA::mVoiceVolumeAppliedAfterMixInCallPropName(
    "voice_volume_applied_after_mixing_in_call");
// Music attenuation in dB while in call (csv or voip)
const String8 AudioPolicyManagerALSA::mInCallMusicAttenuation_dBPropName(
    "in_call_music_attenuation_dB");

// ----------------------------------------------------------------------------
// AudioPolicyManagerALSA
// ----------------------------------------------------------------------------

// ---  class factory

extern "C" AudioPolicyInterface *createAudioPolicyManager(
    AudioPolicyClientInterface *clientInterface)
{
    return new AudioPolicyManagerALSA(clientInterface);
}

extern "C" void destroyAudioPolicyManager(AudioPolicyInterface *interface)
{
    delete interface;
}

status_t AudioPolicyManagerALSA::setDeviceConnectionState(AudioSystem::audio_devices device,
                                                          AudioSystem::device_connection_state state,
                                                          const char *device_address)
{
    // Connect or disconnect only 1 device at a time
    if (AudioSystem::popCount(device) != 1) {
        return BAD_VALUE;
    }

    if (device_address == NULL) {
        ALOGE("setDeviceConnectionState() NULL address");
        return BAD_VALUE;
    }

    if (strlen(device_address) >= MAX_DEVICE_ADDRESS_LEN) {
        ALOGE("setDeviceConnectionState() invalid address: %s", device_address);
        return BAD_VALUE;
    }

    if (AudioSystem::isOutputDevice(device) && state == AudioSystem::DEVICE_STATE_AVAILABLE) {
        if (mAvailableOutputDevices & device) {
            ALOGW("setDeviceConnectionState() device already connected: %x", device);
            return INVALID_OPERATION;
        }

        ALOGV("setDeviceConnectionState() connecting device %x", device);

        // Clear wired headset/headphone device, if any already available, as only
        // one wired headset/headphone device can be connected at a time
        if (device &
            (AudioSystem::DEVICE_OUT_WIRED_HEADSET | AudioSystem::DEVICE_OUT_WIRED_HEADPHONE)) {
            mAvailableOutputDevices =
                (audio_devices_t)(mAvailableOutputDevices &
                                  ~(AudioSystem::DEVICE_OUT_WIRED_HEADSET |
                                    AudioSystem::DEVICE_OUT_WIRED_HEADPHONE));
        }
    }

    return AudioPolicyManagerBase::setDeviceConnectionState(device, state, device_address);
}


status_t AudioPolicyManagerALSA::startOutput(audio_io_handle_t output,
                                             AudioSystem::stream_type stream,
                                             int session)
{
    ALOGD("startOutput() output %d, stream type %d, session %d", output, stream, session);

    return AudioPolicyManagerBase::startOutput(output, stream, session);
}

audio_io_handle_t AudioPolicyManagerALSA::getInput(int inputSource,
                                                   uint32_t samplingRate,
                                                   uint32_t format,
                                                   uint32_t channelMask,
                                                   AudioSystem::audio_in_acoustics acoustics)
{
    audio_devices_t device = getDeviceForInputSource(inputSource);

    audio_io_handle_t activeInput = getActiveInput();
    audio_io_handle_t input = 0;

    // Stop the current active input to enable requested input start in the following cases :
    // - An application already uses an input and the requested input is from a VoIP call or a CSV
    // call record
    // - An application requests an input for a source which has already been previously requested.
    // Since the policy doesn't have enough elements to legislate this use case, the input shall
    // be given for the latest request caused by user's action.
    // Log the remaining usecases for info purpose.
    if (!mInputs.isEmpty() && activeInput) {
        AudioInputDescriptor *inputDesc = mInputs.valueFor(activeInput);

        uint32_t deviceMediaRecMic = (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC |
                                      AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET |
                                      AUDIO_DEVICE_IN_WIRED_HEADSET);

        // If an application uses already an input and the requested input is from a VoIP call
        // or a CSV call record, stop the current active input to enable requested input start.
        if (((REMOVE_DEVICE_DIR(inputDesc->mDevice) & deviceMediaRecMic) &&
             (inputDesc->mInputSource != AUDIO_SOURCE_VOICE_COMMUNICATION)) &&
            ((REMOVE_DEVICE_DIR(device) & AUDIO_DEVICE_IN_VOICE_CALL) ||
             (inputSource == AUDIO_SOURCE_VOICE_COMMUNICATION))) {
            LOGI("Stop current active input %d because of higher priority input %d !",
                 inputDesc->mInputSource, inputSource);
            stopInput(activeInput);
        } else if (inputSource == inputDesc->mInputSource) {
            LOGI("getInput() mPhoneState : %d, device 0x%x, two requests for the same source,"
                 "return the audio input handle to the latest requester!", mPhoneState, device);
            stopInput(activeInput);
        } else if ((AUDIO_SOURCE_CAMCORDER == inputDesc->mInputSource) &&
                   (AUDIO_SOURCE_MIC == inputSource)) {
            // Create a concurrent input and let upper layers close the active camcorder input
            LOGI("Grant request for input %d creation while current camcorder input", inputSource);
        } else if ((REMOVE_DEVICE_DIR(inputDesc->mDevice) & AUDIO_DEVICE_IN_VOICE_CALL) &&
                   (inputSource == AUDIO_SOURCE_VOICE_COMMUNICATION)) {
            LOGI("Incoming VoIP call during VCR or VCR -> VoIP swap");
        } else if ((inputDesc->mInputSource == AUDIO_SOURCE_VOICE_COMMUNICATION) &&
                   (inputSource == AUDIO_SOURCE_VOICE_RECOGNITION)) {
            LOGI("Voice recognition requested during VoIP call");
        } else if ((inputDesc->mInputSource == AUDIO_SOURCE_MIC) &&
                   (inputSource == AUDIO_SOURCE_VOICE_RECOGNITION)) {
            LOGI("Voice recognition requested while current MIC input source");
        }
        // Force use of built-in mic in case of force use of the speaker in VoIP and wsHS connected
        else if ((inputSource == AUDIO_SOURCE_VOICE_COMMUNICATION) &&
                 (REMOVE_DEVICE_DIR(inputDesc->mDevice) & AUDIO_DEVICE_IN_WIRED_HEADSET) &&
                 (getForceUse(AudioSystem::FOR_COMMUNICATION) == AudioSystem::FORCE_SPEAKER)) {
            device = AUDIO_DEVICE_IN_BUILTIN_MIC;
            LOGI(
                "Changing input device to built-in mic as force use to speaker requested with wsHS connected");
        } else {
            LOGW("getInput() mPhoneState : %d, device 0x%x, unhandled input source concurrency,"
                 "return invalid audio input handle!", mPhoneState, device);
            return 0;
        }
    }

    ALOGV("getInput() inputSource %d, samplingRate %d, format %d, channelMask %x, acoustics %x",
          inputSource, samplingRate, format, channelMask, acoustics);

    if (device == 0) {
        ALOGW("getInput() could not find device for inputSource %d", inputSource);
        return 0;
    }

    IOProfile *profile = getInputProfile(device,
                                         samplingRate,
                                         format,
                                         channelMask);
    if (profile == NULL) {
        ALOGW("getInput() could not find profile for device %04x, samplingRate %d, format %d,"
              "channelMask %04x",
              device, samplingRate, format, channelMask);
        return 0;
    }

    if (profile->mModule->mHandle == 0) {
        ALOGE("getInput(): HW module %s not opened", profile->mModule->mName);
        return 0;
    }

    AudioInputDescriptor *inputDesc = new AudioInputDescriptor(profile);

    inputDesc->mInputSource = inputSource;
    inputDesc->mDevice = device;
    inputDesc->mSamplingRate = samplingRate;
    inputDesc->mFormat = (audio_format_t)format;
    inputDesc->mChannelMask = (audio_channel_mask_t)channelMask;
    inputDesc->mRefCount = 0;
    input = mpClientInterface->openInput(profile->mModule->mHandle,
                                         &inputDesc->mDevice,
                                         &inputDesc->mSamplingRate,
                                         &inputDesc->mFormat,
                                         &inputDesc->mChannelMask);

    // only accept input with the exact requested set of parameters
    if (input == 0 ||
        (samplingRate != inputDesc->mSamplingRate) ||
        (format != inputDesc->mFormat) ||
        (channelMask != inputDesc->mChannelMask)) {
        ALOGV("getInput() failed opening input: samplingRate %d, format %d, channelMask %d",
              samplingRate, format, channelMask);
        if (input != 0) {
            mpClientInterface->closeInput(input);
        }
        delete inputDesc;
        return 0;
    }
    mInputs.add(input, inputDesc);

    // Do not call base class method as channel mask information
    // for voice call record is not used on our platform
    return input;
}

status_t AudioPolicyManagerALSA::startInput(audio_io_handle_t input)
{
    ALOGV("startInput() input %d", input);
    ssize_t index = mInputs.indexOfKey(input);
    if (index < 0) {
        ALOGW("startInput() unknow input %d", input);
        return BAD_VALUE;
    }
    AudioInputDescriptor *inputDesc = mInputs.valueAt(index);

#ifdef AUDIO_POLICY_TEST
    if (mTestInput == 0)
#endif // AUDIO_POLICY_TEST
    {
        // refuse 2 active AudioRecord clients at the same time
        // except in the case of incoming VOIP call during voice call recording
        audio_io_handle_t activeInput = getActiveInput();
        if (activeInput != 0) {
            AudioInputDescriptor *activeInputDesc = mInputs.valueFor(activeInput);
            if ((inputDesc->mInputSource & AUDIO_SOURCE_VOICE_COMMUNICATION) &&
                (REMOVE_DEVICE_DIR(activeInputDesc->mDevice) & AUDIO_DEVICE_IN_VOICE_CALL) &&
                ((activeInputDesc->mInputSource == AUDIO_SOURCE_VOICE_UPLINK) ||
                 (activeInputDesc->mInputSource == AUDIO_SOURCE_VOICE_DOWNLINK) ||
                 (activeInputDesc->mInputSource == AUDIO_SOURCE_VOICE_CALL))) {
                ALOGI("startInput() input %d: VCR input %d already started", input, activeInput);
            } else {
                ALOGI("startInput() input %d failed: other input already started", input);
                return INVALID_OPERATION;
            }
        }
    }

    AudioParameter param = AudioParameter();
    param.addInt(String8(AudioParameter::keyRouting), (int)inputDesc->mDevice);

    param.addInt(String8(AudioParameter::keyInputSource), (int)inputDesc->mInputSource);
    ALOGV("startInput() input source = %d", inputDesc->mInputSource);

    mpClientInterface->setParameters(input, param.toString());

    inputDesc->mRefCount = 1;
    inputDesc->mHasStarted = true;
    return NO_ERROR;
}

status_t AudioPolicyManagerALSA::setStreamVolumeIndex(AudioSystem::stream_type stream,
                                                      int index,
                                                      audio_devices_t device)
{

    // check that stream is not negative to avoid out of bounds index
    if (!isStreamValid(stream)) {
        ALOGE("setStreamVolumeIndex() invalid stream of type %d,at volume index %d on device %#04x",
              stream, index, device);
        return BAD_VALUE;
    }

    if ((index < mStreams[stream].mIndexMin) || (index > mStreams[stream].mIndexMax)) {
        return BAD_VALUE;
    }
    if (!audio_is_output_device(device)) {
        return BAD_VALUE;
    }

    // In case of ENFORCE_AUDIBLE stream, force max volume if it cannot be muted
    if (stream == AudioSystem::ENFORCED_AUDIBLE) {
        if (!mStreams[stream].mCanBeMuted) {
            index = mStreams[stream].mIndexMax;
        }
    }

    ALOGV("setStreamVolumeIndex() stream %d, device %04x, index %d",
          stream, device, index);

    // if device is AUDIO_DEVICE_OUT_DEFAULT set default value and
    // clear all device specific values
    if (device == AUDIO_DEVICE_OUT_DEFAULT) {
        mStreams[stream].mIndexCur.clear();
    }
    mStreams[stream].mIndexCur.add(device, index);

    // compute and apply stream volume on all outputs according to connected device
    status_t status = applyVolumeOnStream(stream, device);

    if (status != OK) {
        return status;
    }

    // if the voice volume is set before the mix, slave the media volume to the voice volume
    if (isInCall() &&
        (stream == AudioSystem::VOICE_CALL) &&
        !isVoiceVolumeSetAfterMix()) {
        status = applyVolumeOnStream(AudioSystem::MUSIC, device);
    }

    return status;
}

status_t AudioPolicyManagerALSA::applyVolumeOnStream(AudioSystem::stream_type stream,
                                                     audio_devices_t device)
{

    // force volume setting at media server starting
    // (i.e. when device == AUDIO_DEVICE_OUT_DEFAULT)
    //
    // If the volume is not set at media server starting then it is applied at
    // next applyStreamVolumes() call, for example during media playback,
    // which can lead to performance issue.
    bool forceSetVolume = (device == AUDIO_DEVICE_OUT_DEFAULT);

    int index = mStreams[stream].getVolumeIndex(device);

    // compute and apply stream volume on all outputs according to connected device
    status_t status = NO_ERROR;
    for (size_t i = 0; i < mOutputs.size(); i++) {
        audio_devices_t curDevice =
            getDeviceForVolume((audio_devices_t)mOutputs.valueAt(i)->device());

        if ((device == curDevice) || forceSetVolume) {
            status_t volStatus = checkAndSetVolume(stream, index, mOutputs.keyAt(i), curDevice);
            if (volStatus != NO_ERROR) {
                status = volStatus;
            }
        }
    }
    return status;
}

bool AudioPolicyManagerALSA::isVoiceVolumeSetAfterMix() const
{
    AUDIOCOMMS_ASSERT(mPhoneState == AudioSystem::MODE_IN_CALL ||
                      mPhoneState == AudioSystem::MODE_IN_COMMUNICATION, "Not is CSV or VoIP call");

    // Current method is semantically const and actually does not modify the object's
    // contents as getDeviceForStrategy is called with (fromCache = true)
    audio_devices_t voiceCallDevice =
        const_cast<AudioPolicyManagerALSA *>(this)->getDeviceForStrategy(STRATEGY_PHONE, true);

    if (voiceCallDevice & AUDIO_DEVICE_OUT_ALL_SCO) {
        // Bluetooth devices apply volume at last stage, which means after mix
        return true;
    }

    return mPhoneState == AudioSystem::MODE_IN_CALL ?
           mVoiceVolumeAppliedAfterMixInCall : mVoiceVolumeAppliedAfterMixInComm;
}

float AudioPolicyManagerALSA::getRawVolumeLevel(AudioSystem::stream_type stream,
                                                audio_devices_t device) const
{
    return static_cast<float>(mStreams[stream].getVolumeIndex(device) - mStreams[stream].mIndexMin)
           / (mStreams[stream].mIndexMax - mStreams[stream].mIndexMin);
}

float AudioPolicyManagerALSA::computeVolume(int stream,
                                            int index,
                                            audio_devices_t device)
{
    // Force volume to max for bluetooth SCO as volume is managed by the headset
    if (stream == AudioSystem::BLUETOOTH_SCO) {
        return _maxVolume;
    }

    // Process in call VOICE_CALL/ALARM/DTMF/SYSTEM volume
    // Do not update SYSTEM stream volume when its index is already at min value
    // Note: the voice volume will not be affected by this change
    // because the computeVolume() method is only used for AudioFlinger volumes
    if (isInCall() &&
        (stream == AudioSystem::VOICE_CALL ||
         stream == AudioSystem::DTMF ||
         (stream == AudioSystem::SYSTEM && index != mStreams[stream].mIndexMin))) {

        if (isVoiceVolumeSetAfterMix()) {

            // Set the VOICE_CALL/DTMF/SYSTEM volume to 1.0 to avoid double attenuation
            // when the voice volume will be applied after mix VOICE_CALL/DTMF/SYSTEM and voice
            return _maxVolume;
        }
    }

    // Compute SW attenuation
    float volume = AudioPolicyManagerBase::computeVolume(stream, index, device);

    if (isInCall() && stream == AudioSystem::MUSIC) {

        // Slave media volume to the voice volume when this last one is applied before
        // mixing both streams and when both streams are output to the same device

        audio_devices_t voiceCallDevice = getDeviceForStrategy(STRATEGY_PHONE, true);

        if (!isVoiceVolumeSetAfterMix() && voiceCallDevice == device) {
            volume *= getRawVolumeLevel(AudioSystem::VOICE_CALL, voiceCallDevice);
        }

        // Perform fixed background media volume attenuation
        volume *= pow(_base10, -mInCallMusicAttenuation_dB / _base10);
    }

    return volume;
}

audio_devices_t AudioPolicyManagerALSA::getDeviceForStrategy(routing_strategy strategy,
                                                             bool fromCache)
{
    uint32_t device = 0;
    uint32_t currentDevice = 0;

    device = AudioPolicyManagerBase::getDeviceForStrategy(strategy, fromCache);
    AudioOutputDescriptor *hwOutputDesc = mOutputs.valueFor(mPrimaryOutput);
    AUDIOCOMMS_ASSERT(hwOutputDesc != NULL,
                      "Invalid primary audio output device or no audio output device found");
    currentDevice = static_cast<uint32_t>(hwOutputDesc->device());

    switch (strategy) {
    case STRATEGY_PHONE:
        // in voice call, the ouput device can not be DGTL_DOCK_HEADSET, AUX_DIGITAL (i.e. HDMI) or  ANLG_DOCK_HEADSET
        if ((device == AudioSystem::DEVICE_OUT_AUX_DIGITAL) ||
            (device == AudioSystem::DEVICE_OUT_ANLG_DOCK_HEADSET) ||
            (device == AudioSystem::DEVICE_OUT_DGTL_DOCK_HEADSET) ||
            (device == AudioSystem::DEVICE_OUT_WIDI) ||
            (device == AUDIO_DEVICE_OUT_REMOTE_SUBMIX) ||
            (device == AUDIO_DEVICE_OUT_USB_ACCESSORY) ||
            (device == AUDIO_DEVICE_OUT_USB_DEVICE)) {
            uint32_t forceUseInComm =  getForceUse(AudioSystem::FOR_COMMUNICATION);
            switch (forceUseInComm) {

            case AudioSystem::FORCE_SPEAKER:
                device = mAvailableOutputDevices & AudioSystem::DEVICE_OUT_SPEAKER;
                if (device != 0) {
                    ALOGD("%s- Unsupported device in STRATEGY_PHONE: set Speaker as ouput",
                          __FUNCTION__);
                } else {
                    LOGE("%s- Earpiece device not found", __FUNCTION__);
                }
                break;

            default:
                device = mAvailableOutputDevices & AudioSystem::DEVICE_OUT_EARPIECE;
                if (device != 0) {
                    ALOGD("%s- Unsupported device in STRATEGY_PHONE: set Earpiece as ouput",
                          __FUNCTION__);
                } else {
                    LOGE("%s- Earpiece device not found: set speaker as output", __FUNCTION__);
                    device = mAvailableOutputDevices & AudioSystem::DEVICE_OUT_SPEAKER;
                }
                break;
            }
        }
        break;
    case STRATEGY_ENFORCED_AUDIBLE:
        // During call, if an enforced audible stream is played, output it on the
        // speaker + current device used for speech
        if (isInCall() &&
            !mStreams[AUDIO_STREAM_ENFORCED_AUDIBLE].mCanBeMuted &&
            ((getForceUse(AudioSystem::FOR_COMMUNICATION) == AudioSystem::FORCE_BT_SCO) ||
             (currentDevice == AudioSystem::DEVICE_OUT_EARPIECE))) {
            // Set 2 devices for ALSA module : so the mode will be set as normal
            //  - the BT SCO will be unrouted for instance during a shutter sound
            //  during CSV call.
            //  - the earpiece is not handled by the base class on MEDIA fall through case.
            ALOGV(
                "%s- Current device(0x%x) and speaker stream output for an enforced audible stream",
                __FUNCTION__,
                currentDevice);
            device = (mAvailableOutputDevices &AudioSystem::DEVICE_OUT_SPEAKER) | currentDevice;
        }
        break;
    case STRATEGY_SONIFICATION_LOCAL:
        // Check if the phone state has already passed from MODE_RINGTONE to
        //  MODE_IN_COMMUNICATION or IN_CALL. In this case the output device is Earpiece and not IHF.
        if ((device == AudioSystem::DEVICE_OUT_SPEAKER)
            && (isInCall())
            && (getForceUse(AudioSystem::FOR_COMMUNICATION) != AudioSystem::FORCE_SPEAKER)) {
            ALOGV("%s- Force the device to earpiece and not IHF", __FUNCTION__);
            device = mAvailableOutputDevices & AudioSystem::DEVICE_OUT_EARPIECE;
        }
        break;

    // for all remaining strategies, follow behavior defined by AudioPolicyManagerBase
    case STRATEGY_MEDIA:
    case STRATEGY_SONIFICATION:
    case STRATEGY_SONIFICATION_RESPECTFUL:
    case STRATEGY_DTMF:
    default:
        // do nothing
        break;
    }

    LOGV("getDeviceForStrategy() strategy %d, device 0x%x", strategy, device);

    return (audio_devices_t)device;
}

void AudioPolicyManagerALSA::releaseInput(audio_io_handle_t input)
{
    AudioPolicyManagerBase::releaseInput(input);
    int mInputsSize = mInputs.size();
    ALOGD("%s(input %d) mInputs.size() = %d", __FUNCTION__, input, mInputsSize);

    // Reroute the input only when these conditions are met:
    // - there are still registered inputs
    // - there is no currently active input
    // - there is a previously stopped input
    // In this case the input source will be given back to the latest stopped input
    if ((mInputsSize > 0) && (getActiveInput() == 0)) {
        int iRemainingInputs = mInputsSize;
        bool bValidInput = false;
        while (!bValidInput && iRemainingInputs > 0) {
            AudioInputDescriptor *inputDesc = mInputs.valueAt(iRemainingInputs - 1);
            if (inputDesc->mHasStarted) {
                inputDesc->mRefCount = 1;
                AudioParameter param;
                param.addInt(String8(AudioParameter::keyRouting), (int)inputDesc->mDevice);
                param.addInt(String8(AudioParameter::keyInputSource), (int)inputDesc->mInputSource);
                mpClientInterface->setParameters(mInputs.keyAt(
                                                     iRemainingInputs - 1), param.toString());
                bValidInput = true;
            }
            iRemainingInputs--;
        }
    }
}

AudioPolicyManagerALSA::AudioPolicyManagerALSA(AudioPolicyClientInterface *clientInterface)
    : AudioPolicyManagerBase(clientInterface),
      mVoiceVolumeAppliedAfterMixInComm(false),
      mVoiceVolumeAppliedAfterMixInCall(false),
      mInCallMusicAttenuation_dB(0.0)
{
    /// Load custom properties from audio_policy.conf
    // mVoiceVolumeAppliedAfterMixInComm
    if (getCustomPropertyAsBool(mVoiceVolumeAppliedAfterMixInCommPropName,
                                mVoiceVolumeAppliedAfterMixInComm)) {
        ALOGD("%s- mVoiceVolumeAppliedAfterMixInComm = %d",
              __FUNCTION__,
              mVoiceVolumeAppliedAfterMixInComm);
    }
    // mVoiceVolumeAppliedAfterMixInCall
    if (getCustomPropertyAsBool(mVoiceVolumeAppliedAfterMixInCallPropName,
                                mVoiceVolumeAppliedAfterMixInCall)) {
        ALOGD("%s- mVoiceVolumeAppliedAfterMixInCall = %d",
              __FUNCTION__,
              mVoiceVolumeAppliedAfterMixInCall);
    }
    // mInCallMusicAttenuation_dB
    if (getCustomPropertyAsFloat(mInCallMusicAttenuation_dBPropName, mInCallMusicAttenuation_dB)) {
        ALOGD("%s- mInCallMusicAttenuation_dB = %.0f", __FUNCTION__, mInCallMusicAttenuation_dB);
    }
}

AudioPolicyManagerALSA::~AudioPolicyManagerALSA()
{
}

}  // namespace android
