/*
 * Copyright (C) 2014-2015 Intel Corporation
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
    stream.start = NULL;
    stream.stop = NULL;
    stream.create_mmap_buffer = NULL;
    stream.get_mmap_position = NULL;
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
    stream.get_capture_position = wrapGetCapturePosition;
    stream.start = NULL;
    stream.stop = NULL;
    stream.create_mmap_buffer = NULL;
    stream.get_mmap_position = NULL;
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

int InputStreamWrapper::wrapGetCapturePosition(const audio_stream_in *stream,
                                               int64_t *frames, int64_t *time)
{
    if (frames == NULL || time == NULL) {
        return -EINVAL;
    }
    return static_cast<int>(getCppStream(stream).getCapturePosition(*frames, *time));
}


} // namespace intel_audio
