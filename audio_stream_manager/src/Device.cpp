/*
 * Copyright (C) 2013-2016 Intel Corporation
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
#define LOG_TAG "AudioIntelHal"

#include "Device.hpp"
#include "AudioConversion.hpp"
#include "StreamIn.hpp"
#include "StreamOut.hpp"
#include "CompressedStreamOut.hpp"
#include <AudioCommsAssert.hpp>
#include <hardware/audio.h>
#include <Parameters.hpp>
#include <RouteManagerInstance.hpp>
#include <hardware/audio_effect.h>
#include <utilities/Log.hpp>
#include <string>

/**
 * Introduce a primary flag for input as well to manage route applicability for stream symetrically.
 */
static const audio_input_flags_t AUDIO_INPUT_FLAG_PRIMARY = static_cast<audio_input_flags_t>(0x10);

using namespace std;
using android::status_t;
using audio_comms::utilities::Log;
using audio_comms::utilities::Mutex;

namespace intel_audio
{

Device::Device()
    : mEchoReference(NULL),
      mStreamInterface(NULL),
      mPrimaryOutput(NULL)
{
    // Retrieve the Stream Interface
    mStreamInterface = RouteManagerInstance::getStreamInterface();
    if (mStreamInterface == NULL) {
        Log::Error() << "Failed to get Stream Interface on RouteMgr";
        return;
    }
    /// Start Routing service
    if (mStreamInterface->startService() != android::OK) {
        Log::Error() << __FUNCTION__ << ": Could not start Route Manager stream service";
        // Reset interface pointer to give a chance for initCheck to catch any issue
        // with the RouteMgr.
        mStreamInterface = NULL;
        return;
    }
    mStreamInterface->reconsiderRouting(true);

    Log::Debug() << __FUNCTION__ << ": Route Manager Service successfully started";
}

Device::~Device()
{
    mStreamInterface->stopService();
}

status_t Device::initCheck() const
{
    return mStreamInterface ? android::OK : android::NO_INIT;
}

status_t Device::setVoiceVolume(float volume)
{
    if (mMode == AUDIO_MODE_IN_COMMUNICATION) {
        Log::Debug() << __FUNCTION__
                     << ": Mode in COMMUNICATION: set HW voice volume to Max instead of: "
                     << volume;
        volume = 1.0;
    }
    return mStreamInterface->setVoiceVolume(volume);
}

android::status_t Device::openOutputStream(audio_io_handle_t handle,
                                           audio_devices_t devices,
                                           audio_output_flags_t flags,
                                           audio_config_t &config,
                                           StreamOutInterface * &stream,
                                           const std::string & address)
{
    Log::Debug() << __FUNCTION__ << ": handle=" << handle << ", flags=" << std::hex
                 << static_cast<uint32_t>(flags) << ", devices: 0x" << devices;

    if (!audio_is_output_devices(devices)) {
        Log::Error() << __FUNCTION__ << ": called with bad devices";
        return android::BAD_VALUE;
    }
    // If no flags is provided for output, use primary flag by default
    flags = (flags == AUDIO_OUTPUT_FLAG_NONE) ? AUDIO_OUTPUT_FLAG_PRIMARY : flags;

    if (flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
        CompressedStreamOut *out = new CompressedStreamOut(this, handle, flags, devices, address);
        status_t err = out->set(config);
        if (err != android::OK) {
            delete out;
            return err;
        }
        stream = out;
        mStreams[handle] = out;
        return android::OK;
    }
    StreamOut *out = new StreamOut(this, handle, flags, devices, address);
    status_t err = out->set(config);
    if (err != android::OK) {
        Log::Error() << __FUNCTION__ << ": set error.";
        delete out;
        return err;
    }
    if (mStreams.find(handle) != mStreams.end()) {
        Log::Error() << __FUNCTION__ << ": stream already added";
        delete out;
        return android::BAD_VALUE;
    }
    mStreams[handle] = out;

    if (mPrimaryOutput == NULL && hasPrimaryFlags(*out)) {
        mPrimaryOutput = out;
    }

    // Informs the route manager of stream creation
    mStreamInterface->addStream(*out);
    stream = out;

    Log::Debug() << __FUNCTION__ << ": output created with status=" << err;
    return android::OK;
}

void Device::closeOutputStream(StreamOutInterface *out)
{
    if (!out) {
        Log::Error() << __FUNCTION__ << ": invalid stream handle";
        return;
    }
    // Informs the route manager of stream destruction
    mStreamInterface->removeStream(static_cast<StreamOut &>(*out));
    audio_io_handle_t handle = static_cast<StreamOut *>(out)->getIoHandle();
    if (mStreams.find(handle) == mStreams.end()) {
        Log::Error() << __FUNCTION__ << ": requesting to deleted an output stream with io handle= "
                     << handle << " not tracked by Primary HAL";
    } else {
        if (isPrimaryOutput(*mStreams[handle])) {
            mPrimaryOutput = NULL;
        }
        mStreams.erase(handle);
    }
    delete out;
}

android::status_t Device::openInputStream(audio_io_handle_t handle,
                                          audio_devices_t devices,
                                          audio_config_t &config,
                                          StreamInInterface * &stream,
                                          audio_input_flags_t flags,
                                          const std::string & address,
                                          audio_source_t source)
{
    Log::Debug() << __FUNCTION__ << ": handle=" << handle << ", devices: 0x" << std::hex << devices
                 << ", input source: 0x" << static_cast<uint32_t>(source)
                 << ", input flags: 0x" << static_cast<uint32_t>(flags);
    if (!audio_is_input_device(devices)) {
        Log::Error() << __FUNCTION__ << ": called with bad device " << devices;
        return android::BAD_VALUE;
    }
    // If no flags is provided for input, use primary by default
    flags = (flags == AUDIO_INPUT_FLAG_NONE) ? AUDIO_INPUT_FLAG_PRIMARY : flags;

    StreamIn *in = new StreamIn(this, handle, flags, source, devices, address);
    status_t err = in->set(config);
    if (err != android::OK) {
        Log::Error() << __FUNCTION__ << ": Set err";
        delete in;
        return err;
    }
    if (mStreams.find(handle) != mStreams.end()) {
        Log::Error() << __FUNCTION__ << ": stream already added";
        delete in;
        return android::BAD_VALUE;
    }
    mStreams[handle] = in;

    // Informs the route manager of stream creation
    mStreamInterface->addStream(*in);
    stream = in;

    Log::Debug() << __FUNCTION__ << ": input created with status=" << err;
    return android::OK;
}

void Device::closeInputStream(StreamInInterface *in)
{
    if (!in) {
        Log::Error() << __FUNCTION__ << ": invalid stream handle";
        return;
    }
    // Informs the route manager of stream destruction
    mStreamInterface->removeStream(static_cast<StreamIn &>(*in));
    audio_io_handle_t handle = static_cast<StreamIn *>(in)->getIoHandle();
    if (mStreams.find(handle) == mStreams.end()) {
        Log::Error() << __FUNCTION__ << ": requesting to deleted an input stream with io handle= "
                     << handle << " not tracked by Primary HAL";
    } else {
        mStreams.erase(handle);
    }
    delete in;
}

status_t Device::setMicMute(bool mute)
{
    Log::Verbose() << __FUNCTION__ << ": " << (mute ? "true" : "false");
    KeyValuePairs pair;
    status_t status = pair.add(Parameters::gKeyMicMute, mute);
    if (status != android::OK) {
        return status;
    }
    return mStreamInterface->setParameters(pair.toString());
}

status_t Device::getMicMute(bool &muted) const
{
    KeyValuePairs pair(mStreamInterface->getParameters(Parameters::gKeyMicMute));
    return pair.get(Parameters::gKeyMicMute, muted);
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
        Log::Warning() << __FUNCTION__ << ": bad sampling rate: " << config.sample_rate;
        return 0;
    }
    if (config.format != AUDIO_FORMAT_PCM_16_BIT) {
        Log::Warning() << __FUNCTION__ << ": bad format: " << static_cast<int32_t>(config.format);
        return 0;
    }
    uint32_t channelCount = popcount(config.channel_mask);

    // multichannel capture is currently supported only for submix
    if ((channelCount < 1) || (channelCount > 8)) {
        Log::Warning() << __FUNCTION__ << ": bad channel count: " << channelCount;
        return 0;
    }
    SampleSpec spec(channelCount, config.format, config.sample_rate);
    return spec.convertFramesToBytes(spec.convertUsecToframes(mRecordingBufferTimeUsec));
}

status_t Device::setParameters(const string &keyValuePairs)
{
    Log::Verbose() << __FUNCTION__ << ": key value pair " << keyValuePairs;
    KeyValuePairs pairs(keyValuePairs);
    status_t status = mStreamInterface->setParameters(pairs.toString());
    return status;
}

string Device::getParameters(const string &keys) const
{
    Log::Verbose() << __FUNCTION__ << ": requested keys " << keys;
    return mStreamInterface->getParameters(keys);
}

android::status_t Device::setMode(audio_mode_t mode)
{
    mMode = mode;
    return android::OK;
}

void Device::resetEchoReference(struct echo_reference_itfe *reference)
{
    Log::Debug() << __FUNCTION__ << ": (reference=" << reference << ")";
    // Check that the reset is possible:
    //  - reference and _echoReference shall both point to the same struct (consistency check)
    //  - they should not be NULL because the reset process will remove the reference from
    //    the output voice stream and then free the local echo reference.
    if ((reference == NULL) || (mEchoReference == NULL) || (mEchoReference != reference)) {

        /* Nothing to do */
        return;
    }

    // Get active voice output stream
    IoStream *stream = getStreamInterface().getVoiceOutputStream();
    if (stream == NULL) {
        Log::Error() << __FUNCTION__
                     << ": no voice output found"
                     << " so problem to provide data reference for AEC effect!";
        return;
    }
    StreamOut *out = static_cast<StreamOut *>(stream);
    out->removeEchoReference(mEchoReference);
    release_echo_reference(mEchoReference);
    mEchoReference = NULL;
}

struct echo_reference_itfe *Device::getEchoReference(const SampleSpec &inputSampleSpec)
{
    Log::Debug() << __FUNCTION__;
    resetEchoReference(mEchoReference);

    // Get active voice output stream
    IoStream *stream = getStreamInterface().getVoiceOutputStream();
    if (stream == NULL) {
        Log::Error() << __FUNCTION__ << ": no voice output found,"
                     << " so problem to provide data reference for AEC effect!";
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
        Log::Error() << __FUNCTION__ << ": Could not create echo reference";
        return NULL;
    }
    out->addEchoReference(mEchoReference);
    Log::Debug() << __FUNCTION__ << ": return that mEchoReference=" << mEchoReference << ")";
    return mEchoReference;
}

void Device::printPlatformFwErrorInfo()
{
    mStreamInterface->printPlatformFwErrorInfo();
}

bool Device::hasStream(const audio_io_handle_t &streamHandle)
{
    return mStreams.find(streamHandle) != mStreams.end();
}

bool Device::hasPort(const audio_port_handle_t &portHandle)
{
    return mPorts.find(portHandle) != mPorts.end();
}

bool Device::hasPatchUnsafe(const audio_patch_handle_t &patchHandle) const
{
    return mPatches.find(patchHandle) != mPatches.end();
}

bool Device::getStream(const audio_io_handle_t &streamHandle, Stream * &stream)
{
    if (!hasStream(streamHandle) || mStreams[streamHandle] == NULL) {
        return false;
    }
    stream = mStreams[streamHandle];
    return true;
}

Port &Device::getPort(const audio_port_handle_t &portHandle)
{
    AUDIOCOMMS_ASSERT(hasPort(portHandle), "Port not found within collection");
    return mPorts[portHandle];
}

const Patch &Device::getPatchUnsafe(const audio_patch_handle_t &patchHandle) const
{
    PatchCollection::const_iterator it = mPatches.find(patchHandle);
    AUDIOCOMMS_ASSERT(it != mPatches.end(), "Patch not found within collection");
    return it->second;
}

Patch &Device::getPatchUnsafe(const audio_patch_handle_t &patchHandle)
{
    AUDIOCOMMS_ASSERT(hasPatchUnsafe(patchHandle), "Patch not found within collection");
    return mPatches[patchHandle];
}

Port &Device::getPortFromHandle(const audio_port_handle_t &portHandle)
{
    return mPorts[portHandle];
}

void Device::onPortAttached(const audio_patch_handle_t &patchHandle,
                            const audio_port_handle_t &portHandle)
{
    Port &portToAttach = getPort(portHandle);
    if (portToAttach.getType() != AUDIO_PORT_TYPE_MIX) {
        // Nothing to be done on streams by the Device for DEVICE ports
        return;
    }
    audio_io_handle_t streamHandle = portToAttach.getMixIoHandle();

    Stream *stream = NULL;
    if (!getStream(streamHandle, stream)) {
        Log::Error() << __FUNCTION__ << ": Mix Port IO handle does not match any stream).";
        return;
    }
    audio_patch_handle_t streamPreviousPatchHandle = stream->getPatchHandle();
    if (streamPreviousPatchHandle != AUDIO_PATCH_HANDLE_NONE &&
        streamPreviousPatchHandle != patchHandle) {
        Log::Warning() << __FUNCTION__ << ": setting new patch for already attached mix";
        mPatches.erase(streamPreviousPatchHandle);
    }
    stream->setPatchHandle(patchHandle);

    if (portToAttach.getRole() == AUDIO_PORT_ROLE_SINK) {
        StreamIn *inputStream = static_cast<StreamIn *>(stream);
        inputStream->setInputSource(portToAttach.getMixUseCaseSource());
    }
}

void Device::onPortReleased(const audio_patch_handle_t &patchHandle,
                            const audio_port_handle_t &portHandle)
{
    Port &portToRelease = getPort(portHandle);
    if (portToRelease.getType() != AUDIO_PORT_TYPE_MIX) {
        // Nothing to be done on streams by the Device for DEVICE ports
        return;
    }
    audio_io_handle_t streamHandle = portToRelease.getMixIoHandle();

    // Delete the Mix port if not used any more by any patch.
    if (!portToRelease.isUsed()) {
        mPorts.erase(portHandle);
    }

    Stream *stream = NULL;
    if (!getStream(streamHandle, stream)) {
        Log::Error() << __FUNCTION__ << ": Mix Port IO handle does not match any stream).";
        return;
    }
    AUDIOCOMMS_ASSERT(stream->getPatchHandle() == patchHandle,
                      "Mismatch between stream and patch handle");
    stream->setPatchHandle(AUDIO_PATCH_HANDLE_NONE); // Do not reset the device
}

status_t Device::createAudioPatch(size_t sourcesCount,
                                  const struct audio_port_config sources[],
                                  size_t sinksCount,
                                  const struct audio_port_config sinks[],
                                  audio_patch_handle_t &handle)
{
    handle = Patch::nextUniqueHandle();
    mPatchCollectionLock.lock();

    std::pair<PatchCollection::iterator, bool> ret;
    ret = mPatches.insert(std::pair<audio_patch_handle_t, Patch>(handle, Patch(handle, this)));

    Patch &patch = ret.first->second;
    patch.addPorts(sourcesCount, sources, sinksCount, sinks);
    mPatchCollectionLock.unlock();

    updateParametersSync(patch.hasDevice(AUDIO_PORT_ROLE_SOURCE),
                         patch.hasDevice(AUDIO_PORT_ROLE_SINK),
                         handle);
    // Patch has been created, even if updateParameters failed on one or more parameters, need to
    // return OK to AudioFlinger, unless this patch will not be considered as created and will
    // never be deleted (orphans patch within Audio HAL)
    return android::OK;
}

status_t Device::releaseAudioPatch(audio_patch_handle_t handle)
{
    mPatchCollectionLock.lock();
    if (!hasPatchUnsafe(handle)) {
        Log::Error() << __FUNCTION__ << " Patch handle " << handle
                     << " not found within Primary HAL";
        mPatchCollectionLock.unlock();
        return android::BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": releasing patch handle:" << handle;
    Patch &patch = getPatchUnsafe(handle);
    bool involvedSourceDevices = patch.hasDevice(AUDIO_PORT_ROLE_SOURCE);
    bool involvedSinkDevices = patch.hasDevice(AUDIO_PORT_ROLE_SINK);
    mPatches.erase(handle);
    mPatchCollectionLock.unlock();

    updateParametersSync(involvedSourceDevices, involvedSinkDevices);
    // Patch has been deleted, even if updateParameters failed on one or more parameters, need to
    // return OK to AudioFlinger, unless this patch will not be considered as deleted.
    return android::OK;
}

status_t Device::updateParameters(bool updateSourceDevice, bool updateSinkDevice,
                                  audio_patch_handle_t lastPatch, bool synchronous)
{
    KeyValuePairs pairs;
    // Update now the routing, i.e. the devices in input and/or output
    if (updateSourceDevice) {
        // Source Port update requested: it may impact input streams parameters
        prepareStreamsParameters(AUDIO_PORT_ROLE_SINK, pairs);
    }
    if (updateSinkDevice) {
        // Sink Port update requested: it may impact output streams parameters
        prepareStreamsParameters(AUDIO_PORT_ROLE_SOURCE, pairs, lastPatch);
    }
    if (pairs.toString().empty()) {
        return android::OK;
    }
    Log::Verbose() << __FUNCTION__ << ": Parameters:" << pairs.toString();
    return mStreamInterface->setParameters(pairs.toString(), synchronous);
}

status_t Device::getAudioPort(struct audio_port & /*port*/) const
{
    Log::Warning() << __FUNCTION__ << ": no implementation provided yet";
    return android::INVALID_OPERATION;
}

status_t Device::setAudioPortConfig(const struct audio_port_config & /*config*/)
{
    Log::Warning() << __FUNCTION__ << ": no implementation provided yet";
    return android::INVALID_OPERATION;
}

uint32_t Device::selectOutputDevices(uint32_t streamDeviceMask)
{
    uint32_t selectedDeviceMask = streamDeviceMask;

    AUDIOCOMMS_ASSERT(mPrimaryOutput != NULL, "Primary HAL without primary output is impossible");
    if (isInCall()) {
        // Take the device of the primary output if in call
        selectedDeviceMask = mPrimaryOutput->getDevices();
    }

    return selectedDeviceMask;
}

void Device::prepareStreamsParameters(audio_port_role_t streamPortRole, KeyValuePairs &pairs,
                                      audio_patch_handle_t lastPatch)
{
    audio_devices_t deviceMask = AUDIO_DEVICE_NONE;
    audio_devices_t internalDeviceMask = AUDIO_DEVICE_NONE;
    uint32_t streamsFlagMask = 0;
    uint32_t streamsUseCaseMask = 0;
    uint32_t requestedEffectMask = 0;
    std::string deviceAddress{};

    bool forceDeviceFromLastPatch = (lastPatch != AUDIO_PATCH_HANDLE_NONE);
    // As Klockwork complains about potential dead leack, avoid using Locker helper here.
    mPatchCollectionLock.lock();

    // Loop on all patches that involve source mix ports (i.e. output streams)
    for (PatchCollection::const_iterator it = mPatches.begin(); it != mPatches.end(); ++it) {
        const Patch &patch = it->second;
        audio_port_role_t devicePortRole = getOppositeRole(streamPortRole);
        const Port *mixPort = patch.getMixPort(streamPortRole);

        if (!patch.hasDevice(devicePortRole)) {
            // This patch does not connect any devices for the requested role.
            continue;
        }
        if (patch.hasDevice(streamPortRole)) {
            // This patch is connecting 2 device ports to one another.
            internalDeviceMask |= patch.getDevices(devicePortRole);
        }
        if (mixPort == NULL) {
            // This patch does not involve a valid mix port in the requested role.";
            continue;
        }
        audio_io_handle_t streamHandle = mixPort->getMixIoHandle();
        Stream *stream = NULL;
        if (!getStream(streamHandle, stream)) {
            Log::Error() << __FUNCTION__ << ": Mix Port IO handle does not match any stream).";
            continue;
        }
        // Update device(s) info from patch to stream involved in this patch
        stream->setDevices(patch.getDevices(devicePortRole), patch.getDeviceAddress(devicePortRole));

        if (forceDeviceFromLastPatch && (lastPatch == patch.getHandle()) &&
            !(stream->getDevices() & AUDIO_DEVICE_OUT_AUX_DIGITAL)) {
            deviceMask |= stream->getDevices();
        }
        if (stream->isStarted() && stream->isRoutedByPolicy()) {
            if ((!stream->isMuted() && !forceDeviceFromLastPatch) ||
                deviceMask == AUDIO_DEVICE_NONE) {
                deviceMask |= stream->getDevices();
            }
            deviceAddress += (deviceAddress.empty() ? "" : "|") +
                    patch.getDeviceAddress(devicePortRole);
            streamsFlagMask |= stream->getFlagMask();
            streamsUseCaseMask |= stream->getUseCaseMask();
            requestedEffectMask |= stream->getEffectRequested();
        }
    }

    if (streamPortRole == AUDIO_PORT_ROLE_SOURCE) {
        deviceMask = selectOutputDevices(deviceMask);
        pairs.add(Parameters::gKeyAndroidMode, mode());
    } else {
        pairs.add<int>(Parameters::gKeyVoipBandType, getBandFromActiveInput());
        pairs.add(Parameters::gKeyPreProcRequested, requestedEffectMask);
    }
    pairs.add(Parameters::gKeyUseCases[getDirectionFromMix(streamPortRole)], streamsUseCaseMask);
    pairs.add(Parameters::gKeyDevices[getDirectionFromMix(streamPortRole)],
              deviceMask | internalDeviceMask);
    pairs.add(Parameters::gKeyDeviceAddresses[getDirectionFromMix(streamPortRole)], deviceAddress);
    pairs.add(Parameters::gKeyFlags[getDirectionFromMix(streamPortRole)], streamsFlagMask);
    mPatchCollectionLock.unlock();
}

CAudioBand::Type Device::getBandFromActiveInput() const
{
    const Stream *activeInput = NULL;
    for (StreamCollection::const_iterator it = mStreams.begin(); it != mStreams.end(); ++it) {
        const Stream *stream = it->second;
        if (!stream->isOut() && stream->isStarted() && stream->isRoutedByPolicy()) {
            AUDIOCOMMS_ASSERT(activeInput == NULL, "More than one input is active");
            activeInput = stream;
        }
    }
    return (activeInput != NULL &&
            activeInput->getSampleRate() == mVoiceStreamRateForNarrowBandProcessing) ?
           CAudioBand::ENarrow : CAudioBand::EWide;
}

bool Device::isPrimaryOutput(const Stream &stream) const
{
    return &stream == mPrimaryOutput;
}

bool Device::hasPrimaryFlags(const Stream &stream) const
{
    return stream.isOut() &&
           (stream.getFlagMask() & AUDIO_OUTPUT_FLAG_PRIMARY) ==
           AUDIO_OUTPUT_FLAG_PRIMARY;
}

} // namespace intel_audio
