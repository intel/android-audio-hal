/*
 * Copyright (C) 2014-2018 Intel Corporation
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

#include <DeviceInterface.hpp>
#include <StreamWrapper.hpp>
#include <AudioCommsAssert.hpp>
#include <AudioNonCopyable.hpp>
#include <string.h> /* for strdup */
#include <mutex>
#include <utilities/Log.hpp>

using audio_comms::utilities::Log;

static hw_device_t *primary_dev = NULL; /*primary device instance */
static int32_t ref_count = 0; /*ref count of primary device instance*/

static std::mutex mtx;
namespace intel_audio
{

template <class Device, int CDeviceApiVersion>
class DeviceWrapper : private audio_comms::utilities::NonCopyable
{
public:
    static int open(const hw_module_t *module, const char *name, hw_device_t **device);
    static int close(hw_device_t *device);

private:
    static int openOutputStream(struct audio_hw_device *dev,
                                audio_io_handle_t handle,
                                audio_devices_t devices,
                                audio_output_flags_t flags,
                                audio_config_t *config,
                                audio_stream_out_t **stream_out,
                                const char *address);

    static void closeOutputStream(struct audio_hw_device *dev,
                                  audio_stream_out_t *stream);

    static int openInputStream(struct audio_hw_device *dev,
                               audio_io_handle_t handle,
                               audio_devices_t devices,
                               audio_config_t *config,
                               audio_stream_in_t **stream_in,
                               audio_input_flags_t flags,
                               const char *address,
                               audio_source_t source);

    static void closeInputStream(struct audio_hw_device *dev,
                                 audio_stream_in_t *stream);
    static int initCheck(const struct audio_hw_device *dev);

    static int setVoiceVolume(struct audio_hw_device *dev, float volume);

    static int setMasterVolume(struct audio_hw_device *dev, float volume);

    static int getMasterVolume(struct audio_hw_device *dev, float *volume);

    static int setMasterMute(struct audio_hw_device *dev, bool mute);

    static int getMasterMute(struct audio_hw_device *dev, bool *muted);

    static int setMode(struct audio_hw_device *dev, audio_mode_t mode);

    static int setMicMute(struct audio_hw_device *dev, bool state);

    static int getMicMute(const struct audio_hw_device *dev, bool *state);

    static int setParameters(struct audio_hw_device *dev, const char *keyValuePairs);

    static char *getParameters(const struct audio_hw_device *dev, const char *keys);

    static size_t getInputBufferSize(const struct audio_hw_device *dev,
                                     const audio_config_t *config);

    static int dump(const struct audio_hw_device *dev, int fd);

    static int createAudioPatch(struct audio_hw_device *dev,
                                unsigned int num_sources,
                                const struct audio_port_config *sources,
                                unsigned int num_sinks,
                                const struct audio_port_config *sinks,
                                audio_patch_handle_t *handle);

    static int releaseAudioPatch(struct audio_hw_device *dev, audio_patch_handle_t handle);

    static int getAudioPort(struct audio_hw_device *dev, struct audio_port *port);

    static int setAudioPortConfig(struct audio_hw_device *dev,
                                  const struct audio_port_config *config);

private:
    typedef DeviceWrapper<Device, CDeviceApiVersion> WrappedDevice;

    struct Glue
    {
        audio_hw_device_t mCDevice;
        Device *mCppDevice;
        DeviceWrapper<Device, CDeviceApiVersion> *mWrapper;
    } mGlue;

    DeviceWrapper(Device *dev, const hw_module_t *module);
    ~DeviceWrapper();

    static Device &getCppDevice(const audio_hw_device *device);
};

/*
 * Next is just functions implementation
 */
template <class Device, int CDeviceApiVersion>
Device &DeviceWrapper<Device, CDeviceApiVersion>::getCppDevice(const audio_hw_device *device)
{
    const Glue *glue = reinterpret_cast<const Glue *>(device);
    AUDIOCOMMS_ASSERT(glue != NULL && glue->mCppDevice != NULL, "Invalid cpp device");
    return *glue->mCppDevice;
}

template <class Device, int CDeviceApiVersion>
DeviceWrapper<Device, CDeviceApiVersion>::DeviceWrapper(Device *dev, const hw_module_t *module)
{
    mGlue.mWrapper = this;
    mGlue.mCppDevice = dev;

    audio_hw_device_t &cDevice = mGlue.mCDevice;

    cDevice.common.tag = HARDWARE_DEVICE_TAG; /* header documentation tells that tag must be set to this value*/
    cDevice.common.version = CDeviceApiVersion;
    cDevice.common.module = const_cast<hw_module_t *>(module);
    cDevice.common.close = close;

    cDevice.init_check = initCheck;
    cDevice.set_voice_volume = setVoiceVolume;
    cDevice.set_master_volume = setMasterVolume;
    cDevice.get_master_volume = getMasterVolume;
    cDevice.set_master_mute = setMasterMute;
    cDevice.get_master_mute = getMasterMute;
    cDevice.set_mode = setMode;
    cDevice.set_mic_mute = setMicMute;
    cDevice.get_mic_mute = getMicMute;
    cDevice.set_parameters = setParameters;
    cDevice.get_parameters = getParameters;
    cDevice.get_input_buffer_size = getInputBufferSize;
    cDevice.open_output_stream = openOutputStream;
    cDevice.close_output_stream = closeOutputStream;
    cDevice.open_input_stream = openInputStream;
    cDevice.close_input_stream = closeInputStream;
    cDevice.audio_hw_device_t::dump = dump;
    cDevice.create_audio_patch = createAudioPatch;
    cDevice.release_audio_patch = releaseAudioPatch;
    cDevice.get_audio_port = getAudioPort;
    cDevice.set_audio_port_config = setAudioPortConfig;
}

template <class Device, int CDeviceApiVersion>
DeviceWrapper<Device, CDeviceApiVersion>::~DeviceWrapper()
{
    delete mGlue.mCppDevice;
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::open(const hw_module_t *module,
                                                   const char *name,
                                                   hw_device_t **device)
{

    if (device == NULL || strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) {
        return -EINVAL;
    }

    mtx.try_lock();
    if (primary_dev == NULL) {
        Device *obj = new Device();
        if (obj == NULL) {
            mtx.unlock();
            return -EIO;
        }

        WrappedDevice *wrappedDev = new DeviceWrapper<Device, CDeviceApiVersion>(obj, module);

        primary_dev = reinterpret_cast<hw_device_t *>(wrappedDev);
    }

    *device = primary_dev;
    ref_count++;
    mtx.unlock();
    return 0;
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::close(hw_device_t *device)
{
    mtx.try_lock();
    if (ref_count <= 0) { /*actually here it cannot <0*/
        mtx.unlock();
        Log::Error() << "The device has been closed, no need close again";
        return -EINVAL;
    }

    ref_count--;
    if (ref_count == 0) {
        const WrappedDevice *fakeThis = reinterpret_cast<const Glue *>(device)->mWrapper;
        if (fakeThis != NULL) {
            delete fakeThis;
            primary_dev = NULL;
        }
    }

    mtx.unlock();
    return 0;
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::openOutputStream(struct audio_hw_device *dev,
                                                               audio_io_handle_t handle,
                                                               audio_devices_t devices,
                                                               audio_output_flags_t flags,
                                                               audio_config_t *config,
                                                               audio_stream_out_t **cStream,
                                                               const char *address)
{
    if (cStream == NULL) {
        return -EINVAL;
    }
    *cStream = NULL;

    if (config == NULL) {
        return -EINVAL;
    }

    if (address == NULL) {
        return -EINVAL;
    }

    std::string deviceAddress(address);

    StreamOutInterface *cppStream;
    int error = static_cast<int>(
        getCppDevice(dev).openOutputStream(handle, devices, flags, *config, cppStream,
                                           deviceAddress));
    if (error != 0) {
        return error;
    }

    AUDIOCOMMS_ASSERT(cppStream != NULL, "Inconsistent return status: stream out is null "
                                         "but no error raised");

    *cStream = OutputStreamWrapper::bind(cppStream);

    if (*cStream == NULL) {
        delete cppStream;
        return -ENOMEM;
    }

    return 0;
}

template <class Device, int CDeviceApiVersion>
void DeviceWrapper<Device, CDeviceApiVersion>::closeOutputStream(struct audio_hw_device *dev,
                                                                 audio_stream_out_t *stream)
{
    StreamOutInterface *cppStream = OutputStreamWrapper::release(stream);

    AUDIOCOMMS_ASSERT(cppStream != NULL, "Inconsistent null stream");

    getCppDevice(dev).closeOutputStream(cppStream);
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::openInputStream(struct audio_hw_device *dev,
                                                              audio_io_handle_t handle,
                                                              audio_devices_t devices,
                                                              audio_config_t *config,
                                                              audio_stream_in_t **cStream,
                                                              audio_input_flags_t flags,
                                                              const char *address,
                                                              audio_source_t source)
{
    if (cStream == NULL) {
        return -EINVAL;
    }
    *cStream = NULL;

    if (config == NULL) {
        return -EINVAL;
    }

    if (address == NULL) {
        return -EINVAL;
    }

    std::string deviceAddress(address);

    StreamInInterface *cppStream;
    int error = static_cast<int>(
        getCppDevice(dev).openInputStream(handle, devices, *config,
                                          cppStream, flags, deviceAddress, source));
    if (error != 0) {
        return error;
    }

    AUDIOCOMMS_ASSERT(cppStream != NULL, "Inconsistent return status: stream in is null "
                                         "but no error raised");

    *cStream = InputStreamWrapper::bind(cppStream);

    if (*cStream == NULL) {
        delete cppStream;
        return -ENOMEM;
    }

    return 0;
}

template <class Device, int CDeviceApiVersion>
void DeviceWrapper<Device, CDeviceApiVersion>::closeInputStream(struct audio_hw_device *dev,
                                                                audio_stream_in_t *stream)
{
    StreamInInterface *cppStream = InputStreamWrapper::release(stream);

    AUDIOCOMMS_ASSERT(cppStream != NULL, "Inconsistent null stream");

    getCppDevice(dev).closeInputStream(cppStream);
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::initCheck(const struct audio_hw_device *dev)
{
    return static_cast<int>(getCppDevice(dev).initCheck());
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::setVoiceVolume(struct audio_hw_device *dev,
                                                             float volume)
{
    return static_cast<int>(getCppDevice(dev).setVoiceVolume(volume));
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::setMasterVolume(struct audio_hw_device *dev,
                                                              float volume)
{
    return static_cast<int>(getCppDevice(dev).setMasterVolume(volume));
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::getMasterVolume(struct audio_hw_device *dev,
                                                              float *volume)
{
    if (volume == NULL) {
        return -EINVAL;
    }
    return static_cast<int>(getCppDevice(dev).getMasterVolume(*volume));
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::setMasterMute(struct audio_hw_device *dev, bool mute)
{
    return static_cast<int>(getCppDevice(dev).setMasterMute(mute));
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::getMasterMute(struct audio_hw_device *dev,
                                                            bool *muted)
{
    if (muted == NULL) {
        return -EINVAL;
    }
    return static_cast<int>(getCppDevice(dev).getMasterMute(*muted));
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::setMode(struct audio_hw_device *dev,
                                                      audio_mode_t mode)
{
    return static_cast<int>(getCppDevice(dev).setMode(mode));
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::setMicMute(struct audio_hw_device *dev, bool state)
{
    return static_cast<int>(getCppDevice(dev).setMicMute(state));
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::getMicMute(const struct audio_hw_device *dev,
                                                         bool *state)
{
    if (state == NULL) {
        return -EINVAL;
    }
    return static_cast<int>(getCppDevice(dev).getMicMute(*state));
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::setParameters(struct audio_hw_device *dev,
                                                            const char *keyValuePairs)
{
    std::string keyValuePairList(keyValuePairs);
    return static_cast<int>(getCppDevice(dev).setParameters(keyValuePairList));
}

template <class Device, int CDeviceApiVersion>
char *DeviceWrapper<Device, CDeviceApiVersion>::getParameters(const struct audio_hw_device *dev,
                                                              const char *keys)
{
    std::string keyList(keys);
    return ::strdup(getCppDevice(dev).getParameters(keyList).c_str());
}

template <class Device, int CDeviceApiVersion>
size_t DeviceWrapper<Device, CDeviceApiVersion>::getInputBufferSize(
    const struct audio_hw_device *dev,
    const audio_config_t *config)
{
    AUDIOCOMMS_ASSERT(config != NULL, "Invalid call, config is NULL");

    return getCppDevice(dev).getInputBufferSize(*config);
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::dump(const struct audio_hw_device *dev, int fd)
{
    return static_cast<int>(getCppDevice(dev).dump(fd));
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::createAudioPatch(struct audio_hw_device *dev,
                                                               unsigned int num_sources,
                                                               const struct audio_port_config *sources,
                                                               unsigned int num_sinks,
                                                               const struct audio_port_config *sinks,
                                                               audio_patch_handle_t *handle)
{
    if (sources == NULL || sinks == NULL || handle == NULL) {
        return -EINVAL;
    }
    return static_cast<int>(getCppDevice(dev).createAudioPatch(num_sources,
                                                               sources,
                                                               num_sinks,
                                                               sinks,
                                                               *handle));
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::releaseAudioPatch(struct audio_hw_device *dev,
                                                                audio_patch_handle_t handle)
{
    return static_cast<int>(getCppDevice(dev).releaseAudioPatch(handle));
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::getAudioPort(struct audio_hw_device *dev,
                                                           struct audio_port *port)
{
    if (port == NULL) {
        return -EINVAL;
    }
    return static_cast<int>(getCppDevice(dev).getAudioPort(*port));
}

template <class Device, int CDeviceApiVersion>
int DeviceWrapper<Device, CDeviceApiVersion>::setAudioPortConfig(struct audio_hw_device *dev,
                                                                 const struct audio_port_config *config)
{
    if (config == NULL) {
        return -EINVAL;
    }
    return static_cast<int>(getCppDevice(dev).setAudioPortConfig(*config));
}

} // namespace intel_audio
