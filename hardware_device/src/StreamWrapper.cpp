/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2014-2015 Intel
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

#include <StreamWrapper.hpp>
#include <AudioCommsAssert.hpp>


namespace intel_audio
{

//
// OutputStreamWrapper class C/C++ wrapping
//
OutputStreamWrapper::OutputStreamWrapper(StreamOutInterface *cppOutputStream)
    : StreamWrapper<OutStreamTrait>(cppOutputStream)
{
    audio_stream_out_t &stream = *getCStream();

    stream.get_latency = wrapGetLatency;
    stream.set_volume = wrapSetVolume;
    stream.write = wrapWrite;
    stream.get_render_position = wrapGetRenderPosition;
    stream.get_next_write_timestamp = wrapGetNextWriteTimestamp;
    stream.flush = wrapFlush;
    stream.set_callback = wrapSetCallback;
    stream.pause = wrapPause;
    stream.resume = wrapResume;
    stream.drain = wrapDrain;
    stream.get_presentation_position = wrapGetPresentationPosition;
}

uint32_t OutputStreamWrapper::wrapGetLatency(const audio_stream_out_t *stream)
{
    return getCppStream(stream).getLatency();
}

int OutputStreamWrapper::wrapSetVolume(audio_stream_out_t *stream, float left, float right)
{
    return static_cast<int>(getCppStream(stream).setVolume(left, right));
}

ssize_t OutputStreamWrapper::wrapWrite(audio_stream_out_t *stream, const void *buffer, size_t bytes)
{
    android::status_t status = getCppStream(stream).write(buffer, bytes);
    return (status == android::OK) ? static_cast<ssize_t>(bytes) : static_cast<ssize_t>(status);
}

int OutputStreamWrapper::wrapGetRenderPosition(const audio_stream_out_t *stream,
                                               uint32_t *dspFrames)
{
    if (dspFrames == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }
    return static_cast<int>(getCppStream(stream).getRenderPosition(*dspFrames));
}

int OutputStreamWrapper::wrapGetNextWriteTimestamp(const audio_stream_out_t *stream,
                                                   int64_t *timestamp)
{
    if (timestamp == NULL) {
        return static_cast<int>(android::BAD_VALUE);
    }
    return static_cast<int>(getCppStream(stream).getNextWriteTimestamp(*timestamp));
}

int OutputStreamWrapper::wrapFlush(audio_stream_out_t *stream)
{
    return static_cast<int>(getCppStream(stream).flush());
}

int OutputStreamWrapper::wrapSetCallback(audio_stream_out_t *stream,
                                         stream_callback_t callback, void *cookie)
{
    return static_cast<int>(getCppStream(stream).setCallback(callback, cookie));
}

int OutputStreamWrapper::wrapPause(audio_stream_out_t *stream)
{
    return static_cast<int>(getCppStream(stream).pause());
}

int OutputStreamWrapper::wrapResume(audio_stream_out_t *stream)
{
    return static_cast<int>(getCppStream(stream).resume());
}

int OutputStreamWrapper::wrapDrain(audio_stream_out_t *stream, audio_drain_type_t type)
{
    return static_cast<int>(getCppStream(stream).drain(type));
}

int OutputStreamWrapper::wrapGetPresentationPosition(const audio_stream_out_t *stream,
                                                     uint64_t *frames, struct timespec *timestamp)
{
    if (frames == NULL || timestamp == NULL) {
        return -EINVAL;
    }
    return static_cast<int>(
        getCppStream(stream).getPresentationPosition(*frames, *timestamp));
}

//
// InputStreamWrapper class C/C++ wrapping
//

InputStreamWrapper::InputStreamWrapper(StreamInInterface *cppInputStream)
    : StreamWrapper<InStreamTrait>(cppInputStream)
{
    audio_stream_in_t &stream = *getCStream();

    stream.set_gain = wrapSetGain;
    stream.read = wrapRead;
    stream.get_input_frames_lost = wrapGetInputFramesLost;
}

int InputStreamWrapper::wrapSetGain(audio_stream_in_t *stream, float gain)
{
    return getCppStream(stream).setGain(gain);
}

ssize_t InputStreamWrapper::wrapRead(audio_stream_in_t *stream, void *buffer, size_t bytes)
{
    android::status_t status = getCppStream(stream).read(buffer, bytes);
    return (status == android::OK) ? static_cast<ssize_t>(bytes) : static_cast<ssize_t>(status);
}

uint32_t InputStreamWrapper::wrapGetInputFramesLost(audio_stream_in_t *stream)
{
    return getCppStream(stream).getInputFramesLost();
}


} // namespace intel_audio
