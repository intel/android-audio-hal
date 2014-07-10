/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2014 Intel
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
 */

#include <AudioHwDevice.hpp>
#include <AudioCommsAssert.hpp>


namespace intel_audio
{

using details::AudioStream;

extern "C"
{
AudioHwDevice *createAudioHardware(void);

static struct hw_module_methods_t audio_module_methods = {
open: AudioHwDevice::wrapOpen
};

struct audio_module HAL_MODULE_INFO_SYM = {
common:
{
    tag: HARDWARE_MODULE_TAG,
    module_api_version: AUDIO_MODULE_API_VERSION_0_1,
    hal_api_version: HARDWARE_HAL_API_VERSION,
    id: AUDIO_HARDWARE_MODULE_ID,
    name: "Intel Audio HW HAL",
    author: "The Android Open Source Project",
    methods: &audio_module_methods,
    dso: NULL,
    reserved:
    {
        0
    },
}
};
}; // extern "C"


//
// Helper macros
//
#define FORWARD_CALL_TO_DEV_INSTANCE(constness, device, function)                       \
    ({                                                                                  \
         constness struct ext *_ext = reinterpret_cast<constness struct ext *>(device); \
         AUDIOCOMMS_ASSERT(_ext != NULL && _ext->obj != NULL, "Invalid device");        \
         _ext->obj->function;                                                           \
     })


//
// AudioHwDevice class C/C++ wrapping
//
int AudioHwDevice::wrapOpen(const hw_module_t *module, const char *name, hw_device_t **device)
{
    struct ext *ext_dev;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) {
        return -EINVAL;
    }

    ext_dev = (struct ext *)calloc(1, sizeof(*ext_dev));
    if (ext_dev == NULL) {
        return -ENOMEM;
    }

    ext_dev->device.common.tag = HARDWARE_DEVICE_TAG;
    ext_dev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    ext_dev->device.common.module = const_cast<hw_module_t *>(module);
    ext_dev->device.common.close = AudioHwDevice::wrapClose;

    ext_dev->device.init_check = AudioHwDevice::wrapInitCheck;
    ext_dev->device.set_voice_volume = AudioHwDevice::wrapSetVoiceVolume;
    ext_dev->device.set_master_volume = AudioHwDevice::wrapSetMasterVolume;
    ext_dev->device.get_master_volume = AudioHwDevice::wrapGetMasterVolume;
    ext_dev->device.set_master_mute = AudioHwDevice::wrapSetMasterMute;
    ext_dev->device.get_master_mute = AudioHwDevice::wrapGetMasterMute;
    ext_dev->device.set_mode = AudioHwDevice::wrapSetMode;
    ext_dev->device.set_mic_mute = AudioHwDevice::wrapSetMicMute;
    ext_dev->device.get_mic_mute = AudioHwDevice::wrapGetMicMute;
    ext_dev->device.set_parameters = AudioHwDevice::wrapSetParameters;
    ext_dev->device.get_parameters = AudioHwDevice::wrapGetParameters;
    ext_dev->device.get_input_buffer_size = AudioHwDevice::wrapGetInputBufferSize;
    ext_dev->device.open_output_stream = AudioHwDevice::wrapOpenOutputStream;
    ext_dev->device.close_output_stream = AudioHwDevice::wrapCloseOutputStream;
    ext_dev->device.open_input_stream = AudioHwDevice::wrapOpenInputStream;
    ext_dev->device.close_input_stream = AudioHwDevice::wrapCloseInputStream;
    ext_dev->device.dump = wrapDump;

    ext_dev->obj = createAudioHardware();
    if (ext_dev->obj == NULL) {
        free(ext_dev);
        return -EIO;
    }

    AUDIOCOMMS_ASSERT(device != NULL, "Invalid device argument");
    *device = reinterpret_cast<hw_device_t *>(ext_dev);
    return static_cast<int>(android::OK);
}

int AudioHwDevice::wrapClose(hw_device_t *device)
{
    struct ext *ext_dev = reinterpret_cast<struct ext *>(device);
    AUDIOCOMMS_ASSERT(ext_dev != NULL, "Invalid device argument");

    delete ext_dev->obj;
    ext_dev->obj = NULL;

    free(ext_dev);
    return static_cast<int>(android::OK);
}

int AudioHwDevice::wrapOpenOutputStream(struct audio_hw_device *dev,
                                        audio_io_handle_t handle,
                                        audio_devices_t devices,
                                        audio_output_flags_t flags,
                                        audio_config_t *config,
                                        audio_stream_out_t **stream_out)
{
    struct AudioStream::ext *ext_stream = (struct AudioStream::ext *)calloc(1, sizeof(*ext_stream));
    if (!ext_stream) {
        return static_cast<int>(android::NO_MEMORY);
    }

    struct ext *ext_dev = reinterpret_cast<struct ext *>(dev);
    AUDIOCOMMS_ASSERT(ext_dev != NULL, "Invalid device");

    int error = static_cast<int>(
        ext_dev->obj->openOutputStream(handle, devices, flags, config, &ext_stream->obj.out));
    if (error || ext_stream->obj.out == NULL) {
        free(ext_stream);
        *stream_out = NULL;
        return error;
    }

    ext_stream->dir = AudioStream::output;

    ext_stream->stream.out.common.get_sample_rate = AudioStream::wrapGetSampleRate;
    ext_stream->stream.out.common.set_sample_rate = AudioStream::wrapSetSampleRate;
    ext_stream->stream.out.common.get_buffer_size = AudioStream::wrapGetBufferSize;
    ext_stream->stream.out.common.get_channels = AudioStream::wrapGetChannels;
    ext_stream->stream.out.common.get_format = AudioStream::wrapGetFormat;
    ext_stream->stream.out.common.set_format = AudioStream::wrapSetFormat;
    ext_stream->stream.out.common.standby = AudioStream::wrapStandby;
    ext_stream->stream.out.common.dump = AudioStream::wrapDump;
    ext_stream->stream.out.common.get_device = AudioStream::wrapGetDevice;
    ext_stream->stream.out.common.set_device = AudioStream::wrapSetDevice;
    ext_stream->stream.out.common.set_parameters = AudioStream::wrapSetParameters;
    ext_stream->stream.out.common.get_parameters = AudioStream::wrapGetParameters;
    ext_stream->stream.out.common.add_audio_effect = AudioStream::wrapAddAudioEffect;
    ext_stream->stream.out.common.remove_audio_effect = AudioStream::wrapRemoveAudioEffect;

    ext_stream->stream.out.get_latency = AudioStreamOut::wrapGetLatency;
    ext_stream->stream.out.set_volume = AudioStreamOut::wrapSetVolume;
    ext_stream->stream.out.write = AudioStreamOut::wrapWrite;
    ext_stream->stream.out.get_render_position = AudioStreamOut::wrapGetRenderPosition;
    ext_stream->stream.out.get_next_write_timestamp = AudioStreamOut::wrapGetNextWriteTimestamp;
    ext_stream->stream.out.flush = AudioStreamOut::wrapFlush;
    ext_stream->stream.out.set_callback = AudioStreamOut::wrapSetCallback;
    ext_stream->stream.out.pause = AudioStreamOut::wrapPause;
    ext_stream->stream.out.resume = AudioStreamOut::wrapResume;
    ext_stream->stream.out.drain = AudioStreamOut::wrapDrain;
    ext_stream->stream.out.get_presentation_position = AudioStreamOut::wrapGetPresentationPosition;

    *stream_out = &ext_stream->stream.out;
    return static_cast<int>(android::OK);
}

void AudioHwDevice::wrapCloseOutputStream(struct audio_hw_device *dev,
                                          audio_stream_out_t *stream)
{
    struct ext *ext_dev = reinterpret_cast<struct ext *>(dev);
    AUDIOCOMMS_ASSERT(ext_dev != NULL && ext_dev->obj != NULL, "Invalid device");
    struct AudioStream::ext *ext_stream = reinterpret_cast<struct AudioStream::ext *>(stream);
    AUDIOCOMMS_ASSERT(ext_stream != NULL && ext_stream->obj.out != NULL, "Invalid stream");

    ext_dev->obj->closeOutputStream(ext_stream->obj.out);
    free(ext_stream);
}

int AudioHwDevice::wrapOpenInputStream(struct audio_hw_device *dev,
                                       audio_io_handle_t handle,
                                       audio_devices_t devices,
                                       audio_config_t *config,
                                       audio_stream_in_t **stream_in)
{
    struct AudioStream::ext *ext_stream = (struct AudioStream::ext *)calloc(1, sizeof(*ext_stream));
    if (ext_stream == NULL) {
        return static_cast<int>(android::NO_MEMORY);
    }

    struct ext *ext_dev = reinterpret_cast<struct ext *>(dev);
    AUDIOCOMMS_ASSERT(ext_dev != NULL, "Invalid device");
    int error = static_cast<int>(
        ext_dev->obj->openInputStream(handle, devices, config, &ext_stream->obj.in));
    if (ext_stream->obj.in == NULL) {
        free(ext_stream);
        *stream_in = NULL;
        return error;
    }

    ext_stream->dir = AudioStream::input;

    ext_stream->stream.in.common.get_sample_rate = AudioStream::wrapGetSampleRate;
    ext_stream->stream.in.common.set_sample_rate = AudioStream::wrapSetSampleRate;
    ext_stream->stream.in.common.get_buffer_size = AudioStream::wrapGetBufferSize;
    ext_stream->stream.in.common.get_channels = AudioStream::wrapGetChannels;
    ext_stream->stream.in.common.get_format = AudioStream::wrapGetFormat;
    ext_stream->stream.in.common.set_format = AudioStream::wrapSetFormat;
    ext_stream->stream.in.common.standby = AudioStream::wrapStandby;
    ext_stream->stream.in.common.dump = AudioStream::wrapDump;
    ext_stream->stream.in.common.get_device = AudioStream::wrapGetDevice;
    ext_stream->stream.in.common.set_device = AudioStream::wrapSetDevice;
    ext_stream->stream.in.common.set_parameters = AudioStream::wrapSetParameters;
    ext_stream->stream.in.common.get_parameters = AudioStream::wrapGetParameters;
    ext_stream->stream.in.common.add_audio_effect = AudioStream::wrapAddAudioEffect;
    ext_stream->stream.in.common.remove_audio_effect = AudioStream::wrapRemoveAudioEffect;

    ext_stream->stream.in.set_gain = AudioStreamIn::wrapSetGain;
    ext_stream->stream.in.read = AudioStreamIn::wrapRead;
    ext_stream->stream.in.get_input_frames_lost = AudioStreamIn::wrapGetInputFramesLost;

    *stream_in = &ext_stream->stream.in;
    return static_cast<int>(android::OK);
}

void AudioHwDevice::wrapCloseInputStream(struct audio_hw_device *dev,
                                         audio_stream_in_t *stream)
{
    struct ext *ext_dev = reinterpret_cast<struct ext *>(dev);
    AUDIOCOMMS_ASSERT(ext_dev != NULL && ext_dev->obj != NULL, "Invalid device");
    struct AudioStream::ext *ext_stream = reinterpret_cast<struct AudioStream::ext *>(stream);
    AUDIOCOMMS_ASSERT(ext_stream != NULL && ext_stream->obj.in != NULL, "Invalid stream");

    ext_dev->obj->closeInputStream(ext_stream->obj.in);
    free(ext_stream);
}

int AudioHwDevice::wrapInitCheck(const struct audio_hw_device *dev)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, initCheck()));
}

int AudioHwDevice::wrapSetVoiceVolume(struct audio_hw_device *dev, float volume)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setVoiceVolume(volume)));
}

int AudioHwDevice::wrapSetMasterVolume(struct audio_hw_device *dev, float volume)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setMasterVolume(volume)));
}

int AudioHwDevice::wrapGetMasterVolume(struct audio_hw_device *dev, float *volume)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, getMasterVolume(volume)));
}

int AudioHwDevice::wrapSetMasterMute(struct audio_hw_device *dev, bool mute)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setMasterMute(mute)));
}

int AudioHwDevice::wrapGetMasterMute(struct audio_hw_device *dev, bool *muted)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, getMasterMute(muted)));
}

int AudioHwDevice::wrapSetMode(struct audio_hw_device *dev, audio_mode_t mode)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setMode(mode)));
}

int AudioHwDevice::wrapSetMicMute(struct audio_hw_device *dev, bool state)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setMicMute(state)));
}

int AudioHwDevice::wrapGetMicMute(const struct audio_hw_device *dev, bool *state)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, getMicMute(state)));
}

int AudioHwDevice::wrapSetParameters(struct audio_hw_device *dev, const char *keyValuePairs)
{
    std::string keyValuePairList(keyValuePairs);
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(, dev, setParameters(keyValuePairList)));
}

char *AudioHwDevice::wrapGetParameters(const struct audio_hw_device *dev, const char *keys)
{
    std::string keyList(keys);
    return strdup(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, getParameters(keyList)).c_str());
}

size_t AudioHwDevice::wrapGetInputBufferSize(const struct audio_hw_device *dev,
                                             const audio_config_t *config)
{
    return FORWARD_CALL_TO_DEV_INSTANCE(const, dev, getInputBufferSize(config));
}

int AudioHwDevice::wrapDump(const struct audio_hw_device *dev, int fd)
{
    return static_cast<int>(FORWARD_CALL_TO_DEV_INSTANCE(const, dev, dump(fd)));
}

} // namespace intel_audio
