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

#include <AudioStream.hpp>
#include <AudioCommsAssert.hpp>


namespace intel_audio
{
namespace details
{

//
// Helper macros
//
#define CAST_TO_EXT_STREAM(constness, stream) \
    reinterpret_cast<constness struct AudioStream::ext *>(stream)

#define FORWARD_CALL_TO_STREAM_OUT_INSTANCE(constness, stream, function)                  \
    ({                                                                                    \
         constness struct AudioStream::ext *_ext = CAST_TO_EXT_STREAM(constness, stream); \
         AUDIOCOMMS_ASSERT(_ext != NULL && _ext->obj.out != NULL, "Invalid device");      \
         _ext->obj.out->function;                                                         \
     })

#define FORWARD_CALL_TO_STREAM_IN_INSTANCE(constness, stream, function)                   \
    ({                                                                                    \
         constness struct AudioStream::ext *_ext = CAST_TO_EXT_STREAM(constness, stream); \
         AUDIOCOMMS_ASSERT(_ext != NULL && _ext->obj.in != NULL, "Invalid device");       \
         _ext->obj.in->function;                                                          \
     })

#define FORWARD_CALL_TO_STREAM_INSTANCE(constness, stream, function)                          \
    ({                                                                                        \
         constness struct AudioStream::ext *_ext = CAST_TO_EXT_STREAM(constness, stream);     \
         AUDIOCOMMS_ASSERT(_ext != NULL &&                                                    \
                           _ext->dir == input ? _ext->obj.in != NULL : _ext->obj.out != NULL, \
                           "Invalid device");                                                 \
         _ext->dir == input ? _ext->obj.in->function : _ext->obj.out->function;               \
     })


//
// AudioStream class C/C++ wrapping
//
uint32_t AudioStream::wrapGetSampleRate(const audio_stream_t *stream)
{
    return FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, getSampleRate());
}

int AudioStream::wrapSetSampleRate(audio_stream_t *stream, uint32_t rate)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(, stream, setSampleRate(rate)));
}

size_t AudioStream::wrapGetBufferSize(const audio_stream_t *stream)
{
    return FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, getBufferSize());
}

audio_channel_mask_t AudioStream::wrapGetChannels(const audio_stream_t *stream)
{
    return FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, getChannels());
}

audio_format_t AudioStream::wrapGetFormat(const audio_stream_t *stream)
{
    return FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, getFormat());
}

int AudioStream::wrapSetFormat(audio_stream_t *stream, audio_format_t format)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(, stream, setFormat(format)));
}

int AudioStream::wrapStandby(audio_stream_t *stream)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(, stream, standby()));
}

int AudioStream::wrapDump(const audio_stream_t *stream, int fd)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, dump(fd)));
}

audio_devices_t AudioStream::wrapGetDevice(const audio_stream_t *stream)
{
    return FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, getDevice());
}

int AudioStream::wrapSetDevice(audio_stream_t *stream, audio_devices_t device)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(, stream, setDevice(device)));
}

char *AudioStream::wrapGetParameters(const audio_stream_t *stream, const char *keys)
{
    std::string keyList(keys);
    return strdup(FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, getParameters(keyList)).c_str());
}

int AudioStream::wrapSetParameters(audio_stream_t *stream, const char *keyValuePairs)
{
    std::string keyValuePairList(keyValuePairs);
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(, stream,
                                                            setParameters(keyValuePairList)));
}

int AudioStream::wrapAddAudioEffect(const audio_stream_t *stream, effect_handle_t effect)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(const, stream, addAudioEffect(effect)));
}

int AudioStream::wrapRemoveAudioEffect(const audio_stream_t *stream, effect_handle_t effect)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_INSTANCE(const, stream,
                                                            removeAudioEffect(effect)));
}

} // namespace details


//
// AudioStreamOut class C/C++ wrapping
//
uint32_t AudioStreamOut::wrapGetLatency(const audio_stream_out_t *stream)
{
    return FORWARD_CALL_TO_STREAM_OUT_INSTANCE(const, stream, getLatency());
}

int AudioStreamOut::wrapSetVolume(audio_stream_out_t *stream, float left, float right)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream, setVolume(left, right)));
}

ssize_t AudioStreamOut::wrapWrite(audio_stream_out_t *stream, const void *buffer, size_t bytes)
{
    android::status_t status = FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream, write(buffer, bytes));
    return (status == android::OK) ? static_cast<ssize_t>(bytes) : static_cast<ssize_t>(status);
}

int AudioStreamOut::wrapGetRenderPosition(const audio_stream_out_t *stream,
                                          uint32_t *dspFrames)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(const, stream,
                                                                getRenderPosition(dspFrames)));
}

int AudioStreamOut::wrapGetNextWriteTimestamp(const audio_stream_out_t *stream,
                                              int64_t *timestamp)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(const, stream,
                                                                getNextWriteTimestamp(timestamp)));
}

int AudioStreamOut::wrapFlush(const audio_stream_out_t *stream)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(const, stream, flush()));
}

int AudioStreamOut::wrapSetCallback(audio_stream_out_t *stream,
                                    stream_callback_t callback, void *cookie)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream,
                                                                setCallback(callback, cookie)));
}

int AudioStreamOut::wrapPause(audio_stream_out_t *stream)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream, pause()));
}

int AudioStreamOut::wrapResume(audio_stream_out_t *stream)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream, resume()));
}

int AudioStreamOut::wrapDrain(audio_stream_out_t *stream, audio_drain_type_t type)
{
    return static_cast<int>(FORWARD_CALL_TO_STREAM_OUT_INSTANCE(, stream, drain(type)));
}

int AudioStreamOut::wrapGetPresentationPosition(const audio_stream_out_t *stream,
                                                uint64_t *frames, struct timespec *timestamp)
{
    return static_cast<int>(
        FORWARD_CALL_TO_STREAM_OUT_INSTANCE(const, stream,
                                            getPresentationPosition(frames, timestamp)));
}


//
// AudioStreamIn class C/C++ wrapping
//
int AudioStreamIn::wrapSetGain(audio_stream_in_t *stream, float gain)
{
    return FORWARD_CALL_TO_STREAM_IN_INSTANCE(, stream, setGain(gain));
}

ssize_t AudioStreamIn::wrapRead(audio_stream_in_t *stream, void *buffer, size_t bytes)
{
    android::status_t status = FORWARD_CALL_TO_STREAM_IN_INSTANCE(, stream, read(buffer, bytes));
    return (status == android::OK) ? static_cast<ssize_t>(bytes) : static_cast<ssize_t>(status);
}

uint32_t AudioStreamIn::wrapGetInputFramesLost(audio_stream_in_t *stream)
{
    return FORWARD_CALL_TO_STREAM_IN_INSTANCE(, stream, getInputFramesLost());
}


} // namespace intel_audio
