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

#pragma once

#include <StreamInterface.hpp>
#include <AudioCommsAssert.hpp>
#include <NonCopyable.hpp>
#include <hardware/audio.h>
#include <utils/Errors.h>
#include <string>


namespace intel_audio
{

class OutputStreamWrapper;
class InputStreamWrapper;

/*
 * Next traits describe the coupling between C, Cpp and wrapper structures.
 */
struct OutStreamTrait
{
    typedef StreamOutInterface CppStream;
    typedef audio_stream_out_t CStream;
    typedef OutputStreamWrapper Wrapper;
};

struct InStreamTrait
{
    typedef StreamInInterface CppStream;
    typedef audio_stream_in_t CStream;
    typedef InputStreamWrapper Wrapper;
};

/** Common part of stream wrapper */
template <class Trait>
class StreamWrapper : private audio_comms::utilities::NonCopyable
{
public:
    static typename Trait::CStream *bind(typename Trait::CppStream *cppStream)
    {
        typedef typename Trait::Wrapper Wrapper;
        Wrapper *wrapper = new Wrapper(cppStream);
        if (wrapper == NULL) {
            return NULL;
        }

        return wrapper->getCStream();
    }

    static typename Trait::CppStream *release(const typename Trait::CStream *stream)
    {
        typedef typename StreamWrapper<Trait>::Glue Glue;
        AUDIOCOMMS_ASSERT(stream != NULL, "Invalid stream");
        const Glue &glue = *reinterpret_cast<const Glue *>(stream);
        typename Trait::CppStream &cppStream = getCppStream(stream);
        delete glue.mWrapper;
        return &cppStream;
    }

protected:
    StreamWrapper(typename Trait::CppStream *cppStream)
    {
        glue.mCppStream = cppStream;
        glue.mWrapper = this;

        audio_stream_t &stream = glue.mCStream.common;
        stream.get_sample_rate = wrapGetSampleRate;
        stream.set_sample_rate = wrapSetSampleRate;
        stream.get_buffer_size = wrapGetBufferSize;
        stream.get_channels = wrapGetChannels;
        stream.get_format = wrapGetFormat;
        stream.set_format = wrapSetFormat;
        stream.standby = wrapStandby;
        stream.dump = wrapDump;
        stream.get_device = wrapGetDevice;
        stream.set_device = wrapSetDevice;
        stream.set_parameters = wrapSetParameters;
        stream.get_parameters = wrapGetParameters;
        stream.add_audio_effect = wrapAddAudioEffect;
        stream.remove_audio_effect = wrapRemoveAudioEffect;
    }

    typename Trait::CStream *getCStream()
    {
        return &glue.mCStream;
    }


    /* Helpers that convert C calls into C++ calls */
    static uint32_t wrapGetSampleRate(const audio_stream_t *stream);
    static int wrapSetSampleRate(audio_stream_t *stream, uint32_t rate);
    static size_t wrapGetBufferSize(const audio_stream_t *stream);
    static audio_channel_mask_t wrapGetChannels(const audio_stream_t *stream);
    static audio_format_t wrapGetFormat(const audio_stream_t *stream);
    static int wrapSetFormat(audio_stream_t *stream, audio_format_t format);
    static int wrapStandby(audio_stream_t *stream);
    static int wrapDump(const audio_stream_t *stream, int fd);
    static audio_devices_t wrapGetDevice(const audio_stream_t *stream);
    static int wrapSetDevice(audio_stream_t *stream, audio_devices_t device);
    static char *wrapGetParameters(const audio_stream_t *stream, const char *keys);
    static int wrapSetParameters(audio_stream_t *stream, const char *kvpairs);
    static int wrapAddAudioEffect(const audio_stream_t *stream,
                                  effect_handle_t effect);
    static int wrapRemoveAudioEffect(const audio_stream_t *stream,
                                     effect_handle_t effect);

    virtual ~StreamWrapper() {}

    static typename Trait::CppStream &getCppStream(const typename Trait::CStream *stream)
    {
        AUDIOCOMMS_ASSERT(stream != NULL, "Invalid stream");
        return getCppStream(&stream->common);
    }

    static typename Trait::CppStream &getCppStream(const audio_stream_t *stream)
    {
        typedef typename StreamWrapper<Trait>::Glue Glue;
        AUDIOCOMMS_ASSERT(stream != NULL, "Invalid stream");
        const Glue &glue = *reinterpret_cast<const Glue *>(stream);
        AUDIOCOMMS_ASSERT(glue.mCppStream != NULL,
                          "Inconsistent state: stream" << stream
                                                       << " is not null, but cpp stream is null");
        return *glue.mCppStream;
    }

    void setCppStream(typename Trait::CppStream *cppStream)
    {
        glue.mCppStream = cppStream;
    }

private:
    struct Glue
    {
        typename Trait::CStream mCStream;
        typename Trait::CppStream * mCppStream;
        StreamWrapper<Trait> *mWrapper;
    } glue;
};

/** Audio output stream wrapper. */
class OutputStreamWrapper : public StreamWrapper<OutStreamTrait>
{
private:
    /* The construction is possible only by the base class binding */
    friend class StreamWrapper<OutStreamTrait>;

    OutputStreamWrapper(StreamOutInterface *cppOutputStream);

    /* Helpers that convert C calls into C++ calls */
    static uint32_t wrapGetLatency(const audio_stream_out_t *stream);
    static int wrapSetVolume(audio_stream_out_t *stream, float left, float right);
    static ssize_t wrapWrite(audio_stream_out_t *stream, const void *buffer, size_t bytes);
    static int wrapGetRenderPosition(const audio_stream_out_t *stream, uint32_t *dspFrames);
    static int wrapGetNextWriteTimestamp(const audio_stream_out_t *stream, int64_t *timestamp);
    static int wrapFlush(audio_stream_out_t *stream);
    static int wrapSetCallback(audio_stream_out_t *stream,
                               stream_callback_t callback, void *cookie);
    static int wrapPause(audio_stream_out_t *stream);
    static int wrapResume(audio_stream_out_t *stream);
    static int wrapDrain(audio_stream_out_t *stream, audio_drain_type_t type);
    static int wrapGetPresentationPosition(const audio_stream_out_t *stream,
                                           uint64_t *frames, struct timespec *timestamp);
};

/** Audio input stream wrapper. */
class InputStreamWrapper : public StreamWrapper<InStreamTrait>
{
private:
    /* The construction is possible only by the base class binding */
    friend class StreamWrapper<InStreamTrait>;

    InputStreamWrapper(StreamInInterface *cppOutputStream);

    /* Helpers that convert C calls into C++ calls */
    static int wrapSetGain(audio_stream_in_t *stream, float gain);
    static ssize_t wrapRead(audio_stream_in_t *stream, void *buffer, size_t bytes);
    static uint32_t wrapGetInputFramesLost(audio_stream_in_t *stream);
};

template <class Trait>
uint32_t StreamWrapper<Trait>::wrapGetSampleRate(const audio_stream_t *stream)
{
    return getCppStream(stream).getSampleRate();
}

template <class Trait>
int StreamWrapper<Trait>::wrapSetSampleRate(audio_stream_t *stream, uint32_t rate)
{
    return static_cast<int>(getCppStream(stream).setSampleRate(rate));
}

template <class Trait>
size_t StreamWrapper<Trait>::wrapGetBufferSize(const audio_stream_t *stream)
{
    return getCppStream(stream).getBufferSize();
}

template <class Trait>
audio_channel_mask_t StreamWrapper<Trait>::wrapGetChannels(const audio_stream_t *stream)
{
    return getCppStream(stream).getChannels();
}

template <class Trait>
audio_format_t StreamWrapper<Trait>::wrapGetFormat(const audio_stream_t *stream)
{
    return getCppStream(stream).getFormat();
}

template <class Trait>
int StreamWrapper<Trait>::wrapSetFormat(audio_stream_t *stream, audio_format_t format)
{
    return static_cast<int>(getCppStream(stream).setFormat(format));
}

template <class Trait>
int StreamWrapper<Trait>::wrapStandby(audio_stream_t *stream)
{
    return static_cast<int>(getCppStream(stream).standby());
}

template <class Trait>
int StreamWrapper<Trait>::wrapDump(const audio_stream_t *stream, int fd)
{
    return static_cast<int>(getCppStream(stream).dump(fd));
}

template <class Trait>
audio_devices_t StreamWrapper<Trait>::wrapGetDevice(const audio_stream_t *stream)
{
    return getCppStream(stream).getDevice();
}

template <class Trait>
int StreamWrapper<Trait>::wrapSetDevice(audio_stream_t *stream, audio_devices_t device)
{
    return static_cast<int>(getCppStream(stream).setDevice(device));
}

template <class Trait>
char *StreamWrapper<Trait>::wrapGetParameters(const audio_stream_t *stream, const char *keys)
{
    std::string keyList(keys);
    return strdup(getCppStream(stream).getParameters(keyList).c_str());
}

template <class Trait>
int StreamWrapper<Trait>::wrapSetParameters(audio_stream_t *stream, const char *keyValuePairs)
{
    std::string keyValuePairList(keyValuePairs);
    return static_cast<int>(getCppStream(stream).setParameters(keyValuePairList));
}

template <class Trait>
int StreamWrapper<Trait>::wrapAddAudioEffect(const audio_stream_t *stream, effect_handle_t effect)
{
    return static_cast<int>(getCppStream(stream).addAudioEffect(effect));
}

template <class Trait>
int StreamWrapper<Trait>::wrapRemoveAudioEffect(const audio_stream_t *stream,
                                                effect_handle_t effect)
{
    return static_cast<int>(getCppStream(stream).removeAudioEffect(effect));
}

} // namespace intel_audio
